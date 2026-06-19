#!/usr/bin/env python3
"""
串口数据记录工具：读取 SG90 闭环控制实验的 CSV 输出并保存为 CSV 文件。
串口输出格式：TIME_MS,MODE,TARGET,YAW,OUTPUT,PWM,KP,KI,KD
"""

import argparse
import csv
import sys
import serial
from datetime import datetime


def main():
    parser = argparse.ArgumentParser(description="Log serial CSV data from SG90 closed-loop controller.")
    parser.add_argument("-p", "--port", default="/dev/ttyACM0", help="Serial port (default: /dev/ttyACM0)")
    parser.add_argument("-b", "--baud", type=int, default=115200, help="Baud rate (default: 115200)")
    parser.add_argument("-o", "--output", default=None, help="Output CSV file")
    args = parser.parse_args()

    if args.output is None:
        args.output = f"sg90_log_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"

    print(f"[INFO] Opening {args.port} at {args.baud} baud...")
    try:
        ser = serial.Serial(args.port, args.baud, timeout=1)
    except serial.SerialException as e:
        print(f"[ERROR] Cannot open serial port: {e}")
        sys.exit(1)

    print(f"[INFO] Logging to {args.output}. Press Ctrl+C to stop.")
    with open(args.output, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["TIME_MS", "MODE", "TARGET", "YAW", "OUTPUT", "PWM", "KP", "KI", "KD"])
        try:
            while True:
                line = ser.readline().decode("utf-8", errors="ignore").strip()
                if not line:
                    continue
                parts = line.split(",")
                if len(parts) != 9:
                    continue
                try:
                    row = [int(parts[0]), int(parts[1])] + [float(x) for x in parts[2:8]] + [float(parts[8])]
                except ValueError:
                    continue
                writer.writerow(row)
                print(line)
        except KeyboardInterrupt:
            print("\n[INFO] Stopped by user.")
        finally:
            ser.close()


if __name__ == "__main__":
    main()
