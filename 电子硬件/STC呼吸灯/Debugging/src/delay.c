#include "../inc/delay.h"

void delay_us(unsigned char us)
{
    unsigned char i;
    while(us--) {
        i = 2;
        while(i--);
    }
}

void delay_ms(unsigned int ms)
{
    unsigned int i;
    unsigned char j;
    
    for(i = 0; i < ms; i++) {
        for(j = 0; j < 123; j++);
    }
}

void delay_s(unsigned char s)
{
    unsigned char i;
    
    for(i = 0; i < s; i++) {
        delay_ms(1000);
    }
}
