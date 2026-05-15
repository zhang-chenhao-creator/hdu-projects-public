/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

void Error_Handler(void);
void SoftwarePWM_Tick(void);
void HC595_WriteByte(uint8_t data);
void Display_Digit(uint8_t digit_idx, uint8_t num);
void Seg_ShowSpeed(uint8_t speed);

#define K1_Pin GPIO_PIN_1
#define K1_GPIO_Port GPIOE
#define K2_Pin GPIO_PIN_2
#define K2_GPIO_Port GPIOE
#define K3_Pin GPIO_PIN_3
#define K3_GPIO_Port GPIOE
#define K4_Pin GPIO_PIN_4
#define K4_GPIO_Port GPIOE

#define L1_Pin GPIO_PIN_8
#define L1_GPIO_Port GPIOE
#define L2_Pin GPIO_PIN_9
#define L2_GPIO_Port GPIOE
#define L3_Pin GPIO_PIN_10
#define L3_GPIO_Port GPIOE
#define L4_Pin GPIO_PIN_11
#define L4_GPIO_Port GPIOE
#define L5_Pin GPIO_PIN_12
#define L5_GPIO_Port GPIOE
#define L6_Pin GPIO_PIN_13
#define L6_GPIO_Port GPIOE
#define L7_Pin GPIO_PIN_14
#define L7_GPIO_Port GPIOE
#define L8_Pin GPIO_PIN_15
#define L8_GPIO_Port GPIOE

#define SEG_SER_Pin GPIO_PIN_8
#define SEG_SER_GPIO_Port GPIOC
#define SEG_DISEN_Pin GPIO_PIN_9
#define SEG_DISEN_GPIO_Port GPIOC
#define SEG_SCLK_Pin GPIO_PIN_11
#define SEG_SCLK_GPIO_Port GPIOA
#define SEG_RCLK_Pin GPIO_PIN_8
#define SEG_RCLK_GPIO_Port GPIOA
#define SEG_A0_Pin GPIO_PIN_15
#define SEG_A0_GPIO_Port GPIOA
#define SEG_A1_Pin GPIO_PIN_10
#define SEG_A1_GPIO_Port GPIOC
#define SEG_A2_Pin GPIO_PIN_11
#define SEG_A2_GPIO_Port GPIOC
#define SEG_A3_Pin GPIO_PIN_12
#define SEG_A3_GPIO_Port GPIOA

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
