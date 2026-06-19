#!/usr/bin/env bash
set -e

cd "$(dirname "$0")"

echo "[INFO] Configuring with CMake preset Debug..."
cmake --preset Debug -DCMAKE_TOOLCHAIN_FILE="$(pwd)/cmake/gcc-arm-none-eabi.cmake"

echo "[INFO] Building..."
cmake --build build/Debug

echo "[INFO] Generating .bin and .hex..."
arm-none-eabi-objcopy -O binary build/Debug/SG90_ClosedLoop.elf build/Debug/SG90_ClosedLoop.bin
arm-none-eabi-objcopy -O ihex build/Debug/SG90_ClosedLoop.elf build/Debug/SG90_ClosedLoop.hex

echo "[INFO] Done."
arm-none-eabi-size build/Debug/SG90_ClosedLoop.elf
