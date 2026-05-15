# STM32 Signal Generator

STM32F407 waveform generator and measurement firmware with OLED interaction.

## Technical Highlights

- Generates sine, triangle, sawtooth, and square waves.
- Uses DAC + DMA + TIM6 lookup-table output for analog waveforms.
- Uses TIM2 PWM for square wave generation and duty-cycle control.
- Uses ADC1 sampling and UART5 streaming for waveform measurement.
- Uses an SSD1306 OLED screen and physical keys for mode, frequency, duty-cycle, and page interaction.
- Keeps the personal-information OLED page sanitized as `Author`, `Contributor A`, and `Demo ID`.

## Hardware Notes

- DAC output: `PA5`
- ADC input: `PA0`
- UART5: `PC12` / `PD2`
- Keys: `SW2` on `PE3`, `SW3` on `PE5`
- Display: SSD1306 OLED over I2C

## Build Notes

The project contains CMake, STM32CubeMX, and Keil-related files. Generated build outputs are not part of the public repository.
