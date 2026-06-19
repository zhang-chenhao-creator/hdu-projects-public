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
    # 当特征/架构/行为发生不兼容变更时递增版本号；仅作为元数据写入 pkl
    # v4:      P0 优化（NPC 惩罚平滑化 + 熵 warmup + 监控指标）
    # v4_p1:   P1-1 优化（KL 两级早停 + ratio/vpred 监控）
    # v4_p2:   P1-2 优化（preprocessor 向量化，纯性能，语义不变）
    # v4_p3:   P1-3 优化（reward clip ±10 + value_loss_unclipped + ratio_std + kl_skip 5min 率）
    # v4_p4:   P2 优化（NPC 斥力场扩展 + 危险充电桩惩罚，针对"NPC 卡充电桩"陷阱）
    # v4_p5:   P3 优化（电量渐进压力 + 充电动态放大 + 震荡惩罚 + 安全桩吸引场
    #                   + advantage ±3σ clip，针对云端三大失败模式：不充电/不离桩/卡墙）
    #          P3-5 安全桩吸引场采用 A+C 方案：progress-based 奖励距离减少量（方案A，
    #          防粘桩刷分） + Bresenham 已知墙预检（方案C，防隔墙刷分）。
    MODEL_VERSION = "arch_v4_173d_p5_reward_rework"

    # ------ Feature dimensions ------
    FEATURES = [
        11 * 11,  # 121D local view
        44,       # 44D global state
        8,        # 8D legal action mask
    ]
    FEATURE_SPLIT_SHAPE = FEATURES
    FEATURE_LEN = sum(FEATURES)
    DIM_OF_OBSERVATION = FEATURE_LEN  # = 173

    # ------ Model architecture ------
    HIDDEN_DIMS = (256, 128)
    ACTION_NUM = 8
    VALUE_NUM = 1

    # ------ PPO core ------
    GAMMA = 0.99
    LAMDA = 0.95
    CLIP_PARAM = 0.2
    VF_COEF = 0.5

    # ------ Learning rate schedule ------
    LR_START = 3e-4
    LR_END = 1e-4
    LR_DECAY_STEPS = 2_000_000
    INIT_LEARNING_RATE_START = LR_START

    # ------ Entropy coefficient schedule ------
    BETA_START = 0.003
    BETA_END = 3e-4
    BETA_DECAY_STEPS = 2_000_000
    BETA_WARMUP_FRAC = 0.3

    LABEL_SIZE_LIST = [ACTION_NUM]
    LEGAL_ACTION_SIZE_LIST = LABEL_SIZE_LIST.copy()

    USE_GRAD_CLIP = True
    GRAD_CLIP_RANGE = 0.5

    # ------ KL early stopping (P1-1) ------
    KL_TARGET = 0.02
    KL_HARD_COEF = 2.0
    KL_SOFT_COEF = 1.5
    KL_WINDOW_SIZE = 8

    # ------ P1-3: Reward clip ------
    # 对 preprocessor.reward_process() 返回的 step reward 做对称 clip。
    # 设为 None 或 <=0 可关闭（用于消融）。
    # 理由：v4_p1 训练日志里观察到 E11 事件（KL 正常但 EV 跌到 0.7141），
    # 推测根因是 battery(-30)/npc 多目标叠加(-60+)/charging 满充(+30) 等 outlier
    # 污染 Return RMS 和 td_error。±10 选择的依据：
    #   - 覆盖所有单组件 outlier（最大 NPC 峰值 -20 也能被拦）
    #   - 大于典型 cleaning+exploration 峰值（+0.75~1.5），不影响正常信号
    #   - 与 final_reward 范围（-12.5 / +10.4~18.2）接近，让 per-step 和终局奖励量级统一
    # 终局奖励在 train_workflow.py 外部添加到最后一帧，不经过本 clip。
    # P2-2 危险充电桩 peak=5.0 + NPC peak=20.0 叠加仍在 ±10 内（后者单独已触 clip），
    # 不需要因 P2 调整 clip 阈值。
    # P3 设计验证：低电量满充 charging_reward 最大 8.4（ratio=0.1），
    # 叠加 cleaning_reward(≤1.5) 仍在 +10 内，不触 clip。
    REWARD_CLIP = 10.0

    # ------ P1-3: KL skip rate 5-min window ------
    # 滑窗记录最近 N 次 learn() 调用是否 skip，用于计算 5min skip 率。
    # 按 v4_p1 日志统计 ≈116 step/min，5min ≈ 580 step，取 600 做缓冲。
    KL_SKIP_WINDOW = 600
