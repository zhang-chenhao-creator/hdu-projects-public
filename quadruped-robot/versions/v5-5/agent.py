#!/usr/bin/env python3
# -*- coding: UTF-8 -*-
###########################################################################
# Copyright © 1998 - 2026 Tencent. All Rights Reserved.
###########################################################################
"""
Author: Tencent AI Arena Authors

Robot Vacuum Agent.
清扫大作战 Agent 主类。

Checkpoint 兼容性（继承自 v4_p4）：
  - save_model 带元数据包装（state_dict + version + obs_dim 等）
  - load_model 形状安全部分恢复（self._dim_safe_load_state_dict）
  - model 结构、ACTION_NUM、obs_dim 全部不变 → 与 v4_p4 ckpt 100% 兼容

P3 + 自动校准（本次新增）：
  - 仅作用于 exploit() 路径（评估/提交）；predict()/learn()/save/load 全部不动
  - 开局 ≤16 步自动校准 ACTION_DELTAS（无需人工填表）
  - 校准结果写入类变量 Agent._cached_deltas,跨 episode 复用
  - 校准完成后启用：
      P0: 撞墙屏蔽（基于 passable_map）
      P1: 污染桩屏蔽（基于 NPC 距桩 ≤ 3）
      P2: 低电强制回充（≤20% 朝安全桩,≤8% 朝任意桩）
      P3: 满电离桩（≥92% 在桩格上时屏蔽留桩动作）
  - 部分校准（≥4 个方向）也启用,未知方向自动跳过该规则
  - 任何异常 → fallback baseline,不影响主流程
"""

import os
import traceback

import torch

torch.set_num_threads(1)
torch.set_num_interop_threads(1)

import numpy as np

from agent_ppo.algorithm.algorithm import Algorithm
from agent_ppo.conf.conf import Config
from agent_ppo.feature.definition import ActData, ObsData
from agent_ppo.feature.preprocessor import Preprocessor
from agent_ppo.model.model import Model
from kaiwudrl.interface.agent import BaseAgent


# -------------------- Checkpoint 格式常量 --------------------
CKPT_KEY_STATE_DICT = "state_dict"
CKPT_KEY_VERSION = "model_version"
CKPT_KEY_OBS_DIM = "obs_dim"
CKPT_KEY_ACT_NUM = "act_num"
CKPT_KEY_ARCH = "arch_signature"


class Agent(BaseAgent):
    # ---- 类变量：跨 episode 缓存校准结果 ----
    # None = 未校准；list of (dx, dz) or None = 部分/全部校准结果
    # 一旦写入（无论成功还是部分成功）,后续 episode 直接复用,不再重新校准。
    _cached_deltas = None
    # 校准失败次数(全部 None,完全失败)；超过阈值后锁死 fallback,不再尝试。
    _calib_fail_count = 0
    _CALIB_FAIL_LOCK_THRESHOLD = 3

    def __init__(self, agent_type="player", device=None, logger=None, monitor=None):
        torch.manual_seed(0)
        self.device = device
        self.model = Model(device).to(self.device)
        self.optimizer = torch.optim.Adam(
            params=self.model.parameters(),
            lr=Config.INIT_LEARNING_RATE_START,
            betas=(0.9, 0.999),
            eps=1e-8,
        )
        self.logger = logger
        self.monitor = monitor
        self.algorithm = Algorithm(self.model, self.optimizer, self.device, self.logger, self.monitor)
        self.preprocessor = Preprocessor()
        self.last_action = -1
        self.last_reward = 0.0

        # ---- 校准 / override 状态（每 episode reset） ----
        self._action_deltas_runtime = [None] * Config.ACTION_NUM
        self._calib_done = False
        self._calib_attempts = 0
        self._calib_pending_action = None
        self._calib_pending_pos = None
        self._calib_next_idx = 0  # round-robin 指针,避免卡在永远撞墙的 action 上
        self._prev_pos_for_mapping = None

        super().__init__(agent_type, device, logger, monitor)

    def reset(self, env_obs):
        """Reset per-episode state."""
        self.preprocessor = Preprocessor()
        self.last_action = -1
        self.last_reward = 0.0
        self._prev_pos_for_mapping = None

        # ---- 校准状态 ----
        # 优先用类级别缓存(跨 episode 复用)
        if Agent._cached_deltas is not None:
            self._action_deltas_runtime = list(Agent._cached_deltas)
            self._calib_done = True
            self._calib_attempts = 0
            self._calib_pending_action = None
            self._calib_pending_pos = None
            self._calib_next_idx = 0
        elif (Agent._calib_fail_count >= Agent._CALIB_FAIL_LOCK_THRESHOLD
              or not getattr(Config, "SAFETY_AUTO_CALIBRATE", True)):
            # 多次失败 → 锁死 fallback；或用户关闭了自动校准
            self._action_deltas_runtime = [None] * Config.ACTION_NUM
            self._calib_done = True   # 标记完成,直接进运行期(走 Config.ACTION_DELTAS or NO-OP)
            self._calib_attempts = 0
            self._calib_pending_action = None
            self._calib_pending_pos = None
            self._calib_next_idx = 0
        else:
            # 进入校准期
            self._action_deltas_runtime = [None] * Config.ACTION_NUM
            self._calib_done = False
            self._calib_attempts = 0
            self._calib_pending_action = None
            self._calib_pending_pos = None
            self._calib_next_idx = 0

    def observation_process(self, env_obs):
        """Convert raw env_obs to ObsData (feature + legal action mask)."""
        feature, legal_action, reward = self.preprocessor.feature_process(env_obs, self.last_action)
        self.last_reward = reward

        obs_data = ObsData(
            feature=list(feature),
            legal_action=legal_action,
        )
        remain_info = {}
        return obs_data, remain_info

    def action_process(self, act_data, is_stochastic=True):
        """Extract int action from ActData and update last_action."""
        action = act_data.action if is_stochastic else act_data.d_action
        self.last_action = int(action[0])
        return self.last_action

    def predict(self, list_obs_data):
        """Stochastic inference for training (exploration).

        训练路径,不应用 safety override / 不参与校准。
        """
        obs_data = list_obs_data[0]
        feature = obs_data.feature
        legal_action = obs_data.legal_action

        logits, value = self._run_model(feature)

        legal_arr = np.array(legal_action, dtype=np.float32)
        prob = self._legal_soft_max(logits, legal_arr)
        action = self._legal_sample(prob, use_max=False)
        d_action = self._legal_sample(prob, use_max=True)

        return [
            ActData(
                action=[action],
                d_action=[d_action],
                prob=list(prob),
                value=value,
            )
        ]

    def exploit(self, env_obs):
        """Greedy inference + auto-calibration + safety override.

        三段式：
          1) 校准期：派 action 0~7,反推 deltas
          2) 校准转运行期的过渡帧：完成结果上报,并按当前帧 obs 走运行期路径
          3) 运行期：safety override + greedy predict
        """
        # 一律先做 obs 处理,因为 calibration 需要 cur_pos
        obs_data, _ = self.observation_process(env_obs)

        # ---- 1. 校准期 ----
        if not self._calib_done:
            try:
                next_action = self._calibration_step()
            except Exception as e:
                if self.logger is not None:
                    self.logger.warning(f"[CALIB] step crashed, abort calibration: {e}")
                self._calib_done = True
                next_action = None

            if next_action is not None:
                self.last_action = int(next_action)
                self._prev_pos_for_mapping = self.preprocessor.cur_pos
                return self.last_action
            # next_action is None → 校准刚完成,fall through 到运行期

        # ---- 2/3. 运行期 ----
        self._log_action_mapping_debug()

        try:
            safe_legal, forced_action = self._apply_safety_rules(obs_data.legal_action)
        except Exception as e:
            if self.logger is not None:
                self.logger.warning(f"[SAFETY] apply crashed, fallback: {e}")
            safe_legal, forced_action = list(obs_data.legal_action), None

        if forced_action is not None:
            self.last_action = int(forced_action)
            self._prev_pos_for_mapping = self.preprocessor.cur_pos
            return self.last_action

        if safe_legal != obs_data.legal_action:
            obs_data = ObsData(
                feature=obs_data.feature,
                legal_action=safe_legal,
            )

        act_data = self.predict([obs_data])[0]
        action = self.action_process(act_data, is_stochastic=False)
        self._prev_pos_for_mapping = self.preprocessor.cur_pos
        return action

    def learn(self, list_sample_data):
        """Delegate to Algorithm for PPO update."""
        return self.algorithm.learn(list_sample_data)

    # =========================================================
    # Auto-Calibration
    # =========================================================

    def _calibration_step(self):
        """Drive one step of action-mapping calibration.

        校准状态机：
          - pending_action != None: 上一步派出了某个 action,这一步先收割结果
              cur_pos == pending_pos → 撞墙,不记录,下一轮再试该 action
              cur_pos != pending_pos → 记录 delta = cur - prev
          - 检查是否凑齐 / 超时 → 设 _calib_done
          - 未完成 → 选下一个未确定的 action 派出,设 pending,返回该 action
          - 完成 → 返回 None（让 exploit 走运行期）

        Returns:
            int (action 0-7) 或 None
        """
        # 1) 收割上一步结果
        if self._calib_pending_action is not None and self._calib_pending_pos is not None:
            cur_pos = self.preprocessor.cur_pos
            dx = cur_pos[0] - self._calib_pending_pos[0]
            dz = cur_pos[1] - self._calib_pending_pos[1]
            if (dx, dz) != (0, 0):
                idx = self._calib_pending_action
                # 仅在尚未确定时写入(避免后续轮次覆盖)
                if 0 <= idx < Config.ACTION_NUM and self._action_deltas_runtime[idx] is None:
                    self._action_deltas_runtime[idx] = (int(dx), int(dz))

        # 2) 完成检查
        all_known = all(d is not None for d in self._action_deltas_runtime)
        max_steps = getattr(Config, "CALIB_MAX_STEPS", 16)
        timeout = self._calib_attempts >= max_steps

        if all_known or timeout:
            self._finalize_calibration(timeout=timeout)
            return None

        # 3) 选下一个未确定的 action（round-robin,避免卡在永远撞墙的 action 上）
        next_action = None
        for offset in range(Config.ACTION_NUM):
            idx = (self._calib_next_idx + offset) % Config.ACTION_NUM
            if self._action_deltas_runtime[idx] is None:
                next_action = idx
                self._calib_next_idx = (idx + 1) % Config.ACTION_NUM
                break
        # 上面如果 all_known 早就 return 了,这里 next_action 必然 != None

        # 4) 派出
        self._calib_pending_action = next_action
        self._calib_pending_pos = self.preprocessor.cur_pos
        self._calib_attempts += 1
        return next_action

    def _finalize_calibration(self, timeout):
        """Conclude calibration, persist to class cache, log result."""
        self._calib_done = True
        self._calib_pending_action = None
        self._calib_pending_pos = None

        n_known = sum(1 for d in self._action_deltas_runtime if d is not None)
        min_known = getattr(Config, "CALIB_MIN_KNOWN", 4)

        if n_known >= min_known:
            # 写入类缓存(后续 episode 复用)
            Agent._cached_deltas = list(self._action_deltas_runtime)
            Agent._calib_fail_count = 0
        else:
            Agent._calib_fail_count += 1

        if getattr(Config, "CALIB_LOG", True) and self.logger is not None:
            status = "TIMEOUT" if timeout else "OK"
            self.logger.info(
                f"[CALIB] {status} steps={self._calib_attempts} "
                f"known={n_known}/{Config.ACTION_NUM} "
                f"deltas={self._action_deltas_runtime} "
                f"cached={'yes' if Agent._cached_deltas is not None else 'no'} "
                f"fail_count={Agent._calib_fail_count}"
            )

    # =========================================================
    # Safety Override
    # =========================================================

    def _resolve_deltas(self):
        """Pick the best available deltas: runtime > cache > Config static."""
        rt = getattr(self, "_action_deltas_runtime", None)
        if rt is not None and any(d is not None for d in rt):
            return rt
        if Agent._cached_deltas is not None and any(d is not None for d in Agent._cached_deltas):
            return Agent._cached_deltas
        cfg = getattr(Config, "ACTION_DELTAS", None)
        if cfg is not None and len(cfg) == Config.ACTION_NUM:
            return list(cfg)
        return None

    def _log_action_mapping_debug(self):
        """Optional debug helper for inferring ACTION_DELTAS by hand."""
        if not getattr(Config, "SAFETY_LOG_ACTION_MAPPING", False):
            return
        if self.logger is None:
            return
        if self._prev_pos_for_mapping is None or self.last_action < 0:
            return
        try:
            px, pz = self._prev_pos_for_mapping
            cx, cz = self.preprocessor.cur_pos
            dx, dz = cx - px, cz - pz
            self.logger.info(
                f"[ACTION_MAP_DEBUG] action={self.last_action} "
                f"prev=({px},{pz}) cur=({cx},{cz}) delta=({dx},{dz})"
            )
        except Exception:
            pass

    def _apply_safety_rules(self, legal_action):
        """Inference-only safety rules. Returns (safe_legal, forced_action_or_None).

        防呆：
          - deltas 完全不可用 → NO-OP
          - 部分可用 → 未知方向跳过该规则
          - 屏蔽到无合法动作 → 恢复原 legal_action（不 force）
          - 任何异常 → 调用方 fallback baseline
        """
        deltas = self._resolve_deltas()
        if deltas is None:
            return list(legal_action), None
        if len(deltas) != len(legal_action):
            return list(legal_action), None

        pp = self.preprocessor
        if not hasattr(pp, "cur_pos"):
            return list(legal_action), None
        hx, hz = pp.cur_pos
        if hx == 0 and hz == 0:
            return list(legal_action), None

        battery = getattr(pp, "battery", 1)
        battery_max = max(getattr(pp, "battery_max", 1), 1)
        battery_ratio = battery / battery_max

        legal = list(legal_action)
        original_legal = list(legal_action)
        forced_action = None
        triggered = []

        # ---- 解析 NPC / 充电桩 ----
        npc_positions = []
        for npc in getattr(pp, "npcs", []) or []:
            try:
                npc_positions.append((int(npc["pos"]["x"]), int(npc["pos"]["z"])))
            except (KeyError, TypeError, ValueError):
                pass

        charger_positions = []
        for org in getattr(pp, "organs", []) or []:
            try:
                if org.get("sub_type") == 1:
                    charger_positions.append((int(org["pos"]["x"]), int(org["pos"]["z"])))
            except (KeyError, TypeError, ValueError):
                pass

        # 区分安全桩 / 污染桩(与 preprocessor._get_dangerous_charger_penalty 同口径)
        danger_npc_radius = getattr(pp, "DANGER_CHARGER_NPC_RADIUS", 3)
        approach_radius = getattr(pp, "DANGER_CHARGER_APPROACH_RADIUS", 4)

        safe_chargers, danger_chargers = [], []
        for cx, cz in charger_positions:
            if not npc_positions:
                safe_chargers.append((cx, cz))
                continue
            min_npc_dist = min(
                max(abs(nx - cx), abs(nz - cz)) for nx, nz in npc_positions
            )
            if min_npc_dist > danger_npc_radius:
                safe_chargers.append((cx, cz))
            else:
                danger_chargers.append((cx, cz))

        # ---- P0: 撞墙规避 ----
        if getattr(Config, "SAFETY_BLOCK_WALL", True):
            grid_size = getattr(pp, "GRID_SIZE", 128)
            passable = getattr(pp, "passable_map", None)
            if passable is not None:
                blocked = 0
                for i, d in enumerate(deltas):
                    if d is None or legal[i] == 0:
                        continue
                    dx, dz = d
                    nx, nz = hx + dx, hz + dz
                    if 0 <= nx < grid_size and 0 <= nz < grid_size:
                        if passable[nx, nz] == 0:
                            legal[i] = 0
                            blocked += 1
                if blocked > 0:
                    triggered.append(f"P0_wall x{blocked}")

        # ---- P1: 污染桩规避 ----
        if getattr(Config, "SAFETY_BLOCK_DANGER_CHARGER", True) and danger_chargers:
            blocked = 0
            for i, d in enumerate(deltas):
                if d is None or legal[i] == 0:
                    continue
                dx, dz = d
                nx, nz = hx + dx, hz + dz
                for cx, cz in danger_chargers:
                    cur_d = max(abs(hx - cx), abs(hz - cz))
                    next_d = max(abs(nx - cx), abs(nz - cz))
                    if cur_d <= approach_radius and next_d < cur_d:
                        legal[i] = 0
                        blocked += 1
                        break
            if blocked > 0:
                triggered.append(f"P1_danger x{blocked}")

        # ---- P3: 满电离桩(在 P2 之前,因为满电不应再 force_charge) ----
        if (getattr(Config, "SAFETY_LEAVE_CHARGER", True)
                and battery_ratio >= getattr(Config, "LEAVE_CHARGER_THRESHOLD", 0.92)
                and charger_positions):
            on_charger = any(
                max(abs(hx - cx), abs(hz - cz)) == 0
                for cx, cz in charger_positions
            )
            if on_charger:
                blocked = 0
                for i, d in enumerate(deltas):
                    if d is None or legal[i] == 0:
                        continue
                    dx, dz = d
                    nx, nz = hx + dx, hz + dz
                    still_on_charger = any(
                        max(abs(nx - cx), abs(nz - cz)) == 0
                        for cx, cz in charger_positions
                    )
                    if still_on_charger:
                        legal[i] = 0
                        blocked += 1
                if blocked > 0:
                    triggered.append(f"P3_leave x{blocked}")

        # ---- 屏蔽兜底 ----
        if not any(legal):
            if getattr(Config, "SAFETY_LOG_OVERRIDE", False) and self.logger is not None:
                self.logger.info(
                    f"[SAFETY] all-blocked@({hx},{hz}) batt={battery_ratio:.2f}, "
                    f"restore. triggered={triggered}"
                )
            return original_legal, None

        # ---- P2: 低电强制回充 ----
        if getattr(Config, "SAFETY_FORCE_CHARGE", True):
            emergency_thr = getattr(Config, "EMERGENCY_CHARGE_THRESHOLD", 0.08)
            force_thr = getattr(Config, "FORCE_CHARGE_THRESHOLD", 0.20)
            max_dist = getattr(Config, "FORCE_CHARGE_MAX_DIST", 30)

            target_chargers = None
            mode = None
            if battery_ratio < emergency_thr and charger_positions:
                target_chargers = charger_positions
                mode = "emergency"
            elif battery_ratio < force_thr and safe_chargers:
                target_chargers = safe_chargers
                mode = "normal"

            if target_chargers:
                best_charger = None
                best_dist = 1 << 30
                for cx, cz in target_chargers:
                    d = max(abs(hx - cx), abs(hz - cz))
                    if d < best_dist:
                        best_dist = d
                        best_charger = (cx, cz)

                if (best_charger is not None and 0 < best_dist <= max_dist):
                    cx, cz = best_charger
                    best_action = None
                    best_score = 0  # 必须严格 > 0
                    for i, d in enumerate(deltas):
                        if d is None or legal[i] == 0:
                            continue
                        dx, dz = d
                        nx, nz = hx + dx, hz + dz
                        next_d = max(abs(nx - cx), abs(nz - cz))
                        score = best_dist - next_d
                        if score > best_score:
                            best_score = score
                            best_action = i

                    if best_action is not None:
                        forced_action = best_action
                        triggered.append(
                            f"P2_charge[{mode}]→a{best_action}"
                            f"(target=({cx},{cz}),d={best_dist})"
                        )

        if (triggered
                and getattr(Config, "SAFETY_LOG_OVERRIDE", False)
                and self.logger is not None):
            self.logger.info(
                f"[SAFETY] @({hx},{hz}) batt={battery_ratio:.2f} "
                f"safe={len(safe_chargers)} danger={len(danger_chargers)} "
                f"legal:{original_legal}→{legal} forced={forced_action} "
                f"rules={triggered}"
            )

        return legal, forced_action

    # =========================================================
    # Checkpoint I/O
    # =========================================================

    def save_model(self, path=None, id="1"):
        """Save model checkpoint with metadata wrapper."""
        model_file_path = f"{path}/model.ckpt-{id}.pkl"
        state_dict_cpu = {k: v.detach().clone().cpu() for k, v in self.model.state_dict().items()}

        payload = {
            CKPT_KEY_STATE_DICT: state_dict_cpu,
            CKPT_KEY_VERSION: getattr(Config, "MODEL_VERSION", "unknown"),
            CKPT_KEY_OBS_DIM: int(Config.DIM_OF_OBSERVATION),
            CKPT_KEY_ACT_NUM: int(Config.ACTION_NUM),
            CKPT_KEY_ARCH: {k: list(v.shape) for k, v in state_dict_cpu.items()},
        }

        try:
            torch.save(payload, model_file_path)
            self.logger.info(
                f"save model {model_file_path} successfully "
                f"(version={payload[CKPT_KEY_VERSION]}, obs_dim={payload[CKPT_KEY_OBS_DIM]})"
            )
        except Exception as e:
            self.logger.error(f"save model {model_file_path} failed: {e}")

    def load_model(self, path=None, id="1"):
        """Load model checkpoint with shape-aware partial recovery."""
        model_file_path = f"{path}/model.ckpt-{id}.pkl"

        if not os.path.isfile(model_file_path):
            self.logger.warning(f"load model: {model_file_path} not found, keeping current weights")
            return

        ckpt = self._safe_torch_load(model_file_path)
        if ckpt is None:
            return

        state_dict, meta_info = self._unwrap_checkpoint(ckpt)
        if state_dict is None:
            self.logger.error(f"load model {model_file_path} failed: unrecognized checkpoint format")
            return

        try:
            stats = self._dim_safe_load_state_dict(state_dict)
        except Exception as e:
            self.logger.error(
                f"load model {model_file_path} partial-load crashed: {e}\n{traceback.format_exc()}"
            )
            return

        meta_str = ""
        if meta_info:
            ver = meta_info.get(CKPT_KEY_VERSION, "?")
            od = meta_info.get(CKPT_KEY_OBS_DIM, "?")
            meta_str = f" [ckpt v={ver}, obs_dim={od}]"

        if stats["partial"] > 0 or stats["skipped"] > 0 or stats["missing"] > 0:
            self.logger.info(
                f"load model {model_file_path}{meta_str}: "
                f"matched={stats['matched']}, partial={stats['partial']}, "
                f"skipped={stats['skipped']}, missing={stats['missing']}"
            )
            if stats["partial_detail"]:
                self.logger.info(f"  partial layers: {stats['partial_detail']}")
            if stats["skipped_detail"]:
                self.logger.info(f"  skipped layers: {stats['skipped_detail']}")
            if stats["missing_detail"]:
                self.logger.info(f"  missing layers: {stats['missing_detail']}")
        else:
            self.logger.info(
                f"load model {model_file_path} successfully{meta_str} "
                f"(all {stats['matched']} params matched)"
            )

    def _safe_torch_load(self, path):
        last_err = None
        for kwargs in (
            {"map_location": self.device, "weights_only": False},
            {"map_location": self.device},
        ):
            try:
                return torch.load(path, **kwargs)
            except TypeError as e:
                last_err = e
                continue
            except Exception as e:
                last_err = e
                break
        self.logger.error(f"load model {path} failed to unpickle: {last_err}")
        return None

    def _unwrap_checkpoint(self, ckpt):
        if not isinstance(ckpt, dict):
            return None, None

        if CKPT_KEY_STATE_DICT in ckpt and isinstance(ckpt[CKPT_KEY_STATE_DICT], dict):
            sd = ckpt[CKPT_KEY_STATE_DICT]
            meta = {k: v for k, v in ckpt.items() if k != CKPT_KEY_STATE_DICT}
            return sd, meta

        if len(ckpt) > 0 and all(isinstance(v, torch.Tensor) for v in ckpt.values()):
            return ckpt, None

        return None, None

    def _dim_safe_load_state_dict(self, src_state_dict):
        tgt_state_dict = self.model.state_dict()
        matched, partial, skipped = [], [], []

        missing = [n for n in tgt_state_dict.keys() if n not in src_state_dict]

        src_only = [n for n in src_state_dict.keys() if n not in tgt_state_dict]
        for n in src_only:
            skipped.append(f"{n}(src-only)")

        new_state_dict = {k: v.detach().clone() for k, v in tgt_state_dict.items()}
        for name, tgt_tensor in tgt_state_dict.items():
            if name not in src_state_dict:
                continue
            src_tensor = src_state_dict[name]
            if not isinstance(src_tensor, torch.Tensor):
                skipped.append(f"{name}(not-tensor)")
                continue

            if tuple(src_tensor.shape) == tuple(tgt_tensor.shape):
                new_state_dict[name] = src_tensor.to(tgt_tensor.dtype).to(tgt_tensor.device)
                matched.append(name)
            elif src_tensor.dim() == tgt_tensor.dim():
                buf = tgt_tensor.detach().clone()
                slices = tuple(slice(0, min(s, t)) for s, t in zip(src_tensor.shape, tgt_tensor.shape))
                try:
                    buf[slices] = src_tensor[slices].to(buf.dtype).to(buf.device)
                    new_state_dict[name] = buf
                    partial.append(f"{name}({list(src_tensor.shape)}->{list(tgt_tensor.shape)})")
                except Exception as e:
                    skipped.append(f"{name}(slice-fail:{e})")
            else:
                skipped.append(f"{name}(ndim:{src_tensor.dim()}->{tgt_tensor.dim()})")

        self.model.load_state_dict(new_state_dict, strict=False)

        return {
            "matched": len(matched),
            "partial": len(partial),
            "partial_detail": partial,
            "skipped": len(skipped),
            "skipped_detail": skipped,
            "missing": len(missing),
            "missing_detail": missing,
        }

    # =========================================================
    # Inference helpers
    # =========================================================

    def _run_model(self, feature):
        """Gradient-free forward pass, returns (logits_np, value_np)."""
        self.model.set_eval_mode()
        obs_tensor = (
            torch.tensor(np.array([feature], dtype=np.float32)).view(1, Config.DIM_OF_OBSERVATION).to(self.device)
        )
        with torch.no_grad():
            rst = self.model(obs_tensor, inference=True)
        logits = rst[0].cpu().numpy()[0]
        value = rst[1].cpu().numpy()[0]
        return logits, value

    def _legal_soft_max(self, logits, legal_action):
        """Softmax with legal action masking."""
        _w, _e = 1e20, 1e-5
        tmp = logits - _w * (1.0 - legal_action)
        tmp_max = np.max(tmp, keepdims=True)
        tmp = np.clip(tmp - tmp_max, -_w, 1)
        tmp = (np.exp(tmp) + _e) * legal_action
        return tmp / (np.sum(tmp, keepdims=True) * 1.00001)

    def _legal_sample(self, probs, use_max=False):
        """Sample action from probability distribution (argmax if use_max=True)."""
        if use_max:
            return int(np.argmax(probs))
        return int(np.argmax(np.random.multinomial(1, probs, size=1)))
