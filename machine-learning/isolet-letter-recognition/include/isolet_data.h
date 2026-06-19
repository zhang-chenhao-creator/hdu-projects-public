#ifndef ISOLET_DATA_H
#define ISOLET_DATA_H

/* ISOLET数据集加载模块头文件
 * 功能：提供数据集的读取、预处理、保存和查询功能
 */

#define NUM_FEATURES    617     /* 特征维度 */
#define NUM_CLASSES     26      /* 字母类别数 A-Z */
#define TRAIN_SAMPLES   6238    /* 训练样本数 */
#define TEST_SAMPLES    1559    /* 测试样本数 */
#define MAX_LINE_LEN    16384   /* 每行最大字符数 */

typedef struct {
    float features[NUM_FEATURES];
    int label;  /* 1-26对应A-Z */
} Sample;

typedef struct {
    Sample* samples;
    int num_samples;
    int num_features;
    int num_classes;
} Dataset;

/* 数据集加载函数 */
int load_dataset(const char* filename, Dataset* dataset, int expected_samples);

/* 释放数据集内存 */
void free_dataset(Dataset* dataset);

/* 数据集统计信息 */
void print_dataset_stats(const Dataset* dataset);

/* 数据归一化（Z-score标准化） */
void normalize_dataset(Dataset* dataset, float* mean, float* std, int compute_stats);

/* 保存预处理后的数据为二进制格式 */
int save_dataset_binary(const char* filename, const Dataset* dataset);

/* 从二进制格式加载数据 */
int load_dataset_binary(const char* filename, Dataset* dataset);

/* 查询单个样本 */
const Sample* query_sample(const Dataset* dataset, int index);

/* 打乱数据集 */
void shuffle_dataset(Dataset* dataset, unsigned int seed);

/* 将数据集按批次划分 */
void get_batch(const Dataset* dataset, int* indices, int batch_size, int batch_idx,
               float* batch_features, int* batch_labels);

/* 数据增强：添加高斯噪声 */
void augment_gaussian_noise(float* features, float std_dev, unsigned int* seed);

/* 数据增强：随机擦除部分特征（模拟dropout） */
void augment_random_erase(float* features, float erase_prob, unsigned int* seed);

#endif
