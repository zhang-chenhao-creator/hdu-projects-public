# Low-Power Temperature Collector

Low-power RTC wake-up data acquisition firmware for STM32F407. The project combines DS18B20 temperature sampling, ADC acquisition, UART output, button control, and STOP-mode wake-up logic.

## Features

- STM32 STOP mode with RTC timed wake-up
- DS18B20 temperature acquisition
- ADC sampling for small fixed-size signal windows
- Configurable sampling interval through key input
- UART serial output suitable for VOFA+ display and debugging

## Hardware Configuration

| Peripheral | Pin | Description |
|-----------|-----|-------------|
| DS18B20 | PA1 | Temperature sensor |
| ADC1_CH1 | PA1 | Analog signal acquisition |
| USART1 | PA9/PA10 | Serial communication |
| LED1 | PE0 | Status indicator |
| K1 | PE1 | Increase sampling interval |
| K2 | PE2 | Decrease sampling interval |
| K3 | PE3 | Manual stop acquisition |

## Workflow

1. Initialize HAL, system clock, GPIO, UART, ADC, RTC, and timing helpers.
2. Read DS18B20 temperature and collect ADC samples.
3. Output compact serial data.
4. Enter STOP mode.
5. Wake up through RTC, restore clocks, and repeat.

## Development Environment

- MCU: STM32F407
- IDE: Keil MDK-ARM / STM32CubeIDE
- Config tool: STM32CubeMX
- Debug tool: VOFA+ serial oscilloscope
