# OLED Data Curve Display

STM32 firmware for drawing live data curves on an SSD1306 OLED screen.

## Technical Highlights

- STM32F407 project structure generated with STM32CubeMX.
- FreeRTOS-based task organization for display and peripheral work.
- ADC/UART-oriented data path for collecting and presenting sampled values.
- Custom OLED drawing modules for ASCII text, labels, logo assets, and curve rendering.
- Public version keeps placeholder identity text such as `Author`, `Contributor A`, and `Demo ID` instead of personal information.

## Public Scope

The repository includes firmware source, headers, STM32 configuration, Keil project metadata, and vendor HAL/CMSIS dependencies needed to inspect the project. Build outputs, local debug files, and private course documents are excluded.
