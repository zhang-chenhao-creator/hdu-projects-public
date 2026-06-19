#include "pid.h"

static float pid_wrap_error(float error)
{
    while (error > 180.0f)
    {
        error -= 360.0f;
    }
    while (error < -180.0f)
    {
        error += 360.0f;
    }
    return error;
}

void PID_Init(PID_Controller *pid, float kp, float ki, float kd, float out_min, float out_max)
{
    pid->Kp = kp;
    pid->Ki = ki;
    pid->Kd = kd;
    pid->integral = 0.0f;
    pid->prev_error = 0.0f;
    pid->prev_measured = 0.0f;
    pid->prev_output = 0.0f;
    pid->out_min = out_min;
    pid->out_max = out_max;
    pid->integral_sep_threshold = 5.0f;
}

void PID_Reset(PID_Controller *pid)
{
    pid->integral = 0.0f;
    pid->prev_error = 0.0f;
    pid->prev_measured = 0.0f;
    pid->prev_output = 0.0f;
}

float PID_Calculate(PID_Controller *pid, float setpoint, float measured)
{
    float error = pid_wrap_error(setpoint - measured);
    float p_term;
    float i_term;
    float d_term;
    float output;

#if PID_USE_INCREMENTAL
    p_term = pid->Kp * (error - pid->prev_error);
    i_term = pid->Ki * error;
#if PID_USE_D_ON_MEAS
    d_term = -pid->Kd * (measured - pid->prev_measured);
#else
    d_term = pid->Kd * (error - 2.0f * pid->prev_error + pid->prev_measured);
#endif
    output = pid->prev_output + p_term + i_term + d_term;
#else
    p_term = pid->Kp * error;

#if PID_USE_INTEGRAL_SEP
    if (error > -pid->integral_sep_threshold && error < pid->integral_sep_threshold)
    {
        pid->integral += error;
    }
#else
    pid->integral += error;
#endif

    if (pid->integral > 500.0f)
    {
        pid->integral = 500.0f;
    }
    else if (pid->integral < -500.0f)
    {
        pid->integral = -500.0f;
    }
    i_term = pid->Ki * pid->integral;

#if PID_USE_D_ON_MEAS
    d_term = -pid->Kd * (measured - pid->prev_measured);
#else
    d_term = pid->Kd * (error - pid->prev_error);
#endif

    output = p_term + i_term + d_term;
#endif

    if (output > pid->out_max)
    {
        output = pid->out_max;
    }
    else if (output < pid->out_min)
    {
        output = pid->out_min;
    }

    pid->prev_error = error;
    pid->prev_measured = measured;
    pid->prev_output = output;
    return output;
}
