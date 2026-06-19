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
    MODEL_VERSION = "arch_v4_173d_p4_safe_charge"

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
    REWARD_CLIP = 10.0
    KL_SKIP_WINDOW = 600

    # =========================================================================
    # ------ P3 + Auto-Calibration: Inference-only Safety Override ------
    # =========================================================================
    # 设计：
    #   1) 开局 ≤CALIB_MAX_STEPS 步执行"严格顺序校准"：依次派出 action 0~7,
    #      通过 prev_pos → cur_pos 反推每个动作的 (dx, dz)。
    #      撞墙的动作下一轮再试,直至凑齐或超 CALIB_MAX_STEPS。
    #   2) 校准结果写入类变量 Agent._cached_deltas,后续 episode 复用,
    #      只交一次 8 步代价。
    #   3) 校准期内 P0~P3 全部不启用（裸走）；校准完成后启用。
    #   4) 部分校准（≥CALIB_MIN_KNOWN 个方向）也启用,未知方向跳过该规则。
    #   5) ACTION_DELTAS 仍可手填作为兜底（自动校准失败时使用）。
    # =========================================================================

    # ------ Auto-calibration ------
    SAFETY_AUTO_CALIBRATE = True   # True: 开局自动校准；False: 仅用 ACTION_DELTAS
    CALIB_MAX_STEPS = 16           # 校准期最大步数（>8 给撞墙重试空间）
    CALIB_MIN_KNOWN = 4            # 至少凑齐多少个方向才启用部分 override
    CALIB_LOG = True               # 校准结果打 1 行 INFO 日志（不会刷屏）

    # ------ 调试日志（正式提交前关闭） ------
    SAFETY_LOG_ACTION_MAPPING = False  # 每步打印 action mapping debug（刷屏,只调试用）
    SAFETY_LOG_OVERRIDE = False        # override 触发时打印（每 episode 几十~几百次）

    # ------ ACTION_DELTAS 静态兜底 ------
    # 当 SAFETY_AUTO_CALIBRATE=False,或自动校准失败时使用。
    # None = 不启用静态值。
    ACTION_DELTAS = None

    # ------ 子规则总开关 ------
    SAFETY_BLOCK_WALL = True            # P0: 撞墙屏蔽
    SAFETY_BLOCK_DANGER_CHARGER = True  # P1: 污染桩屏蔽
    SAFETY_FORCE_CHARGE = True          # P2: 低电强制回充
    SAFETY_LEAVE_CHARGER = True         # P3: 满电离桩

    # ------ P2 阈值：低电强制回充 ------
    EMERGENCY_CHARGE_THRESHOLD = 0.08
    FORCE_CHARGE_THRESHOLD = 0.20
    FORCE_CHARGE_MAX_DIST = 30

    # ------ P3 阈值：满电离桩 ------
    LEAVE_CHARGER_THRESHOLD = 0.92
