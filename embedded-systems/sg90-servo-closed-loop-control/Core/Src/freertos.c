/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "oled.h"
#include "tim.h"
#include "mpu6050_dmp.h"
#include "pid.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define KEY_QUEUE_LEN 4

#define OPEN_LOOP_STEP_US      20U
#define TARGET_YAW_STEP_DEG    2.0f
#define CONTROL_PERIOD_MS      10U
#define CONTROL_PERIOD_OPEN_MS 20U
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
osMessageQueueId_t KeyEventQueueHandle;

volatile uint8_t g_closed_loop = 0;
volatile float g_target_angle = 0.0f;
volatile float g_current_yaw = 0.0f;
volatile float g_pid_output = 0.0f;  /* PID 输出为 us 偏差，范围 [-450, +450] */
volatile uint16_t g_servo_pulse_us = 1500U;

static PID_Controller s_pid;
/* USER CODE END Variables */

/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Definitions for ControlTask */
osThreadId_t ControlTaskHandle;
const osThreadAttr_t ControlTask_attributes = {
  .name = "ControlTask",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityHigh,
};

/* Definitions for GuiTask */
osThreadId_t GuiTaskHandle;
const osThreadAttr_t GuiTask_attributes = {
  .name = "GuiTask",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Definitions for KeyTask */
osThreadId_t KeyTaskHandle;
const osThreadAttr_t KeyTask_attributes = {
  .name = "KeyTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
static void FloatToStr1(float val, char *buf, uint8_t buf_size);
static void KeyTask_ApplyServoFromTarget(void);
/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);
void StartControlTask(void *argument);
void StartGuiTask(void *argument);
void StartKeyTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* USER CODE BEGIN PREPOSTSLEEP */
__weak void PreSleepProcessing(uint32_t ulExpectedIdleTime)
{
}

__weak void PostSleepProcessing(uint32_t ulExpectedIdleTime)
{
}
/* USER CODE END PREPOSTSLEEP */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void)
{
  /* USER CODE BEGIN Init */
  KeyEventQueueHandle = osMessageQueueNew(KEY_QUEUE_LEN, sizeof(uint8_t), NULL);
  if (KeyEventQueueHandle == NULL)
  {
    Error_Handler();
  }
  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);
  if (defaultTaskHandle == NULL) Error_Handler();

  ControlTaskHandle = osThreadNew(StartControlTask, NULL, &ControlTask_attributes);
  if (ControlTaskHandle == NULL) Error_Handler();

  GuiTaskHandle = osThreadNew(StartGuiTask, NULL, &GuiTask_attributes);
  if (GuiTaskHandle == NULL) Error_Handler();

  KeyTaskHandle = osThreadNew(StartKeyTask, NULL, &KeyTask_attributes);
  if (KeyTaskHandle == NULL) Error_Handler();

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  for(;;)
  {
    HAL_GPIO_TogglePin(L1_GPIO_Port, L1_Pin);
    osDelay(500);
  }
  /* USER CODE END StartDefaultTask */
}

/* USER CODE BEGIN Header_StartControlTask */
/**
* @brief Function implementing the ControlTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartControlTask */
void StartControlTask(void *argument)
{
  /* USER CODE BEGIN StartControlTask */
  TickType_t xLastWakeTime = xTaskGetTickCount();
  float yaw;

  PID_Init(&s_pid, 1.4f, 0.04f, 0.18f, -450.0f, 450.0f);
  s_pid.integral_sep_threshold = 5.0f;

  for(;;)
  {
    uint32_t period_ms = g_closed_loop ? CONTROL_PERIOD_MS : CONTROL_PERIOD_OPEN_MS;
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(period_ms));

    if (MPU6050_DMP_GetYaw(&yaw) != 0)
      continue;

    g_current_yaw = yaw;

    if (g_closed_loop)
    {
      float pid_out = PID_Calculate(&s_pid, g_target_angle, yaw);
      g_pid_output = pid_out;
      g_servo_pulse_us = SG90_MapClosedLoopPid(pid_out);
      SG90_SetPulseUs(g_servo_pulse_us);
    }
    else
    {
      g_servo_pulse_us = (uint16_t)(1500.0f + g_pid_output);
      if (g_servo_pulse_us < 1000U) g_servo_pulse_us = 1000U;
      if (g_servo_pulse_us > 2000U) g_servo_pulse_us = 2000U;
      SG90_SetPulseUs(g_servo_pulse_us);
    }
  }
  /* USER CODE END StartControlTask */
}

/* USER CODE BEGIN Header_StartGuiTask */
/**
* @brief Function implementing the GuiTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartGuiTask */
void StartGuiTask(void *argument)
{
  /* USER CODE BEGIN StartGuiTask */
  char buf[32];
  uint32_t tick;
  uint32_t tx_cnt = 0;

  for(;;)
  {
    tick = osKernelGetTickCount();

    OLED_Clear();

    OLED_ShowString(0, 0, g_closed_loop ? "MODE: CLOSED" : "MODE: OPEN  ", 16);

    FloatToStr1(g_target_angle, buf, sizeof(buf));
    OLED_ShowString(0, 2, "TGT:", 16);
    OLED_ShowString(40, 2, buf, 16);

    FloatToStr1(g_current_yaw, buf, sizeof(buf));
    OLED_ShowString(0, 4, "YAW:", 16);
    OLED_ShowString(40, 4, buf, 16);

    snprintf(buf, sizeof(buf), "%04u", (unsigned)g_servo_pulse_us);
    OLED_ShowString(0, 6, "PWM:", 16);
    OLED_ShowString(40, 6, buf, 16);

    snprintf(buf, sizeof(buf), "TX%04lu", tx_cnt);
    OLED_ShowString(80, 6, buf, 16);

    OLED_Refresh();

    /* CSV 格式：TIME_MS,MODE,TARGET,YAW,OUTPUT,PWM,KP,KI,KD
       与 tools/log_serial.py / plot_csv.py 保持一致 */
    printf("%lu,%d,%.2f,%.2f,%.2f,%u,%.2f,%.2f,%.2f\r\n",
           tick,
           g_closed_loop ? 1 : 0,
           g_target_angle,
           g_current_yaw,
           g_pid_output,
           (unsigned)g_servo_pulse_us,
           s_pid.Kp, s_pid.Ki, s_pid.Kd);
    fflush(stdout);
    tx_cnt++;

    osDelay(100);
  }
  /* USER CODE END StartGuiTask */
}

/* USER CODE BEGIN Header_StartKeyTask */
/**
* @brief Function implementing the KeyTask thread.
* @param argument: Not used
* */
/* USER CODE END Header_StartKeyTask */
void StartKeyTask(void *argument)
{
  /* USER CODE BEGIN StartKeyTask */
  uint8_t key_id;
  static uint32_t last_tick = 0;

  for(;;)
  {
    if (osMessageQueueGet(KeyEventQueueHandle, &key_id, NULL, portMAX_DELAY) == osOK)
    {
      uint32_t now = osKernelGetTickCount();
      if ((now - last_tick) < 150)
      {
        continue;
      }
      last_tick = now;

      switch (key_id)
      {
        case 3:
          g_closed_loop = !g_closed_loop;
          PID_Reset(&s_pid);
          if (g_closed_loop)
          {
            SG90_SetPulseUs(1500U);
          }
          break;

        case 1:
          if (g_closed_loop)
          {
            g_target_angle += TARGET_YAW_STEP_DEG;
            if (g_target_angle > 180.0f) g_target_angle = 180.0f;
            PID_Reset(&s_pid);
            KeyTask_ApplyServoFromTarget();
          }
          else
          {
            g_pid_output += (float)OPEN_LOOP_STEP_US;
            if (g_pid_output > 500.0f) g_pid_output = 500.0f;
          }
          break;

        case 4:
          if (g_closed_loop)
          {
            g_target_angle -= TARGET_YAW_STEP_DEG;
            if (g_target_angle < -180.0f) g_target_angle = -180.0f;
            PID_Reset(&s_pid);
            KeyTask_ApplyServoFromTarget();
          }
          else
          {
            g_pid_output -= (float)OPEN_LOOP_STEP_US;
            if (g_pid_output < -500.0f) g_pid_output = -500.0f;
          }
          break;

        default:
          break;
      }
    }
  }
  /* USER CODE END StartKeyTask */
}

/* USER CODE BEGIN Application */
static void KeyTask_ApplyServoFromTarget(void)
{
  float err = g_target_angle - g_current_yaw;
  while (err > 180.0f) err -= 360.0f;
  while (err < -180.0f) err += 360.0f;

  float out = s_pid.Kp * err;
  if (out > s_pid.out_max) out = s_pid.out_max;
  else if (out < s_pid.out_min) out = s_pid.out_min;

  g_pid_output = out;
  g_servo_pulse_us = SG90_MapClosedLoopPid(out);
  SG90_SetPulseUs(g_servo_pulse_us);
}

static void FloatToStr1(float val, char *buf, uint8_t buf_size)
{
  int val_int = (int)(val * 10.0f);
  if (val_int < 0)
  {
    snprintf(buf, buf_size, "-%d.%d", (-val_int) / 10, (-val_int) % 10);
  }
  else
  {
    snprintf(buf, buf_size, "%d.%d", val_int / 10, val_int % 10);
  }
}
/* USER CODE END Application */
