#ifndef DATA_H
#define DATA_H

#define N_SAMPLES  506
#define N_FEATURES 13
#define N_COLS     14  /* 13 features + 1 target */

typedef struct {
    double raw[N_SAMPLES][N_COLS];       /* 原始数据 */
    double norm[N_SAMPLES][N_COLS];      /* 归一化数据 */
    double min[N_COLS], max[N_COLS];     /* 每列的min/max */
    double corr[N_FEATURES];             /* 各特征与MEDV的相关系数 */
    const char *names[N_COLS];
} Dataset;

/* 读取数据文件 */
int data_load(Dataset *ds, const char *filename);

/* Min-Max归一化（对特征列和目标列分别归一化） */
void data_normalize(Dataset *ds);

/* 计算各特征与目标列(MEDV=col13)的Pearson相关系数 */
void data_correlation(Dataset *ds);

/* 获取相关系数绝对值最大的top_k个特征的列索引（按相关性降序） */
void data_top_features(const Dataset *ds, int *indices, int top_k);

#endif
