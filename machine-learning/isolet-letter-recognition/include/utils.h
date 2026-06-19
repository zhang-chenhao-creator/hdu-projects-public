#ifndef UTILS_H
#define UTILS_H

/* 工具函数头文件 */

#include <stdio.h>
#include <time.h>

/* 计时器结构体 */
typedef struct {
    clock_t start;
    double elapsed;
} Timer;

/* 计时器操作 */
void timer_start(Timer* timer);
void timer_stop(Timer* timer);
void timer_print(const Timer* timer, const char* label);

/* 内存分配检查 */
void* safe_malloc(size_t size, const char* label);
void* safe_calloc(size_t nmemb, size_t size, const char* label);

/* 数学工具函数 */
float sigmoid(float x);
float relu(float x);
float leaky_relu(float x, float alpha);
float softmax(float* logits, int n, int idx);
float cross_entropy_loss(float* probs, int label, int num_classes);

/* 随机数生成（Xorshift32，高质量且快速） */
typedef struct {
    unsigned int state;
} RNG;

void rng_init(RNG* rng, unsigned int seed);
unsigned int rng_next(RNG* rng);
float rng_uniform(RNG* rng);        /* [0, 1) */
float rng_normal(RNG* rng);         /* 标准正态分布 N(0,1) */
float rng_uniform_range(RNG* rng, float min, float max);

/* 矩阵操作 */
void matmul(const float* A, const float* B, float* C,
            int m, int n, int k);  /* C = A * B, A[m*k], B[k*n], C[m*n] */
void matvec(const float* A, const float* v, float* out,
            int rows, int cols);    /* out = A * v, A[rows*cols], v[cols] */
void transpose(const float* src, float* dst, int rows, int cols);

/* 打印进度条 */
void print_progress(int current, int total, int bar_width);

/* 打印混淆矩阵 */
void print_confusion_matrix(int* cm, int num_classes);

/* 保存和加载模型权重 */
int save_weights(const char* filename, float** weights, int* dims, int num_layers);
int load_weights(const char* filename, float** weights, int* dims, int num_layers);

#endif
