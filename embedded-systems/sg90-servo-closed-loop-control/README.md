# SG90 Servo Closed-Loop Control

STM32F407 firmware for closed-loop SG90 servo control using MPU6050 attitude feedback, PID control, OLED display output, UART logging, and FreeRTOS task structure.

## Highlights

- SG90 servo control loop with PID implementation
- MPU6050 DMP integration for attitude feedback
- FreeRTOS-based task split for control, sensing, and output
- OLED display routines and UART diagnostic output
- CMake/GCC build files alongside STM32 HAL/CMSIS source layout
- Helper scripts for serial logging, CSV plotting, and simulated response analysis

## Repository Notes

Build outputs, generated binaries, local tuning logs, and vendor pack files are excluded. The `tools/` directory keeps only public helper scripts and small sample files useful for understanding the control workflow.
