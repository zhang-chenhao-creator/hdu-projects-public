# Embedded Systems

This folder collects microcontroller projects built around STM32F407 and STC controllers. The projects emphasize peripheral integration, firmware structure, and hardware-facing debugging rather than standalone algorithm demos.

## Projects

- `oled-data-curve-display`: real-time OLED curve display with ADC/UART input handling, FreeRTOS tasks, and SSD1306 rendering.
- `stm32-signal-generator`: waveform generation and measurement using DAC, DMA, PWM, ADC, UART, key input, and OLED pages.
- `sound-triggered-delay-lamp`: microphone-triggered delay lamp with analog thresholding and task-based control.
- `stm32-breathing-led`: breathing LED firmware for timer/GPIO control practice.
- `six-button-buzzer-led`: key scanning, buzzer feedback, LED pattern control, and FreeRTOS coordination.
- `ten-mode-led-patterns`: multi-mode LED pattern firmware with timing and mode-switch logic.
- `stc-breathing-led`: STC/8051-style breathing LED implementation.

Build artifacts, local debug output, generated binaries, and private reports are excluded from the public repository.
