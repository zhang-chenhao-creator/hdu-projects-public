#ifndef SVM_H
#define SVM_H

#include "common.h"

// SVM模型结构（简化版线性SVM）
typedef struct {
    double weights[NUM_CLASSES][NUM_FEATURES];
    double bias[NUM_CLASSES];
    double C;
    int n_iter;
} SVMModel;

// 训练SVM
void *svm_train(const Dataset *train, double C, int max_iter);

// 预测
int svm_predict(const void *model, const double *features);

// 释放模型
void svm_free(void *model);

#endif // SVM_H
