#ifndef _DMP_UTILS_H_
#define _DMP_UTILS_H_

#include <stdio.h>
#include <stdint.h>
#include "stm32f4xx_hal.h"

/* Platform layer for STM32 HAL */
#define log_i(format, ...)  do { (void)0; } while (0)
#define log_e(format, ...)  do { (void)0; } while (0)

#define min(a, b)  (((a) < (b)) ? (a) : (b))

#ifdef __cplusplus
extern "C" {
#endif

int i2c_read(unsigned char slave_addr, unsigned char reg_addr,
             unsigned char length, unsigned char *data);
int i2c_write(unsigned char slave_addr, unsigned char reg_addr,
              unsigned char length, unsigned char const *data);
void delay_ms(unsigned long num_ms);
void get_ms(unsigned long *count);

#ifdef __cplusplus
}
#endif

#endif /* _DMP_UTILS_H_ */
