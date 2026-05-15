#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "data.h"

int data_load(Dataset *ds, const char *filename)
{
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "无法打开文件: %s\n", filename);
        return -1;
    }

    /* 跳过表头行 */
    char buf[1024];
    if (!fgets(buf, sizeof(buf), fp)) {
        fclose(fp);
        return -1;
    }

    for (int i = 0; i < N_SAMPLES; i++) {
        for (int j = 0; j < N_COLS; j++) {
            if (fscanf(fp, "%lf", &ds->raw[i][j]) != 1) {
                fprintf(stderr, "读取数据失败: 行 %d, 列 %d\n", i, j);
                fclose(fp);
                return -1;
            }
        }
    }
    fclose(fp);

    /* 特征名 */
    static const char *names[] = {
        "CRIM", "ZN", "INDUS", "CHAS", "NOX", "RM", "AGE",
        "DIS", "RAD", "TAX", "PTRATIO", "B", "LSTAT", "MEDV"
    };
    memcpy(ds->names, names, sizeof(names));

    return 0;
}

void data_normalize(Dataset *ds)
{
    /* 计算每列的 min / max */
    for (int j = 0; j < N_COLS; j++) {
        ds->min[j] = ds->raw[0][j];
        ds->max[j] = ds->raw[0][j];
    }
    for (int i = 1; i < N_SAMPLES; i++) {
        for (int j = 0; j < N_COLS; j++) {
            if (ds->raw[i][j] < ds->min[j]) ds->min[j] = ds->raw[i][j];
            if (ds->raw[i][j] > ds->max[j]) ds->max[j] = ds->raw[i][j];
        }
    }

    /* Min-Max归一化 */
    for (int i = 0; i < N_SAMPLES; i++) {
        for (int j = 0; j < N_COLS; j++) {
            double range = ds->max[j] - ds->min[j];
            ds->norm[i][j] = (range == 0.0) ? 0.0
                             : (ds->raw[i][j] - ds->min[j]) / range;
        }
    }
}

void data_correlation(Dataset *ds)
{
    int target = N_FEATURES; /* MEDV是第13列(索引13) */

    /* 计算目标列均值 */
    double mean_y = 0.0;
    for (int i = 0; i < N_SAMPLES; i++)
        mean_y += ds->norm[i][target];
    mean_y /= N_SAMPLES;

    double var_y = 0.0;
    for (int i = 0; i < N_SAMPLES; i++) {
        double dy = ds->norm[i][target] - mean_y;
        var_y += dy * dy;
    }

    for (int f = 0; f < N_FEATURES; f++) {
        /* 计算特征列均值 */
        double mean_x = 0.0;
        for (int i = 0; i < N_SAMPLES; i++)
            mean_x += ds->norm[i][f];
        mean_x /= N_SAMPLES;

        double cov = 0.0, var_x = 0.0;
        for (int i = 0; i < N_SAMPLES; i++) {
            double dx = ds->norm[i][f] - mean_x;
            double dy = ds->norm[i][target] - mean_y;
            cov   += dx * dy;
            var_x += dx * dx;
        }

        double denom = sqrt(var_x * var_y);
        ds->corr[f] = (denom == 0.0) ? 0.0 : cov / denom;
    }
}

void data_top_features(const Dataset *ds, int *indices, int top_k)
{
    /* 用绝对值排序，选相关性最强的top_k个 */
    int used[N_FEATURES] = {0};
    for (int k = 0; k < top_k; k++) {
        int best = -1;
        double best_val = -1.0;
        for (int f = 0; f < N_FEATURES; f++) {
            if (used[f]) continue;
            double abs_corr = fabs(ds->corr[f]);
            if (abs_corr > best_val) {
                best_val = abs_corr;
                best = f;
            }
        }
        indices[k] = best;
        used[best] = 1;
    }
}
