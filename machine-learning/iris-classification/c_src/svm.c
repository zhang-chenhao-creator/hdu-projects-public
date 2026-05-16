#include "svm.h"

// 训练SVM（简化版：线性SVM，One-vs-Rest策略）
void *svm_train(const Dataset *train, double C, int max_iter) {
    SVMModel *model = (SVMModel *)malloc(sizeof(SVMModel));
    model->C = C;
    model->n_iter = max_iter;

    // 初始化权重和偏置
    memset(model->weights, 0, sizeof(model->weights));
    memset(model->bias, 0, sizeof(model->bias));

    double learning_rate = 0.001;

    // 对每个类别训练一个二分类SVM (One-vs-Rest)
    for (int cls = 0; cls < NUM_CLASSES; cls++) {
        for (int iter = 0; iter < max_iter; iter++) {
            // 随机打乱训练数据
            int indices[MAX_SAMPLES];
            for (int i = 0; i < train->n_samples; i++) indices[i] = i;
            for (int i = train->n_samples - 1; i > 0; i--) {
                int j = rand() % (i + 1);
                int temp = indices[i];
                indices[i] = indices[j];
                indices[j] = temp;
            }

            for (int idx = 0; idx < train->n_samples; idx++) {
                int i = indices[idx];
                const Sample *s = &train->samples[i];

                // 计算当前样本的得分
                double score = model->bias[cls];
                for (int f = 0; f < NUM_FEATURES; f++) {
                    score += model->weights[cls][f] * s->features[f];
                }

                // 标签：当前类别为+1，其他类别为-1
                int y = (s->label == cls) ? 1 : -1;

                // 梯度更新
                if (y * score < 1.0) {
                    // 违反间隔约束
                    for (int f = 0; f < NUM_FEATURES; f++) {
                        model->weights[cls][f] += learning_rate * (y * s->features[f] - C * model->weights[cls][f]);
                    }
                    model->bias[cls] += learning_rate * y;
                } else {
                    // 仅正则化
                    for (int f = 0; f < NUM_FEATURES; f++) {
                        model->weights[cls][f] -= learning_rate * C * model->weights[cls][f];
                    }
                }
            }

            // 学习率衰减
            learning_rate *= 0.999;
        }
    }

    printf("SVM训练完成 (迭代次数: %d)\n", max_iter);
    return (void *)model;
}

// 预测
int svm_predict(const void *model_ptr, const double *features) {
    const SVMModel *model = (const SVMModel *)model_ptr;

    double max_score = -1e10;
    int predicted = 0;

    for (int cls = 0; cls < NUM_CLASSES; cls++) {
        double score = model->bias[cls];
        for (int f = 0; f < NUM_FEATURES; f++) {
            score += model->weights[cls][f] * features[f];
        }

        if (score > max_score) {
            max_score = score;
            predicted = cls;
        }
    }

    return predicted;
}

// 释放模型
void svm_free(void *model) {
    free(model);
}
