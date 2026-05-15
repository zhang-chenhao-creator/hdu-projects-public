# STM32F407 Signal Generator and Measurement System

This firmware implements a compact signal generation and acquisition system on STM32F407.

## Functional Overview

- Multi-waveform output: sine, triangle, sawtooth, and square waves.
- Parameter control: adjustable frequency for all waveforms and adjustable duty cycle for square waves.
- Signal acquisition: ADC sampling with UART output for host-side visualization.
- Human-machine interface: OLED pages and key-based control for mode, frequency, duty cycle, and information display.

## Architecture

- `Core/Src/main.c`: main loop, key scanning, mode switching, and ADC polling.
- `Core/Src/tim.c`: timer configuration and square-wave PWM control.
- `Core/Src/oled.c`: OLED drawing and page display helpers.
- `Core/Src/usart.c`: UART transmission for sampled waveform points.
- `CMakeLists.txt`: cross-compilation build setup.

## Waveform Generation

Analog waveforms are produced with DAC + DMA + timer-triggered lookup tables. Square waves are produced with TIM2 PWM. Sampling uses ADC1 and sends text-formatted values through UART for host-side plotting.

This public copy keeps the technical source context and removes reports, slides, generated binaries, private identity strings, and local debug output.
