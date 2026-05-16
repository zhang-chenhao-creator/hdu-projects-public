#include "knn.h"

// 计算欧氏距离
static double euclidean_distance(const double *a, const double *b) {
    double sum = 0.0;
    for (int i = 0; i < NUM_FEATURES; i++) {
        double diff = a[i] - b[i];
        sum += diff * diff;
    }
    return sqrt(sum);
}

// 用于排序的结构
typedef struct {
    double distance;
    int label;
} Neighbor;

// 比较函数，用于qsort
static int compare_neighbors(const void *a, const void *b) {
    const Neighbor *na = (const Neighbor *)a;
    const Neighbor *nb = (const Neighbor *)b;
    if (na->distance < nb->distance) return -1;
    if (na->distance > nb->distance) return 1;
    return 0;
}

// KNN预测
int knn_predict(const Dataset *train, const double *features, int k) {
    if (k > train->n_samples) k = train->n_samples;

    // 计算到所有训练样本的距离
    Neighbor neighbors[MAX_SAMPLES];
    for (int i = 0; i < train->n_samples; i++) {
        neighbors[i].distance = euclidean_distance(features, train->samples[i].features);
        neighbors[i].label = train->samples[i].label;
    }

    // 按距离排序
    qsort(neighbors, train->n_samples, sizeof(Neighbor), compare_neighbors);

    // 统计k个最近邻的类别
    int class_count[NUM_CLASSES] = {0};
    for (int i = 0; i < k; i++) {
        class_count[neighbors[i].label]++;
    }

    // 找出出现次数最多的类别
    int max_count = 0;
    int predicted = 0;
    for (int i = 0; i < NUM_CLASSES; i++) {
        if (class_count[i] > max_count) {
            max_count = class_count[i];
            predicted = i;
        }
    }

    return predicted;
}
