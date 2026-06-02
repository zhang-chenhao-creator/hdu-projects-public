#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os2.h"
#include "adc.h"
#include "gpio.h"
#include "usart.h"
#include "GUI.h"
#include "ff.h"
#include <stdio.h>
#include <string.h>

#define ADC_BUF_SIZE 128U
#define CFG_MAGIC 0x43464731UL
#define CFG_VERSION 1U
#define DEFAULT_TEMP_LIMIT_X10 300
#define DEFAULT_ALARM_SECONDS 10U
#define TEMP_LIMIT_MIN_X10 0
#define TEMP_LIMIT_MAX_X10 1250
#define ALARM_SECONDS_MIN 1U
#define ALARM_SECONDS_MAX 3600U
#define TEMP_SAMPLE_MS 1000U
#define UART_REPORT_MS 1000U
#define GUI_REFRESH_FLAG 0x00000001U
#define CONFIG_SAVE_DELAY_MS 1000U
#define DS18B20_SCRATCHPAD_SIZE 9U
#define DS18B20_READ_ATTEMPTS 2U
#define DS18B20_MAX_STALE_READS 3U
#define TEMP_ERR_NOT_READY_X10 (-19990)
#define TEMP_ERR_BUS_LOW_X10 (-10010)
#define TEMP_ERR_NO_PRESENCE_X10 (-10020)
#define TEMP_ERR_READ_ZERO_X10 (-10030)
#define TEMP_ERR_READ_FF_X10 (-10040)
#define TEMP_ERR_CRC_X10 (-10050)
#define KEY_DIAG_ENABLE 0U

typedef enum {
  PAGE_MAIN = 0,
  PAGE_SET_LIMIT,
  PAGE_SET_ALARM
} PageType;

typedef enum {
  KEY_EVT_NONE = 0,
  KEY_EVT_K1,
  KEY_EVT_K2,
  KEY_EVT_K3,
  KEY_EVT_K4
} KeyEvent;

typedef struct {
  uint32_t magic;
  uint16_t version;
  int16_t temp_limit_x10;
  uint16_t alarm_seconds;
  uint32_t crc32;
} AppConfig;

static uint16_t adc_buf[ADC_BUF_SIZE];
static FATFS fs;
static osMessageQueueId_t keyQueueHandle;

static AppConfig cfg;
static volatile int16_t current_temp_x10 = TEMP_ERR_NOT_READY_X10;
static volatile uint16_t current_adc = 0;
static volatile PageType current_page = PAGE_MAIN;
static volatile uint8_t fs_ready = 0;
static volatile uint8_t alarm_active = 0;
static volatile uint8_t temp_valid = 0;
static volatile int16_t ui_temp_limit_x10 = DEFAULT_TEMP_LIMIT_X10;
static volatile uint16_t ui_alarm_seconds = DEFAULT_ALARM_SECONDS;
static volatile uint8_t config_dirty = 0;
static uint32_t config_dirty_tick = 0;

osThreadId_t defaultTaskHandle;
osThreadId_t taskGUIHandle;
osThreadId_t taskKeyHandle;

const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 1536 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

const osThreadAttr_t taskGUI_attributes = {
  .name = "TaskGUI",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal,
};

const osThreadAttr_t taskKey_attributes = {
  .name = "TaskKey",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityHigh,
};

static void StartDefaultTask(void *argument);
static void StartTaskGUI(void *argument);
static void StartTaskKey(void *argument);
static uint32_t CalcCrc32(const void *data, uint32_t len);
static void ConfigDefaults(void);
static uint8_t ConfigValid(const AppConfig *c);
static void ConfigLoad(void);
static void ConfigSave(void);
static void ConfigMarkDirty(void);
static void ConfigSavePending(void);
static void ConfigSaveIfDue(uint32_t now);
static void StorageInit(void);
static void UiSyncConfig(void);
static void GuiRequestRefresh(void);
static void GuiRefreshNow(void);
static void ProcessKey(KeyEvent evt);
static void AlarmUpdate(uint32_t now);
static void PrintText(const char *text);
static void UartReport(void);
static void DrawMainPage(void);
static void DrawLimitPage(void);
static void DrawAlarmPage(void);
static void FormatTemp(char *buf, int16_t temp_x10);
static void FormatCurrentTemp(char *buf, uint32_t size);
static int16_t ReadTempX10(void);
static KeyEvent KeyFromMask(uint16_t key);
#if KEY_DIAG_ENABLE != 0U
static const char *KeyEventName(KeyEvent evt);
#endif
static void DWT_DelayInit(void);
static void DelayUs(uint32_t us);
static void DS18B20_WriteBit(uint8_t bit);
static uint8_t DS18B20_ReadBit(void);
static void DS18B20_WriteByte(uint8_t data);
static uint8_t DS18B20_ReadByte(void);
static uint8_t DS18B20_Crc8(const uint8_t *data, uint8_t len);
static int16_t DS18B20_ReadTempX10(void);

void MX_FREERTOS_Init(void)
{
  DWT_DelayInit();
  keyQueueHandle = osMessageQueueNew(8, sizeof(KeyEvent), NULL);
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);
  taskGUIHandle = osThreadNew(StartTaskGUI, NULL, &taskGUI_attributes);
  taskKeyHandle = osThreadNew(StartTaskKey, NULL, &taskKey_attributes);
}

static void StartDefaultTask(void *argument)
{
  uint32_t last_sample = 0;
  uint32_t last_uart = 0;
  uint32_t now;
  KeyEvent evt;

  (void)argument;

  ConfigDefaults();
  HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_buf, ADC_BUF_SIZE);
  StorageInit();

  for (;;)
  {
    now = osKernelGetTickCount();
    while (osMessageQueueGet(keyQueueHandle, &evt, NULL, 0) == osOK)
    {
      ProcessKey(evt);
    }

    if ((now - last_sample) >= TEMP_SAMPLE_MS)
    {
      last_sample = now;
      current_temp_x10 = ReadTempX10();
      AlarmUpdate(now);
      GuiRequestRefresh();
    }

    if ((now - last_uart) >= UART_REPORT_MS)
    {
      last_uart = now;
      UartReport();
    }

    ConfigSaveIfDue(now);

    osDelay(20);
  }
}

static void StartTaskGUI(void *argument)
{
  (void)argument;

  GUI_Init();
  GUI_SetColor(GUI_COLOR_WHITE);

  for (;;)
  {
    if (current_page == PAGE_SET_LIMIT)
    {
      DrawLimitPage();
    }
    else if (current_page == PAGE_SET_ALARM)
    {
      DrawAlarmPage();
    }
    else
    {
      DrawMainPage();
    }

    GUI_Update();
    (void)osThreadFlagsWait(GUI_REFRESH_FLAG, osFlagsWaitAny, osWaitForever);
  }
}

static void StartTaskKey(void *argument)
{
  uint16_t key;
  uint16_t key_first;
  KeyEvent evt;
#if KEY_DIAG_ENABLE != 0U
  osStatus_t q_status;
  char line[96];
#endif

  (void)argument;

  for (;;)
  {
    key_first = KEY_Scan();
    if (key_first != 0U)
    {
      GuiRequestRefresh();
      osDelay(20);
      key = KEY_Scan();
      if (key != 0U)
      {
        GuiRequestRefresh();
        evt = KeyFromMask(key);
#if KEY_DIAG_ENABLE != 0U
        snprintf(line, sizeof(line),
                 "[KEY] first=0x%04X stable=0x%04X idr=0x%04X evt=%s\r\n",
                 (unsigned int)key_first,
                 (unsigned int)key,
                 (unsigned int)(GPIOE->IDR & (K1_Pin | K2_Pin | K3_Pin | K4_Pin)),
                 KeyEventName(evt));
        PrintText(line);
#endif
        if (evt != KEY_EVT_NONE)
        {
#if KEY_DIAG_ENABLE != 0U
          q_status = osMessageQueuePut(keyQueueHandle, &evt, 0, 0);
          snprintf(line, sizeof(line), "[KEY] queue evt=%s status=%ld\r\n",
                   KeyEventName(evt), (long)q_status);
          PrintText(line);
#else
          osMessageQueuePut(keyQueueHandle, &evt, 0, 0);
#endif
        }

        while (KEY_Scan() != 0U)
        {
          GuiRequestRefresh();
          osDelay(10);
        }
      }
    }

    osDelay(10);
  }
}

static uint32_t CalcCrc32(const void *data, uint32_t len)
{
  const uint8_t *p = (const uint8_t *)data;
  uint32_t crc = 0xFFFFFFFFUL;
  uint32_t i;
  uint8_t bit;

  for (i = 0; i < len; ++i)
  {
    crc ^= p[i];
    for (bit = 0; bit < 8U; ++bit)
    {
      crc = (crc >> 1) ^ (0xEDB88320UL & (0UL - (crc & 1UL)));
    }
  }

  return ~crc;
}

static void ConfigDefaults(void)
{
  memset(&cfg, 0, sizeof(cfg));
  cfg.magic = CFG_MAGIC;
  cfg.version = CFG_VERSION;
  cfg.temp_limit_x10 = DEFAULT_TEMP_LIMIT_X10;
  cfg.alarm_seconds = DEFAULT_ALARM_SECONDS;
  cfg.crc32 = CalcCrc32(&cfg, sizeof(cfg) - sizeof(cfg.crc32));
  UiSyncConfig();
}

static uint8_t ConfigValid(const AppConfig *c)
{
  if (c->magic != CFG_MAGIC || c->version != CFG_VERSION)
  {
    return 0U;
  }
  if (c->temp_limit_x10 < TEMP_LIMIT_MIN_X10 || c->temp_limit_x10 > TEMP_LIMIT_MAX_X10)
  {
    return 0U;
  }
  if (c->alarm_seconds < ALARM_SECONDS_MIN || c->alarm_seconds > ALARM_SECONDS_MAX)
  {
    return 0U;
  }

  return c->crc32 == CalcCrc32(c, sizeof(*c) - sizeof(c->crc32));
}

static void ConfigLoad(void)
{
  FIL file;
  UINT br;
  AppConfig tmp;

  if (f_open(&file, "0:/CFG.BIN", FA_READ) == FR_OK)
  {
    if (f_read(&file, &tmp, sizeof(tmp), &br) == FR_OK && br == sizeof(tmp) && ConfigValid(&tmp))
    {
      cfg = tmp;
    }
    f_close(&file);
  }

  UiSyncConfig();
  GuiRequestRefresh();
}

static void ConfigSave(void)
{
  FIL file;
  UINT bw;

  if (!fs_ready)
  {
    return;
  }

  cfg.magic = CFG_MAGIC;
  cfg.version = CFG_VERSION;
  cfg.crc32 = CalcCrc32(&cfg, sizeof(cfg) - sizeof(cfg.crc32));

  if (f_open(&file, "0:/CFG.BIN", FA_CREATE_ALWAYS | FA_WRITE) == FR_OK)
  {
    f_write(&file, &cfg, sizeof(cfg), &bw);
    f_sync(&file);
    f_close(&file);
  }
}

static void ConfigMarkDirty(void)
{
  config_dirty = 1U;
  config_dirty_tick = osKernelGetTickCount();
}

static void ConfigSavePending(void)
{
  if (config_dirty != 0U && fs_ready != 0U)
  {
    config_dirty = 0U;
    ConfigSave();
  }
}

static void ConfigSaveIfDue(uint32_t now)
{
  if (config_dirty != 0U && (now - config_dirty_tick) >= CONFIG_SAVE_DELAY_MS)
  {
    ConfigSavePending();
  }
}

static void StorageInit(void)
{
  static uint8_t mkfs_work[4096];
  MKFS_PARM opt = {FM_ANY, 0, 0, 0, 0};
  FRESULT res;

  res = f_mount(&fs, "0:", 1);
  if (res == FR_NO_FILESYSTEM)
  {
    res = f_mkfs("0:", &opt, mkfs_work, sizeof(mkfs_work));
    if (res == FR_OK)
    {
      res = f_mount(&fs, "0:", 1);
    }
  }

  fs_ready = (res == FR_OK) ? 1U : 0U;
  if (fs_ready)
  {
    ConfigLoad();
  }
}

static void UiSyncConfig(void)
{
  ui_temp_limit_x10 = cfg.temp_limit_x10;
  ui_alarm_seconds = cfg.alarm_seconds;
}

static void GuiRequestRefresh(void)
{
  if (taskGUIHandle != NULL)
  {
    (void)osThreadFlagsSet(taskGUIHandle, GUI_REFRESH_FLAG);
  }
}

static void GuiRefreshNow(void)
{
  GuiRequestRefresh();
  osDelay(1);
}

static void ProcessKey(KeyEvent evt)
{
  uint8_t changed = 0U;
#if KEY_DIAG_ENABLE != 0U
  char line[80];

  snprintf(line, sizeof(line), "[KEY] ProcessKey evt=%s page=%lu\r\n",
           KeyEventName(evt), (uint32_t)current_page);
  PrintText(line);
#endif

  if (evt == KEY_EVT_K2)
  {
    if (current_page == PAGE_SET_LIMIT)
    {
      current_page = PAGE_MAIN;
      GuiRefreshNow();
      ConfigSavePending();
    }
    else
    {
      current_page = PAGE_SET_LIMIT;
      GuiRefreshNow();
    }
  }
  else if (evt == KEY_EVT_K3)
  {
    if (current_page == PAGE_SET_ALARM)
    {
      current_page = PAGE_MAIN;
      GuiRefreshNow();
      ConfigSavePending();
    }
    else
    {
      current_page = PAGE_SET_ALARM;
      GuiRefreshNow();
    }
  }
  else if (current_page == PAGE_SET_LIMIT && (evt == KEY_EVT_K1 || evt == KEY_EVT_K4))
  {
    if (evt == KEY_EVT_K1 && cfg.temp_limit_x10 <= (TEMP_LIMIT_MAX_X10 - 5))
    {
      cfg.temp_limit_x10 += 5;
      changed = 1U;
    }
    else if (evt == KEY_EVT_K4 && cfg.temp_limit_x10 >= (TEMP_LIMIT_MIN_X10 + 5))
    {
      cfg.temp_limit_x10 -= 5;
      changed = 1U;
    }
    if (changed)
    {
      UiSyncConfig();
      GuiRefreshNow();
      ConfigMarkDirty();
    }
  }
  else if (current_page == PAGE_SET_ALARM && (evt == KEY_EVT_K1 || evt == KEY_EVT_K4))
  {
    if (evt == KEY_EVT_K1 && cfg.alarm_seconds < ALARM_SECONDS_MAX)
    {
      cfg.alarm_seconds++;
      changed = 1U;
    }
    else if (evt == KEY_EVT_K4 && cfg.alarm_seconds > ALARM_SECONDS_MIN)
    {
      cfg.alarm_seconds--;
      changed = 1U;
    }
    if (changed)
    {
      UiSyncConfig();
      GuiRefreshNow();
      ConfigMarkDirty();
    }
  }
}

static void AlarmUpdate(uint32_t now)
{
  static uint32_t alarm_start = 0;
  static uint8_t over_limit_latched = 0;
  uint32_t alarm_ms;

  if (!temp_valid)
  {
    alarm_active = 0U;
    over_limit_latched = 0U;
    BEEP_Set(0);
    return;
  }

  if (current_temp_x10 > cfg.temp_limit_x10)
  {
    if (over_limit_latched == 0U)
    {
      over_limit_latched = 1U;
      if (alarm_active == 0U)
      {
        alarm_active = 1U;
        alarm_start = now;
      }
    }
  }
  else
  {
    over_limit_latched = 0U;
  }

  if (alarm_active != 0U)
  {
    alarm_ms = (uint32_t)cfg.alarm_seconds * 1000UL;
    if ((now - alarm_start) < alarm_ms)
    {
      BEEP_Set(1);
    }
    else
    {
      alarm_active = 0U;
      BEEP_Set(0);
    }
  }
  else
  {
    BEEP_Set(0);
  }
}

static void PrintText(const char *text)
{
  HAL_UART_Transmit(&huart1, (uint8_t *)text, (uint16_t)strlen(text), 1000);
}

static void UartReport(void)
{
  char temp[16];
  char limit[12];
  char line[80];

  FormatCurrentTemp(temp, sizeof(temp));
  FormatTemp(limit, cfg.temp_limit_x10);
  snprintf(line, sizeof(line), "%s,%s,%u,%u,%u\r\n",
           temp,
           limit,
           current_adc,
           (uint32_t)alarm_active,
           (uint32_t)fs_ready);
  PrintText(line);
}

static void DrawMainPage(void)
{
  char temp[16];
  char limit[12];
  char line[24];
  int16_t limit_x10 = ui_temp_limit_x10;
  uint16_t alarm_seconds = ui_alarm_seconds;

  FormatCurrentTemp(temp, sizeof(temp));
  FormatTemp(limit, limit_x10);

  GUI_Clear();
  GUI_SetFont(GUI_DEFAULT_FONT);
  GUI_DispStringAt("Temp Alarm Logger", 0, 0);
  snprintf(line, sizeof(line), "Now: %s C", temp);
  GUI_DispStringAt(line, 0, 14);
  snprintf(line, sizeof(line), "Limit: %s C", limit);
  GUI_DispStringAt(line, 0, 28);
  snprintf(line, sizeof(line), "Alarm:%lus", (uint32_t)alarm_seconds);
  GUI_DispStringAt(line, 0, 42);
  GUI_DispStringAt("VOFA:ON", 0, 56);
}

static void DrawLimitPage(void)
{
  char temp[12];
  char line[24];
  int16_t limit_x10 = ui_temp_limit_x10;

  FormatTemp(temp, limit_x10);
  GUI_Clear();
  GUI_SetFont(GUI_DEFAULT_FONT);
  GUI_DispStringAt("Temp Limit", 0, 0);
  snprintf(line, sizeof(line), "%s C", temp);
  GUI_DispStringAt(line, 0, 18);
  GUI_DispStringAt("K1:+0.5 K4:-0.5", 0, 40);
  GUI_DispStringAt("K2:Exit", 0, 54);
}

static void DrawAlarmPage(void)
{
  char line[24];
  uint16_t alarm_seconds = ui_alarm_seconds;

  GUI_Clear();
  GUI_SetFont(GUI_DEFAULT_FONT);
  GUI_DispStringAt("Alarm Time", 0, 0);
  snprintf(line, sizeof(line), "%lu seconds", (uint32_t)alarm_seconds);
  GUI_DispStringAt(line, 0, 18);
  GUI_DispStringAt("K1:+1 K4:-1", 0, 40);
  GUI_DispStringAt("K3:Exit", 0, 54);
}

static void FormatTemp(char *buf, int16_t temp_x10)
{
  if (temp_x10 < 0)
  {
    temp_x10 = -temp_x10;
    sprintf(buf, "-%d.%d", temp_x10 / 10, temp_x10 % 10);
  }
  else
  {
    sprintf(buf, "%d.%d", temp_x10 / 10, temp_x10 % 10);
  }
}

static void FormatCurrentTemp(char *buf, uint32_t size)
{
  char value[12];

  FormatTemp(value, current_temp_x10);
  if (temp_valid)
  {
    snprintf(buf, size, "%s", value);
  }
  else
  {
    snprintf(buf, size, "ERR%s", value);
  }
}

static int16_t ReadTempX10(void)
{
  static int16_t last_valid_temp_x10 = TEMP_ERR_NOT_READY_X10;
  static uint8_t stale_reads = DS18B20_MAX_STALE_READS;
  uint32_t sum = 0;
  uint32_t i;
  uint16_t adc;
  int16_t temp;
  uint8_t attempt;

  for (i = 0; i < ADC_BUF_SIZE; ++i)
  {
    sum += adc_buf[i] & 0x0FFFU;
  }

  adc = (uint16_t)(sum / ADC_BUF_SIZE);
  current_adc = adc;

  for (attempt = 0U; attempt < DS18B20_READ_ATTEMPTS; attempt++)
  {
    temp = DS18B20_ReadTempX10();
    if (temp >= -550 && temp <= 1250)
    {
      last_valid_temp_x10 = temp;
      stale_reads = 0U;
      temp_valid = 1U;
      return temp;
    }
  }

  if (last_valid_temp_x10 >= -550 && last_valid_temp_x10 <= 1250 && stale_reads < DS18B20_MAX_STALE_READS)
  {
    stale_reads++;
    temp_valid = 1U;
    return last_valid_temp_x10;
  }

  temp_valid = 0U;

  return temp;
}

static void DWT_DelayInit(void)
{
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0U;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static void DelayUs(uint32_t us)
{
  uint32_t start = DWT->CYCCNT;
  uint32_t ticks = us * (SystemCoreClock / 1000000U);

  while ((DWT->CYCCNT - start) < ticks)
  {
  }
}

static void DS18B20_ReleaseBus(void)
{
  HAL_GPIO_WritePin(DS18B20_GPIO_Port, DS18B20_Pin, GPIO_PIN_SET);
}

static int16_t DS18B20_Reset(void)
{
  DS18B20_ReleaseBus();
  DelayUs(5U);
  if (HAL_GPIO_ReadPin(DS18B20_GPIO_Port, DS18B20_Pin) == GPIO_PIN_RESET)
  {
    return TEMP_ERR_BUS_LOW_X10;
  }

  HAL_GPIO_WritePin(DS18B20_GPIO_Port, DS18B20_Pin, GPIO_PIN_RESET);
  DelayUs(480U);
  DS18B20_ReleaseBus();
  DelayUs(70U);
  if (HAL_GPIO_ReadPin(DS18B20_GPIO_Port, DS18B20_Pin) == GPIO_PIN_SET)
  {
    DelayUs(410U);
    return TEMP_ERR_NO_PRESENCE_X10;
  }

  DelayUs(410U);
  if (HAL_GPIO_ReadPin(DS18B20_GPIO_Port, DS18B20_Pin) == GPIO_PIN_RESET)
  {
    return TEMP_ERR_BUS_LOW_X10;
  }

  return 0;
}

static void DS18B20_WriteBit(uint8_t bit)
{
  HAL_GPIO_WritePin(DS18B20_GPIO_Port, DS18B20_Pin, GPIO_PIN_RESET);

  if (bit != 0U)
  {
    DelayUs(6U);
    DS18B20_ReleaseBus();
    DelayUs(64U);
  }
  else
  {
    DelayUs(60U);
    DS18B20_ReleaseBus();
    DelayUs(10U);
  }
}

static uint8_t DS18B20_ReadBit(void)
{
  uint8_t bit;

  HAL_GPIO_WritePin(DS18B20_GPIO_Port, DS18B20_Pin, GPIO_PIN_RESET);
  DelayUs(6U);
  DS18B20_ReleaseBus();
  DelayUs(9U);
  bit = (uint8_t)(HAL_GPIO_ReadPin(DS18B20_GPIO_Port, DS18B20_Pin) == GPIO_PIN_SET);
  DelayUs(55U);

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

static int16_t DS18B20_ReadTempX10(void)
{
  uint8_t scratchpad[DS18B20_SCRATCHPAD_SIZE];
  uint8_t i;
  uint8_t all_zero = 1U;
  uint8_t all_ff = 1U;
  int16_t reset;
  int16_t raw;
  int32_t temp_x10;

  reset = DS18B20_Reset();
  if (reset < 0)
  {
    return reset;
  }

  DS18B20_WriteByte(0xCCU);
  DS18B20_WriteByte(0x44U);
  osDelay(750);

  reset = DS18B20_Reset();
  if (reset < 0)
  {
    return reset;
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

  if (all_zero != 0U)
  {
    return TEMP_ERR_READ_ZERO_X10;
  }
  if (all_ff != 0U)
  {
    return TEMP_ERR_READ_FF_X10;
  }
  if (DS18B20_Crc8(scratchpad, DS18B20_SCRATCHPAD_SIZE - 1U) != scratchpad[8])
  {
    return TEMP_ERR_CRC_X10;
  }

  raw = (int16_t)((uint16_t)scratchpad[1] << 8 | scratchpad[0]);
  temp_x10 = ((int32_t)raw * 10L + ((raw >= 0) ? 8L : -8L)) / 16L;

  return (int16_t)temp_x10;
}

static KeyEvent KeyFromMask(uint16_t key)
{
  if (key & K1_Pin) return KEY_EVT_K1;
  if (key & K2_Pin) return KEY_EVT_K2;
  if (key & K3_Pin) return KEY_EVT_K3;
  if (key & K4_Pin) return KEY_EVT_K4;
  return KEY_EVT_NONE;
}

#if KEY_DIAG_ENABLE != 0U
static const char *KeyEventName(KeyEvent evt)
{
  switch (evt)
  {
    case KEY_EVT_K1:
      return "K1";
    case KEY_EVT_K2:
      return "K2";
    case KEY_EVT_K3:
      return "K3";
    case KEY_EVT_K4:
      return "K4";
    default:
      return "NONE";
  }
}
#endif
