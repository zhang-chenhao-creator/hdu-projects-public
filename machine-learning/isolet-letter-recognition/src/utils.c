#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "utils.h"

/* 计时器操作 */
void timer_start(Timer* timer)
{
    timer->start = clock();
}

void timer_stop(Timer* timer)
{
    timer->elapsed = (double)(clock() - timer->start) / CLOCKS_PER_SEC;
}

void timer_print(const Timer* timer, const char* label)
{
    printf("[%s] Elapsed time: %.3f seconds\n", label, timer->elapsed);
}

/* 安全内存分配 */
void* safe_malloc(size_t size, const char* label)
{
    void* ptr = malloc(size);
    if (!ptr)
    {
        fprintf(stderr, "Error: Failed to allocate %zu bytes for %s\n", size, label);
        exit(EXIT_FAILURE);
    }
    return ptr;
}

void* safe_calloc(size_t nmemb, size_t size, const char* label)
{
    void* ptr = calloc(nmemb, size);
    if (!ptr)
    {
        fprintf(stderr, "Error: Failed to calloc %zu bytes for %s\n", nmemb * size, label);
        exit(EXIT_FAILURE);
    }
    return ptr;
}

/* 激活函数 */
float sigmoid(float x)
{
    if (x >= 0)
    {
        return 1.0f / (1.0f + expf(-x));
    }
    else
    {
        float exp_x = expf(x);
        return exp_x / (1.0f + exp_x);
    }
}

float relu(float x)
{
    return x > 0.0f ? x : 0.0f;
}

float leaky_relu(float x, float alpha)
{
    return x > 0.0f ? x : alpha * x;
}

/* Softmax计算，返回负对数概率 */
float softmax(float* logits, int n, int idx)
{
    float max_logit = logits[0];
    for (int i = 1; i < n; i++)
    {
        if (logits[i] > max_logit) max_logit = logits[i];
    }

    float sum = 0.0f;
    for (int i = 0; i < n; i++)
    {
        logits[i] = expf(logits[i] - max_logit);
        sum += logits[i];
    }

    for (int i = 0; i < n; i++)
    {
        logits[i] /= sum;
    }

    return -logf(logits[idx] + 1e-8f);
}

/* 交叉熵损失 */
float cross_entropy_loss(float* probs, int label, int num_classes)
{
    return -logf(probs[label - 1] + 1e-8f);
}

/* 随机数生成器 */
void rng_init(RNG* rng, unsigned int seed)
{
    rng->state = seed ? seed : 123456789;
}

unsigned int rng_next(RNG* rng)
{
    unsigned int x = rng->state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng->state = x;
    return x;
}

float rng_uniform(RNG* rng)
{
    return (rng_next(rng) & 0x7FFFFFFF) / (float)0x7FFFFFFF;
}

float rng_normal(RNG* rng)
{
    /* Box-Muller变换 */
    float u1 = rng_uniform(rng);
    float u2 = rng_uniform(rng);
    if (u1 < 1e-7f) u1 = 1e-7f;
    return sqrtf(-2.0f * logf(u1)) * cosf(2.0f * 3.14159265358979f * u2);
}

float rng_uniform_range(RNG* rng, float min, float max)
{
    return min + (max - min) * rng_uniform(rng);
}

/* 矩阵乘法 C = A * B, A[m*k], B[k*n], C[m*n] */
void matmul(const float* A, const float* B, float* C, int m, int n, int k)
{
    for (int i = 0; i < m; i++)
    {
        for (int j = 0; j < n; j++)
        {
            float sum = 0.0f;
            for (int l = 0; l < k; l++)
            {
                sum += A[i * k + l] * B[l * n + j];
            }
            C[i * n + j] = sum;
        }
    }
}

/* 矩阵向量乘法 out = A * v */
void matvec(const float* A, const float* v, float* out, int rows, int cols)
{
    for (int i = 0; i < rows; i++)
    {
        float sum = 0.0f;
        for (int j = 0; j < cols; j++)
        {
            sum += A[i * cols + j] * v[j];
        }
        out[i] = sum;
    }
}

/* 矩阵转置 */
void transpose(const float* src, float* dst, int rows, int cols)
{
    for (int i = 0; i < rows; i++)
    {
        for (int j = 0; j < cols; j++)
        {
            dst[j * rows + i] = src[i * cols + j];
        }
    }
}

/* 打印进度条 */
void print_progress(int current, int total, int bar_width)
{
    float ratio = (float)current / total;
    int filled = (int)(ratio * bar_width);

    printf("\r[");
    for (int i = 0; i < bar_width; i++)
    {
        if (i < filled) printf("=");
        else if (i == filled) printf(">");
        else printf(" ");
    }
    printf("] %3.1f%% (%d/%d)", ratio * 100, current, total);
    fflush(stdout);

    if (current == total) printf("\n");
}

/* 打印混淆矩阵 */
void print_confusion_matrix(int* cm, int num_classes)
{
    printf("\nConfusion Matrix (rows=true, cols=pred):\n");
    printf("     ");
    for (int i = 0; i < num_classes; i++)
    {
        printf(" %2c", 'A' + i);
    }
    printf("\n");

    for (int i = 0; i < num_classes; i++)
    {
        printf(" %2c |", 'A' + i);
        for (int j = 0; j < num_classes; j++)
        {
            printf(" %2d", cm[i * num_classes + j]);
        }
        printf("\n");
    }
}

/* 保存权重 */
int save_weights(const char* filename, float** weights, int* dims, int num_layers)
{
    FILE* fp = fopen(filename, "wb");
    if (!fp) return -1;

    fwrite(&num_layers, sizeof(int), 1, fp);
    for (int i = 0; i < num_layers; i++)
    {
        fwrite(&dims[i], sizeof(int), 1, fp);
    }
    for (int i = 0; i < num_layers; i++)
    {
        fwrite(weights[i], sizeof(float), dims[i], fp);
    }

    fclose(fp);
    return 0;
}

/* 加载权重 */
int load_weights(const char* filename, float** weights, int* dims, int num_layers)
{
    FILE* fp = fopen(filename, "rb");
    if (!fp) return -1;

    int loaded_layers;
    fread(&loaded_layers, sizeof(int), 1, fp);
    if (loaded_layers != num_layers)
    {
        fprintf(stderr, "Error: Layer count mismatch\n");
        fclose(fp);
        return -1;
    }

    for (int i = 0; i < num_layers; i++)
    {
        int dim;
        fread(&dim, sizeof(int), 1, fp);
        if (dim != dims[i])
        {
            fprintf(stderr, "Error: Dimension mismatch at layer %d\n", i);
            fclose(fp);
            return -1;
        }
    }

    for (int i = 0; i < num_layers; i++)
    {
        fread(weights[i], sizeof(float), dims[i], fp);
    }

    fclose(fp);
    return 0;
}
