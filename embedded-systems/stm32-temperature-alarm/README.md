English | [中文](README.zh-CN.md)

# STM32 Temperature Alarm

This project is an STM32F407 temperature alarm system. It uses a DS18B20 sensor for temperature measurement, an SSD1306 OLED for display, K1-K4 for parameter setting, a buzzer for alarm output, W25Qxx SPI Flash with FatFs for configuration storage, and USART1 for continuous VOFA+ telemetry.

## Features

- OLED default page shows current temperature, temperature limit, alarm duration, and VOFA+ output state.
- K2 enters/exits the temperature limit page; K1 increases and K4 decreases the limit by 0.5 C (range 0-125.0 C).
- K3 enters/exits the alarm duration page; K1 increases and K4 decreases the duration by 1 second (range 1-3600 s).
- Settings are saved to `0:/CFG.BIN` on the external SPI Flash and loaded after restart.
- When temperature exceeds the limit, the buzzer sounds continuously for the configured alarm duration, then stops automatically.
- USART1 sends one VOFA+ FireWater-compatible CSV telemetry frame every second. VOFA+ output is always enabled.

## Hardware Configuration

| Module | Pin | Notes |
| --- | --- | --- |
| DS18B20 | PE0 | 1-Wire, open-drain output, pull-up |
| K1 | PE1 | Active-low key |
| K2 | PE2 | Active-low key |
| K3 | PE3 | Active-low key |
| K4 | PE4 | Active-low key |
| Buzzer | PB4 | Push-pull output |
| OLED SSD1306 | PB6/PB7 | I2C1 SCL/SDA |
| W25Qxx Flash | PA5/PA6/PA7, PC4 | SPI1 SCK/MISO/MOSI, CS |
| USART1 | PA9/PA10 | TX/RX, 115200 8N1 |
| ADC1_CH1 | PA1 | Diagnostic ADC value in serial telemetry |

## FreeRTOS Tasks

| Task | Priority | Stack | Responsibility |
| --- | --- | --- | --- |
| `defaultTask` | Normal | 1536 words | Temperature sampling, alarm, storage, UART telemetry |
| `TaskGUI` | AboveNormal | 512 words | OLED page rendering |
| `TaskKey` | High | 256 words | K1-K4 scan and event queue |

## Serial Output

VOFA+ settings:

- Data engine: `FireWater`
- Data interface: Serial
- Baud rate: `115200`
- Data bits: 8
- Stop bits: 1
- Parity: None
- Flow control: None

Telemetry frame:

```text
temperature,limit,adc,alarm_active,fs_ready
26.6,30.0,1468,0,1
```

`alarm_active=1` means the buzzer alarm is active. `fs_ready=1` means FatFs mounted successfully.

## Build and Flash

Open the project in Keil MDK-ARM:

```text
MDK-ARM/171717.uvprojx
```

Command-line build:

```powershell
cd D:\hdu\171717\MDK-ARM
D:\keil\core\UV4\UV4.exe -b 171717.uvprojx -j0
```

Command-line flash:

```powershell
cd D:\hdu\171717\MDK-ARM
D:\keil\core\UV4\UV4.exe -f 171717.uvprojx -j0 -o flash_from_codex.log
```

If the serial port has no output immediately after command-line flashing, reset the board or start a Keil debug session and run the target. Keil's `-f` command downloads the program to Flash; running after download depends on the debugger reset/run settings.

## Verification

Latest local checks:

- Keil build: `0 Error(s), 0 Warning(s)`.
- USART1 telemetry: continuous CSV output at 115200 baud.

## References

- [STM32F407 reference manual](https://www.st.com/resource/en/reference_manual/rm0090-stm32f405415-stm32f407417-stm32f427437-and-stm32f429439-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)
- [DS18B20 datasheet](https://www.analog.com/media/en/technical-documentation/data-sheets/ds18b20.pdf)
- [Keil uVision command line](https://www.keil.com/support/man/docs/uv4cl/uv4cl_commandline.htm)
