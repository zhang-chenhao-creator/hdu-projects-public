#!/usr/bin/env python3
# -*- coding: UTF-8 -*-
###########################################################################
# Copyright © 1998 - 2026 Tencent. All Rights Reserved.
###########################################################################
"""
Author: Tencent AI Arena Authors

Standard PPO algorithm for Robot Vacuum.
清扫大作战 PPO 算法。

P0 改动：
  - 熵系数 warmup + linear 衰减
  - 监控：approx_kl / clip_fraction / explained_variance / grad_norm

P1-1 改动：
  - 两级 KL 早停（hard 单 batch 2×target；soft 滑窗均值 1.5×target）
  - SKIP 时 train_step 仍递增，lr/beta 按日历推进

P1-3 改动：
  - 新增 `value_loss_unclipped`：不带 PPO value clip 的朴素 MSE，
    与 clipped 版本同框上报。若两者差距大 → value clip 频繁"救场"
    → td_error 分布厚尾 → reward outlier 未被充分抑制。
  - 新增 `ratio_std`：补齐 P1-1 已有的 ratio_max/min，刻画分布宽度。
  - 新增 `kl_skip_rate_5min`：滑窗 deque 记录最近 N 次 learn() 的 skip 状态，
    提供滑窗 skip 率（%），比瞬时 60s 值更能反映趋势。

P3 改动（本次）：
  - P3-4: 对 normalize 后的 advantage 做 ±3σ clamp，抑制单 batch outlier 污染。
    与 preprocessor.py 的 P3-1~3 reward 重构配合使用，专门缓解 EV 偶发跌至
    <0.85 的症状（800k~810k 日志中观察到 step 801524 ev=0.827 等事件）。
    不改变梯度方向，只压缩极端尾部，几乎零副作用。
"""

import collections
import os
import time

import torch

from agent_ppo.conf.conf import Config


class Algorithm:
    def __init__(self, model, optimizer, device=None, logger=None, monitor=None):
        self.model = model
        self.optimizer = optimizer
        self.parameters = [p for pg in optimizer.param_groups for p in pg["params"]]
        self.device = device
        self.logger = logger
        self.monitor = monitor

        self.clip_param = Config.CLIP_PARAM
        self.vf_coef = Config.VF_COEF
        self.var_beta = Config.BETA_START
        self.label_size = Config.ACTION_NUM

        self.train_step = 0
        self.last_report_time = 0

        # Task 2: Return RMS (Welford)
        self.ret_rms_mean = 0.0
        self.ret_rms_var = 1.0
        self.ret_rms_count = 1e-4

        # 最近一次 backward 后的 grad_norm
        self._last_grad_norm = 0.0

        # ---- P1-1: KL early stopping state ----
        self._kl_window = collections.deque(maxlen=Config.KL_WINDOW_SIZE)
        self._kl_skip_total = 0
        self._kl_skip_hard = 0
        self._kl_skip_soft = 0
        self._kl_skip_since_report = 0

        # ---- P1-3: 滑窗 skip 率 ----
        # 每次 learn() 调用 append(1 if skip else 0)，窗口满时首尾推进
        # 统计 sum(deque)/len(deque) = 最近 N 步的 skip 率
        self._skip_history = collections.deque(maxlen=Config.KL_SKIP_WINDOW)

    def learn(self, list_sample_data):
        obs = torch.stack([s.obs for s in list_sample_data]).to(self.device)
        legal_action = torch.stack([s.legal_action for s in list_sample_data]).to(self.device)
        act = torch.stack([s.act for s in list_sample_data]).to(self.device).view(-1, 1)
        old_prob = torch.stack([s.prob for s in list_sample_data]).to(self.device)
        old_value = torch.stack([s.value for s in list_sample_data]).to(self.device)
        reward_sum = torch.stack([s.reward_sum for s in list_sample_data]).to(self.device)
        advantage = torch.stack([s.advantage for s in list_sample_data]).to(self.device)
        reward = torch.stack([s.reward for s in list_sample_data]).to(self.device)

        self.model.set_train_mode()
        self.optimizer.zero_grad()

        rst_list = self.model(obs)
        logits, value_pred = rst_list[0], rst_list[1]

        total_loss, info = self._compute_loss(
            logits=logits,
            value_pred=value_pred,
            legal_action=legal_action,
            old_action=act,
            old_prob=old_prob,
            old_value=old_value,
            reward_sum=reward_sum,
            advantage=advantage,
        )

        # ---- P1-1: KL early stopping ----
        approx_kl = info["approx_kl"]
        self._kl_window.append(approx_kl)

        kl_hard_thresh = Config.KL_TARGET * Config.KL_HARD_COEF
        kl_soft_thresh = Config.KL_TARGET * Config.KL_SOFT_COEF

        skip_update = False
        skip_reason = None

        if approx_kl > kl_hard_thresh:
            skip_update = True
            skip_reason = "hard"
            self._kl_skip_hard += 1
        elif len(self._kl_window) == self._kl_window.maxlen:
            kl_window_mean = sum(self._kl_window) / len(self._kl_window)
            if kl_window_mean > kl_soft_thresh:
                skip_update = True
                skip_reason = "soft"
                self._kl_skip_soft += 1
                self._kl_window.clear()

        if skip_update:
            self._kl_skip_total += 1
            self._kl_skip_since_report += 1
            self._last_grad_norm = 0.0
        else:
            total_loss.backward()

            if Config.USE_GRAD_CLIP:
                gn = torch.nn.utils.clip_grad_norm_(self.parameters, Config.GRAD_CLIP_RANGE)
                self._last_grad_norm = float(gn) if gn is not None else 0.0
            else:
                total_sq = 0.0
                for p in self.parameters:
                    if p.grad is not None:
                        total_sq += float(p.grad.data.norm(2).item()) ** 2
                self._last_grad_norm = total_sq ** 0.5

            self.optimizer.step()

        # P1-3: 滑窗 skip 历史（skip=1, update=0）
        self._skip_history.append(1 if skip_update else 0)

        # train_step 无论是否 skip 都递增
        self.train_step += 1

        # Task 5: lr decay
        self.update_lr(self.train_step)

        results = {"total_loss": total_loss.item()}

        # Periodic monitoring report
        now = time.time()
        if now - self.last_report_time >= 60:
            results["value_loss"] = round(info["value_loss"], 4)
            results["policy_loss"] = round(info["policy_loss"], 4)
            results["entropy_loss"] = round(info["entropy_loss"], 4)
            results["reward"] = round(reward.mean().item(), 4)

            results["approx_kl"] = round(info["approx_kl"], 6)
            results["clip_fraction"] = round(info["clip_fraction"], 4)
            results["explained_variance"] = round(info["explained_variance"], 4)
            results["grad_norm"] = round(self._last_grad_norm, 4)
            results["beta"] = round(info["beta"], 6)
            results["lr"] = self.optimizer.param_groups[0]["lr"]

            # P0/P2-5：ratio & vpred 诊断
            results["ratio_max"] = round(info["ratio_max"], 4)
            results["ratio_min"] = round(info["ratio_min"], 4)
            results["ratio_std"] = round(info["ratio_std"], 4)          # P1-3 新增
            results["value_pred_min"] = round(info["value_pred_min"], 3)
            results["value_pred_max"] = round(info["value_pred_max"], 3)
            results["value_pred_mean"] = round(info["value_pred_mean"], 3)

            # P1-3：value_loss_unclipped（核心诊断指标）
            # value_loss_gap > 0 说明 value clip 在救场，td_error 有 outlier
            results["value_loss_unclipped"] = round(info["value_loss_unclipped"], 4)
            vloss_gap = info["value_loss"] - info["value_loss_unclipped"]
            results["value_loss_gap"] = round(vloss_gap, 4)

            # P3-4: advantage clip 命中率（诊断 outlier 频度）
            results["adv_clip_fraction"] = round(info["adv_clip_fraction"], 4)

            # P1-1/P1-3: KL skip 计数
            results["kl_skip_total"] = self._kl_skip_total
            results["kl_skip_since_report"] = self._kl_skip_since_report
            results["kl_skip_hard"] = self._kl_skip_hard
            results["kl_skip_soft"] = self._kl_skip_soft

            # P1-3: 滑窗 skip 率（%）
            if len(self._skip_history) > 0:
                skip_rate_window = sum(self._skip_history) / len(self._skip_history)
            else:
                skip_rate_window = 0.0
            results["kl_skip_rate_window"] = round(skip_rate_window * 100, 2)
            results["kl_skip_window_filled"] = len(self._skip_history)

            self.logger.info(
                f"policy_loss: {results['policy_loss']}, "
                f"value_loss: {results['value_loss']}, "
                f"vloss_unclipped: {results['value_loss_unclipped']}, "
                f"vloss_gap: {results['value_loss_gap']}, "
                f"entropy_loss: {results['entropy_loss']}, "
                f"approx_kl: {results['approx_kl']}, "
                f"clip_frac: {results['clip_fraction']}, "
                f"ev: {results['explained_variance']}, "
                f"grad_norm: {results['grad_norm']}, "
                f"beta: {results['beta']}, "
                f"lr: {results['lr']:.2e}, "
                f"ratio[min/max/std]: "
                f"{results['ratio_min']:.3f}/{results['ratio_max']:.3f}/{results['ratio_std']:.4f}, "
                f"adv_clip_frac: {results['adv_clip_fraction']:.4f}, "
                f"vpred[min/max/mean]: "
                f"{results['value_pred_min']:.2f}/{results['value_pred_max']:.2f}/{results['value_pred_mean']:.2f}, "
                f"kl_skip(60s/total/{len(self._skip_history)}-rate%)[H:S]: "
                f"{results['kl_skip_since_report']}/{results['kl_skip_total']}/"
                f"{results['kl_skip_rate_window']}% "
                f"[{results['kl_skip_hard']}:{results['kl_skip_soft']}]"
            )
            if self.monitor:
                self.monitor.put_data({os.getpid(): results})

            self.last_report_time = now
            self._kl_skip_since_report = 0

        return results

    def _compute_loss(self, logits, value_pred, legal_action, old_action, old_prob,
                      old_value, reward_sum, advantage):
        # ---- Value loss (clipped) with return normalization ----
        tdret = reward_sum.squeeze(-1) if reward_sum.dim() > 1 else reward_sum
        vp = value_pred.squeeze(-1) if value_pred.dim() > 1 else value_pred
        ov = old_value.squeeze(-1) if old_value.dim() > 1 else old_value

        self._update_ret_rms(tdret.detach())
        ret_std = (self.ret_rms_var ** 0.5 + 1e-8)

        tdret_norm = tdret / ret_std
        vp_norm = vp / ret_std
        ov_norm = ov / ret_std

        vp_clip = ov_norm + (vp_norm - ov_norm).clamp(-self.clip_param, self.clip_param)

        # 标准 PPO clipped value loss（实际用于 backward）
        value_loss = (
            0.5
            * torch.maximum(
                (tdret_norm - vp_norm) ** 2,
                (tdret_norm - vp_clip) ** 2,
            ).mean()
        )

        # P1-3 诊断：不带 PPO value clip 的朴素 MSE
        # 只做 no_grad 统计，不参与梯度
        with torch.no_grad():
            value_loss_unclipped = 0.5 * ((tdret_norm - vp_norm) ** 2).mean()
            value_loss_unclipped = float(value_loss_unclipped.item())

        # Explained variance (原始尺度)
        with torch.no_grad():
            y_true = tdret
            y_pred = vp
            var_y = y_true.var()
            explained_variance = 1.0 - ((y_true - y_pred).var() / (var_y + 1e-8))
            explained_variance = float(explained_variance.item())

        # ---- Policy loss with advantage normalization + P3-4 clip ----
        prob_dist = self._masked_softmax(logits, legal_action)
        entropy_loss = (-(prob_dist * torch.log(prob_dist.clamp(1e-9, 1))).sum(1)).mean()

        one_hot = torch.nn.functional.one_hot(old_action[:, 0].long(), self.label_size).float()
        new_prob = (one_hot * prob_dist).sum(1, keepdim=True)
        old_action_prob = (one_hot * old_prob).sum(1, keepdim=True)

        ratio = new_prob / old_action_prob.clamp(1e-9)

        adv = advantage.squeeze(-1) if advantage.dim() > 1 else advantage
        adv_mean = adv.mean()
        adv_std = adv.std() + 1e-8
        adv_norm = (adv - adv_mean) / adv_std

        # P3-4: ±3σ clamp 抑制 advantage outlier（缓解 EV 偶发跌至 <0.85）
        # 不改变梯度方向，只压缩极端尾部
        adv_clip_thresh = 3.0
        with torch.no_grad():
            adv_clip_fraction = (adv_norm.abs() > adv_clip_thresh).float().mean()
            adv_clip_fraction = float(adv_clip_fraction.item())
        adv_norm = adv_norm.clamp(-adv_clip_thresh, adv_clip_thresh)
        adv_norm = adv_norm.unsqueeze(-1)

        policy_loss = torch.maximum(
            -ratio * adv_norm,
            -ratio.clamp(1 - self.clip_param, 1 + self.clip_param) * adv_norm,
        ).mean()

        # ---- 诊断指标 (no grad) ----
        with torch.no_grad():
            log_ratio = torch.log(ratio.clamp(1e-9))
            approx_kl = ((ratio - 1.0) - log_ratio).mean()
            approx_kl = float(approx_kl.item())

            clip_fraction = ((ratio - 1.0).abs() > self.clip_param).float().mean()
            clip_fraction = float(clip_fraction.item())

            # ratio 极值 + P1-3 新增 std
            ratio_max = float(ratio.max().item())
            ratio_min = float(ratio.min().item())
            ratio_std = float(ratio.std().item())

            vp_det = vp.detach()
            value_pred_min = float(vp_det.min().item())
            value_pred_max = float(vp_det.max().item())
            value_pred_mean = float(vp_det.mean().item())

        # ---- Total loss ----
        beta = self.get_entropy_coef(self.train_step)
        total_loss = self.vf_coef * value_loss + policy_loss - beta * entropy_loss

        return total_loss, {
            "value_loss": value_loss.item(),
            "value_loss_unclipped": value_loss_unclipped,   # P1-3
            "policy_loss": policy_loss.item(),
            "entropy_loss": entropy_loss.item(),
            "approx_kl": approx_kl,
            "clip_fraction": clip_fraction,
            "explained_variance": explained_variance,
            "beta": beta,
            "ratio_max": ratio_max,
            "ratio_min": ratio_min,
            "ratio_std": ratio_std,                         # P1-3
            "value_pred_min": value_pred_min,
            "value_pred_max": value_pred_max,
            "value_pred_mean": value_pred_mean,
            "adv_clip_fraction": adv_clip_fraction,         # P3-4
        }

    def _update_ret_rms(self, returns):
        """Welford's algorithm for running mean/var."""
        batch_mean = returns.mean().item()
        batch_var = returns.var().item()
        batch_count = returns.numel()
        delta = batch_mean - self.ret_rms_mean
        tot = self.ret_rms_count + batch_count
        self.ret_rms_mean += delta * batch_count / tot
        self.ret_rms_var = (
            self.ret_rms_var * self.ret_rms_count
            + batch_var * batch_count
            + delta ** 2 * self.ret_rms_count * batch_count / tot
        ) / tot
        self.ret_rms_count = tot

    def get_entropy_coef(self, global_step):
        """Warmup + linear decay."""
        start = Config.BETA_START
        end = Config.BETA_END
        total_steps = Config.BETA_DECAY_STEPS
        warmup_frac = Config.BETA_WARMUP_FRAC

        progress = min(global_step / total_steps, 1.0)
        if progress < warmup_frac:
            return start
        decay_progress = (progress - warmup_frac) / (1.0 - warmup_frac)
        return start + (end - start) * decay_progress

    def update_lr(self, global_step):
        start_lr = Config.LR_START
        end_lr = Config.LR_END
        total_steps = Config.LR_DECAY_STEPS
        progress = min(global_step / total_steps, 1.0)
        lr = start_lr + (end_lr - start_lr) * progress
        for param_group in self.optimizer.param_groups:
            param_group['lr'] = lr
        return lr

    def _masked_softmax(self, logits, legal_action):
        label_max, _ = torch.max(logits * legal_action, dim=1, keepdim=True)
        logits = logits - label_max
        logits = logits * legal_action
        logits = logits + 1e5 * (legal_action - 1)
        return torch.nn.functional.softmax(logits, dim=1)
