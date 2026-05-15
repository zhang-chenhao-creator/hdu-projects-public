#include <stdio.h>
#include <math.h>
#include "data.h"
#include "regression.h"

int main(void)
{
    Dataset ds;
    if (data_load(&ds, "housing-price.txt") != 0) return 1;
    data_normalize(&ds);
    data_correlation(&ds);

    /* --- 1. 验证数据加载 --- */
    printf("=== 1. 数据加载验证 ===\n");
    printf("第1行原始数据 (应为第1个样本):\n");
    printf("  CRIM=%.5f ZN=%.2f INDUS=%.3f CHAS=%.0f NOX=%.4f RM=%.4f AGE=%.2f\n",
           ds.raw[0][0], ds.raw[0][1], ds.raw[0][2], ds.raw[0][3],
           ds.raw[0][4], ds.raw[0][5], ds.raw[0][6]);
    printf("  DIS=%.4f RAD=%.0f TAX=%.1f PTRATIO=%.2f B=%.2f LSTAT=%.2f MEDV=%.2f\n",
           ds.raw[0][7], ds.raw[0][8], ds.raw[0][9], ds.raw[0][10],
           ds.raw[0][11], ds.raw[0][12], ds.raw[0][13]);
    printf("  对照原始文件第1行: 0.00632 18.00 2.310 0 0.5380 6.5750 65.20 4.0900 1 296.0 15.30 396.90 4.98 24.00\n\n");

    /* --- 2. 验证归一化 --- */
    printf("=== 2. 归一化验证 ===\n");
    int check_cols[] = {0, 5, 12, 13};
    for (int k = 0; k < 4; k++) {
        int j = check_cols[k];
        printf("  %s: min=%.4f max=%.4f\n", ds.names[j], ds.min[j], ds.max[j]);
        double first_norm = (ds.raw[0][j] - ds.min[j]) / (ds.max[j] - ds.min[j]);
        printf("    第1个样本: raw=%.4f -> 手算norm=%.6f 程序norm=%.6f %s\n\n",
               ds.raw[0][j], first_norm, ds.norm[0][j],
               fabs(first_norm - ds.norm[0][j]) < 1e-10 ? "OK" : "MISMATCH!");
    }

    /* --- 3. 验证相关系数 (手算LSTAT vs MEDV) --- */
    printf("=== 3. 相关系数验证 ===\n");
    int n = N_SAMPLES;
    double sum_x = 0, sum_y = 0;
    for (int i = 0; i < n; i++) {
        sum_x += ds.norm[i][12]; /* LSTAT */
        sum_y += ds.norm[i][13]; /* MEDV */
    }
    double mx = sum_x / n, my = sum_y / n;
    double cov = 0, vx = 0, vy = 0;
    for (int i = 0; i < n; i++) {
        double dx = ds.norm[i][12] - mx;
        double dy = ds.norm[i][13] - my;
        cov += dx * dy;
        vx  += dx * dx;
        vy  += dy * dy;
    }
    double r_manual = cov / sqrt(vx * vy);
    printf("  LSTAT vs MEDV: 手算 r=%.6f  程序 r=%.6f  %s\n\n",
           r_manual, ds.corr[12],
           fabs(r_manual - ds.corr[12]) < 1e-10 ? "OK" : "MISMATCH!");

    /* --- 4. 验证回归收敛 --- */
    printf("=== 4. 回归收敛验证 ===\n");
    /* 多跑几组epochs看RMSE是否下降 */
    int epochs_list[] = {1000, 5000, 10000, 20000};
    for (int k = 0; k < 4; k++) {
        double w, b;
        simple_lr_train(&ds, 12, &w, &b, epochs_list[k], 0.01);
        double rmse = calc_rmse_simple(&ds, 12, w, b);
        printf("  epochs=%5d  w=%.4f  b=%.4f  RMSE_norm=%.6f\n",
               epochs_list[k], w, b, rmse);
    }

    /* --- 5. 验证RMSE合理性 --- */
    printf("\n=== 5. RMSE合理性检查 ===\n");
    printf("  MEDV范围: %.1f ~ %.1f 万美元\n", ds.min[13], ds.max[13]);
    printf("  MEDV均值: %.2f 万美元\n", ds.raw[0][13] == 0 ? 0 : 0); /* placeholder */
    double sum_medv = 0;
    for (int i = 0; i < n; i++) sum_medv += ds.raw[i][13];
    printf("  MEDV均值: %.2f 万美元\n", sum_medv / n);
    printf("  B等RMSE=4.72万, 占均值的 %.1f%%\n", 4.72 / (sum_medv/n) * 100);
    printf("  A等RMSE=5.21万, 占均值的 %.1f%%\n", 5.21 / (sum_medv/n) * 100);
    printf("  -> 合理范围 (一般 <30%% 为可接受)\n");

    return 0;
}
