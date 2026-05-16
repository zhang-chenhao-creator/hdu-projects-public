#ifndef DATA_LOADER_H
#define DATA_LOADER_H

#include "common.h"

// 从文件加载数据集
int load_dataset(const char *filename, Dataset *dataset);

// 数据预处理：标准化
void standardize(Dataset *train, Dataset *test);

// 划分训练集和测试集
void split_dataset(const Dataset *dataset, TrainTestSplit *split, double test_ratio);

// 打印数据集信息
void print_dataset_info(const Dataset *dataset);

#endif // DATA_LOADER_H
