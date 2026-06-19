#ifndef NEURAL_NET_H
#define NEURAL_NET_H

/* 深度神经网络头文件
 * 实现基于CUDA GPU加速的多层感知机(MLP)
 * 支持AdamW优化器、Dropout、LayerNorm
 */

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_LAYERS      8
#define INPUT_DIM       617
#define HIDDEN1_DIM     1024
#define HIDDEN2_DIM     512
#define HIDDEN3_DIM     256
#define OUTPUT_DIM      26
#define NUM_LAYERS      4   /* 隐藏层数 */

/* 网络层类型 */
typedef enum {
    LAYER_LINEAR,
    LAYER_RELU,
    LAYER_DROPOUT,
    LAYER_LAYERNORM
} LayerType;

/* 网络配置 */
typedef struct {
    int input_dim;
    int hidden_dims[NUM_LAYERS];
    int output_dim;
    int num_layers;
    float dropout_rate;
    float weight_decay;
    float learning_rate;
    float beta1;        /* Adam参数 */
    float beta2;
    float eps;
    int batch_size;
    int max_epochs;
    int early_stop_patience;
    float lr_min;
} NetConfig;

/* 网络层（GPU端） */
typedef struct {
    int in_dim;
    int out_dim;

    /* 权重和偏置（GPU内存） */
    float* d_W;         /* [out_dim x in_dim] */
    float* d_b;         /* [out_dim] */

    /* 梯度 */
    float* d_dW;
    float* d_db;

    /* Adam状态 */
    float* d_mW;        /* 一阶矩 */
    float* d_vW;        /* 二阶矩 */
    float* d_mb;
    float* d_vb;

    /* 前向传播缓存 */
    float* d_input;     /* 输入 [batch x in_dim] */
    float* d_output;    /* 输出 [batch x out_dim] */
    float* d_pre_act;   /* 激活前 [batch x out_dim] */

    /* Dropout掩码 */
    float* d_dropout_mask;

    /* LayerNorm参数 */
    float* d_gamma;
    float* d_beta_ln;
    float* d_dgamma;
    float* d_dbeta_ln;
    float* d_mean;      /* [batch] */
    float* d_var;       /* [batch] */

    int use_layernorm;
    int use_dropout;
} Layer;

/* 神经网络 */
typedef struct {
    NetConfig config;
    Layer layers[NUM_LAYERS + 1];  /* +1 for output layer */
    int num_layers_total;

    /* GPU工作内存 */
    float* d_loss_grad;     /* 损失梯度 [batch x output_dim] */
    float* d_probs;         /* Softmax概率 [batch x output_dim] */

    /* 训练状态 */
    int current_step;
    float current_lr;
} NeuralNet;

/* 默认配置 */
void default_config(NetConfig* cfg);

/* 网络生命周期 */
NeuralNet* net_create(const NetConfig* cfg);
void net_destroy(NeuralNet* net);

/* 权重初始化 */
void net_init_weights(NeuralNet* net, unsigned int seed);

/* 前向传播 */
void net_forward(NeuralNet* net, float* d_input, int batch_size, int training);

/* 计算损失和反向传播 */
float net_backward(NeuralNet* net, int* labels, int batch_size);

/* 参数更新 (AdamW) */
void net_update_weights(NeuralNet* net, int batch_size);

/* 推理：返回预测类别 */
void net_predict(NeuralNet* net, float* d_input, int* predictions,
                 float* probs, int batch_size);

/* 评估准确率 */
float net_evaluate(NeuralNet* net, float* h_features, int* h_labels,
                   int num_samples, int batch_size);

/* 保存/加载模型 */
int net_save(NeuralNet* net, const char* filename);
int net_load(NeuralNet* net, const char* filename);

/* 学习率调度（Cosine Annealing） */
void update_learning_rate(NeuralNet* net, int epoch, int total_epochs);

/* 打印网络结构 */
void net_print_architecture(NeuralNet* net);

/* 分配批次相关GPU内存 */
void alloc_batch_memory(NeuralNet* net, int batch_size);

/* CUDA错误检查宏 */
#ifndef CUDA_CHECK
#define CUDA_CHECK(call) do { \
    cudaError_t err = call; \
    if (err != cudaSuccess) { \
        fprintf(stderr, "CUDA error at %s:%d: %s\n", __FILE__, __LINE__, \
                cudaGetErrorString(err)); \
        exit(EXIT_FAILURE); \
    } \
} while(0)
#endif

/* CPU辅助：将数据拷贝到GPU */
float* copy_to_gpu(float* h_data, int num_elements);
void copy_from_gpu(float* h_data, float* d_data, int num_elements);
void free_gpu(float* d_ptr);

#ifdef __cplusplus
}
#endif

#endif
