cmake_minimum_required(VERSION 3.20)

# ---------------------------------------------------------------------------------------
# 0. 交叉编译系统配置 (必须在 project() 之前!)
# ---------------------------------------------------------------------------------------
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY) # 跳过编译器检查时的链接测试

# ---------------------------------------------------------------------------------------
# 1. 项目定义
# ---------------------------------------------------------------------------------------
project(STM32_FinalProject C CXX ASM)

# ---------------------------------------------------------------------------------------
# 2. 基础配置
# ---------------------------------------------------------------------------------------
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# 告诉 CMake 这是一个交叉编译项目 (Bare Metal)
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

# ---------------------------------------------------------------------------------------
# 2. 工具链配置 (Toolchain)
# 实际上推荐这部分放在单独的 toolchain.cmake 文件里，但为了方便展示，先写在这里
# ---------------------------------------------------------------------------------------
set(TOOLCHAIN_PREFIX arm-none-eabi-)
set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}g++)
set(CMAKE_ASM_COMPILER ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_OBJCOPY ${TOOLCHAIN_PREFIX}objcopy)
set(CMAKE_SIZE ${TOOLCHAIN_PREFIX}size)

# ---------------------------------------------------------------------------------------
# 3. 芯片架构参数 (MCU Flags)
# 对应 Makefile 里的 -mcpu=cortex-m4 ...
# ---------------------------------------------------------------------------------------
set(MCU_FLAGS 
    -mcpu=cortex-m4 
    -mthumb 
    -mfpu=fpv4-sp-d16 
    -mfloat-abi=hard
)

# 编译器通用参数
add_compile_options(
    ${MCU_FLAGS}
    -Wall -fdata-sections -ffunction-sections
    -Og -g -gdwarf-2 # 调试优化等级
)

# 链接器参数 (Linker Flags)
# 告诉链接器使用你的链接脚本 .ld
set(LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/STM32F407XX_FLASH.ld")

add_link_options(
    ${MCU_FLAGS}
    -T${LINKER_SCRIPT}
    -Wl,-Map=${CMAKE_PROJECT_NAME}.map,--cref
    -Wl,--gc-sections
    --specs=nano.specs # 使用精简库
    -lc -lm 
    # 删掉 -lnosys，因为它和 Core/Src/syscalls.c 里的实现冲突了
)

# ---------------------------------------------------------------------------------------
# 4. 定义宏 (Defines)
# 对应 Makefile 里的 -DSTM32F407xx ...
# ---------------------------------------------------------------------------------------
add_definitions(
    -DUSE_HAL_DRIVER
    -DSTM32F407xx
)

# ---------------------------------------------------------------------------------------
# 5. 包含目录 (Include Paths)
# 对应 Makefile 里的 -I...
# ---------------------------------------------------------------------------------------
include_directories(
    Core/Inc
    Drivers/STM32F4xx_HAL_Driver/Inc
    Drivers/STM32F4xx_HAL_Driver/Inc/Legacy
    Drivers/CMSIS/Device/ST/STM32F4xx/Include
    Drivers/CMSIS/Include
)

# ---------------------------------------------------------------------------------------
# 6. 源文件 (Sources)
# 使用 file(GLOB_RECURSE ...) 虽然方便，但推荐用 aux_source_directory 或手动列出
# 这里为了稳妥，我们手动模拟你 Makefiel 里的文件列表
# ---------------------------------------------------------------------------------------

# 收集 HAL 库源文件
file(GLOB_RECURSE HAL_DRIVER_SOURCES "Drivers/STM32F4xx_HAL_Driver/Src/*.c")

# 剔除模板文件
list(FILTER HAL_DRIVER_SOURCES EXCLUDE REGEX ".*_template\\.c$")

# 收集 Core 里的源文件
file(GLOB_RECURSE CORE_SOURCES "Core/Src/*.c")

# 启动文件 (Startup code) - 注意这里是 .s 文件
set(STARTUP_SOURCE "startup_stm32f407xx.s")

# ---------------------------------------------------------------------------------------
# 7. 生成可执行文件 (Executable)
# ---------------------------------------------------------------------------------------
add_executable(${CMAKE_PROJECT_NAME}.elf
    ${CORE_SOURCES}
    ${HAL_DRIVER_SOURCES}
    ${STARTUP_SOURCE}
)

# ---------------------------------------------------------------------------------------
# 8. 生成 HEX 和 BIN (Post-build steps)
# ---------------------------------------------------------------------------------------
add_custom_command(TARGET ${CMAKE_PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${CMAKE_PROJECT_NAME}.elf> ${CMAKE_PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${CMAKE_PROJECT_NAME}.elf> ${CMAKE_PROJECT_NAME}.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${CMAKE_PROJECT_NAME}.elf>
    COMMENT "Generating HEX and BIN files..."
)