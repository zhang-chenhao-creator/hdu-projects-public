#ifndef __OLED_H
#define __OLED_H

#include "stm32f4xx_hal.h"

#define OLED_ADDRESS 0x78  // 7位地址0x3C左移1位

void OLED_Init(void);
void OLED_Clear(void);
void OLED_SetPos(uint8_t x, uint8_t y);
void OLED_ShowChar(uint8_t x, uint8_t y, char chr);
void OLED_ShowString(uint8_t x, uint8_t y, char *str, uint8_t size);
void OLED_ShowChinese(uint8_t x, uint8_t y, uint8_t no);
void OLED_Show_Menu(uint8_t selected_page);
void OLED_Refresh(void);

#endif