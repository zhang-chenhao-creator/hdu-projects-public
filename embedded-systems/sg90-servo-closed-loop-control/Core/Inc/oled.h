#ifndef __OLED_H
#define __OLED_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "i2c.h"

/* OLED 参数定义 */
#define OLED_I2C            hi2c1
#define OLED_ADDR           (0x3C << 1)  /* SSD1306地址 */
#define OLED_WIDTH          128
#define OLED_HEIGHT         64
#define OLED_PAGES          8

/* 命令定义 */
#define OLED_CMD_DISPLAY_OFF        0xAE
#define OLED_CMD_DISPLAY_ON         0xAF
#define OLED_CMD_SET_MUX_RATIO      0xA8
#define OLED_CMD_SET_DISPLAY_OFFSET 0xD3
#define OLED_CMD_SET_DISPLAY_START  0x40
#define OLED_CMD_SET_SEGMENT_REMAP  0xA1
#define OLED_CMD_SET_COM_SCAN_DEC   0xC8
#define OLED_CMD_SET_COM_PINS       0xDA
#define OLED_CMD_SET_CONTRAST       0x81
#define OLED_CMD_SET_PRECHARGE      0xD9
#define OLED_CMD_SET_VCOMH          0xDB
#define OLED_CMD_SET_CHARGE_PUMP    0x8D
#define OLED_CMD_MEMORY_MODE        0x20
#define OLED_CMD_COLUMN_ADDR        0x21
#define OLED_CMD_PAGE_ADDR          0x22

extern I2C_HandleTypeDef hi2c1;

/* 函数声明 */
HAL_StatusTypeDef OLED_Init(void);
void OLED_WriteCmd(uint8_t cmd);
void OLED_WriteData(uint8_t data);
void OLED_SetCursor(uint8_t x, uint8_t y);
void OLED_Clear(void);
void OLED_Refresh(void);
void OLED_DrawPixel(uint8_t x, uint8_t y, uint8_t color);
void OLED_DrawChar(uint8_t x, uint8_t y, char ch, uint8_t size);
void OLED_ShowString(uint8_t x, uint8_t y, const char *str, uint8_t size);
void OLED_ShowNum(uint8_t x, uint8_t y, uint32_t num, uint8_t len);
void OLED_Fill(uint8_t dat);

#ifdef __cplusplus
}
#endif

#endif /* __OLED_H */