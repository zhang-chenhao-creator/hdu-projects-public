#!/usr/bin/env python3
# -*- coding: UTF-8 -*-
###########################################################################
# Copyright © 1998 - 2026 Tencent. All Rights Reserved.
###########################################################################
"""
Author: Tencent AI Arena Authors

Standard PPO algorithm for Robot Vacuum.
清扫大作战 PPO 算法。

Loss composition / 损失组成：
  total_loss = vf_coef * value_loss + policy_loss - beta * entropy_loss

P0 改动：
  - 熵系数改为 warmup + linear 衰减（前 30% 维持 BETA_START，之后再衰减）
  - 超参从 Config 读取，不再硬编码
  - 新增监控指标：approx_kl / clip_fraction / explained_variance / grad_norm

P1-1 改动（本次）：
  - 两级 KL 早停机制：
      * HARD（单 batch）：approx_kl > 2×KL_TARGET → skip 本次 backward/step
      * SOFT（滑窗均值）：最近 K 次均值 > 1.5×KL_TARGET → skip 本次 backward/step 并清空窗口
    SKIP 时 train_step 仍递增，lr/beta 调度按日历推进。
  - 扩展监控：ratio_max/min（诊断 clip 单边触发）、value_pred min/max/mean、
              kl_skip_total/hard/soft/since_report。
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
        self.var_beta = Config.BETA_START  # 保留字段（旧代码可能引用）
        self.label_size = Config.ACTION_NUM

        self.train_step = 0
        self.last_report_time = 0

        # Return normalization (RMS) for value target scaling
        # Task 2: 用 Welford 算法追踪 return 的 running variance，解决 value_loss 波动大的问题
        self.ret_rms_mean = 0.0
        self.ret_rms_var = 1.0
        self.ret_rms_count = 1e-4

        # 最近一次 backward 后的 grad_norm，供监控上报
        self._last_grad_norm = 0.0

        # ---- P1-1: KL early stopping state ----
        # 滑窗保存最近 KL_WINDOW_SIZE 次的 approx_kl，用作 "epoch 平均" 代理
        self._kl_window = collections.deque(maxlen=Config.KL_WINDOW_SIZE)
        # 累计计数
        self._kl_skip_total = 0            # 累计 skip 总次数
        self._kl_skip_hard = 0             # 累计 hard skip 次数（单 batch 超 2× 阈值）
        self._kl_skip_soft = 0             # 累计 soft skip 次数（滑窗均值超 1.5× 阈值）
        # 60 秒监控窗口内的 skip 次数（每次上报后清零）
        self._kl_skip_since_report = 0

    def learn(self, list_sample_data):
        """Training entry: perform one PPO gradient step on a batch of SampleData.

        训练入口：接收一批 SampleData，执行一步梯度更新。
        P1-1: 在 backward 前检查 approx_kl 是否越界，越界则 skip 本次 update。
        """
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
        # 先把本次 approx_kl 推入滑窗（hard 或 soft 触发前都会计入，防止 hard 阻塞窗口更新）
        approx_kl = info["approx_kl"]
        self._kl_window.append(approx_kl)

        kl_hard_thresh = Config.KL_TARGET * Config.KL_HARD_COEF
        kl_soft_thresh = Config.KL_TARGET * Config.KL_SOFT_COEF

        skip_update = False
        skip_reason = None

        if approx_kl > kl_hard_thresh:
            # HARD：单 batch 超 2× 阈值 → 立即 skip
            skip_update = True
            skip_reason = "hard"
            self._kl_skip_hard += 1
        elif len(self._kl_window) == self._kl_window.maxlen:
            # SOFT：滑窗已满，检查均值
            kl_window_mean = sum(self._kl_window) / len(self._kl_window)
            if kl_window_mean > kl_soft_thresh:
                skip_update = True
                skip_reason = "soft"
                self._kl_skip_soft += 1
                # 清空窗口，避免连续软触发（给 policy 若干步冷却后重新评估）
                self._kl_window.clear()

        if skip_update:
            # skip 本次 backward / optimizer.step()
            # zero_grad 已在函数开头调用，当前 grad 状态为 0，不会污染下次 update
            self._kl_skip_total += 1
            self._kl_skip_since_report += 1
            self._last_grad_norm = 0.0
        else:
            total_loss.backward()

            # 记录裁剪前的全局 grad_norm（clip_grad_norm_ 返回裁剪前的值）
            if Config.USE_GRAD_CLIP:
                gn = torch.nn.utils.clip_grad_norm_(self.parameters, Config.GRAD_CLIP_RANGE)
                self._last_grad_norm = float(gn) if gn is not None else 0.0
            else:
                # 手动算一下 grad_norm（不裁剪）
                total_sq = 0.0
                for p in self.parameters:
                    if p.grad is not None:
                        total_sq += float(p.grad.data.norm(2).item()) ** 2
                self._last_grad_norm = total_sq ** 0.5

            self.optimizer.step()

        # P1-1: train_step 无论 skip 与否都递增，让 lr/beta 调度按日历推进
        self.train_step += 1

        # Task 5: learning rate decay
        self.update_lr(self.train_step)

        results = {"total_loss": total_loss.item()}

        # Periodic monitoring report
        # 定期上报监控
        now = time.time()
        if now - self.last_report_time >= 60:
            results["value_loss"] = round(info["value_loss"], 4)
            results["policy_loss"] = round(info["policy_loss"], 4)
            results["entropy_loss"] = round(info["entropy_loss"], 4)
            results["reward"] = round(reward.mean().item(), 4)

            # P0：PPO 诊断指标
            results["approx_kl"] = round(info["approx_kl"], 6)
            results["clip_fraction"] = round(info["clip_fraction"], 4)
            results["explained_variance"] = round(info["explained_variance"], 4)
            results["grad_norm"] = round(self._last_grad_norm, 4)
            results["beta"] = round(info["beta"], 6)
            results["lr"] = self.optimizer.param_groups[0]["lr"]

            # P2-5：扩展监控
            results["ratio_max"] = round(info["ratio_max"], 4)
            results["ratio_min"] = round(info["ratio_min"], 4)
            results["value_pred_min"] = round(info["value_pred_min"], 3)
            results["value_pred_max"] = round(info["value_pred_max"], 3)
            results["value_pred_mean"] = round(info["value_pred_mean"], 3)

            # P1-1：KL skip 计数
            results["kl_skip_total"] = self._kl_skip_total
            results["kl_skip_since_report"] = self._kl_skip_since_report
            results["kl_skip_hard"] = self._kl_skip_hard
            results["kl_skip_soft"] = self._kl_skip_soft

            self.logger.info(
                f"policy_loss: {results['policy_loss']}, "
                f"value_loss: {results['value_loss']}, "
                f"entropy_loss: {results['entropy_loss']}, "
                f"approx_kl: {results['approx_kl']}, "
                f"clip_frac: {results['clip_fraction']}, "
                f"ev: {results['explained_variance']}, "
                f"grad_norm: {results['grad_norm']}, "
                f"beta: {results['beta']}, "
                f"lr: {results['lr']:.2e}, "
                f"ratio[min/max]: {results['ratio_min']:.3f}/{results['ratio_max']:.3f}, "
                f"vpred[min/max/mean]: "
                f"{results['value_pred_min']:.2f}/{results['value_pred_max']:.2f}/{results['value_pred_mean']:.2f}, "
                f"kl_skip(60s/total)[H:S]: "
                f"{results['kl_skip_since_report']}/{results['kl_skip_total']} "
                f"[{results['kl_skip_hard']}:{results['kl_skip_soft']}]"
            )
            if self.monitor:
                self.monitor.put_data({os.getpid(): results})

            self.last_report_time = now
            # 重置 60s 计数器
            self._kl_skip_since_report = 0

        return results

    def _compute_loss(self, logits, value_pred, legal_action, old_action, old_prob, old_value, reward_sum, advantage):
        """Compute standard PPO loss (policy + value + entropy).

        计算标准 PPO 三项损失。
        """
        # ---- Value loss (clipped) with return normalization ----
        # Task 2: normalize value targets using running RMS before computing loss
        tdret = reward_sum.squeeze(-1) if reward_sum.dim() > 1 else reward_sum
        vp = value_pred.squeeze(-1) if value_pred.dim() > 1 else value_pred
        ov = old_value.squeeze(-1) if old_value.dim() > 1 else old_value

        # Update running RMS with current batch returns
        self._update_ret_rms(tdret.detach())
        ret_std = (self.ret_rms_var ** 0.5 + 1e-8)

        # Normalize tdret and value_pred to same scale for stable value loss
        tdret_norm = tdret / ret_std
        vp_norm = vp / ret_std
        ov_norm = ov / ret_std

        vp_clip = ov_norm + (vp_norm - ov_norm).clamp(-self.clip_param, self.clip_param)
        value_loss = (
            0.5
            * torch.maximum(
                (tdret_norm - vp_norm) ** 2,
                (tdret_norm - vp_clip) ** 2,
            ).mean()
        )

        # Explained variance 用归一化前的值算，结果更有意义
        # ev = 1 - Var(y - y_hat) / Var(y)，>0.9 很好，<0 说明 critic 没学到东西
        with torch.no_grad():
            y_true = tdret
            y_pred = vp
            var_y = y_true.var()
            explained_variance = 1.0 - ((y_true - y_pred).var() / (var_y + 1e-8))
            explained_variance = float(explained_variance.item())

        # ---- Policy loss (PPO clip) with advantage normalization ----
        # Task 1: batch-normalize advantage before policy loss
        prob_dist = self._masked_softmax(logits, legal_action)
        entropy_loss = (-(prob_dist * torch.log(prob_dist.clamp(1e-9, 1))).sum(1)).mean()

        one_hot = torch.nn.functional.one_hot(old_action[:, 0].long(), self.label_size).float()
        new_prob = (one_hot * prob_dist).sum(1, keepdim=True)
        old_action_prob = (one_hot * old_prob).sum(1, keepdim=True)

        ratio = new_prob / old_action_prob.clamp(1e-9)

        adv = advantage.squeeze(-1) if advantage.dim() > 1 else advantage
        # Task 1: batch-normalize advantage (NOT reward_sum)
        adv_mean = adv.mean()
        adv_std = adv.std() + 1e-8
        adv_norm = (adv - adv_mean) / adv_std
        adv_norm = adv_norm.unsqueeze(-1)

        policy_loss = torch.maximum(
            -ratio * adv_norm,
            -ratio.clamp(1 - self.clip_param, 1 + self.clip_param) * adv_norm,
        ).mean()

        # ---- PPO 诊断指标 (no grad) ----
        with torch.no_grad():
            # approx_kl: Schulman blog 里的无偏估计 E[(r-1) - log(r)]，比 mean(log r) 准
            log_ratio = torch.log(ratio.clamp(1e-9))
            approx_kl = ((ratio - 1.0) - log_ratio).mean()
            approx_kl = float(approx_kl.item())

            # clip_fraction: 有多少比例的 ratio 落在 clip 区间外（健康值 10%~30%）
            clip_fraction = ((ratio - 1.0).abs() > self.clip_param).float().mean()
            clip_fraction = float(clip_fraction.item())

            # P2-5 新增：ratio 极值（诊断 clip 是否单边触发）
            # 若 ratio_max ≫ 1+clip 而 ratio_min 离 1-clip 不远 → clip 主要抑制"正向激进更新"
            # 反之 → clip 主要抑制"负向激进更新"
            ratio_max = float(ratio.max().item())
            ratio_min = float(ratio.min().item())

            # P2-5 新增：value 预测范围（用归一化前的原始值，便于与 reward 量级对照）
            vp_det = vp.detach()
            value_pred_min = float(vp_det.min().item())
            value_pred_max = float(vp_det.max().item())
            value_pred_mean = float(vp_det.mean().item())

        # ---- Total loss ----
        # Task 4 / P0 改动：entropy coefficient 改为 warmup + linear 衰减
        beta = self.get_entropy_coef(self.train_step)
        total_loss = self.vf_coef * value_loss + policy_loss - beta * entropy_loss

        return total_loss, {
            "value_loss": value_loss.item(),
            "policy_loss": policy_loss.item(),
            "entropy_loss": entropy_loss.item(),
            "approx_kl": approx_kl,
            "clip_fraction": clip_fraction,
            "explained_variance": explained_variance,
            "beta": beta,
            # P2-5 新增
            "ratio_max": ratio_max,
            "ratio_min": ratio_min,
            "value_pred_min": value_pred_min,
            "value_pred_max": value_pred_max,
            "value_pred_mean": value_pred_mean,
        }

    # ---- Return Normalization (Welford RMS) ----
    # Task 2
    def _update_ret_rms(self, returns):
        """Update running mean/variance using Welford's algorithm.

        用 Welford 增量算法更新 return 的 running mean/variance。
        """
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

    # ---- Entropy Coefficient Schedule (warmup + linear decay) ----
    # Task 4 + P0 改动
    def get_entropy_coef(self, global_step):
        """Warmup + linear decay of entropy coefficient.

        熵系数：前 WARMUP_FRAC 比例步数维持 BETA_START，之后线性衰减到 BETA_END。

        为什么改：原来从第 1 步就线性衰减，58 分钟内 entropy 从 1.03 崩到 0.56（下降 45%），
        而训练进度只走了 0.34%。前期给策略多一点探索预算。
        """
        start = Config.BETA_START
        end = Config.BETA_END
        total_steps = Config.BETA_DECAY_STEPS
        warmup_frac = Config.BETA_WARMUP_FRAC

        progress = min(global_step / total_steps, 1.0)
        if progress < warmup_frac:
            return start
        decay_progress = (progress - warmup_frac) / (1.0 - warmup_frac)
        return start + (end - start) * decay_progress

    # ---- Learning Rate Decay (linear) ----
    # Task 5
    def update_lr(self, global_step):
        """Linearly decay learning rate from LR_START to LR_END over LR_DECAY_STEPS.

        学习率线性衰减：从 LR_START 到 LR_END，跨 LR_DECAY_STEPS 步。
        """
        start_lr = Config.LR_START
        end_lr = Config.LR_END
        total_steps = Config.LR_DECAY_STEPS
        progress = min(global_step / total_steps, 1.0)
        lr = start_lr + (end_lr - start_lr) * progress
        for param_group in self.optimizer.param_groups:
            param_group['lr'] = lr
        return lr

    def _masked_softmax(self, logits, legal_action):
        """Apply legal action mask to logits before computing softmax.

        对 logits 应用合法动作掩码后计算 softmax。
        """
        label_max, _ = torch.max(logits * legal_action, dim=1, keepdim=True)
        logits = logits - label_max
        logits = logits * legal_action
        logits = logits + 1e5 * (legal_action - 1)
        return torch.nn.functional.softmax(logits, dim=1)
