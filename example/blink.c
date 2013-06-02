// AVR/Arduino test program
// Blinks LED on Arduino Mega 2560 board

#include <inttypes.h>
#include <avr/io.h>
#include <util/delay.h>

//-----------------------------------------------------------------------------

#define setBit(reg, bit, mode) ({if (mode) reg |= 1<<(bit); else reg &= ~(1<<(bit));})
#define pinMode(X, pin, mode)   setBit(DDR ## X, pin, mode)
#define setPin(X, pin, mode)    setBit(PORT ## X, pin, mode)

void delay(uint16_t d)
{
    while(d--) {
        volatile uint16_t i = 0x1000;
        while(i--);
    }
}

int main(void)
{
    // set up 10-bit phase-correct PWM on pin 13 (PORTB bit 7, timer 4B)
    pinMode(B, 7, 1);
    TCCR1A = 0x0B;
    TCCR1B = 0x03;
    OCR1C = 0x0000;

    while(1) {
        // ramp up once
        int i;
        for (i = 0; i < 0x0400; i++) {
            delay(0x0001);
            OCR1C = i;
        }

        // blink a few times
        for (i = 0; i < 10; i++) {
            delay(0x0010);
            OCR1C = 0x0100;
            delay(0x0010);
            OCR1C = 0x03ff;
        }
    }

    return 0;
}
