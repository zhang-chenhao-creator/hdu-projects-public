#!/usr/bin/env python3
# -*- coding: UTF-8 -*-
###########################################################################
# Copyright © 1998 - 2026 Tencent. All Rights Reserved.
###########################################################################
"""
Author: Tencent AI Arena Authors

Configuration for Robot Vacuum PPO agent.
清扫大作战 PPO 配置。
"""


class Config:

    # ------ Model version tag ------
    # 当特征/架构发生不兼容变更时递增版本号；仅作为元数据写入 pkl，供排查用
    # v4:    P0 优化（NPC 惩罚平滑化 + 熵 warmup + 监控指标）
    # v4_p1: P1-1 优化（KL 两级早停 + 扩展监控 ratio/vpred/kl_skip）
    MODEL_VERSION = "arch_v4_173d_p1_kl_earlystop"

    # ------ Feature dimensions ------
    # 特征维度（173D = 121 + 44 + 8）
    # local_view (11×11=121) + global_state (44) + legal_action (8)
    FEATURES = [
        11 * 11,  # 121D local view (含访问记忆)
        44,       # 44D global state (base12 + exploration8 + npc12 + organ12)
        8,        # 8D legal action mask
    ]
    FEATURE_SPLIT_SHAPE = FEATURES
    FEATURE_LEN = sum(FEATURES)
    DIM_OF_OBSERVATION = FEATURE_LEN  # = 173

    # ------ Model architecture ------
    # 隐藏层维度列表；改这里不用动 model.py
    HIDDEN_DIMS = (256, 128)

    # Action space: 8 directional moves
    # 动作空间：8个方向移动
    ACTION_NUM = 8

    # Single-head value
    # 单头价值
    VALUE_NUM = 1

    # ------ PPO core ------
    GAMMA = 0.99
    LAMDA = 0.95
    CLIP_PARAM = 0.2
    VF_COEF = 0.5

    # ------ Learning rate schedule ------
    # Task 5: learning rate linear decay
    LR_START = 3e-4
    LR_END = 1e-4
    LR_DECAY_STEPS = 2_000_000
    # 保留旧名字做兼容（如果别处有人读）
    INIT_LEARNING_RATE_START = LR_START

    # ------ Entropy coefficient schedule ------
    # P0 改动：
    # - 起点从 0.001 提到 0.003（原日志里 58 分钟 entropy 从 1.03 崩到 0.56，过早确定化）
    # - 衰减曲线改成 warmup + linear：前 WARMUP_FRAC 比例步数维持 BETA_START，
    #   之后在剩余步数内线性衰减到 BETA_END
    # - BETA_END 抬到 3e-4（保留最低探索预算，避免后期完全确定性）
    BETA_START = 0.003        # 原 0.001
    BETA_END = 3e-4           # 原 1e-4
    BETA_DECAY_STEPS = 2_000_000
    BETA_WARMUP_FRAC = 0.3    # 前 30% 保持 BETA_START

    LABEL_SIZE_LIST = [ACTION_NUM]
    LEGAL_ACTION_SIZE_LIST = LABEL_SIZE_LIST.copy()

    USE_GRAD_CLIP = True
    GRAD_CLIP_RANGE = 0.5

    # ------ KL early stopping (P1-1) ------
    # 两级 KL 早停，防止 approx_kl 超阈值时 PPO 过度更新。
    #
    # 背景：v4_p0 的训练日志（665K 步附近）显示 approx_kl ∈ [0.028, 0.044]，
    #      持续超出 0.02 的 1.4~2.2 倍，说明同一批 rollout 被 PPO 多 epoch 更新时过头。
    #
    # 触发规则（每次 learn() 调用即一次 mini-batch 梯度步）：
    #   - HARD（单 batch）：此次 approx_kl > KL_TARGET * KL_HARD_COEF        → skip 本次 update
    #   - SOFT（滑窗均值）：最近 KL_WINDOW_SIZE 次均值 > KL_TARGET * KL_SOFT_COEF
    #                                                                      → skip 本次 update 并清空窗口
    #
    # 注：SKIP 时 train_step 仍递增（lr/beta 调度按日历推进，不因 skip 冻结）。
    # 注：KL_WINDOW_SIZE 建议 ≈ 一个 PPO epoch 内的 mini-batch 数量。
    #     若外层框架 n_epochs=4、n_minibatches=8/rollout，则一个 epoch = 8 次 learn() 调用，WINDOW_SIZE=8 合理。
    #     如果不清楚框架内参，初期设 8 足够；后续根据 kl_skip_soft 频率调整。
    KL_TARGET = 0.02          # 标准 PPO 推荐值
    KL_HARD_COEF = 2.0        # 单 batch 硬阈值倍数（→ 0.04）
    KL_SOFT_COEF = 1.5        # 滑窗均值软阈值倍数（→ 0.03）
    KL_WINDOW_SIZE = 8        # 滑窗大小（≈ 一个 epoch 的 mini-batch 数量）
