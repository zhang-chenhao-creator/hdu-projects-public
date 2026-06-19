#include "mpu6050_dmp.h"
#include "inv_mpu.h"
#include "inv_mpu_dmp_motion_driver.h"
#include "math.h"
#include "main.h"

/* 默认朝向矩阵：与 Experiment8 保持一致，陀螺仪绕 Z 轴旋转 180° */
static const unsigned short default_orientation = 0xAC;

/* 上电零点偏移：DMP 初始化后把当前 yaw 记为 0° */
static float s_yaw_offset = 0.0f;
static uint8_t s_offset_ready = 0;

volatile float g_mpu_raw_yaw = 0.0f;
volatile uint8_t g_mpu_offset_ready = 0;

/* 前向声明 */
static int MPU6050_DMP_GetRawYaw(float *yaw);

int MPU6050_DMP_Init(void)
{
    struct int_param_s int_param = {0};

    if (mpu_init(&int_param) != 0)
        return -1;

    if (mpu_set_sensors(INV_XYZ_GYRO | INV_XYZ_ACCEL) != 0)
        return -2;

    if (mpu_configure_fifo(INV_XYZ_GYRO | INV_XYZ_ACCEL) != 0)
        return -3;

    if (mpu_set_sample_rate(50) != 0)
        return -4;

    if (dmp_load_motion_driver_firmware() != 0)
        return -5;

    if (dmp_set_fifo_rate(50) != 0)
        return -6;

    if (dmp_set_orientation(default_orientation) != 0)
        return -7;

    if (dmp_enable_feature(DMP_FEATURE_6X_LP_QUAT | DMP_FEATURE_GYRO_CAL |
                           DMP_FEATURE_SEND_RAW_ACCEL |
                           DMP_FEATURE_SEND_CAL_GYRO) != 0)
        return -8;

    if (mpu_set_dmp_state(1) != 0)
        return -9;

    return 0;
}

/* 获取未经零点偏移补偿的原始 yaw */
static int MPU6050_DMP_GetRawYaw(float *yaw)
{
    short gyro[3], accel[3];
    long quat[4];
    unsigned long timestamp;
    short sensors;
    unsigned char more;
    float q0, q1, q2, q3;

    if (dmp_read_fifo(gyro, accel, quat, &timestamp, &sensors, &more) != 0)
        return -1;

    if (!(sensors & INV_WXYZ_QUAT))
        return -2;

    /* DMP 输出为定点数，比例 2^30 */
    q0 = (float)quat[0] / 1073741824.0f;
    q1 = (float)quat[1] / 1073741824.0f;
    q2 = (float)quat[2] / 1073741824.0f;
    q3 = (float)quat[3] / 1073741824.0f;

    /* 从四元数提取 yaw（Z 轴旋转角） */
    *yaw = atan2f(2.0f * (q1 * q2 + q0 * q3),
                  q0 * q0 + q1 * q1 - q2 * q2 - q3 * q3) * 180.0f / (float)M_PI;

    return 0;
}


/* 获取以初始位置为 0° 的相对 yaw */
int MPU6050_DMP_GetYaw(float *yaw)
{
    float raw_yaw;
    int ret = MPU6050_DMP_GetRawYaw(&raw_yaw);
    if (ret != 0)
        return ret;

    g_mpu_raw_yaw = raw_yaw;
    g_mpu_offset_ready = s_offset_ready;

    if (s_offset_ready)
    {
        float rel = raw_yaw - s_yaw_offset;
        /* 规范化到 [-180, 180] */
        while (rel > 180.0f) rel -= 360.0f;
        while (rel < -180.0f) rel += 360.0f;
        *yaw = rel;
    }
    else
    {
        *yaw = raw_yaw;
    }

    return 0;
}

/* 带重试的 DMP 初始化，缓解上电/接触不稳定导致的初始化失败 */
int MPU6050_DMP_InitWithRetry(int retries)
{
    int ret;
    for (int i = 0; i < retries; i++)
    {
        ret = MPU6050_DMP_Init();
        if (ret == 0)
            return 0;
        HAL_Delay(50);
    }
    return ret;
}

/* 上电零点校准：在传感器静止时采集多帧 yaw，取平均作为 0° 基准 */
void MPU6050_DMP_CalibrateZero(void)
{
    float sum = 0.0f;
    float yaw;
    int valid = 0;

    for (int i = 0; i < 50; i++)
    {
        if (MPU6050_DMP_GetRawYaw(&yaw) == 0)
        {
            sum += yaw;
            valid++;
        }
        HAL_Delay(10);
    }

    if (valid > 0)
    {
        s_yaw_offset = sum / valid;
        s_offset_ready = 1;
    }
}

/* 清空 FIFO 中累积的包，确保读取到最新一帧 */
void MPU6050_DMP_DrainFifo(void)
{
    short gyro[3], accel[3];
    long quat[4];
    unsigned long timestamp;
    short sensors;
    unsigned char more;

    do
    {
        if (dmp_read_fifo(gyro, accel, quat, &timestamp, &sensors, &more) != 0)
            break;
    } while (more);
}
