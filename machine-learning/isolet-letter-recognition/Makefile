# ISOLET英文字母识别系统 Makefile
# 使用CUDA C实现GPU加速的深度学习分类器

# 编译器设置
NVCC = nvcc
CC = gcc

# 检测conda环境中的nvcc
NVCC_PATH := $(shell conda run -n isolet_gpu which nvcc 2>/dev/null)
ifeq ($(NVCC_PATH),)
    NVCC_PATH := $(shell which nvcc 2>/dev/null)
endif
ifneq ($(NVCC_PATH),)
    NVCC := $(NVCC_PATH)
endif

# 编译选项
CUDA_ARCH = -arch=sm_89  # RTX 5090 使用 sm_89 (Blackwell架构)
NVCC_FLAGS = -O3 $(CUDA_ARCH) -use_fast_math -Xcompiler "-Wall -Wextra"
CFLAGS = -O3 -Wall -Wextra -std=c11
LDFLAGS = -lm

# 包含路径
CUDA_INCLUDE = /home/cpc/miniconda3/envs/isolet_gpu/targets/x86_64-linux/include
INCLUDES = -Iinclude -I$(CUDA_INCLUDE)

# 源文件
C_SOURCES = src/isolet_data.c src/utils.c src/main.c
CU_SOURCES = src/neural_net.cu

# 目标文件
C_OBJECTS = $(C_SOURCES:src/%.c=build/%.o)
CU_OBJECTS = $(CU_SOURCES:src/%.cu=build/%.o)
OBJECTS = $(C_OBJECTS) $(CU_OBJECTS)

# 可执行文件
TARGET = isolet_recognizer

# 默认目标
all: $(TARGET)

# 创建build目录
build:
	mkdir -p build

# 编译C源文件
build/%.o: src/%.c | build
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# 编译CUDA源文件
build/%.o: src/%.cu | build
	$(NVCC) $(NVCC_FLAGS) $(INCLUDES) -c $< -o $@

# 链接（使用nvcc链接CUDA对象文件）
$(TARGET): $(OBJECTS)
	$(NVCC) $(NVCC_FLAGS) $(OBJECTS) -o $@ $(LDFLAGS)
	@echo "Build successful! Run with: ./$(TARGET)"

# 清理
clean:
	rm -rf build $(TARGET)
	rm -f classification_results.txt confusion_matrix.txt
	rm -f isolet_model.bin data/*.bin

# 运行训练
run: $(TARGET)
	./$(TARGET) train

# 运行评估
eval: $(TARGET)
	./$(TARGET) eval

# 交互模式
interactive: $(TARGET)
	./$(TARGET)

# 安装依赖（conda环境）
setup:
	@echo "Setting up conda environment..."
	conda create -n isolet_gpu -y python=3.12 cuda-toolkit=13.1 || true
	@echo "Environment ready. Activate with: conda activate isolet_gpu"

# 检查环境
check:
	@echo "NVCC: $(NVCC)"
	@-$(NVCC) --version 2>/dev/null || echo "nvcc not found"
	@echo "GCC: $(CC)"
	@$(CC) --version | head -1
	@nvidia-smi --query-gpu=name,driver_version --format=csv,noheader 2>/dev/null || echo "No GPU detected"

.PHONY: all clean run eval interactive setup check
