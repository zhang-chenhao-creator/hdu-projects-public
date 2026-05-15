/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    gpio.c
  * @brief   GPIO configuration.
  ******************************************************************************
  */
/* USER CODE END Header */

#include "gpio.h"

#define LED_GPIO_MASK (L1_Pin | L2_Pin | L3_Pin | L4_Pin | L5_Pin | L6_Pin | L7_Pin | L8_Pin)

void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();

  HAL_GPIO_WritePin(GPIOE, LED_GPIO_MASK, GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOA, SEG_RCLK_Pin | SEG_SCLK_Pin | SEG_A3_Pin | SEG_A0_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOC, SEG_SER_Pin | SEG_A1_Pin | SEG_A2_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(SEG_DISEN_GPIO_Port, SEG_DISEN_Pin, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin = K1_Pin | K2_Pin | K3_Pin | K4_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = LED_GPIO_MASK;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = SEG_RCLK_Pin | SEG_SCLK_Pin | SEG_A3_Pin | SEG_A0_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = SEG_SER_Pin | SEG_DISEN_Pin | SEG_A1_Pin | SEG_A2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

void LED_WriteMask(uint8_t led_mask)
{
  uint32_t on_bits = ((uint32_t)led_mask << 8U) & LED_GPIO_MASK;
  uint32_t off_bits = LED_GPIO_MASK & ~on_bits;

  GPIOE->BSRR = off_bits | (on_bits << 16U);
}

uint8_t KEY_ReadPressed(void)
{
  if (HAL_GPIO_ReadPin(K1_GPIO_Port, K1_Pin) == GPIO_PIN_RESET) return 1U;
  if (HAL_GPIO_ReadPin(K2_GPIO_Port, K2_Pin) == GPIO_PIN_RESET) return 2U;
  if (HAL_GPIO_ReadPin(K3_GPIO_Port, K3_Pin) == GPIO_PIN_RESET) return 3U;
  if (HAL_GPIO_ReadPin(K4_GPIO_Port, K4_Pin) == GPIO_PIN_RESET) return 4U;

  return 0U;
}
