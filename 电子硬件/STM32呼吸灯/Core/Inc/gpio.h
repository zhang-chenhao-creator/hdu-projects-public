/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    gpio.h
  * @brief   GPIO initialization and board helpers.
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __GPIO_H__
#define __GPIO_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

void MX_GPIO_Init(void);
void LED_WriteMask(uint8_t led_mask);
uint8_t KEY_ReadPressed(void);

#ifdef __cplusplus
}
#endif

#endif /* __GPIO_H__ */
