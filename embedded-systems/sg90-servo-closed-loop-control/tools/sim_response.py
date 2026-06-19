#!/usr/bin/env python3
"""
生成 SG90 闭环控制仿真实验数据，用于填充实验报告示例。
模型：PWM(us) -> 舵机角度，比例 0.1 deg/us，一阶惯性 0.08 s，采样 100 Hz。
"""
import csv
import math
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt


DT = 0.01  # s
STEPS = 1200
KP, KI, KD = 8.0, 0.10, 1.5
TARGET_ANGLES = [0.0] * 100 + [30.0] * 400 + [0.0] * 400 + [-20.0] * 300
PWM_NEUTRAL = 1500.0
PWM_GAIN = 0.1  # deg / us
TAU = 0.08  # servo time constant


def clamp(v, lo, hi):
    return max(lo, min(hi, v))


def main():
    base = Path(__file__).parent
    csv_path = base / "sample_closed_loop.csv"
    png_path = base / "sample_closed_loop.png"

    out = []

    # PID state (matches position PID in pid.c)
    error = last_error = prev_prev_error = 0.0
    integral = 0.0
    last_meas = 0.0
    output = PWM_NEUTRAL

    # Plant state
    angle = 0.0  # actual physical yaw

    for k in range(STEPS):
        t_ms = int(k * DT * 1000)
        target = TARGET_ANGLES[k] if k < len(TARGET_ANGLES) else TARGET_ANGLES[-1]
        measurement = angle + math.sin(k * 0.3) * 0.05  # tiny noise

        err = target - measurement
        # Integral separation
        if abs(err) < 15.0:
            integral += err
            integral = clamp(integral, -500.0, 500.0)
        else:
            integral = 0.0

        p_term = KP * err
        i_term = KI * integral
        d_term = KD * (last_meas - measurement)
        output = p_term + i_term + d_term
        output = clamp(output, 1000.0, 2000.0)

        # Update PID history
        prev_prev_error = last_error
        last_error = error
        error = err
        last_meas = measurement

        # Simple first-order plant
        cmd_angle = (output - PWM_NEUTRAL) / PWM_GAIN
        angle += (cmd_angle - angle) * (DT / (DT + TAU))

        out.append({
            "TIME_MS": t_ms,
            "MODE": 1,
            "TARGET": target,
            "YAW": measurement,
            "OUTPUT": output,
            "KP": KP,
            "KI": KI,
            "KD": KD,
        })

    with open(csv_path, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=["TIME_MS", "MODE", "TARGET", "YAW", "OUTPUT", "KP", "KI", "KD"])
        writer.writeheader()
        writer.writerows(out)

    fig, axes = plt.subplots(2, 1, figsize=(10, 8), sharex=True)
    t = [r["TIME_MS"] / 1000.0 for r in out]
    axes[0].plot(t, [r["TARGET"] for r in out], label="Target (deg)", color="red", linestyle="--")
    axes[0].plot(t, [r["YAW"] for r in out], label="Yaw (deg)", color="blue")
    axes[0].set_ylabel("Angle (deg)")
    axes[0].set_title("SG90 Closed-Loop Control Response (Simulated)")
    axes[0].legend()
    axes[0].grid(True)

    axes[1].plot(t, [r["OUTPUT"] for r in out], label="PID Output (us)", color="green")
    axes[1].set_xlabel("Time (s)")
    axes[1].set_ylabel("PWM (us)")
    axes[1].legend()
    axes[1].grid(True)

    plt.tight_layout()
    plt.savefig(png_path, dpi=150)
    print(f"[INFO] CSV: {csv_path}")
    print(f"[INFO] PNG: {png_path}")


if __name__ == "__main__":
    main()
