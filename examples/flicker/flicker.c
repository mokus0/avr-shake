// Candle-flicker LED program for ATtiny13A
// loosely based on analysis at[0].  Uses hardware PWM, keeps the RNG 
// seed in EEPROM, fades between frames, and supports a "wind" input
// (currently just driven by a random walk).
// 
// [0] http://inkofpark.wordpress.com/2013/12/15/candle-flame-flicker/

#include <avr/eeprom.h>
#include <avr/io.h>
#include <util/delay.h>

#include <stdbool.h>
#include <stdint.h>

// simple 32-bit LFSR PRNG with seed stored in EEPROM
#define POLY 0xA3AC183Cul
static uint32_t lfsr = 1;
static void init_rand() {
    static uint32_t EEMEM boot_seed = 1;
    
    lfsr = eeprom_read_dword(&boot_seed);
    // increment at least once, skip 0 if we hit it
    // (0 is the only truly unacceptable seed)
    do {lfsr++;} while (!lfsr);
    eeprom_write_dword(&boot_seed, lfsr);
}
static inline uint8_t rand(uint8_t bits) {
    uint8_t x = 0;
    uint8_t i;
    
    for (i = 0; i < bits; i++) {
        x <<= 1;
        lfsr = (lfsr >> 1) ^ (-(lfsr & 1ul) & POLY);
        x |= lfsr & 1;
    }
    
    return x;
}

// set up PWM on pin 5 (PORTB bit 0) using TIMER0 (OC0A)
static void init_pwm() {
    // COM0A    = 10  ("normal view")
    // COM0B    = 00  (disabled)
    // WGM0     = 001 (phase-correct PWM, TOP = 0xff, OCR update at TOP)
    // CS       = 010 (1:8 with IO clock: 2342 Hz PWM if clock is 9.6Mhz )
    TCCR0A  = 0x81;         // 1000 0001
    TCCR0B  = 0x02;         // 0000 0010
    DDRB   |= 1 << DDB0;    // pin direction = OUT
}

// approximate a normal distribution with mean 0 and std 32.
// does so by drawing from a binomial distribution and 'fuzzing' it a bit.
// 
// There's some code at [0] calculating the actual distribution of these
// values.  They fit the intended distribution quite well.  There is a
// grand total of 3.11% of the probability misallocated, and no more than
// 1.6% of the total misallocation affects any single bin.  In absolute
// terms, no bin is more than 0.05% off the intended priority, and most
// are considerably closer.
// 
// [0] https://github.com/mokus0/junkbox/blob/master/Haskell/Math/ApproxNormal.hs
static int8_t normal() {
    // n = binomial(16, 0.5): range = 0..15, mean = 8, sd = 2
    // center = (n - 8) * 16; // shift and expand to range = -128 .. 112, mean = 0, sd = 32
    int8_t center = -128;
    uint8_t i;
    for (i = 0; i < 16; i++) {
        center += rand(1) << 4;
    }
    
    // 'fuzz' follows a symmetric triangular distribution with 
    // center 0 and halfwidth 16, so the result (center + fuzz)
    // is a linear interpolation of the binomial PDF, mod 256.
    // (integer overflow corresponds to wrapping around, blending
    // both tails together).
    int8_t fuzz = (int8_t)(rand(4)) - (int8_t)(rand(4));
    return center + fuzz;
}

static uint8_t next_intensity(uint8_t wind) {
    int16_t m = 255 - wind, s = wind >> 1;
    int16_t x = m + ((s * (int16_t) normal()) >> 5);
    return x < 0 ? 0 : x > 255 ? 255 : x;
}

// alternate version based on [0].  Not as good, IMO, but certainly
// much simpler.
//
// [0] http://hackaday.com/2013/12/16/reverse-engineering-a-candle-flicker-led/
// static uint8_t next_intensity() {
//     uint8_t x = 0;
//     uint8_t tries = 4;
//     while (x < 4 && tries--) x = rand(5);
//     return x >= 16 ? 255 : x << 4;
// }

// linear interpolation:
// interpreting alpha as a fraction (fixed-point with exponent -8),
// compute alpha blend of x over y.
static uint8_t lerp(uint8_t alpha, uint8_t x, uint8_t y) {
    uint16_t a = alpha, xx = x, yy = y;
    return (a * xx + (256 - a) * yy) >> 8;
}

#define DT          60 // msec per sample
#define WIND_LOW    70
#define WIND_HIGH   92
int main(void)
{
    init_rand();
    init_pwm();
    
    uint8_t wind = 255;
    uint8_t a = next_intensity(wind);
    while(1)
    {
        uint8_t wind_change = WIND_LOW + rand(8) % (WIND_HIGH - WIND_LOW);
        wind = lerp(64, wind, wind_change);
        
        uint8_t b = next_intensity(wind);
        
        // simple 4-step linear fade from a to b
        uint8_t del     = (b - a) >> 2;
        uint8_t del2    = (b - a) >> 1;
        
        OCR0A = a + del;
        _delay_ms(DT/4);
        OCR0A = a + del2;
        _delay_ms(DT/4);
        OCR0A = b - del;
        _delay_ms(DT/4);
        OCR0A = b;
        _delay_ms(DT/4);
        
        a = b;
    }
}