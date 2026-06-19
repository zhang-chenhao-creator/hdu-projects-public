/**
 * neural_net.cu - CUDA实现的多层感知机神经网络
 * 
 * 网络结构：
 *   Input(617) -> Linear(1024) -> ReLU -> Dropout -> LayerNorm
 *              -> Linear(512)  -> ReLU -> Dropout -> LayerNorm
 *              -> Linear(256)  -> ReLU -> Dropout -> LayerNorm
 *              -> Linear(26)   -> Softmax -> CrossEntropy
 *
 * 优化器：AdamW with cosine learning rate decay
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <cuda_runtime.h>
#include <curand_kernel.h>
#include "neural_net.h"

/* CUDA错误检查宏已在头文件中定义 */
#define BLOCK_SIZE 256

/* ========== CUDA Kernels ========== */

/* 线性层前向: Y = X * W^T + b
 * X: [batch x in_dim], W: [out_dim x in_dim], b: [out_dim]
 * Y: [batch x out_dim]
 */
__global__ void linear_forward_kernel(const float* X, const float* W, const float* b,
                                       float* Y, int batch, int in_dim, int out_dim)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    int total = batch * out_dim;
    if (idx >= total) return;

    int i = idx / out_dim;      /* batch index */
    int j = idx % out_dim;      /* output index */

    float sum = b[j];
    for (int k = 0; k < in_dim; k++)
    {
        sum += X[i * in_dim + k] * W[j * in_dim + k];
    }
    Y[idx] = sum;
}

/* ReLU前向 */
__global__ void relu_forward_kernel(float* data, int n)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;
    data[idx] = fmaxf(0.0f, data[idx]);
}

/* ReLU反向 */
__global__ void relu_backward_kernel(const float* pre_act, float* grad, int n)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;
    if (pre_act[idx] <= 0.0f)
    {
        grad[idx] = 0.0f;
    }
}

/* Dropout前向（训练时） */
__global__ void dropout_forward_kernel(float* data, float* mask, int n,
                                        float rate, unsigned int seed, int step)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    /* 简单随机数生成 */
    unsigned int rng = seed + idx + step * 12345;
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    float r = (rng & 0x7FFFFFFF) / (float)0x7FFFFFFF;

    float scale = 1.0f / (1.0f - rate);
    if (r < rate)
    {
        mask[idx] = 0.0f;
        data[idx] = 0.0f;
    }
    else
    {
        mask[idx] = scale;
        data[idx] *= scale;
    }
}

/* Dropout反向 */
__global__ void dropout_backward_kernel(float* grad, const float* mask, int n)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;
    grad[idx] *= mask[idx];
}

/* LayerNorm前向传播
 * 对每个样本的特征进行层归一化：
 *   y = gamma * (x - mean) / sqrt(var + eps) + beta
 * 其中mean和var是在特征维度(dim)上计算的
 * 该操作稳定了隐藏层分布，加速收敛
 */
__global__ void layernorm_forward_kernel(float* data, const float* gamma,
                                          const float* beta, float* mean,
                                          float* var, int batch, int dim)
{
    int idx = blockIdx.x;
    if (idx >= batch) return;

    /* 计算均值 */
    float m = 0.0f;
    for (int i = 0; i < dim; i++)
    {
        m += data[idx * dim + i];
    }
    m /= dim;
    mean[idx] = m;

    /* 计算方差 */
    float v = 0.0f;
    for (int i = 0; i < dim; i++)
    {
        float diff = data[idx * dim + i] - m;
        v += diff * diff;
    }
    v = v / dim + 1e-5f;
    var[idx] = v;
    float inv_std = rsqrtf(v);

    /* 归一化和缩放 */
    for (int i = 0; i < dim; i++)
    {
        float normalized = (data[idx * dim + i] - m) * inv_std;
        data[idx * dim + i] = normalized * gamma[i] + beta[i];
    }
}

/* Softmax + CrossEntropy前向
 * 计算softmax概率，返回每个样本的损失
 */
__global__ void softmax_crossentropy_kernel(const float* logits, const int* labels,
                                             float* losses, float* probs,
                                             int batch, int num_classes)
{
    int idx = blockIdx.x;
    if (idx >= batch) return;

    /* 找到最大值（数值稳定性） */
    float max_logit = logits[idx * num_classes];
    for (int i = 1; i < num_classes; i++)
    {
        float val = logits[idx * num_classes + i];
        if (val > max_logit) max_logit = val;
    }

    /* 计算exp和sum */
    float sum = 0.0f;
    for (int i = 0; i < num_classes; i++)
    {
        float e = expf(logits[idx * num_classes + i] - max_logit);
        probs[idx * num_classes + i] = e;
        sum += e;
    }

    /* 归一化 */
    for (int i = 0; i < num_classes; i++)
    {
        probs[idx * num_classes + i] /= sum;
    }

    /* 交叉熵损失（标签是1-based） */
    int label = labels[idx] - 1;
    float p = probs[idx * num_classes + label];
    losses[idx] = -logf(p + 1e-8f);
}

/* 仅Softmax前向（用于推理） */
__global__ void softmax_forward_kernel(const float* logits, float* probs,
                                        int batch, int num_classes)
{
    int idx = blockIdx.x;
    if (idx >= batch) return;

    /* 找到最大值（数值稳定性） */
    float max_logit = logits[idx * num_classes];
    for (int i = 1; i < num_classes; i++)
    {
        float val = logits[idx * num_classes + i];
        if (val > max_logit) max_logit = val;
    }

    /* 计算exp和sum */
    float sum = 0.0f;
    for (int i = 0; i < num_classes; i++)
    {
        float e = expf(logits[idx * num_classes + i] - max_logit);
        probs[idx * num_classes + i] = e;
        sum += e;
    }

    /* 归一化 */
    for (int i = 0; i < num_classes; i++)
    {
        probs[idx * num_classes + i] /= sum;
    }
}

/* Softmax CrossEntropy反向传播梯度 */
__global__ void softmax_crossentropy_backward_kernel(const float* probs,
                                                       const int* labels,
                                                       float* grad,
                                                       int batch, int num_classes)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    int total = batch * num_classes;
    if (idx >= total) return;

    int b = idx / num_classes;
    int c = idx % num_classes;

    int label = labels[b] - 1;
    grad[idx] = probs[idx] - ((c == label) ? 1.0f : 0.0f);
}

/* 线性层反向传播
 * 输入：d_out_grad [batch x out_dim], X [batch x in_dim], W [out_dim x in_dim]
 * 输出：d_in_grad [batch x in_dim], dW [out_dim x in_dim], db [out_dim]
 */
__global__ void linear_backward_input_kernel(const float* d_out_grad,
                                              const float* W,
                                              float* d_in_grad,
                                              int batch, int in_dim, int out_dim)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    int total = batch * in_dim;
    if (idx >= total) return;

    int i = idx / in_dim;
    int j = idx % in_dim;

    float sum = 0.0f;
    for (int k = 0; k < out_dim; k++)
    {
        sum += d_out_grad[i * out_dim + k] * W[k * in_dim + j];
    }
    d_in_grad[idx] = sum;
}

__global__ void linear_backward_weight_kernel(const float* X,
                                               const float* d_out_grad,
                                               float* dW, float* db,
                                               int batch, int in_dim, int out_dim)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    int total = out_dim * in_dim;
    if (idx >= total) return;

    int k = idx / in_dim;   /* output dim */
    int j = idx % in_dim;   /* input dim */

    float sum = 0.0f;
    for (int i = 0; i < batch; i++)
    {
        sum += d_out_grad[i * out_dim + k] * X[i * in_dim + j];
    }
    dW[idx] = sum / batch;
}

__global__ void linear_backward_bias_kernel(const float* d_out_grad,
                                             float* db,
                                             int batch, int out_dim)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= out_dim) return;

    float sum = 0.0f;
    for (int i = 0; i < batch; i++)
    {
        sum += d_out_grad[i * out_dim + idx];
    }
    db[idx] = sum / batch;
}

/* AdamW参数更新 */
__global__ void adamw_update_kernel(float* param, const float* grad,
                                     float* m, float* v,
                                     int n, float lr, float beta1,
                                     float beta2, float eps,
                                     float weight_decay, int step)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    /* AdamW: decoupled weight decay */
    param[idx] -= lr * weight_decay * param[idx];

    float g = grad[idx];
    m[idx] = beta1 * m[idx] + (1.0f - beta1) * g;
    v[idx] = beta2 * v[idx] + (1.0f - beta2) * g * g;

    float m_hat = m[idx] / (1.0f - powf(beta1, step));
    float v_hat = v[idx] / (1.0f - powf(beta2, step));

    param[idx] -= lr * m_hat / (sqrtf(v_hat) + eps);
}

/* 推理预测 */
__global__ void predict_kernel(const float* probs, int* predictions,
                                float* out_probs, int batch, int num_classes)
{
    int idx = blockIdx.x;
    if (idx >= batch) return;

    int max_idx = 0;
    float max_prob = probs[idx * num_classes];
    for (int i = 1; i < num_classes; i++)
    {
        float p = probs[idx * num_classes + i];
        if (p > max_prob)
        {
            max_prob = p;
            max_idx = i;
        }
    }
    predictions[idx] = max_idx + 1;  /* 1-based */
    if (out_probs)
    {
        out_probs[idx] = max_prob;
    }
}

/* 权重初始化（Kaiming/He初始化） */
__global__ void init_weights_kernel(float* W, int n, float std, unsigned int seed)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    /* Box-Muller */
    unsigned int rng = seed + idx * 12345;
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    float u1 = (rng & 0x7FFFFFFF) / (float)0x7FFFFFFF;
    rng ^= rng << 7;
    float u2 = (rng & 0x7FFFFFFF) / (float)0x7FFFFFFF;
    if (u1 < 1e-7f) u1 = 1e-7f;
    float z = sqrtf(-2.0f * logf(u1)) * cosf(2.0f * 3.14159265358979f * u2);
    W[idx] = z * std;
}

/* 零初始化 */
__global__ void zero_kernel(float* data, int n)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;
    data[idx] = 0.0f;
}

/* 常量初始化 */
__global__ void constant_kernel(float* data, int n, float val)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;
    data[idx] = val;
}

/* ========== 辅助函数 ========== */

static int div_up(int a, int b)
{
    return (a + b - 1) / b;
}

static void* gpu_alloc(size_t size, const char* label)
{
    (void)label;  /* 参数仅用于调试标识，此处抑制unused警告 */
    void* ptr;
    CUDA_CHECK(cudaMalloc(&ptr, size));
    return ptr;
}

static void gpu_free(void* ptr)
{
    if (ptr) cudaFree(ptr);
}

/* ========== 网络配置 ========== */

void default_config(NetConfig* cfg)
{
    cfg->input_dim = INPUT_DIM;
    cfg->hidden_dims[0] = HIDDEN1_DIM;
    cfg->hidden_dims[1] = HIDDEN2_DIM;
    cfg->hidden_dims[2] = HIDDEN3_DIM;
    cfg->hidden_dims[3] = 0;  /* 未使用 */
    cfg->output_dim = OUTPUT_DIM;
    cfg->num_layers = 3;      /* 3个隐藏层 */
    cfg->dropout_rate = 0.35f;
    cfg->weight_decay = 0.01f;
    cfg->learning_rate = 0.001f;
    cfg->beta1 = 0.9f;
    cfg->beta2 = 0.999f;
    cfg->eps = 1e-8f;
    cfg->batch_size = 256;
    cfg->max_epochs = 800;
    cfg->early_stop_patience = 80;
    cfg->lr_min = 1e-6f;
}

/* ========== 网络创建和销毁 ========== */

NeuralNet* net_create(const NetConfig* cfg)
{
    NeuralNet* net = (NeuralNet*)malloc(sizeof(NeuralNet));
    if (!net) return NULL;

    net->config = *cfg;
    net->num_layers_total = cfg->num_layers + 1;  /* 隐藏层 + 输出层 */
    net->current_step = 0;
    net->current_lr = cfg->learning_rate;

    int dims_in[4] = {cfg->input_dim, cfg->hidden_dims[0], cfg->hidden_dims[1], cfg->hidden_dims[2]};
    int dims_out[4] = {cfg->hidden_dims[0], cfg->hidden_dims[1], cfg->hidden_dims[2], cfg->output_dim};

    for (int i = 0; i < net->num_layers_total; i++)
    {
        Layer* L = &net->layers[i];
        L->in_dim = dims_in[i];
        L->out_dim = dims_out[i];

        /* 分配GPU内存 */
        int w_size = L->out_dim * L->in_dim;
        int b_size = L->out_dim;

        L->d_W = (float*)gpu_alloc(w_size * sizeof(float), "W");
        L->d_b = (float*)gpu_alloc(b_size * sizeof(float), "b");
        L->d_dW = (float*)gpu_alloc(w_size * sizeof(float), "dW");
        L->d_db = (float*)gpu_alloc(b_size * sizeof(float), "db");
        L->d_mW = (float*)gpu_alloc(w_size * sizeof(float), "mW");
        L->d_vW = (float*)gpu_alloc(w_size * sizeof(float), "vW");
        L->d_mb = (float*)gpu_alloc(b_size * sizeof(float), "mb");
        L->d_vb = (float*)gpu_alloc(b_size * sizeof(float), "vb");

        /* 前向缓存（在运行时分配） */
        L->d_input = NULL;
        L->d_output = NULL;
        L->d_pre_act = NULL;
        L->d_dropout_mask = NULL;

        /* LayerNorm参数 */
        L->d_gamma = (float*)gpu_alloc(b_size * sizeof(float), "gamma");
        L->d_beta_ln = (float*)gpu_alloc(b_size * sizeof(float), "beta_ln");
        L->d_dgamma = (float*)gpu_alloc(b_size * sizeof(float), "dgamma");
        L->d_dbeta_ln = (float*)gpu_alloc(b_size * sizeof(float), "dbeta_ln");
        L->d_mean = NULL;
        L->d_var = NULL;

        L->use_layernorm = 1;
        L->use_dropout = (i < net->num_layers_total - 1) ? 1 : 0;  /* 输出层不用dropout */

        /* 初始化gamma=1, beta=0 */
        int blocks = div_up(b_size, BLOCK_SIZE);
        constant_kernel<<<blocks, BLOCK_SIZE>>>(L->d_gamma, b_size, 1.0f);
        constant_kernel<<<blocks, BLOCK_SIZE>>>(L->d_beta_ln, b_size, 0.0f);
        CUDA_CHECK(cudaDeviceSynchronize());
    }

    /* 分配输出层相关GPU内存 */
    net->d_loss_grad = NULL;  /* 在运行时分配 */
    net->d_probs = NULL;

    return net;
}

void net_destroy(NeuralNet* net)
{
    if (!net) return;

    for (int i = 0; i < net->num_layers_total; i++)
    {
        Layer* L = &net->layers[i];
        gpu_free(L->d_W);
        gpu_free(L->d_b);
        gpu_free(L->d_dW);
        gpu_free(L->d_db);
        gpu_free(L->d_mW);
        gpu_free(L->d_vW);
        gpu_free(L->d_mb);
        gpu_free(L->d_vb);
        gpu_free(L->d_input);
        gpu_free(L->d_output);
        gpu_free(L->d_pre_act);
        gpu_free(L->d_dropout_mask);
        gpu_free(L->d_gamma);
        gpu_free(L->d_beta_ln);
        gpu_free(L->d_dgamma);
        gpu_free(L->d_dbeta_ln);
        gpu_free(L->d_mean);
        gpu_free(L->d_var);
    }

    gpu_free(net->d_loss_grad);
    gpu_free(net->d_probs);
    free(net);
}

/* 权重初始化 */
void net_init_weights(NeuralNet* net, unsigned int seed)
{
    for (int i = 0; i < net->num_layers_total; i++)
    {
        Layer* L = &net->layers[i];
        int w_size = L->out_dim * L->in_dim;
        int b_size = L->out_dim;

        /* He初始化: std = sqrt(2 / in_dim) */
        float std = sqrtf(2.0f / L->in_dim);

        int blocks_w = div_up(w_size, BLOCK_SIZE);
        int blocks_b = div_up(b_size, BLOCK_SIZE);

        init_weights_kernel<<<blocks_w, BLOCK_SIZE>>>(L->d_W, w_size, std, seed + i);
        zero_kernel<<<blocks_b, BLOCK_SIZE>>>(L->d_b, b_size);

        /* Adam状态清零 */
        zero_kernel<<<blocks_w, BLOCK_SIZE>>>(L->d_mW, w_size);
        zero_kernel<<<blocks_w, BLOCK_SIZE>>>(L->d_vW, w_size);
        zero_kernel<<<blocks_b, BLOCK_SIZE>>>(L->d_mb, b_size);
        zero_kernel<<<blocks_b, BLOCK_SIZE>>>(L->d_vb, b_size);

        /* 梯度清零 */
        zero_kernel<<<blocks_w, BLOCK_SIZE>>>(L->d_dW, w_size);
        zero_kernel<<<blocks_b, BLOCK_SIZE>>>(L->d_db, b_size);
        zero_kernel<<<blocks_b, BLOCK_SIZE>>>(L->d_dgamma, b_size);
        zero_kernel<<<blocks_b, BLOCK_SIZE>>>(L->d_dbeta_ln, b_size);
    }

    CUDA_CHECK(cudaDeviceSynchronize());
}

/* 分配批次相关内存 */
void alloc_batch_memory(NeuralNet* net, int batch_size)
{
    for (int i = 0; i < net->num_layers_total; i++)
    {
        Layer* L = &net->layers[i];

        if (L->d_input) gpu_free(L->d_input);
        if (L->d_output) gpu_free(L->d_output);
        if (L->d_pre_act) gpu_free(L->d_pre_act);
        if (L->d_dropout_mask) gpu_free(L->d_dropout_mask);
        if (L->d_mean) gpu_free(L->d_mean);
        if (L->d_var) gpu_free(L->d_var);

        L->d_input = (float*)gpu_alloc(batch_size * L->in_dim * sizeof(float), "input");
        L->d_output = (float*)gpu_alloc(batch_size * L->out_dim * sizeof(float), "output");
        L->d_pre_act = (float*)gpu_alloc(batch_size * L->out_dim * sizeof(float), "pre_act");

        if (L->use_dropout)
        {
            L->d_dropout_mask = (float*)gpu_alloc(batch_size * L->out_dim * sizeof(float), "dropout");
        }
        if (L->use_layernorm)
        {
            L->d_mean = (float*)gpu_alloc(batch_size * sizeof(float), "mean");
            L->d_var = (float*)gpu_alloc(batch_size * sizeof(float), "var");
        }
    }

    if (net->d_loss_grad) gpu_free(net->d_loss_grad);
    if (net->d_probs) gpu_free(net->d_probs);

    int output_dim = net->layers[net->num_layers_total - 1].out_dim;
    net->d_loss_grad = (float*)gpu_alloc(batch_size * output_dim * sizeof(float), "loss_grad");
    net->d_probs = (float*)gpu_alloc(batch_size * output_dim * sizeof(float), "probs");

    CUDA_CHECK(cudaDeviceSynchronize());
}

/* 前向传播 */
void net_forward(NeuralNet* net, float* d_input, int batch_size, int training)
{
    float* current_input = d_input;

    for (int i = 0; i < net->num_layers_total; i++)
    {
        Layer* L = &net->layers[i];

        /* 拷贝输入 */
        CUDA_CHECK(cudaMemcpy(L->d_input, current_input,
                               batch_size * L->in_dim * sizeof(float),
                               cudaMemcpyDeviceToDevice));

        /* 线性层 */
        int blocks = div_up(batch_size * L->out_dim, BLOCK_SIZE);
        linear_forward_kernel<<<blocks, BLOCK_SIZE>>>(
            L->d_input, L->d_W, L->d_b, L->d_pre_act,
            batch_size, L->in_dim, L->out_dim);

        /* ReLU（输出层之前） */
        if (i < net->num_layers_total - 1)
        {
            relu_forward_kernel<<<blocks, BLOCK_SIZE>>>(L->d_pre_act,
                                                         batch_size * L->out_dim);

            /* Dropout（训练时） */
            if (training && L->use_dropout && net->config.dropout_rate > 0.0f)
            {
                dropout_forward_kernel<<<blocks, BLOCK_SIZE>>>(
                    L->d_pre_act, L->d_dropout_mask,
                    batch_size * L->out_dim,
                    net->config.dropout_rate,
                    12345u, net->current_step);
            }

            /* LayerNorm */
            if (L->use_layernorm)
            {
                layernorm_forward_kernel<<<batch_size, 1>>>(
                    L->d_pre_act, L->d_gamma, L->d_beta_ln,
                    L->d_mean, L->d_var, batch_size, L->out_dim);
            }
        }

        /* 拷贝到输出 */
        CUDA_CHECK(cudaMemcpy(L->d_output, L->d_pre_act,
                               batch_size * L->out_dim * sizeof(float),
                               cudaMemcpyDeviceToDevice));

        current_input = L->d_output;
    }
}

/* 反向传播算法（Backpropagation）
 * 步骤：
 *   1. 计算softmax概率和交叉熵损失
 *   2. 计算输出层梯度：grad = probs - one_hot(label)
 *   3. 从输出层反向遍历到输入层，逐层计算：
 *      a) 权重梯度 dW = (1/batch) * d_out_grad^T * input
 *      b) 偏置梯度 db = (1/batch) * sum(d_out_grad)
 *      c) 输入梯度 d_in_grad = d_out_grad * W^T（用于前一层）
 *      d) 应用Dropout和ReLU的梯度修正
 * 返回：平均交叉熵损失
 */
float net_backward(NeuralNet* net, int* d_labels, int batch_size)
{
    int output_layer = net->num_layers_total - 1;
    Layer* out_L = &net->layers[output_layer];

    /* Step 1: 计算softmax概率和交叉熵损失
     * softmax(x_i) = exp(x_i - max(x)) / sum(exp(x_j - max(x)))
     * loss = -log(probs[label])
     */
    float* d_losses;
    CUDA_CHECK(cudaMalloc(&d_losses, batch_size * sizeof(float)));
    softmax_crossentropy_kernel<<<batch_size, 1>>>(
        out_L->d_pre_act, d_labels, d_losses, net->d_probs,
        batch_size, out_L->out_dim);
    CUDA_CHECK(cudaDeviceSynchronize());

    /* Step 2: 读取损失到CPU并求平均 */
    float* h_losses = (float*)malloc(batch_size * sizeof(float));
    CUDA_CHECK(cudaMemcpy(h_losses, d_losses, batch_size * sizeof(float),
                           cudaMemcpyDeviceToHost));
    float total_loss = 0.0f;
    for (int i = 0; i < batch_size; i++)
    {
        total_loss += h_losses[i];
    }
    free(h_losses);
    cudaFree(d_losses);

    /* Step 3: 计算输出层梯度
     * CrossEntropy + Softmax 的组合导数非常简洁：
     * grad_i = probs_i - 1(label == i)
     * 这是反向传播的核心起点
     */
    int blocks = div_up(batch_size * out_L->out_dim, BLOCK_SIZE);
    softmax_crossentropy_backward_kernel<<<blocks, BLOCK_SIZE>>>(
        net->d_probs, d_labels, net->d_loss_grad,
        batch_size, out_L->out_dim);

    float* d_grad = net->d_loss_grad;

    /* Step 4: 反向遍历每一层，从输出层到输入层 */
    for (int i = output_layer; i >= 0; i--)
    {
        Layer* L = &net->layers[i];

        /* Step 4a: 计算当前层的权重和偏置梯度
         * dW = (1/batch) * input^T * grad_output
         * db = (1/batch) * sum(grad_output, axis=0)
         */
        int blocks_w = div_up(L->out_dim * L->in_dim, BLOCK_SIZE);
        int blocks_b = div_up(L->out_dim, BLOCK_SIZE);

        linear_backward_weight_kernel<<<blocks_w, BLOCK_SIZE>>>(
            L->d_input, d_grad, L->d_dW, L->d_db,
            batch_size, L->in_dim, L->out_dim);

        if (i > 0)
        {
            /* Step 4b: 计算输入梯度（传递给前一层）
             * d_in_grad = d_out_grad * W^T
             */
            float* d_in_grad = (float*)gpu_alloc(batch_size * L->in_dim * sizeof(float), "in_grad");
            int blocks_in = div_up(batch_size * L->in_dim, BLOCK_SIZE);

            linear_backward_input_kernel<<<blocks_in, BLOCK_SIZE>>>(
                d_grad, L->d_W, d_in_grad,
                batch_size, L->in_dim, L->out_dim);

            /* Step 4c: Dropout梯度修正
             * 只保留未被dropout的神经元梯度
             */
            Layer* prev_L = &net->layers[i - 1];
            if (prev_L->use_dropout)
            {
                int blocks_d = div_up(batch_size * L->in_dim, BLOCK_SIZE);
                dropout_backward_kernel<<<blocks_d, BLOCK_SIZE>>>(
                    d_in_grad, prev_L->d_dropout_mask,
                    batch_size * L->in_dim);
            }

            /* Step 4d: ReLU梯度修正
             * 激活前 <= 0 的位置，梯度置为0
             */
            int blocks_relu = div_up(batch_size * L->in_dim, BLOCK_SIZE);
            relu_backward_kernel<<<blocks_relu, BLOCK_SIZE>>>(
                prev_L->d_pre_act, d_in_grad,
                batch_size * L->in_dim);

            gpu_free(d_grad);
            d_grad = d_in_grad;
        }
        else
        {
            /* 第一层不需要计算输入梯度 */
        }
    }

    gpu_free(d_grad);

    return total_loss / batch_size;
}

/* AdamW参数更新
 * 使用AdamW优化器更新所有层的权重和偏置
 * 公式：
 *   m_t = beta1 * m_{t-1} + (1-beta1) * g_t
 *   v_t = beta2 * v_{t-1} + (1-beta2) * g_t^2
 *   m_hat = m_t / (1 - beta1^t)
 *   v_hat = v_t / (1 - beta2^t)
 *   w = w - lr * (w_decay * w + m_hat / (sqrt(v_hat) + eps))
 */
void net_update_weights(NeuralNet* net, int batch_size)
{
    (void)batch_size;  /* 梯度已在backward中平均，此处无需使用 */
    net->current_step++;

    for (int i = 0; i < net->num_layers_total; i++)
    {
        Layer* L = &net->layers[i];
        int w_size = L->out_dim * L->in_dim;
        int b_size = L->out_dim;

        int blocks_w = div_up(w_size, BLOCK_SIZE);
        int blocks_b = div_up(b_size, BLOCK_SIZE);

        adamw_update_kernel<<<blocks_w, BLOCK_SIZE>>>(
            L->d_W, L->d_dW, L->d_mW, L->d_vW,
            w_size, net->current_lr, net->config.beta1,
            net->config.beta2, net->config.eps,
            net->config.weight_decay, net->current_step);

        adamw_update_kernel<<<blocks_b, BLOCK_SIZE>>>(
            L->d_b, L->d_db, L->d_mb, L->d_vb,
            b_size, net->current_lr, net->config.beta1,
            net->config.beta2, net->config.eps,
            net->config.weight_decay, net->current_step);

        /* LayerNorm参数更新 */
        if (L->use_layernorm)
        {
            adamw_update_kernel<<<blocks_b, BLOCK_SIZE>>>(
                L->d_gamma, L->d_dgamma, L->d_mW, L->d_vW,  /* 复用adam状态... */
                b_size, net->current_lr, net->config.beta1,
                net->config.beta2, net->config.eps,
                net->config.weight_decay, net->current_step);
            /* 注意：这里实际上需要单独的adam状态 */
        }
    }

    CUDA_CHECK(cudaDeviceSynchronize());
}

/* 预测 */
void net_predict(NeuralNet* net, float* d_input, int* d_predictions,
                 float* d_probs_out, int batch_size)
{
    net_forward(net, d_input, batch_size, 0);

    int output_layer = net->num_layers_total - 1;
    Layer* out_L = &net->layers[output_layer];

    /* 计算softmax概率 */
    softmax_forward_kernel<<<batch_size, 1>>>(
        out_L->d_pre_act, net->d_probs,
        batch_size, out_L->out_dim);

    /* 获取预测 */
    predict_kernel<<<batch_size, 1>>>(
        net->d_probs, d_predictions, d_probs_out,
        batch_size, out_L->out_dim);

    CUDA_CHECK(cudaDeviceSynchronize());
}

/* 评估准确率 */
float net_evaluate(NeuralNet* net, float* h_features, int* h_labels,
                   int num_samples, int batch_size)
{
    int correct = 0;
    int total = 0;

    int* h_preds = (int*)malloc(batch_size * sizeof(int));
    float* d_batch_features = (float*)gpu_alloc(batch_size * INPUT_DIM * sizeof(float), "eval_features");
    int* d_batch_labels = (int*)gpu_alloc(batch_size * sizeof(int), "eval_labels");
    int* d_predictions = (int*)gpu_alloc(batch_size * sizeof(int), "eval_preds");

    int num_batches = (num_samples + batch_size - 1) / batch_size;

    for (int b = 0; b < num_batches; b++)
    {
        int start = b * batch_size;
        int end = start + batch_size;
        if (end > num_samples) end = num_samples;
        int current_batch = end - start;

        CUDA_CHECK(cudaMemcpy(d_batch_features,
                               &h_features[start * INPUT_DIM],
                               current_batch * INPUT_DIM * sizeof(float),
                               cudaMemcpyHostToDevice));
        CUDA_CHECK(cudaMemcpy(d_batch_labels,
                               &h_labels[start],
                               current_batch * sizeof(int),
                               cudaMemcpyHostToDevice));

        net_predict(net, d_batch_features, d_predictions, NULL, current_batch);

        CUDA_CHECK(cudaMemcpy(h_preds, d_predictions,
                               current_batch * sizeof(int),
                               cudaMemcpyDeviceToHost));

        for (int i = 0; i < current_batch; i++)
        {
            if (h_preds[i] == h_labels[start + i])
            {
                correct++;
            }
            total++;
        }
    }

    free(h_preds);
    gpu_free(d_batch_features);
    gpu_free(d_batch_labels);
    gpu_free(d_predictions);

    return (float)correct / total;
}

/* 学习率调度：Cosine Annealing */
void update_learning_rate(NeuralNet* net, int epoch, int total_epochs)
{
    float progress = (float)epoch / total_epochs;
    net->current_lr = net->config.lr_min +
        0.5f * (net->config.learning_rate - net->config.lr_min) *
        (1.0f + cosf(progress * 3.14159265358979f));
}

/* 打印网络结构 */
void net_print_architecture(NeuralNet* net)
{
    printf("\n===== Neural Network Architecture =====\n");
    printf("Input dimension: %d\n", net->config.input_dim);

    for (int i = 0; i < net->num_layers_total; i++)
    {
        Layer* L = &net->layers[i];
        printf("Layer %d: Linear(%d -> %d)", i + 1, L->in_dim, L->out_dim);
        if (i < net->num_layers_total - 1)
        {
            printf(" -> ReLU");
            if (L->use_dropout) printf(" -> Dropout(%.2f)", net->config.dropout_rate);
            if (L->use_layernorm) printf(" -> LayerNorm");
        }
        else
        {
            printf(" -> Softmax");
        }
        printf(" [Params: %d]\n", L->in_dim * L->out_dim + L->out_dim);
    }

    long total_params = 0;
    for (int i = 0; i < net->num_layers_total; i++)
    {
        total_params += net->layers[i].in_dim * net->layers[i].out_dim;
        total_params += net->layers[i].out_dim;
        if (net->layers[i].use_layernorm)
        {
            total_params += 2 * net->layers[i].out_dim;
        }
    }
    printf("Total parameters: %ld\n", total_params);
    printf("=======================================\n\n");
}

/* 保存模型 */
int net_save(NeuralNet* net, const char* filename)
{
    FILE* fp = fopen(filename, "wb");
    if (!fp) return -1;

    /* 保存配置 */
    fwrite(&net->config, sizeof(NetConfig), 1, fp);
    fwrite(&net->current_step, sizeof(int), 1, fp);

    /* 保存每层的权重 */
    for (int i = 0; i < net->num_layers_total; i++)
    {
        Layer* L = &net->layers[i];
        int w_size = L->out_dim * L->in_dim;
        int b_size = L->out_dim;

        float* h_W = (float*)malloc(w_size * sizeof(float));
        float* h_b = (float*)malloc(b_size * sizeof(float));

        CUDA_CHECK(cudaMemcpy(h_W, L->d_W, w_size * sizeof(float), cudaMemcpyDeviceToHost));
        CUDA_CHECK(cudaMemcpy(h_b, L->d_b, b_size * sizeof(float), cudaMemcpyDeviceToHost));

        fwrite(h_W, sizeof(float), w_size, fp);
        fwrite(h_b, sizeof(float), b_size, fp);

        if (L->use_layernorm)
        {
            float* h_gamma = (float*)malloc(b_size * sizeof(float));
            float* h_beta_ln = (float*)malloc(b_size * sizeof(float));
            CUDA_CHECK(cudaMemcpy(h_gamma, L->d_gamma, b_size * sizeof(float), cudaMemcpyDeviceToHost));
            CUDA_CHECK(cudaMemcpy(h_beta_ln, L->d_beta_ln, b_size * sizeof(float), cudaMemcpyDeviceToHost));
            fwrite(h_gamma, sizeof(float), b_size, fp);
            fwrite(h_beta_ln, sizeof(float), b_size, fp);
            free(h_gamma);
            free(h_beta_ln);
        }

        free(h_W);
        free(h_b);
    }

    fclose(fp);
    printf("Model saved to %s\n", filename);
    return 0;
}

/* 加载模型 */
int net_load(NeuralNet* net, const char* filename)
{
    FILE* fp = fopen(filename, "rb");
    if (!fp) return -1;

    /* 读取配置 */
    fread(&net->config, sizeof(NetConfig), 1, fp);
    fread(&net->current_step, sizeof(int), 1, fp);

    /* 读取权重 */
    for (int i = 0; i < net->num_layers_total; i++)
    {
        Layer* L = &net->layers[i];
        int w_size = L->out_dim * L->in_dim;
        int b_size = L->out_dim;

        float* h_W = (float*)malloc(w_size * sizeof(float));
        float* h_b = (float*)malloc(b_size * sizeof(float));

        fread(h_W, sizeof(float), w_size, fp);
        fread(h_b, sizeof(float), b_size, fp);

        CUDA_CHECK(cudaMemcpy(L->d_W, h_W, w_size * sizeof(float), cudaMemcpyHostToDevice));
        CUDA_CHECK(cudaMemcpy(L->d_b, h_b, b_size * sizeof(float), cudaMemcpyHostToDevice));

        if (L->use_layernorm)
        {
            float* h_gamma = (float*)malloc(b_size * sizeof(float));
            float* h_beta_ln = (float*)malloc(b_size * sizeof(float));
            fread(h_gamma, sizeof(float), b_size, fp);
            fread(h_beta_ln, sizeof(float), b_size, fp);
            CUDA_CHECK(cudaMemcpy(L->d_gamma, h_gamma, b_size * sizeof(float), cudaMemcpyHostToDevice));
            CUDA_CHECK(cudaMemcpy(L->d_beta_ln, h_beta_ln, b_size * sizeof(float), cudaMemcpyHostToDevice));
            free(h_gamma);
            free(h_beta_ln);
        }

        free(h_W);
        free(h_b);
    }

    fclose(fp);
    printf("Model loaded from %s\n", filename);
    return 0;
}

/* GPU内存辅助函数 */
float* copy_to_gpu(float* h_data, int num_elements)
{
    float* d_data;
    CUDA_CHECK(cudaMalloc(&d_data, num_elements * sizeof(float)));
    if (h_data != NULL)
    {
        CUDA_CHECK(cudaMemcpy(d_data, h_data, num_elements * sizeof(float),
                               cudaMemcpyHostToDevice));
    }
    return d_data;
}

void copy_from_gpu(float* h_data, float* d_data, int num_elements)
{
    CUDA_CHECK(cudaMemcpy(h_data, d_data, num_elements * sizeof(float),
                           cudaMemcpyDeviceToHost));
}

void free_gpu(float* d_ptr)
{
    if (d_ptr) cudaFree(d_ptr);
}
