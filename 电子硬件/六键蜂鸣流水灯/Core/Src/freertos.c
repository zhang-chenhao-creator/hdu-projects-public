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
  * This software is licensed under terms that can be found in the LICENSE file
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
#include "gpio.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
volatile uint8_t g_press_keysta = 0;   /* 当前按键状态 */
volatile uint8_t led_run   = 1;        /* 流水灯运行标志: 1=运行, 0=暂停 */
volatile uint8_t led_speed = 1;        /* 速度档位: 1-4 */
osThreadId_t TaskLED2Handle = NULL;    /* 双向流水灯任务句柄 */
/* USER CODE END Variables */
/* Definitions for TaskBeep1 */
osThreadId_t TaskBeep1Handle;
const osThreadAttr_t TaskBeep1_attributes = {
  .name = "TaskBeep1",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for TaskLED1 */
osThreadId_t TaskLED1Handle;
const osThreadAttr_t TaskLED1_attributes = {
  .name = "TaskLED1",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for TaskKey1 */
osThreadId_t TaskKey1Handle;
const osThreadAttr_t TaskKey1_attributes = {
  .name = "TaskKey1",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityHigh,
};
/* Definitions for BEEPQueue */
osMessageQueueId_t BEEPQueueHandle;
const osMessageQueueAttr_t BEEPQueue_attributes = {
  .name = "BEEPQueue"
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
void StartLed2Task(void *argument);
/* USER CODE END FunctionPrototypes */

void TaskBeep(void *argument);
void TaskLED(void *argument);
void TaskKey(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

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

  /* Create the queue(s) */
  /* creation of BEEPQueue */
  BEEPQueueHandle = osMessageQueueNew (8, 1, &BEEPQueue_attributes);

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of TaskBeep1 */
  TaskBeep1Handle = osThreadNew(TaskBeep, NULL, &TaskBeep1_attributes);

  /* creation of TaskLED1 */
  TaskLED1Handle = osThreadNew(TaskLED, NULL, &TaskLED1_attributes);

  /* creation of TaskKey1 */
  TaskKey1Handle = osThreadNew(TaskKey, NULL, &TaskKey1_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_TaskBeep */
/**
  * @brief  蜂鸣器任务: 等待按键消息队列, 按不同按键发出不同频率提示音
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_TaskBeep */
void TaskBeep(void *argument)
{
  /* USER CODE BEGIN TaskBeep */
  uint8_t key_msg;
  uint16_t freq;

  for (;;)
  {
    /* 阻塞等待按键消息 */
    if (osMessageQueueGet(BEEPQueueHandle, &key_msg, NULL, osWaitForever) == osOK)
    {
      /* 根据按键Pin值选择蜂鸣器频率 */
      if      (key_msg == (uint8_t)K1_Pin) freq = BEEP_FREQ_K1;
      else if (key_msg == (uint8_t)K2_Pin) freq = BEEP_FREQ_K2;
      else if (key_msg == (uint8_t)K3_Pin) freq = BEEP_FREQ_K3;
      else if (key_msg == (uint8_t)K4_Pin) freq = BEEP_FREQ_K4;
      else                                  freq = BEEP_FREQ_K2;

      BEEP_On(freq);
      BEEP_Off();
    }
  }
  /* USER CODE END TaskBeep */
}

/* USER CODE BEGIN Header_TaskLED */
/**
  * @brief  流水灯任务: L1->L8循环, 受led_run和led_speed控制
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_TaskLED */
void TaskLED(void *argument)
{
  /* USER CODE BEGIN TaskLED */
  uint8_t led_pattern = 0x01;
  uint32_t delay_ms;

  for (;;)
  {
    if (led_run)
    {
      LED_SetPattern(led_pattern);

      /* speed=1:500ms, speed=2:250ms, speed=3:167ms, speed=4:125ms */
      delay_ms = 500 / led_speed;
      osDelay(delay_ms);

      led_pattern <<= 1;
      if (led_pattern == 0)
        led_pattern = 0x01;
    }
    else
    {
      /* 暂停状态, 短暂延时释放CPU */
      osDelay(10);
    }
  }
  /* USER CODE END TaskLED */
}

/* USER CODE BEGIN Header_TaskKey */
/**
  * @brief  按键扫描任务(最高优先级):
  *         K1=启动(恢复), K2=暂停(挂起), K3=加速, K4=减速
  *         K5=切换双向流水灯, K6=恢复原流水灯
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_TaskKey */
void TaskKey(void *argument)
{
  /* USER CODE BEGIN TaskKey */
  uint16_t key_val, key_last = 0;
  uint8_t  led_suspended = 0;
  uint8_t  key_msg;

  for (;;)
  {
    key_val = KEY_Scan();

    if (key_val != 0 && key_last == 0)
    {
      /* 消抖延时 */
      osDelay(20);

      /* 二次确认按键有效 */
      if (KEY_Scan() == key_val)
      {
        g_press_keysta = (uint8_t)key_val;

        /*--- K1: 启动(恢复流水灯任务) ---*/
        if ((key_val & K1_Pin) && led_suspended)
        {
          osThreadResume(TaskLED1Handle);
          led_suspended = 0;
        }

        /*--- K2: 暂停(挂起流水灯任务, 关闭所有LED) ---*/
        if ((key_val & K2_Pin) && !led_suspended)
        {
          osThreadSuspend(TaskLED1Handle);
          LED_SetAll(0);
          led_suspended = 1;
        }

        /*--- K3: 加速(最高档4) ---*/
        if (key_val & K3_Pin)
        {
          if (led_speed < 4) led_speed++;
        }

        /*--- K4: 减速(最低档1) ---*/
        if (key_val & K4_Pin)
        {
          if (led_speed > 1) led_speed--;
        }

        /*--- K5: 暂停原流水灯, 动态创建双向流水灯任务 ---*/
        if ((key_val & K5_Pin) && TaskLED2Handle == NULL)
        {
          osThreadSuspend(TaskLED1Handle);
          LED_SetAll(0);
          led_suspended = 1;

          const osThreadAttr_t LED2_attr = {
            .name = "TaskLED2",
            .stack_size = 256 * 4,
            .priority = (osPriority_t) osPriorityNormal
          };
          TaskLED2Handle = osThreadNew(StartLed2Task, NULL, &LED2_attr);
        }

        /*--- K6: 删除双向流水灯任务, 恢复原流水灯 ---*/
        if ((key_val & K6_Pin) && TaskLED2Handle != NULL)
        {
          osThreadTerminate(TaskLED2Handle);
          TaskLED2Handle = NULL;
          LED_SetAll(0);
          osThreadResume(TaskLED1Handle);
          led_suspended = 0;
        }

        /*--- 发送按键消息给蜂鸣器任务 ---*/
        key_msg = (uint8_t)key_val;
        osMessageQueuePut(BEEPQueueHandle, &key_msg, 0, 0);

        /*--- 等待按键释放 ---*/
        while (KEY_Scan() != 0)
          osDelay(10);
      }
    }

    key_last = key_val;
    osDelay(10);
  }
  /* USER CODE END TaskKey */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/**
  * @brief  双向流水灯任务: 两端LED向中间汇聚再向两端扩散
  * @param  argument: Not used
  * @retval None
  */
void StartLed2Task(void *argument)
{
  uint8_t led_pattern = 0x81;   /* 初始: 两端LED亮 (L1 + L8) */
  int8_t  dir = 1;              /* 1=向中间汇聚, -1=向两端扩散 */
  uint32_t delay_ms;

  for (;;)
  {
    LED_SetPattern(led_pattern);

    delay_ms = 500 / led_speed;
    osDelay(delay_ms);

    /* 高4位和低4位分别移动 */
    if (dir > 0)
    {
      led_pattern = ((led_pattern & 0xF0) >> 1) | ((led_pattern & 0x0F) << 1);
    }
    else
    {
      led_pattern = ((led_pattern & 0xF0) << 1) | ((led_pattern & 0x0F) >> 1);
    }

    /* 到达边界则反向 */
    if (led_pattern == 0x18 || led_pattern == 0x81)
      dir = -dir;
  }
}

/* USER CODE END Application */
