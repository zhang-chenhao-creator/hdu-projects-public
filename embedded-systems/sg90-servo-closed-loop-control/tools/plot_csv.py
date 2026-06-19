#!/usr/bin/env python3
"""
绘制 SG90 闭环控制实验 CSV 数据曲线。
用法：python3 plot_csv.py sg90_log_xxx.csv
"""

import argparse
import csv
import sys
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt


def main():
    parser = argparse.ArgumentParser(description="Plot SG90 closed-loop CSV data.")
    parser.add_argument("csv", help="Input CSV file")
    parser.add_argument("-o", "--output", default=None, help="Output PNG file")
    args = parser.parse_args()

    path = Path(args.csv)
    if not path.exists():
        print(f"[ERROR] File not found: {args.csv}")
        sys.exit(1)

    time_ms = []
    mode = []
    target = []
    yaw = []
    output = []
    pwm = []

    with open(path, "r", newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            try:
                time_ms.append(float(row["TIME_MS"]) / 1000.0)
                mode.append(int(row["MODE"]))
                target.append(float(row["TARGET"]))
                yaw.append(float(row["YAW"]))
                output.append(float(row["OUTPUT"]))
                if "PWM" in row:
                    pwm.append(float(row["PWM"]))
            except (KeyError, ValueError):
                continue

    if not time_ms:
        print("[ERROR] No valid data found in CSV.")
        sys.exit(1)

    fig, axes = plt.subplots(2 if not pwm else 3, 1, figsize=(10, 10), sharex=True)

    axes[0].plot(time_ms, target, label="Target (deg)", color="red", linestyle="--")
    axes[0].plot(time_ms, yaw, label="Yaw (deg)", color="blue")
    axes[0].set_ylabel("Angle (deg)")
    axes[0].set_title("SG90 Closed-Loop Control Response")
    axes[0].legend()
    axes[0].grid(True)

    axes[1].plot(time_ms, output, label="PID Output (us)", color="green")
    axes[1].set_ylabel("PID Output (us)")
    axes[1].legend()
    axes[1].grid(True)

    if pwm:
        axes[2].plot(time_ms, pwm, label="PWM (us)", color="orange")
        axes[2].set_xlabel("Time (s)")
        axes[2].set_ylabel("PWM (us)")
        axes[2].legend()
        axes[2].grid(True)

    out_path = args.output or path.with_suffix(".png")
    plt.tight_layout()
    plt.savefig(out_path, dpi=150)
    print(f"[INFO] Plot saved to {out_path}")


if __name__ == "__main__":
    main()
