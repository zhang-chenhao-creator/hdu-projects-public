#!/usr/bin/env python3
# -*- coding: UTF-8 -*-
###########################################################################
# Copyright © 1998 - 2026 Tencent. All Rights Reserved.
###########################################################################
"""
Author: Tencent AI Arena Authors

Feature preprocessor for Robot Vacuum.
清扫大作战特征预处理器。

P0 改动：
  - NPC 惩罚由阶跃函数（-20/-5/-3）改为连续指数衰减。

P1-2 改动：
  - pb2struct 里 _view_map 更新循环向量化（441 iter → 1 op）
  - _update_passable 向量化（441 iter → 1 op）
  - _get_exploration_features 扇形扫描向量化（868 iter → 8 ops）
  - _get_global_state_feature 射线投射向量化（120 iter → 8 ops）

P1-3 改动：
  - reward_process() 返回前对 step reward 做 ±REWARD_CLIP 裁剪（默认 ±10）
  - 目的：抑制 battery/npc/charging 三类 outlier step reward（可达 -30/-60/+30）
    污染 Return RMS 和 td_error。
  - 终局奖励 final_reward 走 train_workflow.py 路径，不经过 reward_process()。

P2 改动：
  - P2-1: NPC 斥力场扩展（NPC_SIGMA 1.0→1.8，NPC_CUTOFF 4→6）
  - P2-2: 新增危险充电桩惩罚 _get_dangerous_charger_penalty()

P3 改动（本次，针对云端三大失败模式的 reward 结构改造）：
  - 失败模式 1: 回避 NPC 但不充电 → 电量耗尽
    P3-1: 电量渐进压力，取代原"电量=0 才 -30"的稀疏信号
    P3-5: 安全桩吸引场（progress-based + 已知墙预检，A+C 方案）
  - 失败模式 2: 找到桩仍继续探索被撞死
    P3-2: 充电吸引力动态放大 + 首踩桩 bonus 渐进化
  - 失败模式 3: 反复撞墙卡角耗电死亡
    P3-3: explore_penalty 上限 0.7→2.5 + 位置震荡检测(惩罚从 0.15 提到 0.3)

P3-5 A+C 方案防刷分设计：
  - 方案 A (progress-based): attraction 依赖"距离减少量"而非"距离函数"，
    停下 / 后退都为 0，根治"粘在桩边持续拿 reward"的刷分漏洞。
  - 方案 C (Bresenham 预检): 画线检查桩路径上是否有已知墙
    (passable_map==0)，有则跳过该桩，根治"隔墙近桩蹭墙刷分"。
  - 配套: OSCILLATION_PENALTY 0.15→0.3，进一步压制原地晃动。
"""

import collections
import numpy as np

from agent_ppo.conf.conf import Config


def _norm(v, v_max, v_min=0.0):
    """Normalize value to [0, 1]."""
    v = float(np.clip(v, v_min, v_max))
    if v_max == v_min:
        return 0.0
    return (v - v_min) / (v_max - v_min)


class Preprocessor:
    GRID_SIZE = 128
    VIEW_HALF = 10
    LOCAL_HALF = 5

    # ------ NPC 斥力场（P0 + P2-1） ------
    # 数值对照（Chebyshev 距离）：
    #   dist=1 → -20.00 (经 REWARD_CLIP 后 -10)
    #   dist=2 → -11.48 (经 REWARD_CLIP 后 -10)
    #   dist=3 → -6.58
    #   dist=4 → -3.78
    #   dist=5 → -2.17
    #   dist=6 → -1.24
    NPC_PEAK = 20.0
    NPC_SIGMA = 1.8
    NPC_CUTOFF = 6

    # ------ 危险充电桩（P2-2） ------
    # 数值对照（robot 距污染桩的 Chebyshev 距离）:
    #   dist=0 (在桩上) → -5.00  |  dist=1 → -3.03  |  dist=2 → -1.84
    #   dist=3 → -1.12           |  dist=4 → -0.68
    DANGER_CHARGER_PEAK = 5.0
    DANGER_CHARGER_SIGMA = 2.0
    DANGER_CHARGER_NPC_RADIUS = 3
    DANGER_CHARGER_APPROACH_RADIUS = 4

    # ============================================================
    # ------ P3: 电量 / 探索 reward 结构改造（三大失败模式） ------
    # ============================================================

    # ---- P3-1: 电量危机渐进压力（修"回避NPC不充电→电量耗尽"）----
    # 替代原"battery=0 才 -30"的稀疏信号，低电量每步累积。
    # 随电量下降二次方增长：
    #   ratio=0.4 → 0       | ratio=0.3 → -0.0009
    #   ratio=0.2 → -0.0038 | ratio=0.1 → -0.0084 | ratio=0.0 → -0.015
    # 1000 步典型电量曲线累积 -5~-10，足以让 policy 学会"电量是资源"。
    # 死亡瞬间仍保留 -30（只触发一次，因为 terminated=True）。
    BATTERY_URGENCY_THRESHOLD = 0.4
    BATTERY_URGENCY_PEAK = 0.015
    BATTERY_DEATH_PENALTY = 30.0

    # ---- P3-2: 充电吸引力动态放大（修"找到桩仍继续探索"）----
    # charging_reward 按电量插值放大：ratio>=0.8→1x, ratio<=0.1→3x
    # 首踩桩 bonus 渐进化：原"<30% 阶跃 +5"改为 <40% 时随缺口线性。
    # 典型数值（满充 10% 一次，皆未触发 ±10 clip）：
    #   ratio=0.1 → 10×0.1×3.0 + (0.4-0.1)×18 = 3.0 + 5.4 = 8.4
    #   ratio=0.2 → 10×0.1×2.3 + (0.4-0.2)×18 = 2.3 + 3.6 = 5.9
    #   ratio=0.3 → 10×0.1×1.7 + (0.4-0.3)×18 = 1.7 + 1.8 = 3.5
    #   ratio≥0.4 → 10×0.1×1   +      0       = 1.0（原行为）
    CHARGE_URGENCY_HIGH = 0.8
    CHARGE_URGENCY_LOW = 0.1
    CHARGE_URGENCY_MAX = 3.0
    CHARGE_BONUS_THRESHOLD = 0.4
    CHARGE_BONUS_PEAK = 18.0

    # ---- P3-3: 震荡 / 卡墙强化惩罚（修"反复撞墙卡角"）----
    # 原 explore_penalty 上限 -0.7 让 policy 学到"卡角伪安全"。
    # 新上限 -2.5 + 新增 20 步位置 std 震荡检测。
    # OSCILLATION_PENALTY 从 0.15 提到 0.3：配合 P3-5 A+C 方案兜底压制粘桩/卡墙刷分。
    EXPLORE_PENALTY_CAP = 2.5
    OSCILLATION_WINDOW = 20
    OSCILLATION_THRESHOLD = 1.5
    OSCILLATION_PENALTY = 0.3

    # ---- P3-5: 安全充电桩吸引场（A+C 方案：progress-based + 已知墙预检）----
    # 补齐"鼓励找安全桩"的正向梯度，同时避免两种刷分 bug：
    #   Bug 1 (粘桩刷分): 位置函数 → robot 停在桩边持续拿 reward
    #     → 方案 A: 改为"距离减少量"函数，不移动 = 不给奖励
    #   Bug 2 (隔墙近桩): Chebyshev 距离不看 passability，robot 蹭墙刷分
    #     → 方案 C: Bresenham 画线预检，路径上有已知墙则跳过该桩
    #
    # 触发条件（全部满足）：
    #   1) battery_ratio < SAFE_CHARGER_ACTIVATION_THRESH (低电量才激活)
    #   2) 桩安全 (NPC 距桩 > DANGER_CHARGER_NPC_RADIUS)
    #   3) 桩路径上无已知墙 (Bresenham check passable_map == 0)
    #   4) robot 距桩 ≤ SAFE_CHARGER_ATTRACTION_RADIUS
    #   5) 本步 Chebyshev 距离比上一步减少 (progress > 0)
    #
    # 未探索格子在 passable_map 中标记为 1（passable），所以方案 C 的策略是：
    #   - 未知区域：不跳过 → robot 可以探索后再判断
    #   - 已知墙：跳过 → 避免隔墙刷分
    #   这是"保守但无假阴性"的设计，符合在线学习的直觉。
    #
    # 数值设计（progress-based，每格进步一次 reward）：
    # 目标：从 dist=8 走到 dist=0 累积 ~15 reward（约为单次满充 charging 的 1/3）
    # 每步 1 格移动 → attraction = PEAK × urgency_mult × 1
    #   battery=0.3 (mult=1.75): 每步 progress = +1.40
    #   battery=0.2 (mult=2.125): 每步 progress = +1.70
    #   battery=0.1 (mult=2.5):   每步 progress = +2.00
    # 对比 cleaning_reward(0.75): 低电量靠近桩 > 单污渍，自然引导 ✅
    # 对比 NPC penalty(dist=5 -2.17): 遇 NPC 时净负收益，优先躲 NPC ✅
    # 对比 oscillation_penalty(-0.3): 粘桩刷分场景下被压制 ✅
    SAFE_CHARGER_PEAK = 0.8
    SAFE_CHARGER_ATTRACTION_RADIUS = 8       # robot 距桩 ≤ 此值才触发吸引
    SAFE_CHARGER_ACTIVATION_THRESH = 0.5     # battery_ratio < 此值 → 吸引场激活
    SAFE_CHARGER_URGENCY_LOW = 0.1           # battery_ratio ≤ 此值 → urgency 最大
    SAFE_CHARGER_URGENCY_MAX = 2.5           # 低电量时吸引放大系数上限

    SCAN_RADIUS = 20
    FAR_START = 11
    RAY_MAX = 30

    def __init__(self):
        self._precompute_lookup_tables()
        self.reset()

    def _precompute_lookup_tables(self):
        """预计算常量查找表（P1-2）。"""
        vsize = 2 * self.VIEW_HALF + 1  # 21

        ri = np.arange(vsize, dtype=np.int32)
        ci = np.arange(vsize, dtype=np.int32)
        self._view_ri_grid, self._view_ci_grid = np.meshgrid(ri, ci, indexing='ij')

        directions = [(0, -1), (1, 0), (0, 1), (-1, 0)]
        self._exp_offsets = []
        self._exp_is_far = []
        for main_dx, main_dz in directions:
            offsets = []
            is_far = []
            for dist in range(1, self.SCAN_RADIUS + 1):
                fan_width = max(1, dist // 2)
                for side in range(-fan_width, fan_width + 1):
                    if main_dx == 0:
                        dx, dz = side, main_dz * dist
                    else:
                        dx, dz = main_dx * dist, side
                    offsets.append((dx, dz))
                    is_far.append(dist >= self.FAR_START)
            self._exp_offsets.append(np.array(offsets, dtype=np.int32))
            self._exp_is_far.append(np.array(is_far, dtype=bool))

        ray_depth = self.VIEW_HALF
        ks = np.arange(1, ray_depth + 1, dtype=np.int32)
        self._ray_ks = ks
        self._ray_local_idx = []
        for dx, dz in [(0, -1), (1, 0), (0, 1), (-1, 0)]:
            local_xs = self.VIEW_HALF + ks * dx
            local_zs = self.VIEW_HALF + ks * dz
            self._ray_local_idx.append((local_xs, local_zs))

    def reset(self):
        self.step_no = 0
        self.battery = 300
        self.battery_max = 300
        self.cur_pos = (0, 0)
        self.dirt_cleaned = 0
        self.last_dirt_cleaned = 0
        self.total_dirt = 1
        self.last_battery = 300
        self.passable_map = np.ones((self.GRID_SIZE, self.GRID_SIZE), dtype=np.int8)
        self.nearest_dirt_dist = 200.0
        self.last_nearest_dirt_dist = 200.0
        self._terrain_map = np.zeros((21, 21), dtype=np.float32)
        self._view_map = np.zeros((21, 21), dtype=np.float32)
        self._legal_act = [1] * 8
        self.visited_map = np.zeros((self.GRID_SIZE, self.GRID_SIZE), dtype=np.float32)
        # P3-3: 位置历史（用于震荡检测）
        self.position_history = collections.deque(maxlen=self.OSCILLATION_WINDOW)
        # P3-5: 上一步最近可达安全桩的 Chebyshev 距离（None 表示无/首次/半径外）
        self.last_safe_charger_dist = None

    def pb2struct(self, env_obs, last_action):
        observation = env_obs["observation"]
        frame_state = observation["frame_state"]
        env_info = observation["env_info"]
        hero = frame_state["heroes"]
        self.npcs = frame_state.get("npcs", [])
        self.organs = frame_state.get("organs", [])

        self.step_no = int(observation["step_no"])
        self.cur_pos = (int(hero["pos"]["x"]), int(hero["pos"]["z"]))
        # P3-3: 维护最近 OSCILLATION_WINDOW 步位置，用于检测原地晃动
        self.position_history.append(self.cur_pos)

        self.last_battery = getattr(self, "battery", 300)
        self.battery = int(hero["battery"])
        self.battery_max = max(int(hero["battery_max"]), 1)

        self.last_dirt_cleaned = self.dirt_cleaned
        self.dirt_cleaned = int(hero["dirt_cleaned"])
        self.total_dirt = max(int(env_info["total_dirt"]), 1)

        self._legal_act = [int(x) for x in (observation.get("legal_action") or [1] * 8)]

        map_info = observation.get("map_info")
        if map_info is not None:
            self._terrain_map = np.array(map_info, dtype=np.float32)

            hx, hz = self.cur_pos

            if 0 <= hx < self.GRID_SIZE and 0 <= hz < self.GRID_SIZE:
                self.visited_map[hx, hz] += 1.0

            # P1-2 向量化 _view_map 更新
            vsize = self._terrain_map.shape[0]
            half = vsize // 2

            gx_grid = hx - half + self._view_ri_grid
            gz_grid = hz - half + self._view_ci_grid

            in_bounds = (
                (gx_grid >= 0) & (gx_grid < self.GRID_SIZE) &
                (gz_grid >= 0) & (gz_grid < self.GRID_SIZE)
            )

            gx_clip = np.clip(gx_grid, 0, self.GRID_SIZE - 1)
            gz_clip = np.clip(gz_grid, 0, self.GRID_SIZE - 1)

            visited_vals = self.visited_map[gx_clip, gz_clip]

            self._view_map = self._terrain_map.copy()

            update_mask = in_bounds & (self._terrain_map == 1) & (visited_vals > 0)
            self._view_map[update_mask] = -np.minimum(visited_vals[update_mask], 4.0)

            self._update_passable(hx, hz)

    def _update_passable(self, hx, hz):
        """P1-2 向量化。"""
        terrain = self._terrain_map
        vsize = terrain.shape[0]
        half = vsize // 2

        gx_grid = hx - half + self._view_ri_grid
        gz_grid = hz - half + self._view_ci_grid

        in_bounds = (
            (gx_grid >= 0) & (gx_grid < self.GRID_SIZE) &
            (gz_grid >= 0) & (gz_grid < self.GRID_SIZE)
        )

        gx_valid = gx_grid[in_bounds]
        gz_valid = gz_grid[in_bounds]
        terrain_valid = terrain[in_bounds]

        self.passable_map[gx_valid, gz_valid] = (terrain_valid != 0).astype(np.int8)

    def _get_local_view_feature(self):
        center = self.VIEW_HALF
        h = self.LOCAL_HALF
        crop = self._view_map[center - h : center + h + 1, center - h : center + h + 1]
        return (crop / 2.0).flatten()

    def _get_exploration_features(self, hx, hz):
        """P1-2 向量化。"""
        unvisited_density = []
        far_dirt_density = []

        for d in range(4):
            offsets = self._exp_offsets[d]
            is_far = self._exp_is_far[d]

            gxs = hx + offsets[:, 0]
            gzs = hz + offsets[:, 1]

            in_bounds = (
                (gxs >= 0) & (gxs < self.GRID_SIZE) &
                (gzs >= 0) & (gzs < self.GRID_SIZE)
            )

            gx_clip = np.clip(gxs, 0, self.GRID_SIZE - 1)
            gz_clip = np.clip(gzs, 0, self.GRID_SIZE - 1)
            passable_vals = self.passable_map[gx_clip, gz_clip]
            visited_vals = self.visited_map[gx_clip, gz_clip]

            valid = in_bounds & (passable_vals != 0)

            unvisited_total = int(valid.sum())
            unvisited_count = int((valid & (visited_vals == 0)).sum())
            unvisited_density.append(unvisited_count / max(unvisited_total, 1))

            far_valid = valid & is_far
            far_total = int(far_valid.sum())
            far_dirt_count = int((far_valid & (visited_vals == 0)).sum())
            far_dirt_density.append(far_dirt_count / max(far_total, 1))

        return unvisited_density + far_dirt_density

    def _get_global_state_feature(self):
        """P1-2 向量化射线投射。"""
        step_norm = _norm(self.step_no, 2000)
        battery_ratio = _norm(self.battery, self.battery_max)
        cleaning_progress = _norm(self.dirt_cleaned, self.total_dirt)
        remaining_dirt = 1.0 - cleaning_progress

        hx, hz = self.cur_pos
        pos_x_norm = _norm(hx, self.GRID_SIZE)
        pos_z_norm = _norm(hz, self.GRID_SIZE)

        terrain = self._terrain_map
        ray_dirt = []
        for d in range(4):
            local_xs, local_zs = self._ray_local_idx[d]
            cells = terrain[local_xs, local_zs]
            dirt_positions = np.where(cells == 2)[0]
            if dirt_positions.size > 0:
                found = int(self._ray_ks[dirt_positions[0]])
            else:
                found = self.RAY_MAX
            ray_dirt.append(_norm(found, self.RAY_MAX))

        self.last_nearest_dirt_dist = self.nearest_dirt_dist
        self.nearest_dirt_dist = self._calc_nearest_dirt_dist()
        nearest_dirt_norm = _norm(self.nearest_dirt_dist, 180)

        dirt_delta = 1.0 if self.nearest_dirt_dist < self.last_nearest_dirt_dist else 0.0

        # NPC features
        npc_feats = []
        for npc in getattr(self, 'npcs', []):
            nx, nz = int(npc['pos']['x']), int(npc['pos']['z'])
            dist_x = np.clip((nx - hx) / 64.0, -1.0, 1.0)
            dist_z = np.clip((nz - hz) / 64.0, -1.0, 1.0)
            dist_norm = min(np.sqrt((nx - hx) ** 2 + (nz - hz) ** 2) / 180.0, 1.0)
            npc_feats.append((dist_norm, dist_x, dist_z))
        npc_feats.sort(key=lambda x: x[0])
        while len(npc_feats) < 4:
            npc_feats.append((1.0, 0.0, 0.0))
        npc_feats = npc_feats[:4]

        npc_flat = []
        for f in npc_feats:
            npc_flat.extend(f)

        # Organ features
        organ_feats = []
        for org in getattr(self, 'organs', []):
            if org.get('sub_type') == 1:
                ox, oz = int(org['pos']['x']), int(org['pos']['z'])
                dist_x = np.clip((ox - hx) / 64.0, -1.0, 1.0)
                dist_z = np.clip((oz - hz) / 64.0, -1.0, 1.0)
                dist_norm = min(np.sqrt((ox - hx) ** 2 + (oz - hz) ** 2) / 180.0, 1.0)
                organ_feats.append((dist_norm, dist_x, dist_z))
        organ_feats.sort(key=lambda x: x[0])
        while len(organ_feats) < 4:
            organ_feats.append((1.0, 0.0, 0.0))
        organ_feats = organ_feats[:4]

        organ_flat = []
        for f in organ_feats:
            organ_flat.extend(f)

        exploration_feats = self._get_exploration_features(hx, hz)

        base_features = [
            step_norm, battery_ratio, cleaning_progress, remaining_dirt,
            pos_x_norm, pos_z_norm,
            ray_dirt[0], ray_dirt[1], ray_dirt[2], ray_dirt[3],
            nearest_dirt_norm, dirt_delta,
        ]

        final_features = base_features + exploration_feats + npc_flat + organ_flat
        return np.array(final_features, dtype=np.float32)

    def _calc_nearest_dirt_dist(self):
        terrain = self._terrain_map
        if terrain is None:
            return 200.0
        dirt_coords = np.argwhere(terrain == 2)
        if len(dirt_coords) == 0:
            return 200.0
        center = self.VIEW_HALF
        dists = np.sqrt((dirt_coords[:, 0] - center) ** 2 + (dirt_coords[:, 1] - center) ** 2)
        return float(np.min(dists))

    def get_legal_action(self):
        return list(self._legal_act)

    def feature_process(self, env_obs, last_action):
        self.pb2struct(env_obs, last_action)

        local_view = self._get_local_view_feature()
        global_state = self._get_global_state_feature()
        legal_action = self.get_legal_action()
        legal_arr = np.array(legal_action, dtype=np.float32)

        feature = np.concatenate([local_view, global_state, legal_arr])

        reward = self.reward_process()

        return feature, legal_action, reward

    def _get_dangerous_charger_penalty(self, hx, hz, npc_positions, charger_positions):
        """
        P2-2: 危险充电桩惩罚。

        当充电桩在 NPC 威胁半径内（DANGER_CHARGER_NPC_RADIUS），
        且 robot 正在接近该桩（DANGER_CHARGER_APPROACH_RADIUS 内），
        施加连续指数衰减惩罚（离桩越近，惩罚越重）。
        """
        if not charger_positions or not npc_positions:
            return 0.0

        penalty = 0.0
        for cx, cz in charger_positions:
            # 1) 此桩是否被 NPC 污染？
            min_npc_to_charger = min(
                max(abs(nx - cx), abs(nz - cz))
                for nx, nz in npc_positions
            )
            if min_npc_to_charger > self.DANGER_CHARGER_NPC_RADIUS:
                continue  # 桩安全，跳过

            # 2) robot 是否正在接近这个污染桩？
            dist_robot_to_charger = max(abs(hx - cx), abs(hz - cz))
            if dist_robot_to_charger > self.DANGER_CHARGER_APPROACH_RADIUS:
                continue  # 离得远，不触发

            # 3) 连续指数衰减：越靠近污染桩惩罚越重
            penalty -= self.DANGER_CHARGER_PEAK * float(
                np.exp(-dist_robot_to_charger / self.DANGER_CHARGER_SIGMA)
            )

        return penalty

    def _has_wall_on_path(self, x0, z0, x1, z1):
        """
        P3-5 方案 C: Bresenham 画线检查路径上是否有"已知墙"（passable_map==0）。

        passable_map 中：1=可通行（含未探索），0=已知墙。
        因此本函数只对"robot 看到过的墙"返回 True，未知区域保守地允许吸引。
        起点和终点本身不计入检查。

        Args:
            x0, z0: 起点（robot 位置）
            x1, z1: 终点（充电桩位置）

        Returns:
            bool: True 表示路径上存在已知墙，该桩应被吸引场跳过
        """
        dx = abs(x1 - x0)
        dz = abs(z1 - z0)
        sx = 1 if x0 < x1 else -1
        sz = 1 if z0 < z1 else -1
        err = dx - dz
        x, z = x0, z0
        # 安全上限：8 格距离最多 ~16 步（Bresenham 最坏情况）
        max_steps = dx + dz + 2
        for _ in range(max_steps):
            if (x, z) != (x0, z0) and (x, z) != (x1, z1):
                if 0 <= x < self.GRID_SIZE and 0 <= z < self.GRID_SIZE:
                    if self.passable_map[x, z] == 0:
                        return True
            if x == x1 and z == z1:
                break
            e2 = 2 * err
            if e2 > -dz:
                err -= dz
                x += sx
            if e2 < dx:
                err += dx
                z += sz
        return False

    def _get_safe_charger_attraction(self, hx, hz, npc_positions, charger_positions,
                                     battery_ratio):
        """
        P3-5 A+C 方案: 安全充电桩吸引场（progress-based + 已知墙预检）。

        方案 A: 把"位置函数"改为"距离减少量函数"——只有 Chebyshev 距离实际减少
                才给奖励，停下或后退都为 0，根治"粘桩刷分"。
        方案 C: Bresenham 画线预检，路径上有已知墙的桩直接跳过，根治"隔墙刷分"。

        触发条件（全部满足）：
          1) battery_ratio < SAFE_CHARGER_ACTIVATION_THRESH
          2) 桩安全 (NPC 距桩 > DANGER_CHARGER_NPC_RADIUS)
          3) Bresenham 路径上无已知墙
          4) robot 距桩 ≤ SAFE_CHARGER_ATTRACTION_RADIUS
          5) cur_dist < last_safe_charger_dist (实际靠近)

        数值（progress-based，每格接近一次奖励）：
          battery=0.3 (mult=1.75):  每步 +1.40
          battery=0.2 (mult=2.125): 每步 +1.70
          battery=0.1 (mult=2.5):   每步 +2.00

        状态维护：
          self.last_safe_charger_dist 在 radius 外 / 无可达桩时重置为 None，
          保证下次进入时"首次不给，后续才给"，防止跨步跳变的假信号。

        Args:
            hx, hz: robot 坐标
            npc_positions: [(nx, nz), ...] NPC 坐标列表
            charger_positions: [(cx, cz), ...] 充电桩坐标（已过滤 sub_type==1）
            battery_ratio: self.battery / self.battery_max

        Returns:
            float: 单步 attraction reward（≥0）；无进步、无可达桩、高电量均为 0
        """
        # 1) 高电量不触发，同时重置状态
        if battery_ratio >= self.SAFE_CHARGER_ACTIVATION_THRESH:
            self.last_safe_charger_dist = None
            return 0.0

        if not charger_positions:
            self.last_safe_charger_dist = None
            return 0.0

        # 2) 过滤：安全桩 + 路径无已知墙
        reachable_safe = []
        for cx, cz in charger_positions:
            # 安全？(NPC 在附近则跳过)
            if npc_positions:
                min_npc = min(
                    max(abs(nx - cx), abs(nz - cz))
                    for nx, nz in npc_positions
                )
                if min_npc <= self.DANGER_CHARGER_NPC_RADIUS:
                    continue
            # 路径可达？(方案 C)
            if self._has_wall_on_path(hx, hz, cx, cz):
                continue
            reachable_safe.append((cx, cz))

        if not reachable_safe:
            self.last_safe_charger_dist = None
            return 0.0

        # 3) 当前最近可达安全桩的 Chebyshev 距离
        cur_dist = min(
            max(abs(hx - cx), abs(hz - cz))
            for cx, cz in reachable_safe
        )

        # 4) 超出吸引半径不计，重置状态
        if cur_dist > self.SAFE_CHARGER_ATTRACTION_RADIUS:
            self.last_safe_charger_dist = None
            return 0.0

        # 5) 计算 progress delta（方案 A 核心）
        prev_dist = self.last_safe_charger_dist
        self.last_safe_charger_dist = cur_dist

        # 首次进入半径不给（无前值可对比），或无变化/后退不给
        if prev_dist is None:
            return 0.0
        delta = prev_dist - cur_dist
        if delta <= 0:
            return 0.0
        # clip 到单步最大移动 1 格：防止"最近桩切换"时的跳变假信号
        delta = min(delta, 1.0)

        # urgency multiplier (与 P3-2 同款线性插值)
        if battery_ratio <= self.SAFE_CHARGER_URGENCY_LOW:
            urgency_mult = self.SAFE_CHARGER_URGENCY_MAX
        else:
            frac = (self.SAFE_CHARGER_ACTIVATION_THRESH - battery_ratio) / (
                self.SAFE_CHARGER_ACTIVATION_THRESH - self.SAFE_CHARGER_URGENCY_LOW
            )
            urgency_mult = 1.0 + (self.SAFE_CHARGER_URGENCY_MAX - 1.0) * frac

        return delta * self.SAFE_CHARGER_PEAK * urgency_mult

    def reward_process(self):
        """
        P3 版 reward shaping（针对三大云端失败模式）：
          1) 回避 NPC 但不充电 → 电量耗尽
             → P3-1 电量渐进压力（替代原 -30 稀疏信号）
             → P3-5 安全桩吸引场（低电量时正向梯度）
          2) 找到桩仍继续探索被撞死
             → P3-2 充电动态放大 + 低电量 bonus 渐进化
          3) 反复撞墙卡角耗电死亡
             → P3-3 探索惩罚上限 0.7→2.5 + 位置震荡检测
        终局 WIN/FAIL 走 train_workflow.py 外部路径，不在此处处理。
        REWARD_CLIP=±10 仍然生效。
        """
        hx, hz = self.cur_pos
        battery_ratio = self.battery / max(1, self.battery_max)

        # ---------- 基础信号 ----------
        cleaned_this_step = max(0, self.dirt_cleaned - self.last_dirt_cleaned)
        cleaning_reward = 0.75 * cleaned_this_step
        step_penalty = -0.001

        # ---------- 位置解析（NPC / 充电桩，一次性解析复用） ----------
        npc_positions = [
            (int(npc['pos']['x']), int(npc['pos']['z']))
            for npc in getattr(self, 'npcs', [])
        ]
        charger_positions = [
            (int(org['pos']['x']), int(org['pos']['z']))
            for org in getattr(self, 'organs', [])
            if org.get('sub_type') == 1
        ]

        # ---------- NPC penalty (P0 + P2-1, 不变) ----------
        npc_penalty = 0.0
        for nx, nz in npc_positions:
            dist_inf = max(abs(nx - hx), abs(nz - hz))
            if dist_inf <= self.NPC_CUTOFF:
                npc_penalty -= self.NPC_PEAK * float(
                    np.exp(-(dist_inf - 1) / self.NPC_SIGMA)
                )

        # ---------- 危险充电桩 (P2-2, 不变) ----------
        dangerous_charger_penalty = self._get_dangerous_charger_penalty(
            hx, hz, npc_positions, charger_positions
        )

        # ---------- P3-5: 安全充电桩吸引场 ----------
        # 低电量时向安全桩方向产生正向梯度，补齐"只惩罚危险不奖励安全"的盲点
        safe_charger_attraction = self._get_safe_charger_attraction(
            hx, hz, npc_positions, charger_positions, battery_ratio
        )

        # ---------- P3-1: 电量危机渐进压力 ----------
        # 低电量阶段每步累积小额压力，随电量下降二次方增长
        # 死亡瞬间追加 -30（terminated 前最后一帧触发）
        battery_penalty = 0.0
        if battery_ratio < self.BATTERY_URGENCY_THRESHOLD:
            urgency = (self.BATTERY_URGENCY_THRESHOLD - battery_ratio) \
                      / self.BATTERY_URGENCY_THRESHOLD
            battery_penalty -= self.BATTERY_URGENCY_PEAK * (urgency ** 2)
        if self.battery <= 0:
            battery_penalty -= self.BATTERY_DEATH_PENALTY

        # ---------- P3-2: 充电吸引力动态放大 ----------
        # charging_reward 按电量插值放大（1x~3x）
        # 首踩桩 bonus 渐进化（替代原 <30% 阶跃 +5）
        charging_reward = 0.0
        battery_change = self.battery - getattr(self, "last_battery", self.battery)

        if battery_change > 0 and self.last_battery < self.battery_max * 0.92:
            if battery_ratio >= self.CHARGE_URGENCY_HIGH:
                urgency_mult = 1.0
            elif battery_ratio <= self.CHARGE_URGENCY_LOW:
                urgency_mult = self.CHARGE_URGENCY_MAX
            else:
                frac = (self.CHARGE_URGENCY_HIGH - battery_ratio) / (
                    self.CHARGE_URGENCY_HIGH - self.CHARGE_URGENCY_LOW
                )
                urgency_mult = 1.0 + (self.CHARGE_URGENCY_MAX - 1.0) * frac

            charging_reward += battery_change * 0.10 * urgency_mult

            if battery_ratio < self.CHARGE_BONUS_THRESHOLD:
                bonus_frac = self.CHARGE_BONUS_THRESHOLD - battery_ratio
                charging_reward += bonus_frac * self.CHARGE_BONUS_PEAK

        # ---------- 探索 reward (不变) ----------
        explore_reward = 0.0
        if self.visited_map[hx, hz] == 1:
            explore_reward += 0.14

        # ---------- P3-3: 探索惩罚强化 + 震荡检测 ----------
        # v_count 上限 0.7 → 2.5（让重复踩踏持续累积压力）
        # 新增位置震荡检测：20 步位置 std<1.5 → -0.15/step
        # 只在非充电状态下触发（桩边合法停留不应被惩罚）
        explore_penalty = 0.0
        oscillation_penalty = 0.0
        if battery_change <= 0:
            v_count = self.visited_map[hx, hz]
            if v_count > 1:
                explore_penalty -= min(
                    (v_count - 1) ** 2 * 0.01,
                    self.EXPLORE_PENALTY_CAP
                )

            if len(self.position_history) >= self.OSCILLATION_WINDOW:
                positions = np.array(self.position_history, dtype=np.float32)
                pos_std = positions.std(axis=0).sum()
                if pos_std < self.OSCILLATION_THRESHOLD:
                    oscillation_penalty = -self.OSCILLATION_PENALTY

        # ---------- 汇总 + P1-3 对称 clip ----------
        total = (
            cleaning_reward + step_penalty
            + npc_penalty + dangerous_charger_penalty
            + safe_charger_attraction
            + battery_penalty + charging_reward
            + explore_reward + explore_penalty + oscillation_penalty
        )

        clip_val = getattr(Config, 'REWARD_CLIP', None)
        if clip_val is not None and clip_val > 0:
            if total > clip_val:
                total = float(clip_val)
            elif total < -clip_val:
                total = float(-clip_val)

        return total
