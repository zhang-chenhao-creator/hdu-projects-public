#include "evaluator.h"

// 评估模型
void evaluate(const Dataset *test, const int *predictions, EvalResult *result) {
    result->n_test = test->n_samples;

    // 初始化混淆矩阵
    memset(result->confusion_matrix, 0, sizeof(result->confusion_matrix));

    // 计算混淆矩阵
    for (int i = 0; i < test->n_samples; i++) {
        int true_label = test->samples[i].label;
        int pred_label = predictions[i];
        result->confusion_matrix[true_label][pred_label]++;
        result->predictions[i] = predictions[i];
    }

    // 计算准确率
    int correct = 0;
    for (int i = 0; i < NUM_CLASSES; i++) {
        correct += result->confusion_matrix[i][i];
    }
    result->accuracy = (double)correct / test->n_samples;

    // 计算每个类别的精确率、召回率、F1值
    for (int cls = 0; cls < NUM_CLASSES; cls++) {
        int tp = result->confusion_matrix[cls][cls];
        int fp = 0, fn = 0;

        for (int i = 0; i < NUM_CLASSES; i++) {
            if (i != cls) {
                fp += result->confusion_matrix[i][cls];  // 其他类别被预测为当前类别
                fn += result->confusion_matrix[cls][i];  // 当前类别被预测为其他类别
            }
        }

        // 精确率
        if (tp + fp > 0) {
            result->precision[cls] = (double)tp / (tp + fp);
        } else {
            result->precision[cls] = 0.0;
        }

        // 召回率
        if (tp + fn > 0) {
            result->recall[cls] = (double)tp / (tp + fn);
        } else {
            result->recall[cls] = 0.0;
        }

        // F1值
        if (result->precision[cls] + result->recall[cls] > 0) {
            result->f1_score[cls] = 2 * result->precision[cls] * result->recall[cls] /
                                   (result->precision[cls] + result->recall[cls]);
        } else {
            result->f1_score[cls] = 0.0;
        }
    }
}

// 打印评估结果
void print_eval_result(const EvalResult *result, const char *algorithm_name) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║            %s 评估结果                          \n", algorithm_name);
    printf("╚══════════════════════════════════════════════════════════════╝\n");

    printf("\n【整体准确率】%.2f%%\n", result->accuracy * 100);

    printf("\n【各类别指标】\n");
    printf("┌────────────────┬──────────┬──────────┬──────────┐\n");
    printf("│ 类别           │ 精确率   │ 召回率   │ F1值     │\n");
    printf("├────────────────┼──────────┼──────────┼──────────┤\n");

    for (int i = 0; i < NUM_CLASSES; i++) {
        printf("│ %-14s │ %6.2f%%  │ %6.2f%%  │ %6.4f   │\n",
               CLASS_NAMES_CN[i],
               result->precision[i] * 100,
               result->recall[i] * 100,
               result->f1_score[i]);
    }
    printf("└────────────────┴──────────┴──────────┴──────────┘\n");

    printf("\n【混淆矩阵】\n");
    printf("预测→     ");
    for (int i = 0; i < NUM_CLASSES; i++) {
        printf("%-10s", CLASS_NAMES_CN[i]);
    }
    printf("\n实际↓\n");

    for (int i = 0; i < NUM_CLASSES; i++) {
        printf("%-10s", CLASS_NAMES_CN[i]);
        for (int j = 0; j < NUM_CLASSES; j++) {
            printf("%-10d", result->confusion_matrix[i][j]);
        }
        printf("\n");
    }
    printf("\n");
}

// 导出评估结果到CSV文件
void export_eval_to_csv(const EvalResult *result, const char *algorithm_name, const char *filename) {
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        printf("警告: 无法创建文件 %s\n", filename);
        return;
    }

    fprintf(fp, "算法:,%s\n", algorithm_name);
    fprintf(fp, "准确率,%.4f\n", result->accuracy);
    fprintf(fp, "\n");

    fprintf(fp, "类别,精确率,召回率,F1值\n");
    for (int i = 0; i < NUM_CLASSES; i++) {
        fprintf(fp, "%s,%.4f,%.4f,%.4f\n",
                CLASS_NAMES_CN[i],
                result->precision[i],
                result->recall[i],
                result->f1_score[i]);
    }

    fprintf(fp, "\n混淆矩阵\n");
    fprintf(fp, ",");
    for (int i = 0; i < NUM_CLASSES; i++) {
        fprintf(fp, "%s,", CLASS_NAMES_CN[i]);
    }
    fprintf(fp, "\n");

    for (int i = 0; i < NUM_CLASSES; i++) {
        fprintf(fp, "%s,", CLASS_NAMES_CN[i]);
        for (int j = 0; j < NUM_CLASSES; j++) {
            fprintf(fp, "%d,", result->confusion_matrix[i][j]);
        }
        fprintf(fp, "\n");
    }

    fclose(fp);
    printf("评估结果已导出到: %s\n", filename);
}
