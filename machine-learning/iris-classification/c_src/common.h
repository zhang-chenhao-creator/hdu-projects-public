#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

// 常量定义
#define MAX_SAMPLES 200
#define NUM_FEATURES 4
#define NUM_CLASSES 3
#define MAX_LINE_LEN 256

// 特征名称
extern const char *FEATURE_NAMES[NUM_FEATURES];
// 类别名称
extern const char *CLASS_NAMES[NUM_CLASSES];
// 类别中文名称
extern const char *CLASS_NAMES_CN[NUM_CLASSES];

// 数据结构：单个样本
typedef struct {
    double features[NUM_FEATURES];
    int label;  // 0, 1, 2
} Sample;

// 数据集
typedef struct {
    Sample samples[MAX_SAMPLES];
    int n_samples;
} Dataset;

// 训练/测试集
typedef struct {
    Dataset train;
    Dataset test;
} TrainTestSplit;

// 评估结果
typedef struct {
    double accuracy;
    double precision[NUM_CLASSES];
    double recall[NUM_CLASSES];
    double f1_score[NUM_CLASSES];
    int confusion_matrix[NUM_CLASSES][NUM_CLASSES];
    int predictions[MAX_SAMPLES];
    int n_test;
} EvalResult;

// 函数声明：数据加载
int load_dataset(const char *filename, Dataset *dataset);

// 函数声明：数据预处理
void standardize(Dataset *train, Dataset *test);
void split_dataset(const Dataset *dataset, TrainTestSplit *split, double test_ratio);

// 函数声明：分类算法
int knn_predict(const Dataset *train, const double *features, int k);
int dt_predict(const void *tree, const double *features);
void *dt_train(const Dataset *train, int max_depth);
void dt_free(void *tree);

int svm_predict(const void *model, const double *features);
void *svm_train(const Dataset *train, double C, int max_iter);
void svm_free(void *model);

// 函数声明：评估
void evaluate(const Dataset *test, const int *predictions, EvalResult *result);
void print_eval_result(const EvalResult *result, const char *algorithm_name);

#endif // COMMON_H
