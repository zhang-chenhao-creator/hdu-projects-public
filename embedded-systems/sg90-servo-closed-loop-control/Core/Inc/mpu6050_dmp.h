#ifndef __MPU6050_DMP_H__
#define __MPU6050_DMP_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

extern volatile float g_mpu_raw_yaw;
extern volatile uint8_t g_mpu_offset_ready;

int MPU6050_DMP_Init(void);
int MPU6050_DMP_InitWithRetry(int retries);
int MPU6050_DMP_GetYaw(float *yaw);
void MPU6050_DMP_CalibrateZero(void);
void MPU6050_DMP_DrainFifo(void);

#ifdef __cplusplus
}
#endif

#endif /* __MPU6050_DMP_H__ */
