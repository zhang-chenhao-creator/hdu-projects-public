/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Low power RTC wakeup and data acquisition demo
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
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define AUDIO_SAMPLE_COUNT 128U
#define RTC_BKP_MAGIC 0x5050U
#define DS18B20_ERROR_TEMP (-1999.0f)
#define DS18B20_ERR_BUS_LOW (-1001.0f)
#define DS18B20_ERR_NO_PRESENCE (-1002.0f)
#define DS18B20_ERR_READ_ZERO (-1003.0f)
#define DS18B20_ERR_READ_FF (-1004.0f)
#define DS18B20_ERR_CRC (-1005.0f)
#define DS18B20_SCRATCHPAD_SIZE 9U
#define VOFA_OUTPUT_FIREWATER 1U
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
RTC_HandleTypeDef hrtc;
UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
volatile uint32_t g_interval = 3U;
volatile uint8_t g_rtc_wakeup = 0U;
volatile uint8_t g_manual_stop = 0U;
uint16_t g_audio[AUDIO_SAMPLE_COUNT];
float g_temperature = DS18B20_ERROR_TEMP;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_RTC_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */
void MX_FREERTOS_Init(void);
void App_OnRtcWakeupIrq(void);
void App_OnButtonIrq(uint16_t GPIO_Pin);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#if defined(__CC_ARM)
#pragma import(__use_no_semihosting)

struct __FILE
{
  int handle;
};

FILE __stdout;

void _sys_exit(int x)
{
  (void)x;
  while (1)
  {
  }
}

void _ttywrch(int ch)
{
  (void)ch;
}
#endif

#ifdef __GNUC__
int __io_putchar(int ch)
#else
int fputc(int ch, FILE *f)
#endif
{
  HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1U, HAL_MAX_DELAY);
  return ch;
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_RTC_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  MX_FREERTOS_Init();
  vTaskStartScheduler();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    Error_Handler();
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  HAL_PWR_EnableBkUpAccess();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }

  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_RTC;
  PeriphClkInitStruct.RTCClockSelection = RCC_RTCCLKSOURCE_LSI;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{
  ADC_ChannelConfTypeDef sConfig = {0};

  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_84CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief RTC Initialization Function
  * @param None
  * @retval None
  */
static void MX_RTC_Init(void)
{
  RTC_TimeTypeDef sTime = {0};
  RTC_DateTypeDef sDate = {0};

  hrtc.Instance = RTC;
  hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
  hrtc.Init.AsynchPrediv = 127;
  hrtc.Init.SynchPrediv = 249;
  hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
  hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
  hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR0) != RTC_BKP_MAGIC)
  {
    sTime.Hours = 10;
    sTime.Minutes = 50;
    sTime.Seconds = 0;
    sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
    sTime.StoreOperation = RTC_STOREOPERATION_RESET;
    if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN) != HAL_OK)
    {
      Error_Handler();
    }

    sDate.WeekDay = RTC_WEEKDAY_MONDAY;
    sDate.Month = RTC_MONTH_MAY;
    sDate.Date = 11;
    sDate.Year = 26;
    if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN) != HAL_OK)
    {
      Error_Handler();
    }

    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR0, RTC_BKP_MAGIC);
  }
}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();

  HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(DS18B20_GPIO_Port, DS18B20_Pin, GPIO_PIN_SET);

  GPIO_InitStruct.Pin = DS18B20_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(DS18B20_GPIO_Port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = LED1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED1_GPIO_Port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = K1_Pin|K2_Pin|K3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  HAL_NVIC_SetPriority(EXTI1_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI1_IRQn);
  HAL_NVIC_SetPriority(EXTI2_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI2_IRQn);
  HAL_NVIC_SetPriority(EXTI3_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI3_IRQn);
}

/* USER CODE BEGIN 4 */
#if 0
/* Legacy bare-metal implementation kept only as migration reference. */
static void DWT_Delay_Init(void)
{
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0U;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static void Delay_us(uint32_t us)
{
  uint32_t start = DWT->CYCCNT;
  uint32_t ticks = us * (SystemCoreClock / 1000000U);

  while ((DWT->CYCCNT - start) < ticks)
  {
  }
}

static void DS18B20_Pin_Output(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  HAL_GPIO_WritePin(DS18B20_GPIO_Port, DS18B20_Pin, GPIO_PIN_SET);
  GPIO_InitStruct.Pin = DS18B20_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(DS18B20_GPIO_Port, &GPIO_InitStruct);
}

static void DS18B20_ReleaseBus(void)
{
  HAL_GPIO_WritePin(DS18B20_GPIO_Port, DS18B20_Pin, GPIO_PIN_SET);
}

static float DS18B20_ResetError(void)
{
  DS18B20_ReleaseBus();
  Delay_us(5);
  if (HAL_GPIO_ReadPin(DS18B20_GPIO_Port, DS18B20_Pin) == GPIO_PIN_RESET)
  {
    return DS18B20_ERR_BUS_LOW;
  }

  DS18B20_Pin_Output();
  HAL_GPIO_WritePin(DS18B20_GPIO_Port, DS18B20_Pin, GPIO_PIN_RESET);
  Delay_us(480);
  DS18B20_ReleaseBus();
  Delay_us(70);
  if (HAL_GPIO_ReadPin(DS18B20_GPIO_Port, DS18B20_Pin) == GPIO_PIN_SET)
  {
    Delay_us(410);
    return DS18B20_ERR_NO_PRESENCE;
  }

  Delay_us(410);
  if (HAL_GPIO_ReadPin(DS18B20_GPIO_Port, DS18B20_Pin) == GPIO_PIN_RESET)
  {
    return DS18B20_ERR_BUS_LOW;
  }

  return 0.0f;
}

static void DS18B20_WriteBit(uint8_t bit)
{
  DS18B20_Pin_Output();
  HAL_GPIO_WritePin(DS18B20_GPIO_Port, DS18B20_Pin, GPIO_PIN_RESET);

  if (bit != 0U)
  {
    Delay_us(6);
    DS18B20_ReleaseBus();
    Delay_us(64);
  }
  else
  {
    Delay_us(60);
    DS18B20_ReleaseBus();
    Delay_us(10);
  }
}

static uint8_t DS18B20_ReadBit(void)
{
  uint8_t bit;

  DS18B20_Pin_Output();
  HAL_GPIO_WritePin(DS18B20_GPIO_Port, DS18B20_Pin, GPIO_PIN_RESET);
  Delay_us(6);
  DS18B20_ReleaseBus();
  Delay_us(9);
  bit = (HAL_GPIO_ReadPin(DS18B20_GPIO_Port, DS18B20_Pin) == GPIO_PIN_SET);
  Delay_us(55);

  return bit;
}

static void DS18B20_WriteByte(uint8_t data)
{
  uint8_t i;

  for (i = 0U; i < 8U; i++)
  {
    DS18B20_WriteBit(data & 0x01U);
    data >>= 1;
  }
}

static uint8_t DS18B20_ReadByte(void)
{
  uint8_t i;
  uint8_t data = 0U;

  for (i = 0U; i < 8U; i++)
  {
    data >>= 1;
    if (DS18B20_ReadBit() != 0U)
    {
      data |= 0x80U;
    }
  }

  return data;
}

static uint8_t DS18B20_Crc8(const uint8_t *data, uint8_t len)
{
  uint8_t crc = 0U;
  uint8_t i;

  while (len-- != 0U)
  {
    uint8_t inbyte = *data++;
    for (i = 0U; i < 8U; i++)
    {
      uint8_t mix = (crc ^ inbyte) & 0x01U;
      crc >>= 1;
      if (mix != 0U)
      {
        crc ^= 0x8CU;
      }
      inbyte >>= 1;
    }
  }

  return crc;
}

static float DS18B20_GetTemp(void)
{
  uint8_t scratchpad[DS18B20_SCRATCHPAD_SIZE];
  uint8_t i;
  uint8_t all_zero = 1U;
  uint8_t all_ff = 1U;
  int16_t raw;
  float reset_error;

  reset_error = DS18B20_ResetError();
  if (reset_error < 0.0f)
  {
    return reset_error;
  }

  DS18B20_WriteByte(0xCCU);
  DS18B20_WriteByte(0x44U);
  HAL_Delay(750);

  reset_error = DS18B20_ResetError();
  if (reset_error < 0.0f)
  {
    return reset_error;
  }

  DS18B20_WriteByte(0xCCU);
  DS18B20_WriteByte(0xBEU);
  for (i = 0U; i < DS18B20_SCRATCHPAD_SIZE; i++)
  {
    scratchpad[i] = DS18B20_ReadByte();
    if (scratchpad[i] != 0x00U)
    {
      all_zero = 0U;
    }
    if (scratchpad[i] != 0xFFU)
    {
      all_ff = 0U;
    }
  }

  if ((all_zero != 0U) || (all_ff != 0U) ||
      (DS18B20_Crc8(scratchpad, DS18B20_SCRATCHPAD_SIZE - 1U) != scratchpad[8]))
  {
    if (all_zero != 0U)
    {
      return DS18B20_ERR_READ_ZERO;
    }
    if (all_ff != 0U)
    {
      return DS18B20_ERR_READ_FF;
    }
    return DS18B20_ERR_CRC;
  }

  raw = (int16_t)((uint16_t)scratchpad[1] << 8 | scratchpad[0]);

  return (float)raw * 0.0625f;
}

static uint16_t ADC1_ReadOnce(void)
{
  uint16_t value = 0U;

  if (HAL_ADC_Start(&hadc1) == HAL_OK)
  {
    if (HAL_ADC_PollForConversion(&hadc1, 10U) == HAL_OK)
    {
      value = (uint16_t)HAL_ADC_GetValue(&hadc1);
    }
  }
  HAL_ADC_Stop(&hadc1);

  return value;
}

static void Collect_Audio(void)
{
  uint32_t i;

  for (i = 0U; i < AUDIO_SAMPLE_COUNT; i++)
  {
    g_audio[i] = ADC1_ReadOnce();
  }
}

static void Print_SignedTenths(int32_t value10)
{
  if (value10 < 0)
  {
    printf("-%ld.%ld", (-value10) / 10L, (-value10) % 10L);
  }
  else
  {
    printf("%ld.%ld", value10 / 10L, value10 % 10L);
  }
}

static void Print_Data(void)
{
  uint32_t i;
#if VOFA_OUTPUT_FIREWATER != 0U
  uint32_t sum = 0U;
  uint16_t min_value = 0xFFFFU;
  uint16_t max_value = 0U;
  int32_t temp10;

  for (i = 0U; i < AUDIO_SAMPLE_COUNT; i++)
  {
    uint16_t value = g_audio[i];

    sum += value;
    if (value < min_value)
    {
      min_value = value;
    }
    if (value > max_value)
    {
      max_value = value;
    }
  }

  temp10 = (int32_t)(g_temperature * 10.0f);
  Print_SignedTenths(temp10);
  printf(",%lu,%u,%u,%lu\r\n",
         sum / AUDIO_SAMPLE_COUNT,
         min_value,
         max_value,
         g_interval);
#else
  RTC_TimeTypeDef time = {0};
  RTC_DateTypeDef date = {0};

  HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BIN);
  HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BIN);

  printf("\r\nTime: %02u:%02u:%02u\r\n", time.Hours, time.Minutes, time.Seconds);
  printf("Interval: %lus\r\n", g_interval);

  if (g_temperature <= (DS18B20_ERROR_TEMP + 0.1f))
  {
    printf("Temp: DS18B20 error\r\n");
  }
  else
  {
    int32_t temp10 = (int32_t)(g_temperature * 10.0f);
    printf("Temp: ");
    Print_SignedTenths(temp10);
    printf(" C\r\n");
  }

  printf("ADC[0..127]:");
  for (i = 0U; i < AUDIO_SAMPLE_COUNT; i++)
  {
    printf(" %u", g_audio[i]);
  }
  printf("\r\n");
#endif
}

static void Set_RTC_Wakeup(uint32_t seconds)
{
  if (seconds < 1U)
  {
    seconds = 1U;
  }
  else if (seconds > 60U)
  {
    seconds = 60U;
  }

  HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);
  if (HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, seconds - 1U, RTC_WAKEUPCLOCK_CK_SPRE_16BITS) != HAL_OK)
  {
    Error_Handler();
  }
}

static void Enter_Stop_Mode(void)
{
  Set_RTC_Wakeup(g_interval);

  while (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_TC) == RESET)
  {
  }

  g_rtc_wakeup = 0U;
  __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);
  HAL_SuspendTick();
  HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
  SystemClock_Config();
  HAL_ResumeTick();
  DWT_Delay_Init();
}
#endif

void HAL_RTCEx_WakeUpTimerEventCallback(RTC_HandleTypeDef *hrtc_arg)
{
  if (hrtc_arg->Instance == RTC)
  {
    App_OnRtcWakeupIrq();
  }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if ((GPIO_Pin == K1_Pin) || (GPIO_Pin == K2_Pin) || (GPIO_Pin == K3_Pin))
  {
    App_OnButtonIrq(GPIO_Pin);
  }
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  volatile uint32_t i;

  __disable_irq();
  while (1)
  {
    HAL_GPIO_TogglePin(LED1_GPIO_Port, LED1_Pin);
    for (i = 0U; i < 300000U; i++)
    {
    }
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  (void)file;
  (void)line;
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
