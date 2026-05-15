#include "oled.h"
#include "i2c.h"  // CubeMX生成的I2C头文件
#include "oled_font.h"

extern I2C_HandleTypeDef hi2c1;  // CubeMX生成的I2C句柄

// 向OLED写入一个字节
static void OLED_WriteByte(uint8_t data, uint8_t cmd) {
    uint8_t buf[2];
    buf[0] = cmd ? 0x40 : 0x00;  // 0x00=命令, 0x40=数据[^1^]
    buf[1] = data;
    HAL_I2C_Master_Transmit(&hi2c1, OLED_ADDRESS, buf, 2, 100);
}

// 初始化OLED
void OLED_Init(void) {
    HAL_Delay(100);
    OLED_WriteByte(0xAE, 0);  // 关闭显示
    OLED_WriteByte(0x20, 0);  // 设置内存寻址模式
    OLED_WriteByte(0x10, 0);  // 00=水平,01=垂直,10=页
    OLED_WriteByte(0xB0, 0);  // 设置页起始地址
    OLED_WriteByte(0xC8, 0);  // 设置COM输出扫描方向
    OLED_WriteByte(0x00, 0);  // 设置低列地址
    OLED_WriteByte(0x10, 0);  // 设置高列地址
    OLED_WriteByte(0x40, 0);  // 设置起始行
    OLED_WriteByte(0x81, 0);  // 设置对比度
    OLED_WriteByte(0xCF, 0);  // 对比度值
    OLED_WriteByte(0xA1, 0);  // 设置段重映射
    OLED_WriteByte(0xA6, 0);  // 正常显示(非反相)
    OLED_WriteByte(0xA8, 0);  // 设置复用比
    OLED_WriteByte(0x3F, 0);  // 1/64复用
    OLED_WriteByte(0xA4, 0);  // 显示RAM内容
    OLED_WriteByte(0xD3, 0);  // 设置显示偏移
    OLED_WriteByte(0x00, 0);  // 无偏移
    OLED_WriteByte(0xD5, 0);  // 设置显示时钟分频
    OLED_WriteByte(0xF0, 0);  // 分频比
    OLED_WriteByte(0xD9, 0);  // 设置预充电周期
    OLED_WriteByte(0xF1, 0);  // 周期值
    OLED_WriteByte(0xDA, 0);  // 设置COM引脚配置
    OLED_WriteByte(0x12, 0);  // 配置值
    OLED_WriteByte(0xDB, 0);  // 设置VCOMH取消选择电平
    OLED_WriteByte(0x40, 0);  // 电平值
    OLED_WriteByte(0x8D, 0);  // 设置电荷泵
    OLED_WriteByte(0x14, 0);  // 启用电荷泵
    OLED_WriteByte(0xAF, 0);  // 开启显示
}

// 清屏
void OLED_Clear(void) {
    for(uint8_t i=0; i<8; i++) {
        OLED_WriteByte(0xB0 + i, 0);  // 设置页地址
        OLED_WriteByte(0x00, 0);      // 设置列低地址
        OLED_WriteByte(0x10, 0);      // 设置列高地址
        for(uint8_t j=0; j<128; j++) {
            OLED_WriteByte(0x00, 1);  // 写入数据0
        }
    }
}

// 设置光标位置
void OLED_SetPos(uint8_t x, uint8_t y) {
    OLED_WriteByte(0xb0 + y, 0);
    OLED_WriteByte(((x & 0xf0) >> 4) | 0x10, 0);
    OLED_WriteByte((x & 0x0f), 0);
}

// 显示字符
void OLED_ShowChar(uint8_t x, uint8_t y, char chr) {
    uint8_t c = 0, i = 0;
    c = chr - ' '; // 得到偏移后的值
    if(x > 120) { x = 0; y += 2; }
    OLED_SetPos(x, y);
    for(i = 0; i < 8; i++)
        OLED_WriteByte(F8X16[c][i], 1);
    OLED_SetPos(x, y + 1);
    for(i = 0; i < 8; i++)
        OLED_WriteByte(F8X16[c][i + 8], 1);
}

// 显示字符串
void OLED_ShowString(uint8_t x, uint8_t y, char *str, uint8_t size) {
    while(*str != '\0') {
        OLED_ShowChar(x, y, *str);
        x += 8;
        if(x > 120) { x = 0; y += 2; }
        str++;
    }
}

// 显示汉字 (使用自定义字库)
void OLED_ShowChinese(uint8_t x, uint8_t y, uint8_t no) {
    uint8_t t;
    OLED_SetPos(x, y);
    for(t = 0; t < 16; t++) {
        OLED_WriteByte(zh16x16[no][t + 4], 1); // 跳过前4个字节
    }
    OLED_SetPos(x, y + 1);
    for(t = 0; t < 16; t++) {
        OLED_WriteByte(zh16x16[no][t + 16 + 4], 1); // 跳过前4个字节
    }
}

// 刷新显示（如使用显存时需要）
void OLED_Refresh(void) {
    // 如果使用显存方式，在此刷新
}

void OLED_Show_Menu(uint8_t selected_page)
{
    OLED_Clear();
    
    // Page 0: Info
    if(selected_page == 0) OLED_ShowString(0, 0, "-> Info", 16);
    else OLED_ShowString(0, 0, "   Info", 16);
    
    // Page 1: Mode
    if(selected_page == 1) OLED_ShowString(0, 2, "-> Mode", 16);
    else OLED_ShowString(0, 2, "   Mode", 16);
    
    // Page 2: Freq
    if(selected_page == 2) OLED_ShowString(0, 4, "-> Freq", 16);
    else OLED_ShowString(0, 4, "   Freq", 16);
    
    // Page 3: Duty
    if(selected_page == 3) OLED_ShowString(0, 6, "-> Duty", 16);
    else OLED_ShowString(0, 6, "   Duty", 16);
}