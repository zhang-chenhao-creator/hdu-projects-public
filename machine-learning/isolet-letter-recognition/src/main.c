/**
 * main.c - ISOLET英文字母识别系统主程序
 * 
 * 功能模块：
 *   1. 数据加载与预处理
 *   2. 深度神经网络训练（CUDA GPU加速）
 *   3. 模型评估与预测
 *   4. 数据统计与查询
 *   5. 混淆矩阵分析
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <cuda_runtime.h>
#include "isolet_data.h"
#include "neural_net.h"
#include "utils.h"

/* 模型文件路径 */
#define MODEL_FILE      "isolet_model.bin"
#define TRAIN_BIN_FILE  "data/train.bin"
#define TEST_BIN_FILE   "data/test.bin"

/* 全局数据集 */
Dataset g_train_set;
Dataset g_test_set;
float g_mean[NUM_FEATURES];
float g_std[NUM_FEATURES];
NeuralNet* g_net = NULL;

/* 函数声明 */
void show_menu(void);
void load_and_preprocess_data(void);
void train_model(void);
void evaluate_model(void);
void query_sample_info(void);
void classify_single_sample(void);
void show_statistics(void);
void save_results(void);
void interactive_predict(void);

/* CUDA内存分配辅助函数 */
static void* cuda_malloc_wrapper(size_t size)
{
    void* ptr;
    cudaError_t err = cudaMalloc(&ptr, size);
    if (err != cudaSuccess)
    {
        fprintf(stderr, "CUDA malloc failed: %s\n", cudaGetErrorString(err));
        exit(EXIT_FAILURE);
    }
    return ptr;
}

/* 主函数 */
int main(int argc, char* argv[])
{
    printf("\n");
    printf("=================================================\n");
    printf("  ISOLET English Letter Recognition System\n");
    printf("  Based on CUDA-Accelerated Deep Neural Network\n");
    printf("  Author: Shuuyou - HDU EI Class\n");
    printf("=================================================\n\n");

    /* 加载和预处理数据 */
    load_and_preprocess_data();

    /* 如果有命令行参数，支持快捷操作 */
    if (argc > 1)
    {
        if (strcmp(argv[1], "train") == 0)
        {
            train_model();
            evaluate_model();
            return 0;
        }
        else if (strcmp(argv[1], "eval") == 0)
        {
            evaluate_model();
            return 0;
        }
        else if (strcmp(argv[1], "save") == 0)
        {
            save_results();
            return 0;
        }
    }

    /* 交互式菜单 */
    int choice;
    do {
        show_menu();
        printf("Enter your choice: ");
        if (scanf("%d", &choice) != 1)
        {
            /* 清除错误输入 */
            int c;
            while ((c = getchar()) != '\n' && c != EOF);
            choice = 0;
        }

        switch (choice) {
            case 1:
                train_model();
                break;
            case 2:
                evaluate_model();
                break;
            case 3:
                query_sample_info();
                break;
            case 4:
                classify_single_sample();
                break;
            case 5:
                show_statistics();
                break;
            case 6:
                interactive_predict();
                break;
            case 7:
                save_results();
                break;
            case 0:
                printf("Exiting system...\n");
                break;
            default:
                printf("Invalid choice! Please try again.\n");
        }
        printf("\n");
    } while (choice != 0);

    /* 清理资源 */
    if (g_net)
    {
        net_destroy(g_net);
        g_net = NULL;
    }
    free_dataset(&g_train_set);
    free_dataset(&g_test_set);

    printf("System shutdown. Goodbye!\n\n");
    return 0;
}

/* 显示主菜单 */
void show_menu(void)
{
    printf("========== Main Menu ==========\n");
    printf("  1. Train Neural Network\n");
    printf("  2. Evaluate Model Accuracy\n");
    printf("  3. Query Sample Information\n");
    printf("  4. Classify Single Sample\n");
    printf("  5. Show Dataset Statistics\n");
    printf("  6. Interactive Prediction\n");
    printf("  7. Save Classification Results\n");
    printf("  0. Exit System\n");
    printf("===============================\n");
}

/* 加载和预处理数据 */
void load_and_preprocess_data(void)
{
    printf("\n[Data Loading] Loading ISOLET dataset...\n");
    Timer timer;
    timer_start(&timer);

    /* 尝试加载二进制缓存 */
    int use_binary = 0;
    FILE* fp = fopen(TRAIN_BIN_FILE, "rb");
    if (fp)
    {
        fclose(fp);
        fp = fopen(TEST_BIN_FILE, "rb");
        if (fp)
        {
            fclose(fp);
            use_binary = 1;
        }
    }

    if (use_binary)
    {
        printf("Loading from binary cache...\n");
        load_dataset_binary(TRAIN_BIN_FILE, &g_train_set);
        load_dataset_binary(TEST_BIN_FILE, &g_test_set);
    }
    else
    {
        /* 从文本加载 */
        if (load_dataset("data/isolet1+2+3+4.data", &g_train_set, TRAIN_SAMPLES) != 0)
        {
            fprintf(stderr, "Failed to load training data!\n");
            exit(EXIT_FAILURE);
        }
        if (load_dataset("data/isolet5.data", &g_test_set, TEST_SAMPLES) != 0)
        {
            fprintf(stderr, "Failed to load test data!\n");
            exit(EXIT_FAILURE);
        }

        /* 数据标准化（Z-score） */
        printf("Normalizing data (Z-score)...\n");
        normalize_dataset(&g_train_set, g_mean, g_std, 1);
        normalize_dataset(&g_test_set, g_mean, g_std, 0);

        /* 保存为二进制缓存 */
        save_dataset_binary(TRAIN_BIN_FILE, &g_train_set);
        save_dataset_binary(TEST_BIN_FILE, &g_test_set);
    }

    timer_stop(&timer);
    timer_print(&timer, "Data Loading");

    print_dataset_stats(&g_train_set);
    print_dataset_stats(&g_test_set);
}

/* 训练模型 */
void train_model(void)
{
    printf("\n[Training] Initializing neural network...\n");

    /* 创建网络 */
    NetConfig cfg;
    default_config(&cfg);

    if (g_net)
    {
        net_destroy(g_net);
    }
    g_net = net_create(&cfg);
    if (!g_net)
    {
        fprintf(stderr, "Failed to create neural network!\n");
        return;
    }

    net_print_architecture(g_net);

    /* 初始化权重 */
    unsigned int seed = (unsigned int)time(NULL);
    net_init_weights(g_net, seed);

    /* 分配批次内存 */
    alloc_batch_memory(g_net, cfg.batch_size);

    /* 准备训练数据数组 */
    float* train_features = (float*)malloc(g_train_set.num_samples * INPUT_DIM * sizeof(float));
    int* train_labels = (int*)malloc(g_train_set.num_samples * sizeof(int));
    for (int i = 0; i < g_train_set.num_samples; i++)
    {
        memcpy(&train_features[i * INPUT_DIM], g_train_set.samples[i].features, INPUT_DIM * sizeof(float));
        train_labels[i] = g_train_set.samples[i].label;
    }

    /* 分配GPU内存 */
    float* d_batch_features = copy_to_gpu(NULL, cfg.batch_size * INPUT_DIM);
    int* d_batch_labels = (int*)cuda_malloc_wrapper(cfg.batch_size * sizeof(int));
    int* d_predictions = (int*)cuda_malloc_wrapper(cfg.batch_size * sizeof(int));

    printf("\n[Training] Starting training...\n");
    printf("Epochs: %d, Batch size: %d\n", cfg.max_epochs, cfg.batch_size);
    printf("Initial LR: %.6f, Weight Decay: %.4f\n", cfg.learning_rate, cfg.weight_decay);
    printf("-------------------------------------------\n");

    Timer timer;
    timer_start(&timer);

    float best_acc = 0.0f;
    int patience_counter = 0;
    int* indices = (int*)malloc(g_train_set.num_samples * sizeof(int));

    for (int epoch = 0; epoch < cfg.max_epochs; epoch++)
    {
        /* 学习率调度 */
        update_learning_rate(g_net, epoch, cfg.max_epochs);

        /* 打乱数据 */
        for (int i = 0; i < g_train_set.num_samples; i++) indices[i] = i;
        unsigned int rng_seed = seed + epoch * 1000;
        for (int i = g_train_set.num_samples - 1; i > 0; i--)
        {
            rng_seed = rng_seed * 1103515245 + 12345;
            int j = rng_seed % (i + 1);
            int tmp = indices[i];
            indices[i] = indices[j];
            indices[j] = tmp;
        }

        /* 训练一个epoch */
        int num_batches = (g_train_set.num_samples + cfg.batch_size - 1) / cfg.batch_size;
        float epoch_loss = 0.0f;
        int num_loss_samples = 0;

        for (int b = 0; b < num_batches; b++)
        {
            int start = b * cfg.batch_size;
            int end = start + cfg.batch_size;
            if (end > g_train_set.num_samples) end = g_train_set.num_samples;
            int current_batch = end - start;

            /* 准备批次数据 */
            float* batch_data = (float*)malloc(current_batch * INPUT_DIM * sizeof(float));
            int* batch_lbl = (int*)malloc(current_batch * sizeof(int));

            for (int i = 0; i < current_batch; i++)
            {
                int idx = indices[start + i];
                memcpy(&batch_data[i * INPUT_DIM], &train_features[idx * INPUT_DIM],
                       INPUT_DIM * sizeof(float));
                batch_lbl[i] = train_labels[idx];

                /* 数据增强：添加高斯噪声 */
                unsigned int aug_seed = seed + epoch * 10000 + b * 100 + i;
                augment_gaussian_noise(&batch_data[i * INPUT_DIM], 0.03f, &aug_seed);
            }

            /* 拷贝到GPU */
            CUDA_CHECK(cudaMemcpy(d_batch_features, batch_data,
                                   current_batch * INPUT_DIM * sizeof(float),
                                   cudaMemcpyHostToDevice));
            CUDA_CHECK(cudaMemcpy(d_batch_labels, batch_lbl,
                                   current_batch * sizeof(int),
                                   cudaMemcpyHostToDevice));

            /* 前向传播 */
            net_forward(g_net, d_batch_features, current_batch, 1);

            /* 反向传播 */
            float loss = net_backward(g_net, d_batch_labels, current_batch);
            epoch_loss += loss * current_batch;
            num_loss_samples += current_batch;

            /* 参数更新 */
            net_update_weights(g_net, current_batch);

            free(batch_data);
            free(batch_lbl);
        }

        epoch_loss /= num_loss_samples;

        /* 每10个epoch评估一次 */
        if ((epoch + 1) % 10 == 0 || epoch == 0)
        {
            float train_acc = net_evaluate(g_net, train_features, train_labels,
                                           g_train_set.num_samples, cfg.batch_size);

            float* test_features = (float*)malloc(g_test_set.num_samples * INPUT_DIM * sizeof(float));
            int* test_labels = (int*)malloc(g_test_set.num_samples * sizeof(int));
            for (int i = 0; i < g_test_set.num_samples; i++)
            {
                memcpy(&test_features[i * INPUT_DIM], g_test_set.samples[i].features, INPUT_DIM * sizeof(float));
                test_labels[i] = g_test_set.samples[i].label;
            }
            float test_acc = net_evaluate(g_net, test_features, test_labels,
                                          g_test_set.num_samples, cfg.batch_size);
            free(test_features);
            free(test_labels);

            printf("Epoch %3d/%d | Loss: %.4f | Train Acc: %.2f%% | Test Acc: %.2f%% | LR: %.2e\n",
                   epoch + 1, cfg.max_epochs, epoch_loss,
                   train_acc * 100.0f, test_acc * 100.0f, g_net->current_lr);

            /* 早停 */
            if (test_acc > best_acc)
            {
                best_acc = test_acc;
                patience_counter = 0;
                /* 保存最佳模型 */
                net_save(g_net, MODEL_FILE);
            }
            else
            {
                patience_counter++;
            }

            if (patience_counter >= cfg.early_stop_patience)
            {
                printf("Early stopping triggered! Best accuracy: %.2f%%\n", best_acc * 100.0f);
                break;
            }
        }
        else
        {
            printf("Epoch %3d/%d | Loss: %.4f | LR: %.2e\n",
                   epoch + 1, cfg.max_epochs, epoch_loss, g_net->current_lr);
        }
    }

    timer_stop(&timer);
    timer_print(&timer, "Training");

    free(indices);
    free(train_features);
    free(train_labels);
    free_gpu(d_batch_features);
    cudaFree(d_batch_labels);
    cudaFree(d_predictions);

    printf("\n[Training] Best model saved to %s\n", MODEL_FILE);
}

/* 评估模型 */
void evaluate_model(void)
{
    if (!g_net)
    {
        printf("No trained model found. Loading from %s...\n", MODEL_FILE);
        NetConfig cfg;
        default_config(&cfg);
        g_net = net_create(&cfg);
        if (net_load(g_net, MODEL_FILE) != 0)
        {
            printf("No saved model found. Please train first.\n");
            return;
        }
        alloc_batch_memory(g_net, cfg.batch_size);
    }

    printf("\n[Evaluation] Evaluating model on test set...\n");
    Timer timer;
    timer_start(&timer);

    /* 准备测试数据 */
    float* test_features = (float*)malloc(g_test_set.num_samples * INPUT_DIM * sizeof(float));
    int* test_labels = (int*)malloc(g_test_set.num_samples * sizeof(int));
    for (int i = 0; i < g_test_set.num_samples; i++)
    {
        memcpy(&test_features[i * INPUT_DIM], g_test_set.samples[i].features, INPUT_DIM * sizeof(float));
        test_labels[i] = g_test_set.samples[i].label;
    }

    float accuracy = net_evaluate(g_net, test_features, test_labels,
                                  g_test_set.num_samples, g_net->config.batch_size);

    /* 计算混淆矩阵和每类准确率 */
    int* cm = (int*)calloc(NUM_CLASSES * NUM_CLASSES, sizeof(int));
    int* class_correct = (int*)calloc(NUM_CLASSES, sizeof(int));
    int* class_total = (int*)calloc(NUM_CLASSES, sizeof(int));

    float* d_batch_features = copy_to_gpu(NULL, g_net->config.batch_size * INPUT_DIM);
    int* d_predictions = (int*)cuda_malloc_wrapper(g_net->config.batch_size * sizeof(int));
    int* h_preds = (int*)malloc(g_net->config.batch_size * sizeof(int));

    int num_batches = (g_test_set.num_samples + g_net->config.batch_size - 1) / g_net->config.batch_size;

    for (int b = 0; b < num_batches; b++)
    {
        int start = b * g_net->config.batch_size;
        int end = start + g_net->config.batch_size;
        if (end > g_test_set.num_samples) end = g_test_set.num_samples;
        int current_batch = end - start;

        CUDA_CHECK(cudaMemcpy(d_batch_features, &test_features[start * INPUT_DIM],
                               current_batch * INPUT_DIM * sizeof(float),
                               cudaMemcpyHostToDevice));

        net_predict(g_net, d_batch_features, d_predictions, NULL, current_batch);

        CUDA_CHECK(cudaMemcpy(h_preds, d_predictions,
                               current_batch * sizeof(int),
                               cudaMemcpyDeviceToHost));

        for (int i = 0; i < current_batch; i++)
        {
            int true_label = test_labels[start + i];
            int pred_label = h_preds[i];
            cm[(true_label - 1) * NUM_CLASSES + (pred_label - 1)]++;
            class_total[true_label - 1]++;
            if (true_label == pred_label)
            {
                class_correct[true_label - 1]++;
            }
        }
    }

    free(h_preds);
    free_gpu(d_batch_features);
    cudaFree(d_predictions);

    timer_stop(&timer);

    printf("\n========== Evaluation Results ==========\n");
    printf("Test Accuracy: %.2f%% (%d/%d)\n",
           accuracy * 100.0f,
           (int)(accuracy * g_test_set.num_samples),
           g_test_set.num_samples);
    timer_print(&timer, "Evaluation");
    printf("========================================\n\n");

    /* 每类准确率 */
    printf("Per-class Accuracy:\n");
    printf("-------------------\n");
    for (int c = 0; c < NUM_CLASSES; c++)
    {
        float class_acc = (class_total[c] > 0) ?
            (float)class_correct[c] / class_total[c] : 0.0f;
        printf("  Class %c: %.1f%% (%d/%d)\n",
               'A' + c, class_acc * 100.0f, class_correct[c], class_total[c]);
    }

    /* 混淆矩阵 */
    print_confusion_matrix(cm, NUM_CLASSES);

    free(cm);
    free(class_correct);
    free(class_total);
    free(test_features);
    free(test_labels);
}

/* 查询样本信息 */
void query_sample_info(void)
{
    int dataset_choice;
    printf("\nSelect dataset:\n");
    printf("  1. Training set (%d samples)\n", g_train_set.num_samples);
    printf("  2. Test set (%d samples)\n", g_test_set.num_samples);
    printf("Choice: ");
    scanf("%d", &dataset_choice);

    Dataset* ds = (dataset_choice == 2) ? &g_test_set : &g_train_set;

    int index;
    printf("Enter sample index (0-%d): ", ds->num_samples - 1);
    scanf("%d", &index);

    const Sample* s = query_sample(ds, index);
    if (!s)
    {
        printf("Invalid index!\n");
        return;
    }

    printf("\n========== Sample Information ==========\n");
    printf("Index: %d\n", index);
    printf("Label: %c (class %d)\n", 'A' + s->label - 1, s->label);
    printf("\nFeatures (first 20):\n");
    for (int i = 0; i < 20 && i < NUM_FEATURES; i++)
    {
        printf("  Feature %3d: %.6f\n", i + 1, s->features[i]);
    }
    printf("  ... (%d more features)\n", NUM_FEATURES - 20);

    /* 特征统计 */
    float min_val = s->features[0];
    float max_val = s->features[0];
    float avg = 0.0f;
    for (int i = 0; i < NUM_FEATURES; i++)
    {
        if (s->features[i] < min_val) min_val = s->features[i];
        if (s->features[i] > max_val) max_val = s->features[i];
        avg += s->features[i];
    }
    avg /= NUM_FEATURES;

    printf("\nFeature Statistics:\n");
    printf("  Min: %.6f\n", min_val);
    printf("  Max: %.6f\n", max_val);
    printf("  Mean: %.6f\n", avg);
    printf("========================================\n\n");
}

/* 对单个样本分类 */
void classify_single_sample(void)
{
    if (!g_net)
    {
        printf("No trained model. Please train first or load a model.\n");
        return;
    }

    int dataset_choice;
    printf("\nSelect dataset:\n");
    printf("  1. Training set\n");
    printf("  2. Test set\n");
    printf("Choice: ");
    scanf("%d", &dataset_choice);

    Dataset* ds = (dataset_choice == 2) ? &g_test_set : &g_train_set;

    int index;
    printf("Enter sample index (0-%d): ", ds->num_samples - 1);
    scanf("%d", &index);

    if (index < 0 || index >= ds->num_samples)
    {
        printf("Invalid index!\n");
        return;
    }

    /* 使用模型预测 */
    float* d_features = copy_to_gpu(ds->samples[index].features, INPUT_DIM);
    int* d_pred = (int*)cuda_malloc_wrapper(sizeof(int));
    float* d_prob = (float*)cuda_malloc_wrapper(sizeof(float));
    int h_pred;
    float h_prob;

    net_predict(g_net, d_features, d_pred, d_prob, 1);

    CUDA_CHECK(cudaMemcpy(&h_pred, d_pred, sizeof(int), cudaMemcpyDeviceToHost));
    CUDA_CHECK(cudaMemcpy(&h_prob, d_prob, sizeof(float), cudaMemcpyDeviceToHost));

    int true_label = ds->samples[index].label;

    printf("\n========== Classification Result ==========\n");
    printf("Sample Index: %d\n", index);
    printf("True Label:   %c (class %d)\n", 'A' + true_label - 1, true_label);
    printf("Predicted:    %c (class %d)\n", 'A' + h_pred - 1, h_pred);
    printf("Confidence:   %.4f%%\n", h_prob * 100.0f);
    printf("Result:       %s\n", (h_pred == true_label) ? "CORRECT" : "WRONG");
    printf("===========================================\n\n");

    free_gpu(d_features);
    cudaFree(d_pred);
    cudaFree(d_prob);
}

/* 显示数据集统计 */
void show_statistics(void)
{
    printf("\n========== Comprehensive Statistics ==========\n");

    /* 训练集统计 */
    printf("\n--- Training Set ---\n");
    print_dataset_stats(&g_train_set);

    /* 测试集统计 */
    printf("\n--- Test Set ---\n");
    print_dataset_stats(&g_test_set);

    /* 特征统计 */
    printf("\n--- Feature Statistics ---\n");
    printf("Feature dimension: %d\n", NUM_FEATURES);
    printf("Features normalized: Yes (Z-score)\n");

    /* 计算整体特征均值和标准差 */
    float overall_mean = 0.0f;
    for (int i = 0; i < NUM_FEATURES; i++)
    {
        overall_mean += g_mean[i];
    }
    overall_mean /= NUM_FEATURES;

    float overall_std = 0.0f;
    for (int i = 0; i < NUM_FEATURES; i++)
    {
        overall_std += g_std[i];
    }
    overall_std /= NUM_FEATURES;

    printf("Original mean (avg across features): %.4f\n", overall_mean);
    printf("Original std  (avg across features): %.4f\n", overall_std);
    printf("==============================================\n\n");
}

/* 保存分类结果 */
void save_results(void)
{
    if (!g_net)
    {
        printf("No trained model found. Loading from %s...\n", MODEL_FILE);
        NetConfig cfg;
        default_config(&cfg);
        g_net = net_create(&cfg);
        if (net_load(g_net, MODEL_FILE) != 0)
        {
            printf("No saved model found. Please train first.\n");
            return;
        }
        alloc_batch_memory(g_net, cfg.batch_size);
    }

    printf("\n[Saving] Classifying test set and saving results...\n");

    /* 准备测试数据 */
    float* test_features = (float*)malloc(g_test_set.num_samples * INPUT_DIM * sizeof(float));
    int* test_labels = (int*)malloc(g_test_set.num_samples * sizeof(int));
    for (int i = 0; i < g_test_set.num_samples; i++)
    {
        memcpy(&test_features[i * INPUT_DIM], g_test_set.samples[i].features, INPUT_DIM * sizeof(float));
        test_labels[i] = g_test_set.samples[i].label;
    }

    /* 批量预测 */
    int* predictions = (int*)malloc(g_test_set.num_samples * sizeof(int));
    float* confidences = (float*)malloc(g_test_set.num_samples * sizeof(float));

    float* d_batch_features = copy_to_gpu(NULL, g_net->config.batch_size * INPUT_DIM);
    int* d_predictions = (int*)cuda_malloc_wrapper(g_net->config.batch_size * sizeof(int));
    float* d_probs = (float*)cuda_malloc_wrapper(g_net->config.batch_size * sizeof(float));
    int* h_preds = (int*)malloc(g_net->config.batch_size * sizeof(int));
    float* h_probs = (float*)malloc(g_net->config.batch_size * sizeof(float));

    int num_batches = (g_test_set.num_samples + g_net->config.batch_size - 1) / g_net->config.batch_size;

    for (int b = 0; b < num_batches; b++)
    {
        int start = b * g_net->config.batch_size;
        int end = start + g_net->config.batch_size;
        if (end > g_test_set.num_samples) end = g_test_set.num_samples;
        int current_batch = end - start;

        CUDA_CHECK(cudaMemcpy(d_batch_features, &test_features[start * INPUT_DIM],
                               current_batch * INPUT_DIM * sizeof(float),
                               cudaMemcpyHostToDevice));

        net_predict(g_net, d_batch_features, d_predictions, d_probs, current_batch);

        CUDA_CHECK(cudaMemcpy(h_preds, d_predictions,
                               current_batch * sizeof(int), cudaMemcpyDeviceToHost));
        CUDA_CHECK(cudaMemcpy(h_probs, d_probs,
                               current_batch * sizeof(float), cudaMemcpyDeviceToHost));

        for (int i = 0; i < current_batch; i++)
        {
            predictions[start + i] = h_preds[i];
            confidences[start + i] = h_probs[i];
        }
    }

    free(h_preds);
    free(h_probs);
    free_gpu(d_batch_features);
    cudaFree(d_predictions);
    cudaFree(d_probs);

    /* 保存结果到文件 */
    FILE* fp = fopen("classification_results.txt", "w");
    if (!fp)
    {
        fprintf(stderr, "Failed to create results file!\n");
        return;
    }

    fprintf(fp, "ISOLET Classification Results\n");
    fprintf(fp, "==============================\n\n");
    fprintf(fp, "%-8s %-12s %-12s %-12s %-15s\n",
            "Index", "True", "Predicted", "Correct", "Confidence");
    fprintf(fp, "---------------------------------------------------------\n");

    int correct = 0;
    for (int i = 0; i < g_test_set.num_samples; i++)
    {
        int is_correct = (predictions[i] == test_labels[i]) ? 1 : 0;
        if (is_correct) correct++;

        fprintf(fp, "%-8d %-12c %-12c %-12s %-15.4f%%\n",
                i,
                'A' + test_labels[i] - 1,
                'A' + predictions[i] - 1,
                is_correct ? "Yes" : "No",
                confidences[i] * 100.0f);
    }

    fprintf(fp, "\n==============================\n");
    fprintf(fp, "Total: %d, Correct: %d, Accuracy: %.2f%%\n",
            g_test_set.num_samples, correct,
            (float)correct / g_test_set.num_samples * 100.0f);
    fclose(fp);

    /* 保存混淆矩阵 */
    FILE* cm_fp = fopen("confusion_matrix.txt", "w");
    if (cm_fp)
    {
        int* cm = (int*)calloc(NUM_CLASSES * NUM_CLASSES, sizeof(int));
        for (int i = 0; i < g_test_set.num_samples; i++)
        {
            cm[(test_labels[i] - 1) * NUM_CLASSES + (predictions[i] - 1)]++;
        }

        fprintf(cm_fp, "Confusion Matrix (rows=true, cols=pred):\n");
        fprintf(cm_fp, "     ");
        for (int i = 0; i < NUM_CLASSES; i++)
        {
            fprintf(cm_fp, " %2c", 'A' + i);
        }
        fprintf(cm_fp, "\n");

        for (int i = 0; i < NUM_CLASSES; i++)
        {
            fprintf(cm_fp, " %2c |", 'A' + i);
            for (int j = 0; j < NUM_CLASSES; j++)
            {
                fprintf(cm_fp, " %2d", cm[i * NUM_CLASSES + j]);
            }
            fprintf(cm_fp, "\n");
        }

        free(cm);
        fclose(cm_fp);
    }

    printf("Results saved to:\n");
    printf("  - classification_results.txt\n");
    printf("  - confusion_matrix.txt\n");

    free(predictions);
    free(confidences);
    free(test_features);
    free(test_labels);
}

/* 交互式预测：输入特征值进行分类 */
void interactive_predict(void)
{
    if (!g_net)
    {
        printf("No trained model. Please train first.\n");
        return;
    }

    printf("\n[Interactive Prediction]\n");
    printf("Note: Input 617 features (space or comma separated)\n");
    printf("Or enter 'sample N' to use test sample N\n");
    printf("Enter 'quit' to exit\n\n");

    /* 消耗掉之前的换行符 */
    int c;
    while ((c = getchar()) != '\n' && c != EOF);

    char input[16384];
    while (1)
    {
        printf("> ");
        fflush(stdout);
        if (!fgets(input, sizeof(input), stdin)) break;

        /* 去除换行符 */
        input[strcspn(input, "\n")] = 0;

        if (strcmp(input, "quit") == 0 || strcmp(input, "q") == 0)
        {
            break;
        }

        float features[NUM_FEATURES];
        int idx = -1;

        /* 检查是否是sample命令 */
        if (strncmp(input, "sample ", 7) == 0)
        {
            idx = atoi(input + 7);
            if (idx >= 0 && idx < g_test_set.num_samples)
            {
                memcpy(features, g_test_set.samples[idx].features, NUM_FEATURES * sizeof(float));
            }
            else
            {
                printf("Invalid sample index!\n");
                continue;
            }
        }
        else
        {
            /* 解析617个浮点数 */
            char* ptr = input;
            int count = 0;
            while (*ptr && count < NUM_FEATURES)
            {
                while (*ptr == ' ' || *ptr == ',') ptr++;
                if (*ptr == 0) break;
                features[count++] = strtof(ptr, &ptr);
            }
            if (count != NUM_FEATURES)
            {
                printf("Error: Expected %d features, got %d\n", NUM_FEATURES, count);
                continue;
            }
        }

        /* 预测 */
        float* d_features = copy_to_gpu(features, NUM_FEATURES);
        int* d_pred = (int*)cuda_malloc_wrapper(sizeof(int));
        float* d_prob = (float*)cuda_malloc_wrapper(sizeof(float));
        int h_pred;
        float h_prob;

        net_predict(g_net, d_features, d_pred, d_prob, 1);

        CUDA_CHECK(cudaMemcpy(&h_pred, d_pred, sizeof(int), cudaMemcpyDeviceToHost));
        CUDA_CHECK(cudaMemcpy(&h_prob, d_prob, sizeof(float), cudaMemcpyDeviceToHost));

        printf("Predicted: %c (class %d), Confidence: %.2f%%\n",
               'A' + h_pred - 1, h_pred, h_prob * 100.0f);

        free_gpu(d_features);
        cudaFree(d_pred);
        cudaFree(d_prob);
    }
}
