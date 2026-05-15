#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "data.h"
#include "regression.h"

#define EPOCHS    10000
#define LR        0.01

static void print_sep(void)
{
    printf("============================================================\n");
}

int main(void)
{
    Dataset ds;

    /* 1. 加载数据 */
    if (data_load(&ds, "housing-price.txt") != 0)
        return 1;
    printf("数据加载成功: %d 个样本, %d 个特征\n", N_SAMPLES, N_FEATURES);

    /* 2. 归一化 */
    data_normalize(&ds);
    printf("数据归一化完成 (Min-Max)\n\n");

    /* 归一化RMSE转原始房价单位的缩放因子 */
    double price_range = ds.max[N_FEATURES] - ds.min[N_FEATURES];

    /* ==================== D等: 一元线性回归 ==================== */
    print_sep();
    printf("D等: 一元线性回归 (特征: LSTAT)\n");
    print_sep();

    int d_feat = 12; /* LSTAT */
    double w_d, b_d;
    simple_lr_train(&ds, d_feat, &w_d, &b_d, EPOCHS, LR);
    double rmse_d_norm = calc_rmse_simple(&ds, d_feat, w_d, b_d);
    double rmse_d = rmse_d_norm * price_range;

    printf("  特征: %s\n", ds.names[d_feat]);
    printf("  归一化RMSE: %.6f\n", rmse_d_norm);
    printf("  原始房价RMSE: %.2f (万美元)\n\n", rmse_d);

    /* ==================== C等: 相关系数 + 最优特征回归 ==================== */
    print_sep();
    printf("C等: 相关系数分析 + 最优特征一元回归\n");
    print_sep();

    data_correlation(&ds);
    printf("  各特征与MEDV的Pearson相关系数:\n");
    for (int f = 0; f < N_FEATURES; f++)
        printf("    %-8s: %+.4f\n", ds.names[f], ds.corr[f]);

    int best_feat;
    data_top_features(&ds, &best_feat, 1);
    printf("\n  最相关特征: %s (r=%.4f)\n", ds.names[best_feat], ds.corr[best_feat]);

    double w_c, b_c;
    simple_lr_train(&ds, best_feat, &w_c, &b_c, EPOCHS, LR);
    double rmse_c_norm = calc_rmse_simple(&ds, best_feat, w_c, b_c);
    double rmse_c = rmse_c_norm * price_range;

    printf("  归一化RMSE: %.6f\n", rmse_c_norm);
    printf("  原始房价RMSE: %.2f (万美元)\n\n", rmse_c);

    /* ==================== B等: 全部13个特征多元回归 ==================== */
    print_sep();
    printf("B等: 全部13个特征多元线性回归\n");
    print_sep();

    int all_feats[N_FEATURES];
    for (int i = 0; i < N_FEATURES; i++) all_feats[i] = i;

    double w_b[N_FEATURES], b_b;
    multi_lr_train(&ds, all_feats, N_FEATURES, w_b, &b_b, EPOCHS, LR);
    double rmse_b_norm = calc_rmse_multi(&ds, all_feats, N_FEATURES, w_b, b_b);
    double rmse_b = rmse_b_norm * price_range;

    printf("  归一化RMSE: %.6f\n", rmse_b_norm);
    printf("  原始房价RMSE: %.2f (万美元)\n\n", rmse_b);

    /* ==================== A等: 相关系数Top4多元回归 ==================== */
    print_sep();
    printf("A等: 相关系数Top 4特征多元回归\n");
    print_sep();

    int top4[4];
    data_top_features(&ds, top4, 4);
    printf("  选中的4个特征:\n");
    for (int i = 0; i < 4; i++)
        printf("    %d. %s (r=%.4f)\n", i + 1, ds.names[top4[i]], ds.corr[top4[i]]);

    double w_a[4], b_a;
    multi_lr_train(&ds, top4, 4, w_a, &b_a, EPOCHS, LR);
    double rmse_a_norm = calc_rmse_multi(&ds, top4, 4, w_a, b_a);
    double rmse_a = rmse_a_norm * price_range;

    printf("  归一化RMSE: %.6f\n", rmse_a_norm);
    printf("  原始房价RMSE: %.2f (万美元)\n\n", rmse_a);

    /* ==================== 汇总 ==================== */
    print_sep();
    printf("结果汇总\n");
    print_sep();
    printf("  D等 (LSTAT一元回归)        RMSE = %.2f 万美元\n", rmse_d);
    printf("  C等 (最优特征一元回归)      RMSE = %.2f 万美元\n", rmse_c);
    printf("  B等 (全部13特征多元回归)    RMSE = %.2f 万美元\n", rmse_b);
    printf("  A等 (Top4特征多元回归)      RMSE = %.2f 万美元\n", rmse_a);
    print_sep();

    return 0;
}
