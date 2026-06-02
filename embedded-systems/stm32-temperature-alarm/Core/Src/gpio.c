#include "gpio.h"

#define BEEP_PWM_FREQ_HZ 2000U
#define BEEP_PWM_COUNTER_HZ 1000000U

static TIM_HandleTypeDef htim3_beep;
static uint8_t beep_pwm_ready = 0U;

static void BEEP_GPIO_OutputInit(void);
static void BEEP_PWM_Init(void);

void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();

  HAL_GPIO_WritePin(W25Q_CS_GPIO_Port, W25Q_CS_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(DS18B20_GPIO_Port, DS18B20_Pin, GPIO_PIN_SET);

  GPIO_InitStruct.Pin = W25Q_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(W25Q_CS_GPIO_Port, &GPIO_InitStruct);

  BEEP_GPIO_OutputInit();
  BEEP_PWM_Init();

  GPIO_InitStruct.Pin = DS18B20_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(DS18B20_GPIO_Port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = K1_Pin | K2_Pin | K3_Pin | K4_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
}

uint16_t KEY_Scan(void)
{
  uint16_t key = 0;

  if (HAL_GPIO_ReadPin(K1_GPIO_Port, K1_Pin) == GPIO_PIN_RESET) key |= K1_Pin;
  if (HAL_GPIO_ReadPin(K2_GPIO_Port, K2_Pin) == GPIO_PIN_RESET) key |= K2_Pin;
  if (HAL_GPIO_ReadPin(K3_GPIO_Port, K3_Pin) == GPIO_PIN_RESET) key |= K3_Pin;
  if (HAL_GPIO_ReadPin(K4_GPIO_Port, K4_Pin) == GPIO_PIN_RESET) key |= K4_Pin;

  return key;
}

void BEEP_Set(uint8_t on)
{
  if (beep_pwm_ready != 0U)
  {
    __HAL_TIM_SET_COMPARE(&htim3_beep,
                          TIM_CHANNEL_1,
                          on ? ((htim3_beep.Init.Period + 1U) / 2U) : 0U);
  }
  else
  {
    HAL_GPIO_WritePin(BEEP_GPIO_Port, BEEP_Pin, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
  }
}

static void BEEP_PWM_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  RCC_ClkInitTypeDef clkconfig = {0};
  uint32_t flash_latency;
  uint32_t tim_clock;
  uint32_t prescaler;

  __HAL_RCC_TIM3_CLK_ENABLE();

  GPIO_InitStruct.Pin = BEEP_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF2_TIM3;
  HAL_GPIO_Init(BEEP_GPIO_Port, &GPIO_InitStruct);

  HAL_RCC_GetClockConfig(&clkconfig, &flash_latency);
  tim_clock = HAL_RCC_GetPCLK1Freq();
  if (clkconfig.APB1CLKDivider != RCC_HCLK_DIV1)
  {
    tim_clock *= 2U;
  }

  prescaler = (tim_clock / BEEP_PWM_COUNTER_HZ) - 1U;

  htim3_beep.Instance = TIM3;
  htim3_beep.Init.Prescaler = prescaler;
  htim3_beep.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3_beep.Init.Period = (BEEP_PWM_COUNTER_HZ / BEEP_PWM_FREQ_HZ) - 1U;
  htim3_beep.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3_beep.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim3_beep) != HAL_OK)
  {
    beep_pwm_ready = 0U;
    BEEP_GPIO_OutputInit();
    return;
  }

  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0U;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim3_beep, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    beep_pwm_ready = 0U;
    BEEP_GPIO_OutputInit();
    return;
  }

  if (HAL_TIM_PWM_Start(&htim3_beep, TIM_CHANNEL_1) == HAL_OK)
  {
    beep_pwm_ready = 1U;
  }
}

static void BEEP_GPIO_OutputInit(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  GPIO_InitStruct.Pin = BEEP_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(BEEP_GPIO_Port, &GPIO_InitStruct);
  HAL_GPIO_WritePin(BEEP_GPIO_Port, BEEP_Pin, GPIO_PIN_RESET);
}
