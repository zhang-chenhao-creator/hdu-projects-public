#include "dmp_utils.h"
#include "i2c.h"

extern I2C_HandleTypeDef hi2c2;

int i2c_write(unsigned char slave_addr, unsigned char reg_addr,
              unsigned char length, unsigned char const *data)
{
    HAL_StatusTypeDef status;
    status = HAL_I2C_Mem_Write(&hi2c2, (uint16_t)(slave_addr << 1), reg_addr,
                               I2C_MEMADD_SIZE_8BIT, (uint8_t *)data, length, 100);
    return (status == HAL_OK) ? 0 : -1;
}

int i2c_read(unsigned char slave_addr, unsigned char reg_addr,
             unsigned char length, unsigned char *data)
{
    HAL_StatusTypeDef status;
    status = HAL_I2C_Mem_Read(&hi2c2, (uint16_t)(slave_addr << 1), reg_addr,
                              I2C_MEMADD_SIZE_8BIT, data, length, 100);
    return (status == HAL_OK) ? 0 : -1;
}

void delay_ms(unsigned long num_ms)
{
    HAL_Delay((uint32_t)num_ms);
}

void get_ms(unsigned long *count)
{
    *count = (unsigned long)HAL_GetTick();
}
