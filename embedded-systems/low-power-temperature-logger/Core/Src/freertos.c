/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    freertos.c
  * @brief   FreeRTOS tasks for low power temperature and audio collection.
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern ADC_HandleTypeDef hadc1;
extern RTC_HandleTypeDef hrtc;
extern UART_HandleTypeDef huart1;
extern void SystemClock_Config(void);

#define AUDIO_SAMPLE_COUNT      128U
#define INTERVAL_MIN_SEC        1U
#define INTERVAL_MAX_SEC        60U
#define UART_LINE_MAX           32U
#define UART_CMD_IDLE_TIMEOUT_MS 100U

#define DS18B20_ERROR_TEMP      (-1999.0f)
#define DS18B20_ERR_BUS_LOW     (-1001.0f)
#define DS18B20_ERR_NO_PRESENCE (-1002.0f)
#define DS18B20_ERR_READ_ZERO   (-1003.0f)
#define DS18B20_ERR_READ_FF     (-1004.0f)
#define DS18B20_ERR_CRC         (-1005.0f)
#define DS18B20_SCRATCHPAD_SIZE 9U

volatile uint32_t g_interval_sec = 3U;

static TaskHandle_t g_data_task_handle;
static TaskHandle_t g_uart_task_handle;
static QueueHandle_t g_uart_rx_queue;
static SemaphoreHandle_t g_uart_tx_mutex;
static uint8_t g_uart_rx_byte;
static uint16_t g_audio[AUDIO_SAMPLE_COUNT];
static float g_temperature = DS18B20_ERROR_TEMP;

static void DataCollectTask(void *argument);
static void UartTask(void *argument);
static void DWT_Delay_Init(void);
static void Delay_us(uint32_t us);
static void DS18B20_Pin_Output(void);
static void DS18B20_ReleaseBus(void);
static void DS18B20_WriteBit(uint8_t bit);
static uint8_t DS18B20_ReadBit(void);
static void DS18B20_WriteByte(uint8_t data);
static uint8_t DS18B20_ReadByte(void);
static uint8_t DS18B20_Crc8(const uint8_t *data, uint8_t len);
static float DS18B20_ResetError(void);
static float DS18B20_GetTemp(void);
static uint16_t ADC1_ReadOnce(void);
static void Collect_Audio(void);
static void Print_SignedTenths(int32_t value10);
static void Print_Data(void);
static void Enter_Stop_Mode(void);
static void Set_RTC_Wakeup(uint32_t seconds);
static uint32_t Get_Interval(void);
static void Set_Interval(uint32_t seconds);
static void Start_Uart_Rx_IT(void);
static void Configure_Uart_Rx_EXTI_Wakeup(void);
static void Process_Uart_Command(char *line);

void MX_FREERTOS_Init(void)
{
  DWT_Delay_Init();

  g_uart_rx_queue = xQueueCreate(64U, sizeof(uint8_t));
  g_uart_tx_mutex = xSemaphoreCreateMutex();

  if ((g_uart_rx_queue == NULL) || (g_uart_tx_mutex == NULL))
  {
    Error_Handler();
  }

  if (xTaskCreate(DataCollectTask, "DataCollect", 512U, NULL,
                  tskIDLE_PRIORITY + 2U, &g_data_task_handle) != pdPASS)
  {
    Error_Handler();
  }

  if (xTaskCreate(UartTask, "UartTask", 384U, NULL,
                  tskIDLE_PRIORITY + 3U, &g_uart_task_handle) != pdPASS)
  {
    Error_Handler();
  }

  Configure_Uart_Rx_EXTI_Wakeup();
  Start_Uart_Rx_IT();
}

static void DataCollectTask(void *argument)
{
  (void)argument;

  for (;;)
  {
    HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_SET);

    g_temperature = DS18B20_GetTemp();
    Collect_Audio();
    Print_Data();

    HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_RESET);
    Enter_Stop_Mode();
  }
}

static void UartTask(void *argument)
{
  uint8_t ch;
  char line[UART_LINE_MAX];
  uint32_t len = 0U;
  TickType_t wait_ticks = portMAX_DELAY;

  (void)argument;

  for (;;)
  {
    if (xQueueReceive(g_uart_rx_queue, &ch, wait_ticks) == pdPASS)
    {
      if ((ch == '\r') || (ch == '\n'))
      {
        if (len > 0U)
        {
          line[len] = '\0';
          Process_Uart_Command(line);
          len = 0U;
          wait_ticks = portMAX_DELAY;
        }
      }
      else if (len < (UART_LINE_MAX - 1U))
      {
        line[len++] = (char)ch;
        wait_ticks = pdMS_TO_TICKS(UART_CMD_IDLE_TIMEOUT_MS);
      }
      else
      {
        len = 0U;
        wait_ticks = portMAX_DELAY;
        if (xSemaphoreTake(g_uart_tx_mutex, portMAX_DELAY) == pdPASS)
        {
          printf("ERR unknown cmd\r\n");
          xSemaphoreGive(g_uart_tx_mutex);
        }
      }
    }
    else if (len > 0U)
    {
      line[len] = '\0';
      Process_Uart_Command(line);
      len = 0U;
      wait_ticks = portMAX_DELAY;
    }
  }
}

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
  Delay_us(5U);
  if (HAL_GPIO_ReadPin(DS18B20_GPIO_Port, DS18B20_Pin) == GPIO_PIN_RESET)
  {
    return DS18B20_ERR_BUS_LOW;
  }

  DS18B20_Pin_Output();
  HAL_GPIO_WritePin(DS18B20_GPIO_Port, DS18B20_Pin, GPIO_PIN_RESET);
  Delay_us(480U);
  DS18B20_ReleaseBus();
  Delay_us(70U);
  if (HAL_GPIO_ReadPin(DS18B20_GPIO_Port, DS18B20_Pin) == GPIO_PIN_SET)
  {
    Delay_us(410U);
    return DS18B20_ERR_NO_PRESENCE;
  }

  Delay_us(410U);
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
    Delay_us(6U);
    DS18B20_ReleaseBus();
    Delay_us(64U);
  }
  else
  {
    Delay_us(60U);
    DS18B20_ReleaseBus();
    Delay_us(10U);
  }
}

static uint8_t DS18B20_ReadBit(void)
{
  uint8_t bit;

  DS18B20_Pin_Output();
  HAL_GPIO_WritePin(DS18B20_GPIO_Port, DS18B20_Pin, GPIO_PIN_RESET);
  Delay_us(6U);
  DS18B20_ReleaseBus();
  Delay_us(9U);
  bit = (uint8_t)(HAL_GPIO_ReadPin(DS18B20_GPIO_Port, DS18B20_Pin) == GPIO_PIN_SET);
  Delay_us(55U);

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
  vTaskDelay(pdMS_TO_TICKS(750U));

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
  RTC_TimeTypeDef time = {0};
  RTC_DateTypeDef date = {0};
  uint32_t i;

  HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BIN);
  HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BIN);

  if (xSemaphoreTake(g_uart_tx_mutex, portMAX_DELAY) == pdPASS)
  {
    printf("\r\nRTC: 20%02u-%02u-%02u %02u:%02u:%02u\r\n",
           date.Year, date.Month, date.Date,
           time.Hours, time.Minutes, time.Seconds);
    printf("Interval: %lus\r\n", Get_Interval());

    if (g_temperature <= (DS18B20_ERROR_TEMP + 0.1f))
    {
      printf("Temp error: ");
      Print_SignedTenths((int32_t)(g_temperature * 10.0f));
      printf("\r\n");
    }
    else
    {
      printf("Temp: ");
      Print_SignedTenths((int32_t)(g_temperature * 10.0f));
      printf(" C\r\n");
    }

    printf("MIC[0..127]:");
    for (i = 0U; i < AUDIO_SAMPLE_COUNT; i++)
    {
      printf(" %u", g_audio[i]);
    }
    printf("\r\n");

    xSemaphoreGive(g_uart_tx_mutex);
  }
}

static void Set_RTC_Wakeup(uint32_t seconds)
{
  if (seconds < INTERVAL_MIN_SEC)
  {
    seconds = INTERVAL_MIN_SEC;
  }
  else if (seconds > INTERVAL_MAX_SEC)
  {
    seconds = INTERVAL_MAX_SEC;
  }

  HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);
  if (HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, seconds - 1U,
                                  RTC_WAKEUPCLOCK_CK_SPRE_16BITS) != HAL_OK)
  {
    Error_Handler();
  }
}

static void Enter_Stop_Mode(void)
{
  Set_RTC_Wakeup(Get_Interval());
  Start_Uart_Rx_IT();

  while (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_TC) == RESET)
  {
  }

  __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);
  HAL_SuspendTick();
  HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
  SystemClock_Config();
  HAL_ResumeTick();
  DWT_Delay_Init();
  Start_Uart_Rx_IT();
}

static uint32_t Get_Interval(void)
{
  uint32_t value;

  taskENTER_CRITICAL();
  value = g_interval_sec;
  taskEXIT_CRITICAL();

  return value;
}

static void Set_Interval(uint32_t seconds)
{
  if (seconds < INTERVAL_MIN_SEC)
  {
    seconds = INTERVAL_MIN_SEC;
  }
  else if (seconds > INTERVAL_MAX_SEC)
  {
    seconds = INTERVAL_MAX_SEC;
  }

  taskENTER_CRITICAL();
  g_interval_sec = seconds;
  taskEXIT_CRITICAL();
}

static void Start_Uart_Rx_IT(void)
{
  if (huart1.RxState == HAL_UART_STATE_READY)
  {
    (void)HAL_UART_Receive_IT(&huart1, &g_uart_rx_byte, 1U);
  }
}

static void Configure_Uart_Rx_EXTI_Wakeup(void)
{
  __HAL_RCC_SYSCFG_CLK_ENABLE();

  SYSCFG->EXTICR[2] &= ~SYSCFG_EXTICR3_EXTI10;
  SYSCFG->EXTICR[2] |= SYSCFG_EXTICR3_EXTI10_PA;

  __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_10);
  EXTI->IMR |= GPIO_PIN_10;
  EXTI->FTSR |= GPIO_PIN_10;
  EXTI->RTSR &= ~GPIO_PIN_10;

  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 5U, 0U);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
}

static void Process_Uart_Command(char *line)
{
  char cmd[UART_LINE_MAX];
  char *arg;
  uint32_t i;
  long value;

  for (i = 0U; (i < (UART_LINE_MAX - 1U)) && (line[i] != '\0'); i++)
  {
    cmd[i] = (char)toupper((unsigned char)line[i]);
  }
  cmd[i] = '\0';

  if (strcmp(cmd, "GET") == 0)
  {
    if (xSemaphoreTake(g_uart_tx_mutex, portMAX_DELAY) == pdPASS)
    {
      printf("OK interval=%lus\r\n", Get_Interval());
      xSemaphoreGive(g_uart_tx_mutex);
    }
    return;
  }

  if (strncmp(cmd, "SET", 3U) == 0)
  {
    arg = &cmd[3];
  }
  else if (strncmp(cmd, "ET", 2U) == 0)
  {
    arg = &cmd[2];
  }
  else if (strncmp(cmd, "INT", 3U) == 0)
  {
    arg = &cmd[3];
  }
  else if (strncmp(cmd, "NT", 2U) == 0)
  {
    arg = &cmd[2];
  }
  else
  {
    if (xSemaphoreTake(g_uart_tx_mutex, portMAX_DELAY) == pdPASS)
    {
      printf("ERR unknown cmd\r\n");
      xSemaphoreGive(g_uart_tx_mutex);
    }
    return;
  }

  while ((*arg == ' ') || (*arg == '=') || (*arg == '\t'))
  {
    arg++;
  }

  if ((*arg < '0') || (*arg > '9'))
  {
    if (xSemaphoreTake(g_uart_tx_mutex, portMAX_DELAY) == pdPASS)
    {
      printf("ERR unknown cmd\r\n");
      xSemaphoreGive(g_uart_tx_mutex);
    }
    return;
  }

  value = strtol(arg, NULL, 10);
  if ((value < (long)INTERVAL_MIN_SEC) || (value > (long)INTERVAL_MAX_SEC))
  {
    if (xSemaphoreTake(g_uart_tx_mutex, portMAX_DELAY) == pdPASS)
    {
      printf("ERR range 1-60\r\n");
      xSemaphoreGive(g_uart_tx_mutex);
    }
    return;
  }

  Set_Interval((uint32_t)value);
  Set_RTC_Wakeup((uint32_t)value);

  if (g_data_task_handle != NULL)
  {
    xTaskNotifyGive(g_data_task_handle);
  }

  if (xSemaphoreTake(g_uart_tx_mutex, portMAX_DELAY) == pdPASS)
  {
    printf("OK interval=%lds\r\n", value);
    xSemaphoreGive(g_uart_tx_mutex);
  }
}

void App_OnRtcWakeupIrq(void)
{
  BaseType_t higher_priority_task_woken = pdFALSE;

  if (g_data_task_handle != NULL)
  {
    vTaskNotifyGiveFromISR(g_data_task_handle, &higher_priority_task_woken);
    portYIELD_FROM_ISR(higher_priority_task_woken);
  }
}

void App_OnButtonIrq(uint16_t GPIO_Pin)
{
  BaseType_t higher_priority_task_woken = pdFALSE;

  if (GPIO_Pin == K1_Pin)
  {
    if (g_interval_sec < INTERVAL_MAX_SEC)
    {
      g_interval_sec++;
    }
  }
  else if (GPIO_Pin == K2_Pin)
  {
    if (g_interval_sec > INTERVAL_MIN_SEC)
    {
      g_interval_sec--;
    }
  }

  if (g_data_task_handle != NULL)
  {
    vTaskNotifyGiveFromISR(g_data_task_handle, &higher_priority_task_woken);
    portYIELD_FROM_ISR(higher_priority_task_woken);
  }
}

void App_OnUartRxWakeIrq(void)
{
  BaseType_t higher_priority_task_woken = pdFALSE;

  if (g_data_task_handle != NULL)
  {
    vTaskNotifyGiveFromISR(g_data_task_handle, &higher_priority_task_woken);
    portYIELD_FROM_ISR(higher_priority_task_woken);
  }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  BaseType_t higher_priority_task_woken = pdFALSE;

  if (huart->Instance == USART1)
  {
    if (g_uart_rx_queue != NULL)
    {
      (void)xQueueSendFromISR(g_uart_rx_queue, &g_uart_rx_byte,
                              &higher_priority_task_woken);
    }
    (void)HAL_UART_Receive_IT(&huart1, &g_uart_rx_byte, 1U);
    portYIELD_FROM_ISR(higher_priority_task_woken);
  }
}

void vApplicationMallocFailedHook(void)
{
  Error_Handler();
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
  (void)xTask;
  (void)pcTaskName;
  Error_Handler();
}
