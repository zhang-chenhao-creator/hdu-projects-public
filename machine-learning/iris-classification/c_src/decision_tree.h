#ifndef DECISION_TREE_H
#define DECISION_TREE_H

#include "common.h"

// 决策树节点
typedef struct TreeNode {
    int is_leaf;           // 是否为叶节点
    int class_label;       // 叶节点的类别
    int feature_idx;       // 分裂特征索引
    double threshold;      // 分裂阈值
    struct TreeNode *left;  // 左子树 (<= threshold)
    struct TreeNode *right; // 右子树 (> threshold)
} TreeNode;

// 训练决策树
void *dt_train(const Dataset *train, int max_depth);

// 预测
int dt_predict(const void *tree, const double *features);

// 释放决策树
void dt_free(void *tree);

#endif // DECISION_TREE_H
