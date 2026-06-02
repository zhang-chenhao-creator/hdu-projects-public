#include "ff.h"
#include "rtc.h"

DWORD get_fattime(void)
{
  RTC_TimeTypeDef time = {0};
  RTC_DateTypeDef date = {0};
  DWORD year;

  HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BIN);
  HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BIN);

  year = (DWORD)(date.Year + 20U);
  return (year << 25) |
         ((DWORD)date.Month << 21) |
         ((DWORD)date.Date << 16) |
         ((DWORD)time.Hours << 11) |
         ((DWORD)time.Minutes << 5) |
         ((DWORD)(time.Seconds / 2U));
}
