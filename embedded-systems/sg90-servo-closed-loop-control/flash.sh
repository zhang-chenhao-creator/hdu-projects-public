#!/usr/bin/env bash
set -e

cd "$(dirname "$0")"

ELF="build/Debug/SG90_ClosedLoop.elf"
HEX="build/Debug/SG90_ClosedLoop.hex"
PACK="Keil.STM32F4xx_DFP.3.1.1.pack"

if [ ! -f "$ELF" ]; then
    echo "[ERROR] $ELF not found. Run ./build.sh first."
    exit 1
fi

if [ ! -f "$PACK" ]; then
    echo "[ERROR] $PACK not found. Please download Keil.STM32F4xx_DFP.3.1.1.pack to the project root."
    exit 1
fi

echo "[INFO] Generating $HEX ..."
arm-none-eabi-objcopy -O ihex "$ELF" "$HEX"

echo "[INFO] Flashing $HEX via CMSIS-DAP (pyocd) ..."
pyocd flash --target stm32f407vetx --pack "$PACK" "$HEX"

echo "[INFO] Flash done."
