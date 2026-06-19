#!/usr/bin/env python3
"""
静音调参助手：舵机不输出扭矩时，读取串口数据并实时提示
手应该往哪个方向偏，辅助人在回路调参。

串口格式：TIME_MS,MODE,TARGET,YAW,OUTPUT,KP,KI,KD
用法：
    python3 quiet_tuning_assistant.py -p /dev/ttyACM0

提示说明：
    error = target - yaw
    error > 0 : 当前 yaw 偏小，请逆时针转动手（yaw 增大方向）
    error < 0 : 当前 yaw 偏大，请顺时针转动手（yaw 减小方向）
"""

import argparse
import csv
import sys
import time
from collections import deque
from datetime import datetime

try:
    import serial
except ImportError:
    print("[ERROR] 缺少 pyserial。请运行：pip3 install pyserial")
    sys.exit(1)


def clamp(val, lo, hi):
    return max(lo, min(hi, val))


def direction_hint(error):
    if abs(error) < 2.0:
        return "  ✓ 保持不动  "
    if error > 0:
        return "  <<< 逆时针转  "
    return "  顺时针转 >>>  "


def tuning_advice(errors, outputs, kp, ki, kd):
    """基于滑动窗口给出极简 PID 调参建议。"""
    if len(errors) < 20:
        return "数据不足，继续采样..."

    err = errors[-1]
    err_abs = abs(err)
    err_sign = 1 if err >= 0 else -1
    same_sign_count = sum(1 for e in errors if (e >= 0) == (err_sign >= 0))
    total = len(errors)
    ratio = same_sign_count / total

    # 输出震荡判断：输出多次上下穿越均值
    mean_out = sum(outputs) / len(outputs)
    crossings = sum(
        1
        for i in range(1, len(outputs))
        if (outputs[i - 1] - mean_out) * (outputs[i] - mean_out) < 0
    )

    if err_abs > 20.0 and ratio > 0.85:
        if kp < 8.0:
            return "误差长期偏大 → 可谨慎增大 Kp"
        return "误差仍大但 Kp 已高 → 检查机械/安装或降低 Kp"

    if crossings > len(outputs) * 0.25 and err_abs < 10.0:
        if kd < 0.5:
            return "输出震荡 → 增大 Kd 或减小 Kp"
        return "输出仍震荡 → 继续减小 Kp"

    if err_abs < 5.0 and ratio > 0.75:
        if ki < 0.05:
            return "接近目标但有小稳态误差 → 可微调 Ki"
        return "稳态误差仍存在 → 检查积分分离阈值或 Ki 是否过大"

    if err_abs < 2.0:
        return "当前跟踪良好，保持参数"

    return "继续观察..."


def main():
    parser = argparse.ArgumentParser(
        description="Quiet tuning assistant: read serial and tell you which way to turn your hand."
    )
    parser.add_argument("-p", "--port", default="/dev/ttyACM0", help="串口设备 (default: /dev/ttyACM0)")
    parser.add_argument("-b", "--baud", type=int, default=115200, help="波特率 (default: 115200)")
    parser.add_argument("-o", "--output", default=None, help="同时保存 CSV 日志")
    parser.add_argument("-w", "--window", type=int, default=50, help="调参建议滑动窗口样本数 (default: 50)")
    args = parser.parse_args()

    if args.output is None:
        args.output = f"quiet_tune_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"

    print(f"[INFO] 打开串口 {args.port} @ {args.baud} ...")
    try:
        ser = serial.Serial(args.port, args.baud, timeout=1)
    except serial.SerialException as e:
        print(f"[ERROR] 无法打开串口: {e}")
        sys.exit(1)

    print(f"[INFO] 开始静音调参辅助，日志保存到 {args.output}")
    print("[INFO] 按 Ctrl+C 停止\n")

    errors = deque(maxlen=args.window)
    outputs = deque(maxlen=args.window)

    with open(args.output, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["TIME_MS", "MODE", "TARGET", "YAW", "OUTPUT", "KP", "KI", "KD"])

        last_ui_update = 0
        try:
            while True:
                line = ser.readline().decode("utf-8", errors="ignore").strip()
                if not line:
                    continue
                parts = line.split(",")
                if len(parts) != 8:
                    continue
                try:
                    tick, mode = int(parts[0]), int(parts[1])
                    target, yaw, output, kp, ki, kd = (float(x) for x in parts[2:])
                except ValueError:
                    continue

                error = target - yaw
                errors.append(error)
                outputs.append(output)
                writer.writerow([tick, mode, target, yaw, output, kp, ki, kd])

                now = time.time()
                if now - last_ui_update >= 0.2:  # 每 200 ms 刷新一次界面
                    last_ui_update = now
                    hint = direction_hint(error)
                    advice = tuning_advice(errors, outputs, kp, ki, kd)
                    mode_str = "闭环" if mode else "开环/静音"
                    sys.stdout.write(
                        f"\r[{mode_str}] TGT={target:7.2f} YAW={yaw:7.2f} "
                        f"ERR={error:7.2f} OUT={output:7.1f} |{hint}| {advice}"
                    )
                    sys.stdout.flush()

        except KeyboardInterrupt:
            print("\n[INFO] 已停止")
        finally:
            ser.close()


if __name__ == "__main__":
    main()
