#include <stdio.h>
#include <math.h>
#include "regression.h"

void simple_lr_train(const Dataset *ds, int feat_col,
                     double *w, double *b,
                     int epochs, double lr)
{
    int target = N_FEATURES; /* MEDV列 */
    int n = N_SAMPLES;

    *w = 0.0;
    *b = 0.0;

    for (int ep = 0; ep < epochs; ep++) {
        double dw = 0.0, db = 0.0;
        for (int i = 0; i < n; i++) {
            double pred = (*w) * ds->norm[i][feat_col] + (*b);
            double err  = pred - ds->norm[i][target];
            dw += err * ds->norm[i][feat_col];
            db += err;
        }
        *w -= lr * (2.0 / n) * dw;
        *b -= lr * (2.0 / n) * db;
    }
}

void multi_lr_train(const Dataset *ds, const int *feat_cols, int n_feats,
                    double *weights, double *b,
                    int epochs, double lr)
{
    int target = N_FEATURES;
    int n = N_SAMPLES;

    for (int j = 0; j < n_feats; j++)
        weights[j] = 0.0;
    *b = 0.0;

    for (int ep = 0; ep < epochs; ep++) {
        double dw[13] = {0};
        double db = 0.0;
        for (int i = 0; i < n; i++) {
            double pred = *b;
            for (int j = 0; j < n_feats; j++)
                pred += weights[j] * ds->norm[i][feat_cols[j]];
            double err = pred - ds->norm[i][target];
            for (int j = 0; j < n_feats; j++)
                dw[j] += err * ds->norm[i][feat_cols[j]];
            db += err;
        }
        for (int j = 0; j < n_feats; j++)
            weights[j] -= lr * (2.0 / n) * dw[j];
        *b -= lr * (2.0 / n) * db;
    }
}

double calc_rmse_simple(const Dataset *ds, int feat_col, double w, double b)
{
    int target = N_FEATURES;
    double sum = 0.0;
    for (int i = 0; i < N_SAMPLES; i++) {
        double pred = w * ds->norm[i][feat_col] + b;
        double err  = pred - ds->norm[i][target];
        sum += err * err;
    }
    return sqrt(sum / N_SAMPLES);
}

double calc_rmse_multi(const Dataset *ds, const int *feat_cols, int n_feats,
                       const double *weights, double b)
{
    int target = N_FEATURES;
    double sum = 0.0;
    for (int i = 0; i < N_SAMPLES; i++) {
        double pred = b;
        for (int j = 0; j < n_feats; j++)
            pred += weights[j] * ds->norm[i][feat_cols[j]];
        double err = pred - ds->norm[i][target];
        sum += err * err;
    }
    return sqrt(sum / N_SAMPLES);
}
