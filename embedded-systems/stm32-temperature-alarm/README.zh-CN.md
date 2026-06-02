[English](README.md) | 中文

# STM32 温度报警系统

这是一个基于 STM32F407 的温度报警系统。温度采集使用 DS18B20，OLED 显示当前状态，K1-K4 用于参数设置，蜂鸣器用于报警，W25Qxx SPI Flash + FatFs 用于保存配置，USART1 持续输出 VOFA+ 遥测数据。

## 功能

- OLED 默认显示当前温度、温度上限、报警时长、VOFA+ 输出状态。
- K2 进入/退出温度上限设置页；K1 上调、K4 下调，每次 0.5 C，范围 0-125.0 C。
- K3 进入/退出报警时长设置页；K1 上调、K4 下调，每次 1 秒，范围 1-3600 秒。
- 设置参数保存到外部 SPI Flash 的 `0:/CFG.BIN`，重启后自动加载。
- 温度超过上限时蜂鸣器持续鸣响，达到设置的报警时长后自动关闭。
- USART1 每秒输出一行兼容 VOFA+ FireWater 的 CSV 数据。VOFA+ 输出始终开启。

## 硬件配置

| 模块 | 引脚 | 说明 |
| --- | --- | --- |
| DS18B20 | PE0 | 单总线，开漏输出，上拉 |
| K1 | PE1 | 低电平按下 |
| K2 | PE2 | 低电平按下 |
| K3 | PE3 | 低电平按下 |
| K4 | PE4 | 低电平按下 |
| 蜂鸣器 | PB4 | 推挽输出 |
| OLED SSD1306 | PB6/PB7 | I2C1 SCL/SDA |
| W25Qxx Flash | PA5/PA6/PA7, PC4 | SPI1 SCK/MISO/MOSI, CS |
| USART1 | PA9/PA10 | TX/RX，115200 8N1 |
| ADC1_CH1 | PA1 | 串口遥测里的辅助 ADC 值 |

## FreeRTOS 任务

| 任务 | 优先级 | 栈空间 | 职责 |
| --- | --- | --- | --- |
| `defaultTask` | Normal | 1536 words | 温度采样、报警、存储、串口遥测 |
| `TaskGUI` | AboveNormal | 512 words | OLED 页面刷新 |
| `TaskKey` | High | 256 words | K1-K4 扫描和事件入队 |

## 串口输出

VOFA+ 配置：

- 数据引擎：`FireWater`
- 数据接口：串口
- 波特率：`115200`
- 数据位：8
- 停止位：1
- 校验位：None
- 数据流控：None

遥测格式：

```text
temperature,limit,adc,alarm_active,fs_ready
26.6,30.0,1468,0,1
```

`alarm_active=1` 表示蜂鸣器报警正在触发。`fs_ready=1` 表示 FatFs 已成功挂载。

## 编译和烧录

Keil 工程：

```text
MDK-ARM/171717.uvprojx
```

命令行编译：

```powershell
cd D:\hdu\171717\MDK-ARM
D:\keil\core\UV4\UV4.exe -b 171717.uvprojx -j0
```

命令行烧录：

```powershell
cd D:\hdu\171717\MDK-ARM
D:\keil\core\UV4\UV4.exe -f 171717.uvprojx -j0 -o flash_from_codex.log
```

如果命令行烧录后串口没有马上输出，按一下板子的复位键，或在 Keil 调试会话里运行目标程序。Keil 的 `-f` 负责下载 Flash，下载后是否自动运行取决于调试器的 reset/run 设置。

## 验证记录

最近一次本地检查：

- Keil 编译：`0 Error(s), 0 Warning(s)`。
- USART1 遥测：115200 波特率下持续输出 CSV。

## 参考资料

- [STM32F407 参考手册](https://www.st.com/resource/en/reference_manual/rm0090-stm32f405415-stm32f407417-stm32f427437-and-stm32f429439-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)
- [DS18B20 数据手册](https://www.analog.com/media/en/technical-documentation/data-sheets/ds18b20.pdf)
- [Keil uVision 命令行说明](https://www.keil.com/support/man/docs/uv4cl/uv4cl_commandline.htm)
