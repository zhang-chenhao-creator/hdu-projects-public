# Quadruped Robot Reinforcement Learning Iterations

This folder contains selected public-safe code snapshots from a competition-oriented robot locomotion / reinforcement-learning exploration project.

The included files focus on algorithmic and engineering changes:

- PPO training logic and stability controls.
- Feature preprocessing for local/global robot state.
- Reward shaping for exploration, charging, safety, and collision avoidance.
- Inference-time safety overrides and action calibration logic.

## Included Snapshots

- `versions/v4-2`: PPO algorithm, configuration, and vectorized preprocessing baseline with KL monitoring, return normalization, reward clipping, and early reward-structure changes.
- `versions/v5-2`: later PPO/preprocessing iteration with expanded reward shaping and safety-related feature logic.
- `versions/v5-5`: compact agent/config snapshot focused on checkpoint compatibility, inference-time action calibration, wall masking, charger filtering, and low-battery return behavior.

## Excluded Content

The public repository intentionally excludes training logs, reports, archives, datasets, checkpoints, model weights, run folders, experiment tracking output, cache directories, editor settings, and AI conversations.

## Source Boundary

These files are selected code snapshots, not a complete runnable competition package. They preserve upstream Tencent AI Arena template copyright headers where present, and the public documentation only describes the visible technical changes without claiming private scores or unverified competition results.
