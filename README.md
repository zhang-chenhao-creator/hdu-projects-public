# HDU Project Portfolio

This repository is a curated public portfolio of selected engineering projects from Hangzhou Dianzi University coursework, lab practice, and competition-oriented exploration.

The focus is on readable source code and reproducible technical context, not on raw course deliverables. Reports, slides, private data, generated binaries, local editor settings, training logs, model weights, and AI conversation records are intentionally excluded.

## Project Areas

### Embedded Systems

- [`embedded-systems/oled-data-curve-display`](embedded-systems/oled-data-curve-display): STM32 OLED waveform display using FreeRTOS, ADC, UART, and SSD1306 drawing code.
- [`embedded-systems/stm32-signal-generator`](embedded-systems/stm32-signal-generator): STM32F407 waveform generator and measurement system using DAC, DMA, PWM, ADC, UART, and OLED interaction.
- [`embedded-systems/sound-triggered-delay-lamp`](embedded-systems/sound-triggered-delay-lamp): microphone-triggered delayed lighting firmware on STM32 with ADC sampling and FreeRTOS task structure.
- [`embedded-systems/stm32-breathing-led`](embedded-systems/stm32-breathing-led): STM32 breathing LED firmware focused on timer/GPIO control and embedded project setup.
- [`embedded-systems/six-button-buzzer-led`](embedded-systems/six-button-buzzer-led): STM32 key, buzzer, LED, and FreeRTOS interaction demo.
- [`embedded-systems/ten-mode-led-patterns`](embedded-systems/ten-mode-led-patterns): STM32 multi-mode LED control firmware with mode switching and timing logic.
- [`embedded-systems/stc-breathing-led`](embedded-systems/stc-breathing-led): STC microcontroller breathing LED implementation using classic 8051-style tooling.

### C Programming

- [`c-programming/student-record-system`](c-programming/student-record-system): console student record system with linked-list storage, role-based users, file I/O, and menu-driven workflows.
- [`c-programming/linear-regression-house-price`](c-programming/linear-regression-house-price): C implementation of a small linear regression pipeline with data parsing, training, prediction, and verification modules.
- [`c-programming/snake-game`](c-programming/snake-game): Windows console snake game with buffered rendering, keyboard input, collision logic, and score handling.

### Robotics

- [`quadruped-robot`](quadruped-robot): selected public-safe reinforcement-learning code snapshots for quadruped/robot locomotion exploration, including PPO training logic, feature preprocessing, reward shaping, and inference-time safety rules.

## Public Boundary

This repository keeps only source code, headers, configuration files, and minimal English documentation needed to understand the projects.

Excluded content includes:

- student IDs, class information, teammate real names, and private contact details
- course reports, homework documents, defense slides, spreadsheets, and PDFs
- compressed archives, generated binaries, build outputs, cache directories, and editor settings
- datasets, checkpoints, model weights, logs, run folders, and experiment tracking outputs
- local AI conversations and private configuration files

Some embedded projects include vendor-generated STM32/CMSIS/HAL files because they are required to keep the firmware structure understandable. The quadruped code retains upstream competition template copyright headers where present.
