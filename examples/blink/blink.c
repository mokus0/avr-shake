// AVR/Arduino test program
// Blinks LED on ATtiny13A

#include <avr/io.h>
#include <util/delay.h>

int main(void)
{
    DDRB    = 1 << DDB4;
    PORTB   = 1 << PORTB4;
    
    while(1)
    {
        _delay_ms(250);
        PORTB = 0 << PORTB4;
        _delay_ms(250);
        PORTB = 1 << PORTB4;
    }
}