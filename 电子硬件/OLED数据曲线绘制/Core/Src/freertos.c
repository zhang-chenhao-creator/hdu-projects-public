#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os2.h"
#include "main.h"
#include "adc.h"
#include "usart.h"
#include "GUI.h"
#include <stdio.h>

#define ADC_BUF_SIZE 128U
#define ENABLE_USART_DEBUG 0

typedef enum {
  PAGE_LOGO = 0,
  PAGE_NAME,
  PAGE_MAIN
} PageType;

extern GUI_FLASH const GUI_FONT GUI_FontHZ_Info_16;
extern GUI_FLASH const GUI_FONT GUI_FontHZ_AdLabel_16;
extern GUI_FLASH const GUI_BITMAP bmHduLogo;

volatile PageType g_current_page = PAGE_LOGO;
volatile uint16_t g_ad_value = 0;
volatile uint8_t g_curve_data[128];
volatile uint8_t g_curve_ready = 0;
volatile uint8_t g_adc_running = 0;

static uint16_t adc_buf[ADC_BUF_SIZE];
static uint32_t page_enter_tick = 0;

osThreadId_t defaultTaskHandle;
osThreadId_t taskGUIHandle;
osThreadId_t taskKeyHandle;

const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

const osThreadAttr_t taskGUI_attributes = {
  .name = "TaskGUI",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

const osThreadAttr_t taskKey_attributes = {
  .name = "TaskKey",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal,
};

static void StartDefaultTask(void *argument);
static void StartTaskGUI(void *argument);
static void StartTaskKey(void *argument);
static void DrawPageLogo(void);
static void DrawPageName(void);
static void DrawPageMain(void);
static void SetPage(PageType page);
static uint8_t KeyPressed(GPIO_TypeDef *port, uint16_t pin);

void MX_FREERTOS_Init(void)
{
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);
  taskGUIHandle = osThreadNew(StartTaskGUI, NULL, &taskGUI_attributes);
  taskKeyHandle = osThreadNew(StartTaskKey, NULL, &taskKey_attributes);
}

static void StartDefaultTask(void *argument)
{
  (void)argument;

  g_adc_running = (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_buf, ADC_BUF_SIZE) == HAL_OK);

  for (;;)
  {
#if ENABLE_USART_DEBUG
    printf("AD:%u\r\n", g_ad_value);
#endif
    osDelay(200);
  }
}

static void StartTaskGUI(void *argument)
{
  (void)argument;

  GUI_Init();
  GUI_SetColor(GUI_COLOR_WHITE);
  SetPage(PAGE_LOGO);

  for (;;)
  {
    switch (g_current_page)
    {
      case PAGE_LOGO:
        DrawPageLogo();
        break;
      case PAGE_NAME:
        DrawPageName();
        break;
      case PAGE_MAIN:
      default:
        DrawPageMain();
        break;
    }
    GUI_Update();
    osDelay(50);
  }
}

static void StartTaskKey(void *argument)
{
  (void)argument;

  for (;;)
  {
    if (KeyPressed(K1_GPIO_Port, K1_Pin))
    {
      SetPage(PAGE_LOGO);
    }

    if (KeyPressed(K2_GPIO_Port, K2_Pin))
    {
      SetPage(PAGE_MAIN);
    }

    osDelay(20);
  }
}

static void DrawPageLogo(void)
{
  GUI_Clear();
  GUI_DrawBitmap(&bmHduLogo, 32, 0);

  if (osKernelGetTickCount() - page_enter_tick >= 3000U)
  {
    SetPage(PAGE_NAME);
  }
}

static void DrawPageName(void)
{
  GUI_Clear();
  GUI_SetFont(&GUI_FontHZ_Info_16);
  GUI_DispStringHCenterAt("Author", 64, 8);
  GUI_SetFont(GUI_DEFAULT_FONT);
  GUI_DispStringHCenterAt("Exp9 Audio Curve", 64, 34);
  GUI_DispStringHCenterAt("Demo ID", 64, 48);

  if (osKernelGetTickCount() - page_enter_tick >= 2000U)
  {
    SetPage(PAGE_MAIN);
  }
}

static void DrawPageMain(void)
{
  char buf[24];
  uint8_t curve[128];

  GUI_Clear();
  GUI_SetFont(&GUI_FontHZ_AdLabel_16);
  GUI_DispStringAt("\345\275\223\345\211\215", 0, 0);
  GUI_DispStringAt("\345\200\274", 50, 0);
  GUI_SetFont(GUI_DEFAULT_FONT);
  GUI_DispStringAt("AD", 34, 4);
  sprintf(buf, ":%4u", g_ad_value);
  GUI_DispStringAt(buf, 66, 4);
  GUI_DrawLine(0, 17, 127, 17);

  if (g_curve_ready)
  {
    for (uint32_t i = 0; i < 128U; ++i)
    {
      curve[i] = g_curve_data[i];
    }

    for (uint32_t i = 0; i < 127U; ++i)
    {
      int y1 = 63 - ((int)curve[i] * 45 / 255);
      int y2 = 63 - ((int)curve[i + 1U] * 45 / 255);
      GUI_DrawLine((int)i, y1, (int)i + 1, y2);
    }
  }
}

static void SetPage(PageType page)
{
  g_current_page = page;
  page_enter_tick = osKernelGetTickCount();
}

static uint8_t KeyPressed(GPIO_TypeDef *port, uint16_t pin)
{
  if (HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_RESET)
  {
    osDelay(20);
    if (HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_RESET)
    {
      while (HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_RESET)
      {
        osDelay(10);
      }
      return 1U;
    }
  }
  return 0U;
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
  uint32_t sum = 0;

  if (hadc->Instance != ADC1)
  {
    return;
  }

  for (uint32_t i = 0; i < ADC_BUF_SIZE; ++i)
  {
    uint16_t value = adc_buf[i] & 0x0FFFU;
    sum += value;
    g_curve_data[i] = (uint8_t)(value >> 4);
  }

  g_ad_value = (uint16_t)(sum / ADC_BUF_SIZE);
  g_curve_ready = 1U;
}

int fputc(int ch, FILE *f)
{
  (void)f;
  HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 10);
  return ch;
}
