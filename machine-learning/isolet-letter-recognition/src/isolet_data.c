#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "isolet_data.h"

/* 从文本文件加载ISOLET数据集
 * 格式：每行617个浮点特征，逗号分隔，最后是类别标签(1-26)后接小数点
 */
int load_dataset(const char* filename, Dataset* dataset, int expected_samples)
{
    FILE* fp = fopen(filename, "r");
    if (!fp)
    {
        fprintf(stderr, "Error: Cannot open file %s\n", filename);
        return -1;
    }

    dataset->samples = (Sample*)malloc(expected_samples * sizeof(Sample));
    if (!dataset->samples)
    {
        fprintf(stderr, "Error: Memory allocation failed\n");
        fclose(fp);
        return -1;
    }

    dataset->num_samples = 0;
    dataset->num_features = NUM_FEATURES;
    dataset->num_classes = NUM_CLASSES;

    char line[MAX_LINE_LEN];
    int line_num = 0;

    while (fgets(line, sizeof(line), fp) && dataset->num_samples < expected_samples)
    {
        line_num++;
        char* ptr = line;
        int feature_idx = 0;

        /* 解析617个特征值 */
        while (feature_idx < NUM_FEATURES)
        {
            /* 跳过空白和逗号 */
            while (*ptr == ' ' || *ptr == ',' || *ptr == '\t') ptr++;
            if (*ptr == '\0' || *ptr == '\n') break;

            char* endptr;
            float val = strtof(ptr, &endptr);
            if (ptr == endptr)
            {
                fprintf(stderr, "Warning: Failed to parse feature %d at line %d\n",
                        feature_idx, line_num);
                break;
            }
            dataset->samples[dataset->num_samples].features[feature_idx] = val;
            feature_idx++;
            ptr = endptr;
        }

        if (feature_idx != NUM_FEATURES)
        {
            fprintf(stderr, "Warning: Line %d has only %d features, expected %d\n",
                    line_num, feature_idx, NUM_FEATURES);
            continue;
        }

        /* 跳过空白和逗号，解析标签 */
        while (*ptr == ' ' || *ptr == ',' || *ptr == '\t') ptr++;
        if (*ptr == '\0' || *ptr == '\n')
        {
            fprintf(stderr, "Warning: Missing label at line %d\n", line_num);
            continue;
        }

        int label = (int)strtol(ptr, NULL, 10);
        if (label < 1 || label > NUM_CLASSES)
        {
            fprintf(stderr, "Warning: Invalid label %d at line %d\n", label, line_num);
            continue;
        }

        dataset->samples[dataset->num_samples].label = label;
        dataset->num_samples++;
    }

    fclose(fp);

    if (dataset->num_samples != expected_samples)
    {
        fprintf(stderr, "Warning: Loaded %d samples, expected %d\n",
                dataset->num_samples, expected_samples);
    }

    printf("Successfully loaded %d samples from %s\n", dataset->num_samples, filename);
    return 0;
}

/* 释放数据集内存 */
void free_dataset(Dataset* dataset)
{
    if (dataset && dataset->samples)
    {
        free(dataset->samples);
        dataset->samples = NULL;
        dataset->num_samples = 0;
    }
}

/* 打印数据集统计信息 */
void print_dataset_stats(const Dataset* dataset)
{
    if (!dataset || dataset->num_samples == 0) return;

    printf("\n===== Dataset Statistics =====\n");
    printf("Total samples: %d\n", dataset->num_samples);
    printf("Features per sample: %d\n", dataset->num_features);
    printf("Number of classes: %d\n", dataset->num_classes);

    /* 统计各类别样本数 */
    int class_counts[NUM_CLASSES] = {0};
    for (int i = 0; i < dataset->num_samples; i++)
    {
        class_counts[dataset->samples[i].label - 1]++;
    }

    printf("\nClass distribution:\n");
    for (int c = 0; c < NUM_CLASSES; c++)
    {
        printf("  %c (class %2d): %3d samples\n", 'A' + c, c + 1, class_counts[c]);
    }
    printf("==============================\n\n");
}

/* Z-score标准化数据集 */
void normalize_dataset(Dataset* dataset, float* mean, float* std, int compute_stats)
{
    int n = dataset->num_samples;

    if (compute_stats)
    {
        /* 计算均值 */
        for (int j = 0; j < NUM_FEATURES; j++)
        {
            mean[j] = 0.0f;
            for (int i = 0; i < n; i++)
            {
                mean[j] += dataset->samples[i].features[j];
            }
            mean[j] /= n;
        }

        /* 计算标准差 */
        for (int j = 0; j < NUM_FEATURES; j++)
        {
            std[j] = 0.0f;
            for (int i = 0; i < n; i++)
            {
                float diff = dataset->samples[i].features[j] - mean[j];
                std[j] += diff * diff;
            }
            std[j] = sqrtf(std[j] / n);
            if (std[j] < 1e-8f) std[j] = 1.0f;  /* 防止除零 */
        }
    }

    /* 应用标准化 */
    for (int i = 0; i < n; i++)
    {
        for (int j = 0; j < NUM_FEATURES; j++)
        {
            dataset->samples[i].features[j] =
                (dataset->samples[i].features[j] - mean[j]) / std[j];
        }
    }
}

/* 保存为二进制格式 */
int save_dataset_binary(const char* filename, const Dataset* dataset)
{
    FILE* fp = fopen(filename, "wb");
    if (!fp)
    {
        fprintf(stderr, "Error: Cannot create file %s\n", filename);
        return -1;
    }

    fwrite(&dataset->num_samples, sizeof(int), 1, fp);
    fwrite(&dataset->num_features, sizeof(int), 1, fp);
    fwrite(&dataset->num_classes, sizeof(int), 1, fp);

    for (int i = 0; i < dataset->num_samples; i++)
    {
        fwrite(dataset->samples[i].features, sizeof(float), NUM_FEATURES, fp);
        fwrite(&dataset->samples[i].label, sizeof(int), 1, fp);
    }

    fclose(fp);
    printf("Saved %d samples to %s\n", dataset->num_samples, filename);
    return 0;
}

/* 从二进制格式加载 */
int load_dataset_binary(const char* filename, Dataset* dataset)
{
    FILE* fp = fopen(filename, "rb");
    if (!fp)
    {
        fprintf(stderr, "Error: Cannot open file %s\n", filename);
        return -1;
    }

    fread(&dataset->num_samples, sizeof(int), 1, fp);
    fread(&dataset->num_features, sizeof(int), 1, fp);
    fread(&dataset->num_classes, sizeof(int), 1, fp);

    dataset->samples = (Sample*)malloc(dataset->num_samples * sizeof(Sample));
    if (!dataset->samples)
    {
        fprintf(stderr, "Error: Memory allocation failed\n");
        fclose(fp);
        return -1;
    }

    for (int i = 0; i < dataset->num_samples; i++)
    {
        fread(dataset->samples[i].features, sizeof(float), NUM_FEATURES, fp);
        fread(&dataset->samples[i].label, sizeof(int), 1, fp);
    }

    fclose(fp);
    printf("Loaded %d samples from %s\n", dataset->num_samples, filename);
    return 0;
}

/* 查询单个样本 */
const Sample* query_sample(const Dataset* dataset, int index)
{
    if (!dataset || index < 0 || index >= dataset->num_samples)
    {
        return NULL;
    }
    return &dataset->samples[index];
}

/* 打乱数据集（Fisher-Yates算法） */
void shuffle_dataset(Dataset* dataset, unsigned int seed)
{
    if (!dataset || dataset->num_samples <= 1) return;

    /* 简单的LCG随机数生成器 */
    unsigned int rng = seed ? seed : 123456789;

    for (int i = dataset->num_samples - 1; i > 0; i--)
    {
        rng = rng * 1103515245 + 12345;
        int j = rng % (i + 1);

        /* 交换样本 */
        Sample tmp = dataset->samples[i];
        dataset->samples[i] = dataset->samples[j];
        dataset->samples[j] = tmp;
    }
}

/* 获取一个批次的数据 */
void get_batch(const Dataset* dataset, int* indices, int batch_size, int batch_idx,
               float* batch_features, int* batch_labels)
{
    int start = batch_idx * batch_size;
    int end = start + batch_size;
    if (end > dataset->num_samples) end = dataset->num_samples;

    int idx = 0;
    for (int i = start; i < end; i++)
    {
        int sample_idx = indices ? indices[i] : i;
        memcpy(&batch_features[idx * NUM_FEATURES],
               dataset->samples[sample_idx].features,
               NUM_FEATURES * sizeof(float));
        batch_labels[idx] = dataset->samples[sample_idx].label;
        idx++;
    }
}

/* 数据增强：添加高斯噪声 */
void augment_gaussian_noise(float* features, float std_dev, unsigned int* seed)
{
    unsigned int rng = *seed;
    for (int i = 0; i < NUM_FEATURES; i++)
    {
        /* Box-Muller变换生成高斯噪声 */
        rng = rng * 1103515245 + 12345;
        float u1 = (rng & 0x7FFFFFFF) / (float)0x7FFFFFFF;
        rng = rng * 1103515245 + 12345;
        float u2 = (rng & 0x7FFFFFFF) / (float)0x7FFFFFFF;

        if (u1 < 1e-7f) u1 = 1e-7f;
        float z = sqrtf(-2.0f * logf(u1)) * cosf(2.0f * 3.14159265358979f * u2);
        features[i] += std_dev * z;
    }
    *seed = rng;
}

/* 数据增强：随机擦除部分特征 */
void augment_random_erase(float* features, float erase_prob, unsigned int* seed)
{
    unsigned int rng = *seed;
    for (int i = 0; i < NUM_FEATURES; i++)
    {
        rng = rng * 1103515245 + 12345;
        float r = (rng & 0x7FFFFFFF) / (float)0x7FFFFFFF;
        if (r < erase_prob)
        {
            features[i] = 0.0f;
        }
    }
    *seed = rng;
}
