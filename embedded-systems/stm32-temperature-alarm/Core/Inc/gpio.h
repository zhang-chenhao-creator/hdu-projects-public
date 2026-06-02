#ifndef __GPIO_H__
#define __GPIO_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

void MX_GPIO_Init(void);
uint16_t KEY_Scan(void);
void BEEP_Set(uint8_t on);

#ifdef __cplusplus
}
#endif

#endif
