#include "data_loader.h"

// 全局变量定义
const char *FEATURE_NAMES[NUM_FEATURES] = {"花萼长度", "花萼宽度", "花瓣长度", "花瓣宽度"};
const char *CLASS_NAMES[NUM_CLASSES] = {"Iris-setosa", "Iris-versicolor", "Iris-virginica"};
const char *CLASS_NAMES_CN[NUM_CLASSES] = {"山鸢尾", "变色鸢尾", "弗吉尼亚鸢尾"};

// 根据类别名称获取标签索引
static int get_label_index(const char *label) {
    for (int i = 0; i < NUM_CLASSES; i++) {
        if (strcmp(label, CLASS_NAMES[i]) == 0) {
            return i;
        }
    }
    return -1;  // 未知类别
}

// 从文件加载数据集
int load_dataset(const char *filename, Dataset *dataset) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        printf("错误: 无法打开文件 %s\n", filename);
        return -1;
    }

    char line[MAX_LINE_LEN];
    dataset->n_samples = 0;

    while (fgets(line, sizeof(line), fp) && dataset->n_samples < MAX_SAMPLES) {
        // 去除换行符
        line[strcspn(line, "\r\n")] = 0;

        // 跳过空行
        if (strlen(line) == 0) continue;

        // 解析CSV行
        char *token;
        char *rest = line;
        double features[NUM_FEATURES];
        char label_str[64];
        int feat_idx = 0;

        // 读取4个特征
        while ((token = strtok(rest, ",")) != NULL && feat_idx < NUM_FEATURES) {
            rest = NULL;
            features[feat_idx++] = atof(token);
        }

        // 读取类别标签
        if (token != NULL) {
            strncpy(label_str, token, sizeof(label_str) - 1);
            label_str[sizeof(label_str) - 1] = 0;

            int label = get_label_index(label_str);
            if (label >= 0) {
                Sample *s = &dataset->samples[dataset->n_samples];
                memcpy(s->features, features, sizeof(features));
                s->label = label;
                dataset->n_samples++;
            }
        }
    }

    fclose(fp);
    printf("成功加载 %d 条数据\n", dataset->n_samples);
    return 0;
}

// 划分训练集和测试集
void split_dataset(const Dataset *dataset, TrainTestSplit *split, double test_ratio) {
    int n = dataset->n_samples;
    int n_test = (int)(n * test_ratio);
    int n_train = n - n_test;

    // 创建索引数组并打乱
    int indices[MAX_SAMPLES];
    for (int i = 0; i < n; i++) indices[i] = i;

    // Fisher-Yates洗牌算法
    srand((unsigned int)time(NULL));
    for (int i = n - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int temp = indices[i];
        indices[i] = indices[j];
        indices[j] = temp;
    }

    // 划分数据
    split->train.n_samples = n_train;
    split->test.n_samples = n_test;

    for (int i = 0; i < n_train; i++) {
        split->train.samples[i] = dataset->samples[indices[i]];
    }
    for (int i = 0; i < n_test; i++) {
        split->test.samples[i] = dataset->samples[indices[n_train + i]];
    }

    printf("数据划分完成: 训练集 %d 条, 测试集 %d 条\n", n_train, n_test);
}

// 数据标准化
void standardize(Dataset *train, Dataset *test) {
    double mean[NUM_FEATURES] = {0};
    double std[NUM_FEATURES] = {0};

    // 计算训练集均值
    for (int i = 0; i < train->n_samples; i++) {
        for (int j = 0; j < NUM_FEATURES; j++) {
            mean[j] += train->samples[i].features[j];
        }
    }
    for (int j = 0; j < NUM_FEATURES; j++) {
        mean[j] /= train->n_samples;
    }

    // 计算训练集标准差
    for (int i = 0; i < train->n_samples; i++) {
        for (int j = 0; j < NUM_FEATURES; j++) {
            double diff = train->samples[i].features[j] - mean[j];
            std[j] += diff * diff;
        }
    }
    for (int j = 0; j < NUM_FEATURES; j++) {
        std[j] = sqrt(std[j] / train->n_samples);
        if (std[j] < 1e-10) std[j] = 1.0;  // 防止除零
    }

    // 标准化训练集
    for (int i = 0; i < train->n_samples; i++) {
        for (int j = 0; j < NUM_FEATURES; j++) {
            train->samples[i].features[j] =
                (train->samples[i].features[j] - mean[j]) / std[j];
        }
    }

    // 用训练集的参数标准化测试集
    for (int i = 0; i < test->n_samples; i++) {
        for (int j = 0; j < NUM_FEATURES; j++) {
            test->samples[i].features[j] =
                (test->samples[i].features[j] - mean[j]) / std[j];
        }
    }

    printf("数据标准化完成\n");
}

// 打印数据集信息
void print_dataset_info(const Dataset *dataset) {
    int class_count[NUM_CLASSES] = {0};
    for (int i = 0; i < dataset->n_samples; i++) {
        class_count[dataset->samples[i].label]++;
    }
    printf("数据集样本数: %d\n", dataset->n_samples);
    for (int i = 0; i < NUM_CLASSES; i++) {
        printf("  %s: %d 条\n", CLASS_NAMES_CN[i], class_count[i]);
    }
}
