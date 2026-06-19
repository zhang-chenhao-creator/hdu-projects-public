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

P1-2 改动（本次，纯性能优化，特征语义 bit-exact 不变）：
  - pb2struct 里 _view_map 更新循环向量化（441 iter → 1 op）
  - _update_passable 向量化（441 iter → 1 op）
  - _get_exploration_features 扇形扫描向量化（868 iter → 8 ops）
  - _get_global_state_feature 射线投射向量化（120 iter → 8 ops）
  - __init__ 里预计算常量索引网格（21×21 meshgrid、4 方向扇形偏移）

  预计每步 preprocessing 时间从 ~1.9ms 降到 ~0.25ms（7~8× 速度提升）。
  数值语义完全保持，与 v4_p1 checkpoint 兼容，不需要 bump MODEL_VERSION。
"""

import numpy as np


def _norm(v, v_max, v_min=0.0):
    """Normalize value to [0, 1].

    将值线性归一化到 [0, 1]。
    """
    v = float(np.clip(v, v_min, v_max))
    if v_max == v_min:
        return 0.0
    return (v - v_min) / (v_max - v_min)


class Preprocessor:
    """Feature preprocessor for Robot Vacuum.

    清扫大作战特征预处理器。
    """

    GRID_SIZE = 128
    VIEW_HALF = 10  # Full local view radius (21×21) / 完整局部视野半径
    LOCAL_HALF = 5  # Cropped view radius (11×11) / 裁剪后的视野半径

    # ---- NPC 惩罚参数（P0 改动）----
    NPC_PEAK = 20.0
    NPC_SIGMA = 1.0
    NPC_CUTOFF = 4

    # ---- Exploration scan 参数（P1-2 提升到 class 常量，供预计算使用）----
    SCAN_RADIUS = 20   # 扫描半径
    FAR_START = 11     # 从视野边缘之外开始扫描远距离污渍

    # ---- Ray casting 参数 ----
    RAY_MAX = 30       # 射线最大距离（用于归一化）

    def __init__(self):
        # P1-2: 预计算所有不随 episode 变化的索引网格/偏移数组。
        # 这些只在进程启动时算一次，后续每步调用都复用。
        self._precompute_lookup_tables()
        self.reset()

    # =========================================================================
    # P1-2: 预计算常量查找表
    # =========================================================================
    def _precompute_lookup_tables(self):
        """Precompute constant index arrays used by vectorized feature extraction.

        预计算所有不随 episode 变化的索引网格与偏移数组（P1-2）。
        """
        vsize = 2 * self.VIEW_HALF + 1  # 21

        # ---- 21×21 索引网格（供 pb2struct 和 _update_passable 使用）----
        # ri_grid[i,j] = i, ci_grid[i,j] = j
        ri = np.arange(vsize, dtype=np.int32)
        ci = np.arange(vsize, dtype=np.int32)
        self._view_ri_grid, self._view_ci_grid = np.meshgrid(ri, ci, indexing='ij')

        # ---- 4 方向扇形扫描偏移（供 _get_exploration_features 使用）----
        # 每个方向预计算两个数组：
        #   _exp_offsets[d]: shape (M, 2) — 相对 agent 的 (dx, dz) 偏移
        #   _exp_is_far[d]:  shape (M,)   — bool，该 cell 是否属于 "远距离"（dist >= FAR_START）
        # 这样运行时只需做一次 hx+offsets 的加法和整块的 mask 运算。
        directions = [(0, -1), (1, 0), (0, 1), (-1, 0)]  # N, E, S, W
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

        # ---- 射线投射偏移（供 _get_global_state_feature 使用）----
        # 原代码 max_ray=30，但视野只有 21×21（VIEW_HALF=10），所以 dist>10 时
        # cell 始终取 else=0 分支（等价于"视野外无污渍"），不会命中。
        # 因此有效扫描深度 = VIEW_HALF = 10。超出部分 found=RAY_MAX 作为"未命中"。
        # 这与原代码完全等价，仅把 for 循环展开成向量。
        ray_depth = self.VIEW_HALF  # 10
        ks = np.arange(1, ray_depth + 1, dtype=np.int32)  # [1..10]
        self._ray_ks = ks
        # 每个方向的 local view 坐标（以 local view center 为原点）
        # local_x = VIEW_HALF + k*dx, local_z = VIEW_HALF + k*dz
        # 当 dx, dz ∈ {-1, 0, 1} 且 k ∈ [1, VIEW_HALF] 时，local 坐标永远在 [0, 20]
        self._ray_local_idx = []
        for dx, dz in [(0, -1), (1, 0), (0, 1), (-1, 0)]:
            local_xs = self.VIEW_HALF + ks * dx
            local_zs = self.VIEW_HALF + ks * dz
            self._ray_local_idx.append((local_xs, local_zs))

    def reset(self):
        """Reset all internal state at episode start.

        对局开始时重置所有状态。
        """
        self.step_no = 0
        self.battery = 300
        self.battery_max = 300

        self.cur_pos = (0, 0)

        self.dirt_cleaned = 0
        self.last_dirt_cleaned = 0
        self.total_dirt = 1

        self.last_battery = 300

        # Global passable map (0=obstacle, 1=passable), used for ray computation
        # 维护全局通行地图（0=障碍, 1=可通行），用于射线计算
        self.passable_map = np.ones((self.GRID_SIZE, self.GRID_SIZE), dtype=np.int8)

        # Nearest dirt distance
        # 最近污渍距离
        self.nearest_dirt_dist = 200.0
        self.last_nearest_dirt_dist = 200.0

        # _terrain_map: 纯地形，0=未知, 1=空地, 2=污渍
        # _view_map:    叠加了访问记忆的视野，供特征提取使用
        self._terrain_map = np.zeros((21, 21), dtype=np.float32)
        self._view_map = np.zeros((21, 21), dtype=np.float32)
        self._legal_act = [1] * 8

        # 记录访问过的坐标热力图，避免在死胡同里鬼打墙
        self.visited_map = np.zeros((self.GRID_SIZE, self.GRID_SIZE), dtype=np.float32)

    def pb2struct(self, env_obs, last_action):
        """Parse and cache essential fields from observation dict.

        从 env_obs 字典中提取并缓存所有需要的状态量。
        """
        observation = env_obs["observation"]
        frame_state = observation["frame_state"]
        env_info = observation["env_info"]
        hero = frame_state["heroes"]
        self.npcs = frame_state.get("npcs", [])
        self.organs = frame_state.get("organs", [])

        self.step_no = int(observation["step_no"])
        self.cur_pos = (int(hero["pos"]["x"]), int(hero["pos"]["z"]))

        # Battery / 电量
        self.last_battery = getattr(self, "battery", 300)
        self.battery = int(hero["battery"])
        self.battery_max = max(int(hero["battery_max"]), 1)

        # Cleaning progress / 清扫进度
        self.last_dirt_cleaned = self.dirt_cleaned
        self.dirt_cleaned = int(hero["dirt_cleaned"])
        self.total_dirt = max(int(env_info["total_dirt"]), 1)

        # Legal actions / 合法动作
        self._legal_act = [int(x) for x in (observation.get("legal_action") or [1] * 8)]

        # Local view map (21×21) / 局部视野地图
        map_info = observation.get("map_info")
        if map_info is not None:
            # 先把原始地形存入 _terrain_map，保持其纯净不被污染
            self._terrain_map = np.array(map_info, dtype=np.float32)

            hx, hz = self.cur_pos

            # 更新已访问热力图
            if 0 <= hx < self.GRID_SIZE and 0 <= hz < self.GRID_SIZE:
                self.visited_map[hx, hz] += 1.0

            # ---- P1-2 向量化：基于纯净的 _terrain_map 生成叠加访问记忆的 _view_map ----
            # 原代码：21×21=441 次 Python 循环 + 每次都做 visited_map 索引和比较
            # 新代码：预计算的 ri/ci 索引网格 + 单次 np.minimum / mask 赋值
            vsize = self._terrain_map.shape[0]  # 21
            half = vsize // 2

            # 每个 (ri, ci) 对应全局坐标 (gx, gz) = (hx-half+ri, hz-half+ci)
            gx_grid = hx - half + self._view_ri_grid  # (21, 21)
            gz_grid = hz - half + self._view_ci_grid  # (21, 21)

            # 是否落在全局地图范围内
            in_bounds = (
                (gx_grid >= 0) & (gx_grid < self.GRID_SIZE) &
                (gz_grid >= 0) & (gz_grid < self.GRID_SIZE)
            )

            # 安全索引（越界的 clip 到边界值，后续用 mask 过滤掉）
            gx_clip = np.clip(gx_grid, 0, self.GRID_SIZE - 1)
            gz_clip = np.clip(gz_grid, 0, self.GRID_SIZE - 1)

            # 一次性取出 21×21 个访问次数
            visited_vals = self.visited_map[gx_clip, gz_clip]

            # 先 copy 纯地形（越界/非空地/未访问的格子保持原 terrain 值）
            self._view_map = self._terrain_map.copy()

            # 写入访问记忆：必须 in_bounds & terrain==1 & visited>0
            # 写入值：-min(visited, 4.0)
            update_mask = in_bounds & (self._terrain_map == 1) & (visited_vals > 0)
            self._view_map[update_mask] = -np.minimum(visited_vals[update_mask], 4.0)

            self._update_passable(hx, hz)

    def _update_passable(self, hx, hz):
        """Write local terrain into global passable map.

        将局部地形写入全局通行地图。
        P1-2 向量化：21×21=441 次 Python 循环 → 单次 numpy 花式索引写入。
        """
        terrain = self._terrain_map
        vsize = terrain.shape[0]  # 21
        half = vsize // 2

        gx_grid = hx - half + self._view_ri_grid
        gz_grid = hz - half + self._view_ci_grid

        in_bounds = (
            (gx_grid >= 0) & (gx_grid < self.GRID_SIZE) &
            (gz_grid >= 0) & (gz_grid < self.GRID_SIZE)
        )

        # 只对 in_bounds 的位置做写入：terrain != 0 → passable=1，否则 0
        gx_valid = gx_grid[in_bounds]
        gz_valid = gz_grid[in_bounds]
        terrain_valid = terrain[in_bounds]

        self.passable_map[gx_valid, gz_valid] = (terrain_valid != 0).astype(np.int8)

    def _get_local_view_feature(self):
        """Local view feature (121D): crop center 11×11 from 21×21.

        局部视野特征（121D）：从 21×21 视野中心裁剪 11×11。
        使用含访问记忆的 _view_map（负数代表走过的格子）。
        """
        center = self.VIEW_HALF
        h = self.LOCAL_HALF
        crop = self._view_map[center - h : center + h + 1, center - h : center + h + 1]
        return (crop / 2.0).flatten()

    def _get_exploration_features(self, hx, hz):
        """Exploration guidance features (8D).

        探索引导特征（8D）：帮助机器人感知哪个方向更"值得去"。

        [0~3] unvisited_density_NESW: 四方向扇形区域内未访问格子的密度 [0,1]
        [4~7] far_dirt_density_NESW:  四方向远距离（视野外）污渍密度 [0,1]

        P1-2 向量化：原每步 ~868 次 Python 循环，现每方向一次 numpy batch 运算。
        """
        unvisited_density = []
        far_dirt_density = []

        for d in range(4):
            offsets = self._exp_offsets[d]   # (M, 2) int32
            is_far = self._exp_is_far[d]     # (M,) bool

            # 每个 cell 的全局坐标
            gxs = hx + offsets[:, 0]
            gzs = hz + offsets[:, 1]

            # 边界 mask
            in_bounds = (
                (gxs >= 0) & (gxs < self.GRID_SIZE) &
                (gzs >= 0) & (gzs < self.GRID_SIZE)
            )

            # 安全索引查表
            gx_clip = np.clip(gxs, 0, self.GRID_SIZE - 1)
            gz_clip = np.clip(gzs, 0, self.GRID_SIZE - 1)
            passable_vals = self.passable_map[gx_clip, gz_clip]
            visited_vals = self.visited_map[gx_clip, gz_clip]

            # 有效格子：in_bounds && passable!=0（障碍/未知不计入分母）
            valid = in_bounds & (passable_vals != 0)

            # 未访问密度（整个扇形）
            unvisited_total = int(valid.sum())
            unvisited_count = int((valid & (visited_vals == 0)).sum())
            unvisited_density.append(unvisited_count / max(unvisited_total, 1))

            # 远距离密度（只看 is_far 部分）
            far_valid = valid & is_far
            far_total = int(far_valid.sum())
            far_dirt_count = int((far_valid & (visited_vals == 0)).sum())
            far_dirt_density.append(far_dirt_count / max(far_total, 1))

        return unvisited_density + far_dirt_density  # 8D

    def _get_global_state_feature(self):
        """Global state feature (44D).

        全局状态特征（44D）。

        Base features (12D) / 基础特征：
          [0]  step_norm         step progress / 步数归一化 [0,1]
          [1]  battery_ratio     battery level / 电量比 [0,1]
          [2]  cleaning_progress cleaned ratio / 已清扫比例 [0,1]
          [3]  remaining_dirt    remaining dirt ratio / 剩余污渍比例 [0,1]
          [4]  pos_x_norm        x position / x 坐标归一化 [0,1]
          [5]  pos_z_norm        z position / z 坐标归一化 [0,1]
          [6]  ray_N_dirt        north ray distance / 向上（z-）方向最近污渍距离
          [7]  ray_E_dirt        east ray distance / 向右（x+）方向
          [8]  ray_S_dirt        south ray distance / 向下（z+）方向
          [9]  ray_W_dirt        west ray distance / 向左（x-）方向
          [10] nearest_dirt_norm nearest dirt Euclidean distance / 最近污渍欧氏距离归一化
          [11] dirt_delta        approaching dirt indicator / 是否在接近污渍（1=是, 0=否）

        Exploration features (8D) / 探索引导特征：
          [12~15] unvisited_density_NESW  四方向未访问格子密度
          [16~19] far_dirt_density_NESW   四方向远距离可探索密度

        NPC features (12D = 4 NPCs × 3D) / 官方机器人特征：
          each: [dist_norm, dist_x, dist_z]

        Organ features (12D = 4 organs × 3D) / 充电桩特征：
          each: [dist_norm, dist_x, dist_z]
        """
        step_norm = _norm(self.step_no, 2000)
        battery_ratio = _norm(self.battery, self.battery_max)
        cleaning_progress = _norm(self.dirt_cleaned, self.total_dirt)
        remaining_dirt = 1.0 - cleaning_progress

        hx, hz = self.cur_pos
        pos_x_norm = _norm(hx, self.GRID_SIZE)
        pos_z_norm = _norm(hz, self.GRID_SIZE)

        # ---- P1-2 向量化：4-directional ray casting for nearest dirt ----
        # 原代码：4 方向 × 30 步循环 + 每步 np.clip + 条件分支
        # 新代码：预计算 local 索引，一次性查表找第一个 cell==2
        #
        # 语义说明：原代码 max_ray=30 但视野只有 ±VIEW_HALF=10；cells 超出 local
        # view 时走 else 分支 cell=0（视为障碍/未知），不可能命中污渍。
        # 所以有效扫描深度 = VIEW_HALF = 10；未命中时 found = RAY_MAX = 30（同原）。
        terrain = self._terrain_map
        ray_dirt = []
        for d in range(4):
            local_xs, local_zs = self._ray_local_idx[d]  # 预计算：长度 10
            cells = terrain[local_xs, local_zs]          # shape (10,)
            dirt_positions = np.where(cells == 2)[0]     # 命中 cell==2 的下标
            if dirt_positions.size > 0:
                # 第一个命中对应的 k（1-based 步长）
                found = int(self._ray_ks[dirt_positions[0]])
            else:
                found = self.RAY_MAX  # 30，与原代码未命中时语义一致
            ray_dirt.append(_norm(found, self.RAY_MAX))

        # Nearest dirt Euclidean distance (estimated from local view)
        # 最近污渍欧氏距离（视野内粗估）
        self.last_nearest_dirt_dist = self.nearest_dirt_dist
        self.nearest_dirt_dist = self._calc_nearest_dirt_dist()
        nearest_dirt_norm = _norm(self.nearest_dirt_dist, 180)

        dirt_delta = 1.0 if self.nearest_dirt_dist < self.last_nearest_dirt_dist else 0.0

        # Official Robots (NPC) Features (4 x 3 = 12D)
        # NPC ≤ 4，循环成本可忽略，不向量化
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

        # Charging Stations (Organ) Features (4 x 3 = 12D)
        # Organ ≤ 4，同上，不向量化
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

        # Exploration guidance features / 探索引导特征（8D）
        exploration_feats = self._get_exploration_features(hx, hz)

        base_features = [
            step_norm,
            battery_ratio,
            cleaning_progress,
            remaining_dirt,
            pos_x_norm,
            pos_z_norm,
            ray_dirt[0],
            ray_dirt[1],
            ray_dirt[2],
            ray_dirt[3],
            nearest_dirt_norm,
            dirt_delta,
        ]

        final_features = base_features + exploration_feats + npc_flat + organ_flat
        return np.array(final_features, dtype=np.float32)

    def _calc_nearest_dirt_dist(self):
        """Find nearest dirt Euclidean distance from local terrain map.

        从局部地形中找最近污渍的欧氏距离。
        已经是向量化实现，无需改动。
        """
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
        """Return legal action mask (8D list).

        返回合法动作掩码（8D list）。
        """
        return list(self._legal_act)

    def feature_process(self, env_obs, last_action):
        """Generate 173D feature vector, legal action mask, and scalar reward.

        生成 173D 特征向量、合法动作掩码和标量奖励。
        feature breakdown: local_view(121D) + global_state(44D) + legal_action(8D) = 173D
        """
        self.pb2struct(env_obs, last_action)

        local_view = self._get_local_view_feature()      # 121D
        global_state = self._get_global_state_feature()  # 44D
        legal_action = self.get_legal_action()           # 8D
        legal_arr = np.array(legal_action, dtype=np.float32)

        feature = np.concatenate([local_view, global_state, legal_arr])  # 173D

        reward = self.reward_process()

        return feature, legal_action, reward

    def reward_process(self):
        # Cleaning reward / 清扫奖励
        cleaned_this_step = max(0, self.dirt_cleaned - self.last_dirt_cleaned)
        cleaning_reward = 0.75 * cleaned_this_step

        # Step penalty / 时间惩罚
        step_penalty = -0.001

        # ---- NPC penalty（P0：阶跃 -> 连续指数斥力场）----
        hx, hz = self.cur_pos
        npc_penalty = 0.0
        for npc in getattr(self, 'npcs', []):
            nx, nz = int(npc['pos']['x']), int(npc['pos']['z'])
            dist_inf = max(abs(nx - hx), abs(nz - hz))
            if dist_inf <= self.NPC_CUTOFF:
                npc_penalty -= self.NPC_PEAK * float(np.exp(-(dist_inf - 1) / self.NPC_SIGMA))

        # Battery penalty
        battery_penalty = 0.0
        if self.battery <= 0:
            battery_penalty -= 30.0  # Task 3: 原 -10，改为 -30 增加耗尽代价

        # Charging reward
        charging_reward = 0.0
        battery_change = self.battery - getattr(self, "last_battery", self.battery)

        if battery_change > 0 and self.last_battery < self.battery_max * 0.92:
            charging_reward += battery_change * 0.10  # 原 0.28

            if self.last_battery < self.battery_max * 0.3:
                charging_reward += 5.0  # 原 12.0

        # Exploration reward / 探索奖励（踏入从未访问的格子给正向奖励）
        explore_reward = 0.0
        hx, hz = self.cur_pos
        if self.visited_map[hx, hz] == 1:
            explore_reward += 0.14

        # Exploration penalty / 探索惩罚（防止鬼打墙）
        explore_penalty = 0.0
        if battery_change <= 0:
            v_count = self.visited_map[hx, hz]
            if v_count > 1:
                explore_penalty -= min((v_count - 1) ** 2 * 0.01, 0.7)

        return cleaning_reward + step_penalty + npc_penalty + battery_penalty + charging_reward + explore_reward + explore_penalty
