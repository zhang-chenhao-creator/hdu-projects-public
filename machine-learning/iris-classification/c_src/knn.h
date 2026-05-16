#ifndef KNN_H
#define KNN_H

#include "common.h"

// KNN预测
int knn_predict(const Dataset *train, const double *features, int k);

#endif // KNN_H
