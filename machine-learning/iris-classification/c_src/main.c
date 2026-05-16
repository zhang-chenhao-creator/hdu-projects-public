#include "common.h"
#include "data_loader.h"
#include "knn.h"
#include "decision_tree.h"
#include "svm.h"
#include "evaluator.h"

// 全局数据
static Dataset dataset;
static TrainTestSplit split;
static int data_loaded = 0;
static int data_split = 0;

// 清屏函数
void clear_screen() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

// 打印标题
void print_title() {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║                    鸢尾花分类系统                            ║\n");
    printf("║                    Iris Classification System                ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    printf("\n");
}

// 打印主菜单
void print_menu() {
    printf("┌──────────────────────────────────────────────────────────────┐\n");
    printf("│                         主菜单                               │\n");
    printf("├──────────────────────────────────────────────────────────────┤\n");
    printf("│  1. 加载数据                                                │\n");
    printf("│  2. 划分训练集/测试集                                        │\n");
    printf("│  3. KNN分类                                                 │\n");
    printf("│  4. 决策树分类                                               │\n");
    printf("│  5. SVM分类                                                  │\n");
    printf("│  6. 算法对比                                                 │\n");
    printf("│  7. 数据可视化                                               │\n");
    printf("│  0. 退出程序                                                 │\n");
    printf("└──────────────────────────────────────────────────────────────┘\n");
    printf("\n请选择功能 [0-7]: ");
}

// 加载数据
void load_data() {
    char filename[256];
    printf("\n请输入数据文件路径 (直接回车使用默认文件 irisdata.txt): ");
    fgets(filename, sizeof(filename), stdin);
    filename[strcspn(filename, "\r\n")] = 0;

    if (strlen(filename) == 0) {
        strcpy(filename, "../irisdata.txt");
    }

    if (load_dataset(filename, &dataset) == 0) {
        print_dataset_info(&dataset);
        data_loaded = 1;
        data_split = 0;
    }
}

// 划分数据
void split_data() {
    if (!data_loaded) {
        printf("\n错误: 请先加载数据！\n");
        return;
    }

    double ratio;
    printf("\n请输入测试集比例 (0.1-0.5, 默认0.3): ");
    char input[32];
    fgets(input, sizeof(input), stdin);

    if (strlen(input) <= 1) {
        ratio = 0.3;
    } else {
        ratio = atof(input);
        if (ratio < 0.1 || ratio > 0.5) {
            printf("比例超出范围，使用默认值0.3\n");
            ratio = 0.3;
        }
    }

    split_dataset(&dataset, &split, ratio);
    standardize(&split.train, &split.test);
    data_split = 1;

    printf("\n训练集信息:\n");
    print_dataset_info(&split.train);
    printf("测试集信息:\n");
    print_dataset_info(&split.test);
}

// KNN分类
void run_knn() {
    if (!data_split) {
        printf("\n错误: 请先划分数据集！\n");
        return;
    }

    int k;
    printf("\n请输入K值 (1-20, 默认5): ");
    char input[32];
    fgets(input, sizeof(input), stdin);

    if (strlen(input) <= 1) {
        k = 5;
    } else {
        k = atoi(input);
        if (k < 1 || k > 20) {
            printf("K值超出范围，使用默认值5\n");
            k = 5;
        }
    }

    printf("\n正在进行KNN分类 (K=%d)...\n", k);

    int predictions[MAX_SAMPLES];
    for (int i = 0; i < split.test.n_samples; i++) {
        predictions[i] = knn_predict(&split.train, split.test.samples[i].features, k);
    }

    EvalResult result;
    evaluate(&split.test, predictions, &result);

    char algo_name[64];
    sprintf(algo_name, "KNN (K=%d)", k);
    print_eval_result(&result, algo_name);

    // 导出结果
    char csv_name[128];
    sprintf(csv_name, "result_knn_k%d.csv", k);
    export_eval_to_csv(&result, algo_name, csv_name);
}

// 决策树分类
void run_decision_tree() {
    if (!data_split) {
        printf("\n错误: 请先划分数据集！\n");
        return;
    }

    int max_depth;
    printf("\n请输入最大深度 (1-20, 默认5, 0表示不限制): ");
    char input[32];
    fgets(input, sizeof(input), stdin);

    if (strlen(input) <= 1) {
        max_depth = 5;
    } else {
        max_depth = atoi(input);
        if (max_depth < 0 || max_depth > 20) {
            printf("深度超出范围，使用默认值5\n");
            max_depth = 5;
        }
    }

    printf("\n正在训练决策树 (最大深度=%d)...\n", max_depth);

    void *tree = dt_train(&split.train, max_depth);

    int predictions[MAX_SAMPLES];
    for (int i = 0; i < split.test.n_samples; i++) {
        predictions[i] = dt_predict(tree, split.test.samples[i].features);
    }

    EvalResult result;
    evaluate(&split.test, predictions, &result);

    char algo_name[64];
    if (max_depth == 0) {
        sprintf(algo_name, "决策树 (深度=不限)");
    } else {
        sprintf(algo_name, "决策树 (深度=%d)", max_depth);
    }
    print_eval_result(&result, algo_name);

    // 导出结果
    char csv_name[128];
    sprintf(csv_name, "result_dt_depth%d.csv", max_depth);
    export_eval_to_csv(&result, algo_name, csv_name);

    dt_free(tree);
}

// SVM分类
void run_svm() {
    if (!data_split) {
        printf("\n错误: 请先划分数据集！\n");
        return;
    }

    double C;
    printf("\n请输入正则化参数C (0.01-100, 默认1.0): ");
    char input[32];
    fgets(input, sizeof(input), stdin);

    if (strlen(input) <= 1) {
        C = 1.0;
    } else {
        C = atof(input);
        if (C < 0.01 || C > 100) {
            printf("C值超出范围，使用默认值1.0\n");
            C = 1.0;
        }
    }

    int max_iter;
    printf("请输入最大迭代次数 (100-5000, 默认1000): ");
    fgets(input, sizeof(input), stdin);

    if (strlen(input) <= 1) {
        max_iter = 1000;
    } else {
        max_iter = atoi(input);
        if (max_iter < 100 || max_iter > 5000) {
            printf("迭代次数超出范围，使用默认值1000\n");
            max_iter = 1000;
        }
    }

    printf("\n正在训练SVM (C=%.2f, 迭代次数=%d)...\n", C, max_iter);

    void *model = svm_train(&split.train, C, max_iter);

    int predictions[MAX_SAMPLES];
    for (int i = 0; i < split.test.n_samples; i++) {
        predictions[i] = svm_predict(model, split.test.samples[i].features);
    }

    EvalResult result;
    evaluate(&split.test, predictions, &result);

    char algo_name[64];
    sprintf(algo_name, "SVM (C=%.2f)", C);
    print_eval_result(&result, algo_name);

    // 导出结果
    char csv_name[128];
    sprintf(csv_name, "result_svm_C%.2f.csv", C);
    export_eval_to_csv(&result, algo_name, csv_name);

    svm_free(model);
}

// 算法对比
void compare_algorithms() {
    if (!data_split) {
        printf("\n错误: 请先划分数据集！\n");
        return;
    }

    printf("\n正在运行所有算法进行对比...\n\n");

    // 存储各算法结果
    EvalResult knn_result, dt_result, svm_result;
    char knn_name[64] = "KNN (K=5)";
    char dt_name[64] = "决策树 (深度=5)";
    char svm_name[64] = "SVM (C=1.0)";

    // KNN
    printf("1/3 训练KNN...\n");
    int knn_pred[MAX_SAMPLES];
    for (int i = 0; i < split.test.n_samples; i++) {
        knn_pred[i] = knn_predict(&split.train, split.test.samples[i].features, 5);
    }
    evaluate(&split.test, knn_pred, &knn_result);

    // 决策树
    printf("2/3 训练决策树...\n");
    void *tree = dt_train(&split.train, 5);
    int dt_pred[MAX_SAMPLES];
    for (int i = 0; i < split.test.n_samples; i++) {
        dt_pred[i] = dt_predict(tree, split.test.samples[i].features);
    }
    evaluate(&split.test, dt_pred, &dt_result);
    dt_free(tree);

    // SVM
    printf("3/3 训练SVM...\n");
    void *model = svm_train(&split.train, 1.0, 1000);
    int svm_pred[MAX_SAMPLES];
    for (int i = 0; i < split.test.n_samples; i++) {
        svm_pred[i] = svm_predict(model, split.test.samples[i].features);
    }
    evaluate(&split.test, svm_pred, &svm_result);
    svm_free(model);

    // 打印对比结果
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║                      算法对比结果                            ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");

    printf("\n┌────────────────┬──────────┬──────────┬──────────┬──────────┐\n");
    printf("│ 算法           │ 准确率   │ 精确率   │ 召回率   │ F1值     │\n");
    printf("│                │          │ (宏平均) │ (宏平均) │ (宏平均) │\n");
    printf("├────────────────┼──────────┼──────────┼──────────┼──────────┤\n");

    // 计算宏平均
    double avg_p, avg_r, avg_f;

    avg_p = avg_r = avg_f = 0;
    for (int i = 0; i < NUM_CLASSES; i++) {
        avg_p += knn_result.precision[i];
        avg_r += knn_result.recall[i];
        avg_f += knn_result.f1_score[i];
    }
    printf("│ %-14s │ %6.2f%%  │ %6.2f%%  │ %6.2f%%  │ %6.4f   │\n",
           knn_name, knn_result.accuracy * 100,
           avg_p / NUM_CLASSES * 100, avg_r / NUM_CLASSES * 100, avg_f / NUM_CLASSES);

    avg_p = avg_r = avg_f = 0;
    for (int i = 0; i < NUM_CLASSES; i++) {
        avg_p += dt_result.precision[i];
        avg_r += dt_result.recall[i];
        avg_f += dt_result.f1_score[i];
    }
    printf("│ %-14s │ %6.2f%%  │ %6.2f%%  │ %6.2f%%  │ %6.4f   │\n",
           dt_name, dt_result.accuracy * 100,
           avg_p / NUM_CLASSES * 100, avg_r / NUM_CLASSES * 100, avg_f / NUM_CLASSES);

    avg_p = avg_r = avg_f = 0;
    for (int i = 0; i < NUM_CLASSES; i++) {
        avg_p += svm_result.precision[i];
        avg_r += svm_result.recall[i];
        avg_f += svm_result.f1_score[i];
    }
    printf("│ %-14s │ %6.2f%%  │ %6.2f%%  │ %6.2f%%  │ %6.4f   │\n",
           svm_name, svm_result.accuracy * 100,
           avg_p / NUM_CLASSES * 100, avg_r / NUM_CLASSES * 100, avg_f / NUM_CLASSES);

    printf("└────────────────┴──────────┴──────────┴──────────┴──────────┘\n");

    // 找出最佳算法
    double best_acc = knn_result.accuracy;
    const char *best_algo = knn_name;

    if (dt_result.accuracy > best_acc) {
        best_acc = dt_result.accuracy;
        best_algo = dt_name;
    }
    if (svm_result.accuracy > best_acc) {
        best_acc = svm_result.accuracy;
        best_algo = svm_name;
    }

    printf("\n最佳算法: %s (准确率: %.2f%%)\n", best_algo, best_acc * 100);

    // 导出对比结果到CSV
    FILE *fp = fopen("algorithm_comparison.csv", "w");
    if (fp) {
        fprintf(fp, "算法,准确率,精确率(宏平均),召回率(宏平均),F1值(宏平均)\n");

        avg_p = avg_r = avg_f = 0;
        for (int i = 0; i < NUM_CLASSES; i++) {
            avg_p += knn_result.precision[i];
            avg_r += knn_result.recall[i];
            avg_f += knn_result.f1_score[i];
        }
        fprintf(fp, "%s,%.4f,%.4f,%.4f,%.4f\n", knn_name,
                knn_result.accuracy, avg_p / NUM_CLASSES, avg_r / NUM_CLASSES, avg_f / NUM_CLASSES);

        avg_p = avg_r = avg_f = 0;
        for (int i = 0; i < NUM_CLASSES; i++) {
            avg_p += dt_result.precision[i];
            avg_r += dt_result.recall[i];
            avg_f += dt_result.f1_score[i];
        }
        fprintf(fp, "%s,%.4f,%.4f,%.4f,%.4f\n", dt_name,
                dt_result.accuracy, avg_p / NUM_CLASSES, avg_r / NUM_CLASSES, avg_f / NUM_CLASSES);

        avg_p = avg_r = avg_f = 0;
        for (int i = 0; i < NUM_CLASSES; i++) {
            avg_p += svm_result.precision[i];
            avg_r += svm_result.recall[i];
            avg_f += svm_result.f1_score[i];
        }
        fprintf(fp, "%s,%.4f,%.4f,%.4f,%.4f\n", svm_name,
                svm_result.accuracy, avg_p / NUM_CLASSES, avg_r / NUM_CLASSES, avg_f / NUM_CLASSES);

        fclose(fp);
        printf("\n对比结果已导出到: algorithm_comparison.csv\n");
    }
}

// 数据可视化（文本形式）
void visualize_data() {
    if (!data_loaded) {
        printf("\n错误: 请先加载数据！\n");
        return;
    }

    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║                      数据统计信息                            ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");

    // 统计各类别数量
    int class_count[NUM_CLASSES] = {0};
    for (int i = 0; i < dataset.n_samples; i++) {
        class_count[dataset.samples[i].label]++;
    }

    printf("\n【样本分布】\n");
    for (int i = 0; i < NUM_CLASSES; i++) {
        printf("%-14s: %3d 条 (", CLASS_NAMES_CN[i], class_count[i]);
        int bar_len = class_count[i] / 2;
        for (int j = 0; j < bar_len; j++) printf("█");
        printf(")\n");
    }

    // 统计各特征的均值和标准差
    printf("\n【特征统计】\n");
    printf("┌────────────────┬──────────┬──────────┬──────────┬──────────┐\n");
    printf("│ 特征           │ 均值     │ 标准差   │ 最小值   │ 最大值   │\n");
    printf("├────────────────┼──────────┼──────────┼──────────┼──────────┤\n");

    for (int feat = 0; feat < NUM_FEATURES; feat++) {
        double sum = 0, sum_sq = 0;
        double min_val = dataset.samples[0].features[feat];
        double max_val = dataset.samples[0].features[feat];

        for (int i = 0; i < dataset.n_samples; i++) {
            double val = dataset.samples[i].features[feat];
            sum += val;
            sum_sq += val * val;
            if (val < min_val) min_val = val;
            if (val > max_val) max_val = val;
        }

        double mean = sum / dataset.n_samples;
        double std = sqrt(sum_sq / dataset.n_samples - mean * mean);

        printf("│ %-14s │ %8.2f │ %8.2f │ %8.2f │ %8.2f │\n",
               FEATURE_NAMES[feat], mean, std, min_val, max_val);
    }
    printf("└────────────────┴──────────┴──────────┴──────────┴──────────┘\n");

    // 各类别特征均值
    printf("\n【各类别特征均值】\n");
    printf("┌────────────────┬──────────┬──────────┬──────────┬──────────┐\n");
    printf("│ 类别           │ 花萼长度 │ 花萼宽度 │ 花瓣长度 │ 花瓣宽度 │\n");
    printf("├────────────────┼──────────┼──────────┼──────────┼──────────┤\n");

    for (int cls = 0; cls < NUM_CLASSES; cls++) {
        double means[NUM_FEATURES] = {0};
        int count = 0;

        for (int i = 0; i < dataset.n_samples; i++) {
            if (dataset.samples[i].label == cls) {
                for (int f = 0; f < NUM_FEATURES; f++) {
                    means[f] += dataset.samples[i].features[f];
                }
                count++;
            }
        }

        for (int f = 0; f < NUM_FEATURES; f++) {
            means[f] /= count;
        }

        printf("│ %-14s │ %8.2f │ %8.2f │ %8.2f │ %8.2f │\n",
               CLASS_NAMES_CN[cls], means[0], means[1], means[2], means[3]);
    }
    printf("└────────────────┴──────────┴──────────┴──────────┴──────────┘\n");
}

int main() {
    int choice;
    char input[32];

    while (1) {
        clear_screen();
        print_title();
        print_menu();

        fgets(input, sizeof(input), stdin);
        choice = atoi(input);

        switch (choice) {
            case 1:
                load_data();
                break;
            case 2:
                split_data();
                break;
            case 3:
                run_knn();
                break;
            case 4:
                run_decision_tree();
                break;
            case 5:
                run_svm();
                break;
            case 6:
                compare_algorithms();
                break;
            case 7:
                visualize_data();
                break;
            case 0:
                printf("\n感谢使用，再见！\n");
                return 0;
            default:
                printf("\n无效选择，请重新输入！\n");
        }

        printf("\n按回车键继续...");
        fgets(input, sizeof(input), stdin);
    }

    return 0;
}
