#include "decision_tree.h"

// 计算熵
static double entropy(const int *counts, int total) {
    if (total == 0) return 0.0;
    double ent = 0.0;
    for (int i = 0; i < NUM_CLASSES; i++) {
        if (counts[i] > 0) {
            double p = (double)counts[i] / total;
            ent -= p * log2(p);
        }
    }
    return ent;
}

// 统计各类别数量
static void count_classes(const Sample *samples, int n, int *counts) {
    memset(counts, 0, NUM_CLASSES * sizeof(int));
    for (int i = 0; i < n; i++) {
        counts[samples[i].label]++;
    }
}

// 找出最多的类别
static int majority_class(const int *counts) {
    int max_count = 0;
    int majority = 0;
    for (int i = 0; i < NUM_CLASSES; i++) {
        if (counts[i] > max_count) {
            max_count = counts[i];
            majority = i;
        }
    }
    return majority;
}

// 创建叶节点
static TreeNode *create_leaf(int class_label) {
    TreeNode *node = (TreeNode *)malloc(sizeof(TreeNode));
    node->is_leaf = 1;
    node->class_label = class_label;
    node->feature_idx = -1;
    node->threshold = 0.0;
    node->left = NULL;
    node->right = NULL;
    return node;
}

// 递归构建决策树
static TreeNode *build_tree(Sample *samples, int n, int depth, int max_depth) {
    int class_counts[NUM_CLASSES];
    count_classes(samples, n, class_counts);

    // 终止条件：所有样本属于同一类别
    int non_zero = 0;
    for (int i = 0; i < NUM_CLASSES; i++) {
        if (class_counts[i] > 0) non_zero++;
    }
    if (non_zero == 1) {
        for (int i = 0; i < NUM_CLASSES; i++) {
            if (class_counts[i] > 0) return create_leaf(i);
        }
    }

    // 终止条件：达到最大深度
    if (max_depth > 0 && depth >= max_depth) {
        return create_leaf(majority_class(class_counts));
    }

    // 终止条件：样本数太少
    if (n < 2) {
        return create_leaf(majority_class(class_counts));
    }

    // 计算当前熵
    double parent_entropy = entropy(class_counts, n);

    // 寻找最佳分裂点
    int best_feature = -1;
    double best_threshold = 0.0;
    double best_gain = -1.0;

    for (int feat = 0; feat < NUM_FEATURES; feat++) {
        // 获取该特征的所有值并排序
        double values[MAX_SAMPLES];
        for (int i = 0; i < n; i++) {
            values[i] = samples[i].features[feat];
        }

        // 简单排序
        for (int i = 0; i < n - 1; i++) {
            for (int j = 0; j < n - i - 1; j++) {
                if (values[j] > values[j + 1]) {
                    double temp = values[j];
                    values[j] = values[j + 1];
                    values[j + 1] = temp;
                }
            }
        }

        // 尝试不同的阈值
        for (int i = 0; i < n - 1; i++) {
            if (values[i] == values[i + 1]) continue;

            double threshold = (values[i] + values[i + 1]) / 2.0;

            // 分裂数据
            int left_counts[NUM_CLASSES] = {0};
            int right_counts[NUM_CLASSES] = {0};
            int left_n = 0, right_n = 0;

            for (int j = 0; j < n; j++) {
                if (samples[j].features[feat] <= threshold) {
                    left_counts[samples[j].label]++;
                    left_n++;
                } else {
                    right_counts[samples[j].label]++;
                    right_n++;
                }
            }

            // 计算信息增益
            double left_entropy = entropy(left_counts, left_n);
            double right_entropy = entropy(right_counts, right_n);
            double weighted_entropy = ((double)left_n / n) * left_entropy +
                                      ((double)right_n / n) * right_entropy;
            double gain = parent_entropy - weighted_entropy;

            if (gain > best_gain) {
                best_gain = gain;
                best_feature = feat;
                best_threshold = threshold;
            }
        }
    }

    // 如果没有找到好的分裂点，创建叶节点
    if (best_gain <= 0) {
        return create_leaf(majority_class(class_counts));
    }

    // 创建内部节点
    TreeNode *node = (TreeNode *)malloc(sizeof(TreeNode));
    node->is_leaf = 0;
    node->feature_idx = best_feature;
    node->threshold = best_threshold;

    // 分裂数据
    Sample left_samples[MAX_SAMPLES], right_samples[MAX_SAMPLES];
    int left_n = 0, right_n = 0;

    for (int i = 0; i < n; i++) {
        if (samples[i].features[best_feature] <= best_threshold) {
            left_samples[left_n++] = samples[i];
        } else {
            right_samples[right_n++] = samples[i];
        }
    }

    // 递归构建子树
    node->left = build_tree(left_samples, left_n, depth + 1, max_depth);
    node->right = build_tree(right_samples, right_n, depth + 1, max_depth);

    return node;
}

// 训练决策树
void *dt_train(const Dataset *train, int max_depth) {
    // 创建数据副本用于构建树
    Sample *samples = (Sample *)malloc(train->n_samples * sizeof(Sample));
    memcpy(samples, train->samples, train->n_samples * sizeof(Sample));

    TreeNode *tree = build_tree(samples, train->n_samples, 0, max_depth);

    free(samples);
    return (void *)tree;
}

// 预测
int dt_predict(const void *tree, const double *features) {
    const TreeNode *node = (const TreeNode *)tree;

    while (!node->is_leaf) {
        if (features[node->feature_idx] <= node->threshold) {
            node = node->left;
        } else {
            node = node->right;
        }
    }

    return node->class_label;
}

// 释放决策树
static void free_tree(TreeNode *node) {
    if (node == NULL) return;
    free_tree(node->left);
    free_tree(node->right);
    free(node);
}

void dt_free(void *tree) {
    free_tree((TreeNode *)tree);
}
