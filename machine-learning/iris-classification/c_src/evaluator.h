#ifndef EVALUATOR_H
#define EVALUATOR_H

#include "common.h"

// 评估模型
void evaluate(const Dataset *test, const int *predictions, EvalResult *result);

// 打印评估结果
void print_eval_result(const EvalResult *result, const char *algorithm_name);

// 导出评估结果到CSV文件
void export_eval_to_csv(const EvalResult *result, const char *algorithm_name, const char *filename);

#endif // EVALUATOR_H
