/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : FreeRTOS tasks for software PWM breathing LEDs.
  ******************************************************************************
  */
/* USER CODE END Header */

#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

#include "gpio.h"
#include <stdio.h>

#define PWM_MAX_DUTY       100U
#define SPEED_MIN          1U
#define SPEED_MAX          5U
#define DEFAULT_SPEED      3U
#define DEFAULT_LED_NUM    8U
#define DIGIT_BLANK        10U
#define SEG_HOLD_MS        2U

volatile uint8_t breathe_enable = 1U;
volatile uint8_t light_on = 1U;
volatile uint8_t breathe_speed = DEFAULT_SPEED;
volatile uint8_t breathe_led_num = DEFAULT_LED_NUM;
volatile uint8_t breathe_duty = 0U;
volatile uint8_t breathe_dir = 0U;

static volatile uint8_t pwm_phase = 0U;
static const uint8_t seg_table[10] = {
  0x3FU, 0x06U, 0x5BU, 0x4FU, 0x66U,
  0x6DU, 0x7DU, 0x07U, 0x7FU, 0x6FU
};
static const uint16_t speed_delay_ms[SPEED_MAX + 1U] = {0U, 35U, 25U, 18U, 12U, 7U};

osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

osThreadId_t taskLEDHandle;
const osThreadAttr_t taskLED_attributes = {
  .name = "StartTaskLED",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

osThreadId_t taskKeyHandle;
const osThreadAttr_t taskKey_attributes = {
  .name = "StartTaskKey",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

osThreadId_t taskSegHandle;
const osThreadAttr_t taskSeg_attributes = {
  .name = "StartTaskSeg",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

static void StartDefaultTask(void *argument);
static void StartTaskLED(void *argument);
static void StartTaskKey(void *argument);
static void StartTaskSeg(void *argument);
static void BreatheStep(void);
static uint8_t BuildLedMask(uint8_t count);
static uint8_t KeyGetPressedDebounced(void);
void HC595_WriteByte(uint8_t data);
void Display_Digit(uint8_t digit_idx, uint8_t num);
void Seg_ShowSpeed(uint8_t speed);

void MX_FREERTOS_Init(void)
{
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);
  taskLEDHandle = osThreadNew(StartTaskLED, NULL, &taskLED_attributes);
  taskKeyHandle = osThreadNew(StartTaskKey, NULL, &taskKey_attributes);
  taskSegHandle = osThreadNew(StartTaskSeg, NULL, &taskSeg_attributes);
}

void SoftwarePWM_Tick(void)
{
  uint8_t duty;
  uint8_t mask = 0U;

  pwm_phase++;
  if (pwm_phase >= PWM_MAX_DUTY)
  {
    pwm_phase = 0U;
  }

  if (light_on != 0U)
  {
    duty = breathe_duty;
    if (duty >= PWM_MAX_DUTY || pwm_phase < duty)
    {
      mask = BuildLedMask(breathe_led_num);
    }
  }

  LED_WriteMask(mask);
}

static void StartDefaultTask(void *argument)
{
  (void)argument;

  for (;;)
  {
    printf("Breathe:%s | Light:%s | Speed:%u | LEDs:%u | Duty:%u\r\n",
           breathe_enable ? "RUN" : "PAUSE",
           light_on ? "ON" : "OFF",
           breathe_speed,
           breathe_led_num,
           breathe_duty);
    osDelay(500U);
  }
}

static void StartTaskLED(void *argument)
{
  (void)argument;

  for (;;)
  {
    if ((light_on != 0U) && (breathe_enable != 0U))
    {
      BreatheStep();
    }

    osDelay(speed_delay_ms[breathe_speed]);
  }
}

static void StartTaskKey(void *argument)
{
  uint8_t key;

  (void)argument;

  for (;;)
  {
    key = KeyGetPressedDebounced();

    if (key == 1U)
    {
      if (light_on != 0U)
      {
        light_on = 0U;
        breathe_enable = 0U;
      }
      else
      {
        light_on = 1U;
        breathe_enable = 0U;
        breathe_duty = PWM_MAX_DUTY;
      }
    }
    else if (key == 2U)
    {
      light_on = 1U;
      breathe_enable = (breathe_enable == 0U) ? 1U : 0U;
    }
    else if (key == 3U)
    {
      breathe_speed++;
      if (breathe_speed > SPEED_MAX)
      {
        breathe_speed = SPEED_MIN;
      }
    }
    else if (key == 4U)
    {
      breathe_led_num++;
      if (breathe_led_num > 8U)
      {
        breathe_led_num = 1U;
      }
    }

    osDelay(10U);
  }
}

static void StartTaskSeg(void *argument)
{
  (void)argument;

  for (;;)
  {
    Seg_ShowSpeed(breathe_speed);
  }
}

static void BreatheStep(void)
{
  if (breathe_dir == 0U)
  {
    if (breathe_duty < PWM_MAX_DUTY)
    {
      breathe_duty++;
    }
    else
    {
      breathe_dir = 1U;
    }
  }
  else
  {
    if (breathe_duty > 0U)
    {
      breathe_duty--;
    }
    else
    {
      breathe_dir = 0U;
    }
  }
}

static uint8_t BuildLedMask(uint8_t count)
{
  if (count >= 8U)
  {
    return 0xFFU;
  }

  if (count == 0U)
  {
    return 0x00U;
  }

  return (uint8_t)((1U << count) - 1U);
}

static uint8_t KeyGetPressedDebounced(void)
{
  uint8_t key = KEY_ReadPressed();

  if (key == 0U)
  {
    return 0U;
  }

  osDelay(20U);
  if (KEY_ReadPressed() != key)
  {
    return 0U;
  }

  while (KEY_ReadPressed() != 0U)
  {
    osDelay(10U);
  }

  return key;
}

void HC595_WriteByte(uint8_t data)
{
  for (uint8_t i = 0U; i < 8U; i++)
  {
    HAL_GPIO_WritePin(SEG_SER_GPIO_Port, SEG_SER_Pin,
                      (data & 0x80U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(SEG_SCLK_GPIO_Port, SEG_SCLK_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(SEG_SCLK_GPIO_Port, SEG_SCLK_Pin, GPIO_PIN_SET);
    data <<= 1U;
  }

  HAL_GPIO_WritePin(SEG_RCLK_GPIO_Port, SEG_RCLK_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(SEG_RCLK_GPIO_Port, SEG_RCLK_Pin, GPIO_PIN_SET);
}

void Display_Digit(uint8_t digit_idx, uint8_t num)
{
  uint8_t seg_data = 0U;

  if (num < 10U)
  {
    seg_data = seg_table[num];
  }

  HAL_GPIO_WritePin(SEG_A3_GPIO_Port, SEG_A3_Pin, GPIO_PIN_RESET);
  HC595_WriteByte(seg_data);
  HAL_GPIO_WritePin(SEG_A0_GPIO_Port, SEG_A0_Pin,
                    (digit_idx & 0x01U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(SEG_A1_GPIO_Port, SEG_A1_Pin,
                    (digit_idx & 0x02U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(SEG_A2_GPIO_Port, SEG_A2_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(SEG_A3_GPIO_Port, SEG_A3_Pin, GPIO_PIN_SET);
}

void Seg_ShowSpeed(uint8_t speed)
{
  Display_Digit(0U, speed);
  osDelay(SEG_HOLD_MS);
  Display_Digit(1U, DIGIT_BLANK);
  osDelay(SEG_HOLD_MS);
  Display_Digit(2U, DIGIT_BLANK);
  osDelay(SEG_HOLD_MS);
  Display_Digit(3U, DIGIT_BLANK);
  osDelay(SEG_HOLD_MS);
}
