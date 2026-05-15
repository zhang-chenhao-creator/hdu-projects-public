#ifndef REGRESSION_H
#define REGRESSION_H

#include "data.h"

/* 一元线性回归：用1个特征预测目标 */
void simple_lr_train(const Dataset *ds, int feat_col,
                     double *w, double *b,
                     int epochs, double lr);

/* 多元线性回归：用多个特征预测目标 */
void multi_lr_train(const Dataset *ds, const int *feat_cols, int n_feats,
                    double *weights, double *b,
                    int epochs, double lr);

/* 计算RMSE（在归一化空间） */
double calc_rmse_simple(const Dataset *ds, int feat_col, double w, double b);
double calc_rmse_multi(const Dataset *ds, const int *feat_cols, int n_feats,
                       const double *weights, double b);

#endif
