#include "../inc/STC8H.h"
#include "../inc/delay.h"

#define LED1 P16
#define LED2 P17
#define LED3 P32
#define LED4 P33
#define LED5 P34
#define LED6 P35
#define LED7 P36
#define LED8 P37
#define LED9 P54

#define PWM_MAX 100
#define BREATH_REPEAT 6

const unsigned char breath_table[] = {
    0, 1, 2, 3, 5, 7, 10, 13, 17, 22,
    28, 35, 43, 52, 62, 73, 84, 92, 97, 100,
    97, 92, 84, 73, 62, 52, 43, 35, 28, 22,
    17, 13, 10, 7, 5, 3, 2, 1
};

void led_write(unsigned char on)
{
    // active low: 0 = ON, 1 = OFF
    LED1 = on ? 0 : 1;
    LED2 = on ? 0 : 1;
    LED3 = on ? 0 : 1;
    LED4 = on ? 0 : 1;
    LED5 = on ? 0 : 1;
    LED6 = on ? 0 : 1;
    LED7 = on ? 0 : 1;
    LED8 = on ? 0 : 1;
    LED9 = on ? 0 : 1;
}

void io_init(void)
{
    // STC8H IO mode: M1=0, M0=1 means push-pull output.
    P1M1 &= ~0xC0;
    P1M0 |= 0xC0;
    P3M1 &= ~0xFC;
    P3M0 |= 0xFC;
    P5M1 &= ~0x10;
    P5M0 |= 0x10;

    led_write(0);
}

void pwm_frame(unsigned char duty)
{
    unsigned char pwm_step;

    for (pwm_step = 0; pwm_step < PWM_MAX; pwm_step++) {
        led_write(pwm_step < duty);
        delay_us(60);
    }
}

void main()
{
    unsigned char i;
    unsigned char repeat;
    unsigned char table_len;

    io_init();
    table_len = sizeof(breath_table) / sizeof(breath_table[0]);

    while (1) {
        for (i = 0; i < table_len; i++) {
            for (repeat = 0; repeat < BREATH_REPEAT; repeat++) {
                pwm_frame(breath_table[i]);
            }
        }
    }
}
