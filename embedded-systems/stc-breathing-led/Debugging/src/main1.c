#include "../inc/STC8H.h"
#include "../inc/delay.h"

// Simple running-light: LEDs light up sequentially
sbit LED1 = P1^6;
sbit LED2 = P1^7;
sbit LED3 = P3^2;  // Test: moved from P3^2 to P1^0 to verify
sbit LED4 = P3^3;
sbit LED5 = P3^4;
sbit LED6 = P3^5;
sbit LED7 = P3^6;
sbit LED8 = P3^7;
sbit LED9 = P5^4;

void turn_off_all()
{
    LED1 = 1;
    LED2 = 1;
    LED3 = 1;
    LED4 = 1;
    LED5 = 1;
    LED6 = 1;
    LED7 = 1;
    LED8 = 1;
    LED9 = 1;
}

void main()
{
    // configure push-pull for P1/P3/P5
    P1M0 = 0x00;
    P1M1 = 0x00;
    P3M0 = 0x00;
    P3M1 = 0x00;
    P5M0 = 0x00;
    P5M1 = 0x00;

    while(1)
    {
        // LED1 on
        turn_off_all();
        LED1 = 0;
        delay_ms(2000);

        // LED2 on
        turn_off_all();
        LED2 = 0;
        delay_ms(2000);

        // LED3 on
        turn_off_all();
        LED3 = 0;
        delay_ms(2000);

        // LED4 on
        turn_off_all();
        LED4 = 0;
        delay_ms(2000);

        // LED5 on
        turn_off_all();
        LED5 = 0;
        delay_ms(2000);

        // LED6 on
        turn_off_all();
        LED6 = 0;
        delay_ms(2000);

        // LED7 on
        turn_off_all();
        LED7 = 0;
        delay_ms(2000);

        // LED8 on
        turn_off_all();
        LED8 = 0;
        delay_ms(2000);

        // LED9 on
        turn_off_all();
        LED9 = 0;
        delay_ms(2000);
    }
}
