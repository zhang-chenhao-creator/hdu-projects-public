#ifndef __PID_H__
#define __PID_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* 0=位置式(默认)  1=增量式(扩展实验) */
#ifndef PID_USE_INCREMENTAL
#define PID_USE_INCREMENTAL  0
#endif

/* 1=积分分离  1=微分先行(对测量值微分) */
#ifndef PID_USE_INTEGRAL_SEP
#define PID_USE_INTEGRAL_SEP 1
#endif

#ifndef PID_USE_D_ON_MEAS
#define PID_USE_D_ON_MEAS    1
#endif

typedef struct {
    float Kp;
    float Ki;
    float Kd;

    float integral;
    float prev_error;
    float prev_measured;
    float prev_output;

    float out_min;
    float out_max;
    float integral_sep_threshold;
} PID_Controller;

void PID_Init(PID_Controller *pid, float kp, float ki, float kd, float out_min, float out_max);
void PID_Reset(PID_Controller *pid);
float PID_Calculate(PID_Controller *pid, float setpoint, float measured);

#ifdef __cplusplus
}
#endif

#endif /* __PID_H__ */
