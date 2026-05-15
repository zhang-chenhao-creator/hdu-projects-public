/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    gpio.c
  * @brief   This file provides code for the configuration
  *          of all used GPIO pins.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "gpio.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/*----------------------------------------------------------------------------*/
/* Configure GPIO                                                             */
/*----------------------------------------------------------------------------*/
/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

/** Configure pins as
        * Analog
        * Input
        * Output
        * EVENT_OUT
        * EXTI
*/
void MX_GPIO_Init(void)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOE, L1_Pin|L2_Pin|L3_Pin|L4_Pin
                          |L5_Pin|L6_Pin|L7_Pin|L8_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10|GPIO_PIN_11|BEEP_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : K2_Pin K3_Pin K4_Pin K1_Pin */
  GPIO_InitStruct.Pin = K2_Pin|K3_Pin|K4_Pin|K1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pins : K5_Pin K6_Pin */
  GPIO_InitStruct.Pin = K5_Pin|K6_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pins : L1_Pin L2_Pin L3_Pin L4_Pin
                           L5_Pin L6_Pin L7_Pin L8_Pin */
  GPIO_InitStruct.Pin = L1_Pin|L2_Pin|L3_Pin|L4_Pin
                          |L5_Pin|L6_Pin|L7_Pin|L8_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pins : PB10 PB11 BEEP_Pin */
  GPIO_InitStruct.Pin = GPIO_PIN_10|GPIO_PIN_11|BEEP_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

}

/* USER CODE BEGIN 2 */

/*------- LED控制函数 -------*/

/**
 * @brief 控制所有LED开关
 * @param state: 1=全亮, 0=全灭
 * @note  LED低电平点亮(共阳), 所以state=1时写RESET
 */
void LED_SetAll(uint8_t state)
{
    uint16_t pins = L1_Pin | L2_Pin | L3_Pin | L4_Pin |
                    L5_Pin | L6_Pin | L7_Pin | L8_Pin;
    HAL_GPIO_WritePin(GPIOE, pins, state ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

/**
 * @brief 按位模式控制8个LED
 * @param pattern: bit0=L1, bit1=L2, ..., bit7=L8, 1=亮 0=灭
 */
void LED_SetPattern(uint8_t pattern)
{
    HAL_GPIO_WritePin(GPIOE, L1_Pin, (pattern & 0x01) ? GPIO_PIN_RESET : GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOE, L2_Pin, (pattern & 0x02) ? GPIO_PIN_RESET : GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOE, L3_Pin, (pattern & 0x04) ? GPIO_PIN_RESET : GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOE, L4_Pin, (pattern & 0x08) ? GPIO_PIN_RESET : GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOE, L5_Pin, (pattern & 0x10) ? GPIO_PIN_RESET : GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOE, L6_Pin, (pattern & 0x20) ? GPIO_PIN_RESET : GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOE, L7_Pin, (pattern & 0x40) ? GPIO_PIN_RESET : GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOE, L8_Pin, (pattern & 0x80) ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

/*------- 按键扫描函数 -------*/

/**
 * @brief 扫描6个按键状态
 * @return 按下的按键Pin掩码 (K1-K4低电平有效, K5-K6高电平有效)
 */
uint16_t KEY_Scan(void)
{
    uint16_t key_val = 0;

    /* K1-K4: 上拉输入, 按下为低电平 */
    if (HAL_GPIO_ReadPin(K1_GPIO_Port, K1_Pin) == GPIO_PIN_RESET) key_val |= K1_Pin;
    if (HAL_GPIO_ReadPin(K2_GPIO_Port, K2_Pin) == GPIO_PIN_RESET) key_val |= K2_Pin;
    if (HAL_GPIO_ReadPin(K3_GPIO_Port, K3_Pin) == GPIO_PIN_RESET) key_val |= K3_Pin;
    if (HAL_GPIO_ReadPin(K4_GPIO_Port, K4_Pin) == GPIO_PIN_RESET) key_val |= K4_Pin;

    /* K5-K6: 下拉输入, 按下为高电平 */
    if (HAL_GPIO_ReadPin(K5_GPIO_Port, K5_Pin) == GPIO_PIN_SET) key_val |= K5_Pin;
    if (HAL_GPIO_ReadPin(K6_GPIO_Port, K6_Pin) == GPIO_PIN_SET) key_val |= K6_Pin;

    return key_val;
}

/*------- 蜂鸣器控制函数 -------*/

/**
 * @brief 微秒级忙等延时 (168MHz主频)
 */
static void BEEP_Delay_us(uint32_t us)
{
    volatile uint32_t count = us * 168 / 5;
    while (count--);
}

/**
 * @brief 驱动蜂鸣器发出指定频率的声音, 持续约100ms
 * @param freq: 频率(Hz), 例如1000
 */
void BEEP_On(uint16_t freq)
{
    uint32_t delay_us = 500000 / freq;           /* 半周期(us) */
    uint32_t cycles   = (uint32_t)freq / 10;     /* 约100ms的周期数 */

    for (uint32_t i = 0; i < cycles; i++) {
        HAL_GPIO_WritePin(BEEP_GPIO_Port, BEEP_Pin, GPIO_PIN_SET);
        BEEP_Delay_us(delay_us);
        HAL_GPIO_WritePin(BEEP_GPIO_Port, BEEP_Pin, GPIO_PIN_RESET);
        BEEP_Delay_us(delay_us);
    }
}

/**
 * @brief 关闭蜂鸣器
 */
void BEEP_Off(void)
{
    HAL_GPIO_WritePin(BEEP_GPIO_Port, BEEP_Pin, GPIO_PIN_RESET);
}

/* USER CODE END 2 */
