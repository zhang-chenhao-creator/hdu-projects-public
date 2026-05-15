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
#include "adc.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* 运行模式 */
#define APP_MODE_RUN              0U
#define APP_MODE_SET              1U

/* 声控与阈值参数 */
#define MIC_THRESHOLD_DEFAULT     2000U
#define MIC_THRESHOLD_STEP        50U
#define MIC_THRESHOLD_MIN         0U
#define MIC_THRESHOLD_MAX         4095U

/* 10 s 倒计时，任务以 100 ms 为节拍 */
#define COUNTDOWN_SECONDS         10U
#define COUNTDOWN_TICKS_PER_SEC   10U

/* 按键与显示节拍 */
#define KEY_DEBOUNCE_MS           20U
#define TASK_LOOP_MS              20U
#define LED_TASK_PERIOD_MS        100U
#define DISPLAY_DIGIT_HOLD_MS     2U

/* 10 以上用作空白 */
#define DIGIT_BLANK               10U
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
volatile uint8_t current_mode = APP_MODE_RUN;
volatile uint16_t sound_threshold = MIC_THRESHOLD_DEFAULT;
volatile uint16_t countdown_ticks = 0U;
volatile uint8_t led_latched_on = 0U;

/* 数码管段码表，共阴极，0-9 */
static const uint8_t seg_table[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
/* USER CODE END Variables */
osThreadId defaultTaskHandle;
osThreadId ledTaskHandle;
osThreadId dispTaskHandle;

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
static uint16_t ReadMicSample(void);
static uint8_t DebouncedKeyPressed(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState pressed_state);
void HC595_WriteByte(uint8_t data);
void Display_Digit(uint8_t digit_idx, uint8_t num);
void Control_LEDs(uint8_t state);
/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void const * argument);
void StartTask02(void const * argument);
void StartTask03(void const * argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* GetIdleTaskMemory prototype (linked to static allocation support) */
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize );

/* USER CODE BEGIN GET_IDLE_TASK_MEMORY */
static StaticTask_t xIdleTaskTCBBuffer;
static StackType_t xIdleStack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize )
{
  *ppxIdleTaskTCBBuffer = &xIdleTaskTCBBuffer;
  *ppxIdleTaskStackBuffer = &xIdleStack[0];
  *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
  /* place for user code */
}
/* USER CODE END GET_IDLE_TASK_MEMORY */

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

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* definition and creation of defaultTask */
  osThreadDef(defaultTask, StartDefaultTask, osPriorityNormal, 0, 128);
  defaultTaskHandle = osThreadCreate(osThread(defaultTask), NULL);

  /* definition and creation of ledTask */
  osThreadDef(ledTask, StartTask02, osPriorityNormal, 0, 128);
  ledTaskHandle = osThreadCreate(osThread(ledTask), NULL);

  /* definition and creation of dispTask */
  osThreadDef(dispTask, StartTask03, osPriorityNormal, 0, 128);
  dispTaskHandle = osThreadCreate(osThread(dispTask), NULL);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void const * argument)
{
  /* USER CODE BEGIN StartDefaultTask */
    /* 使能数码管显示，DISEN 低电平有效 */
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_9, GPIO_PIN_RESET);
    Control_LEDs(0U);

  /* Infinite loop */
  for(;;)
  {
        if (DebouncedKeyPressed(GPIOE, GPIO_PIN_1, GPIO_PIN_RESET) != 0U)
        {
            current_mode = (current_mode == APP_MODE_RUN) ? APP_MODE_SET : APP_MODE_RUN;
            led_latched_on = 0U;
            countdown_ticks = 0U;
            Control_LEDs(0U);
        }

        if (current_mode == APP_MODE_SET)
        {
            if (DebouncedKeyPressed(GPIOE, GPIO_PIN_2, GPIO_PIN_RESET) != 0U)
            {
                if (sound_threshold <= (MIC_THRESHOLD_MAX - MIC_THRESHOLD_STEP))
                {
                    sound_threshold += MIC_THRESHOLD_STEP;
                }
                else
                {
                    sound_threshold = MIC_THRESHOLD_MAX;
                }
            }

            if (DebouncedKeyPressed(GPIOE, GPIO_PIN_3, GPIO_PIN_RESET) != 0U)
            {
                if (sound_threshold >= MIC_THRESHOLD_STEP)
                {
                    sound_threshold -= MIC_THRESHOLD_STEP;
                }
                else
                {
                    sound_threshold = MIC_THRESHOLD_MIN;
                }
            }
        }
        else
        {
            uint16_t adc_sample = ReadMicSample();

            if (adc_sample > sound_threshold)
            {
                led_latched_on = 1U;
                countdown_ticks = COUNTDOWN_SECONDS * COUNTDOWN_TICKS_PER_SEC;
            }
        }

        osDelay(TASK_LOOP_MS);
  }
  /* USER CODE END StartDefaultTask */
}

/* USER CODE BEGIN Header_StartTask02 */
/**
* @brief Function implementing the ledTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTask02 */
void StartTask02(void const * argument)
{
  /* USER CODE BEGIN StartTask02 */
  /* Infinite loop */
  for(;;)
  {
        if (current_mode == APP_MODE_SET)
        {
            led_latched_on = 0U;
            countdown_ticks = 0U;
            Control_LEDs(0U);
        }
        else
        {
            if (led_latched_on != 0U)
            {
                Control_LEDs(1U);

                if (countdown_ticks > 0U)
                {
                    countdown_ticks--;

                    if (countdown_ticks == 0U)
                    {
                        led_latched_on = 0U;
                        Control_LEDs(0U);
                    }
                }
            }
            else
            {
                Control_LEDs(0U);
            }
        }

        osDelay(LED_TASK_PERIOD_MS);
  }
  /* USER CODE END StartTask02 */
}

/* USER CODE BEGIN Header_StartTask03 */
/**
* @brief Function implementing the dispTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTask03 */
void StartTask03(void const * argument)
{
  /* USER CODE BEGIN StartTask03 */
  /* Infinite loop */
  for(;;)
  {
        if (current_mode == APP_MODE_SET)
        {
                uint16_t threshold_value = sound_threshold;

                Display_Digit(0U, (uint8_t)((threshold_value / 1000U) % 10U));
                osDelay(DISPLAY_DIGIT_HOLD_MS);

                Display_Digit(1U, (uint8_t)((threshold_value / 100U) % 10U));
                osDelay(DISPLAY_DIGIT_HOLD_MS);

                Display_Digit(2U, (uint8_t)((threshold_value / 10U) % 10U));
                osDelay(DISPLAY_DIGIT_HOLD_MS);

                Display_Digit(3U, (uint8_t)(threshold_value % 10U));
                osDelay(DISPLAY_DIGIT_HOLD_MS);
        }
        else
        {
            uint16_t remaining_seconds = 0U;

            if ((led_latched_on != 0U) || (countdown_ticks > 0U))
            {
                remaining_seconds = (countdown_ticks + (COUNTDOWN_TICKS_PER_SEC - 1U)) / COUNTDOWN_TICKS_PER_SEC;

                if (remaining_seconds > COUNTDOWN_SECONDS)
                {
                    remaining_seconds = COUNTDOWN_SECONDS;
                }

                Display_Digit(0U, (uint8_t)((remaining_seconds / 10U) % 10U));
                osDelay(DISPLAY_DIGIT_HOLD_MS);
                Display_Digit(1U, (uint8_t)(remaining_seconds % 10U));
                osDelay(DISPLAY_DIGIT_HOLD_MS);
                Display_Digit(2U, DIGIT_BLANK);
                osDelay(DISPLAY_DIGIT_HOLD_MS);
                Display_Digit(3U, DIGIT_BLANK);
                osDelay(DISPLAY_DIGIT_HOLD_MS);
            }
            else
            {
                Display_Digit(0U, DIGIT_BLANK);
                osDelay(DISPLAY_DIGIT_HOLD_MS);
                Display_Digit(1U, DIGIT_BLANK);
                osDelay(DISPLAY_DIGIT_HOLD_MS);
                Display_Digit(2U, DIGIT_BLANK);
                osDelay(DISPLAY_DIGIT_HOLD_MS);
                Display_Digit(3U, DIGIT_BLANK);
                osDelay(DISPLAY_DIGIT_HOLD_MS);
            }
        }

        osDelay(1U);
  }
  /* USER CODE END StartTask03 */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
static uint16_t ReadMicSample(void)
{
    uint16_t sample = 0U;

    if (HAL_ADC_Start(&hadc1) == HAL_OK)
    {
        if (HAL_ADC_PollForConversion(&hadc1, 5U) == HAL_OK)
        {
            sample = (uint16_t)HAL_ADC_GetValue(&hadc1);
        }

        (void)HAL_ADC_Stop(&hadc1);
    }

    return sample;
}

static uint8_t DebouncedKeyPressed(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState pressed_state)
{
    if (HAL_GPIO_ReadPin(port, pin) != pressed_state)
    {
        return 0U;
    }

    osDelay(KEY_DEBOUNCE_MS);

    if (HAL_GPIO_ReadPin(port, pin) != pressed_state)
    {
        return 0U;
    }

    while (HAL_GPIO_ReadPin(port, pin) == pressed_state)
    {
        osDelay(10U);
    }

    return 1U;
}

/**
 * @brief 74HC595 串行移位输出函数
 */
void HC595_WriteByte(uint8_t data) {
    for(int i = 0; i < 8; i++) {
        // SER 写数据位
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_8, (data & 0x80) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        // SCK 产生上升沿移位
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_11, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_11, GPIO_PIN_SET);
        data <<= 1;
    }
    // DISLK (RCLK) 产生上升沿锁存输出
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_SET);
}

/**
 * @brief 数码管单选与显示函数 (带消隐防残影)
 */
void Display_Digit(uint8_t digit_idx, uint8_t num) {
    uint8_t seg_data = 0x00U;

    if (num < 10U)
    {
        seg_data = seg_table[num];
    }

    /* A3 是数码管总使能脚，先关显示，再更新段码与位选，最后开显示 */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_RESET);

    /* 先锁存段码，再切换位选 */
    HC595_WriteByte(seg_data);

    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, (digit_idx & 0x01U) ? GPIO_PIN_SET : GPIO_PIN_RESET); /* A0 */
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_10, (digit_idx & 0x02U) ? GPIO_PIN_SET : GPIO_PIN_RESET); /* A1 */
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_11, GPIO_PIN_RESET);                                        /* A2 固定为 0 */

    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_SET);
}

/**
 * @brief 控制 8 个 LED 的亮灭
 */
void Control_LEDs(uint8_t state) {
    // 学习板上 L1-L8 对应 PE8-PE15，低电平点亮
    HAL_GPIO_WritePin(GPIOE,
                      GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 |
                      GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15,
                      state ? GPIO_PIN_RESET : GPIO_PIN_SET);
}
/* USER CODE END Application */