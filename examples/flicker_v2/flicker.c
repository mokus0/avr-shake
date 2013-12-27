// Candle-flicker LED program for ATtiny13A based on analysis at[0]
// and IIR filter proposed and tested at [1].
// 
// Hardware/peripheral usage notes:
// LED control output on pin 5.
// Uses hardware PWM and the PWM timer's overflow to
// count out the frames.  Keeps the RNG seed in EEPROM.
// 
// [0] http://inkofpark.wordpress.com/2013/12/15/candle-flame-flicker/
// [1] http://inkofpark.wordpress.com/2013/12/23/arduino-flickering-candle/

#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/sleep.h>
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
#define TIMER0_OVF_RATE     2343.75 // Hz
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

#ifdef INKOFPARK

// fixed-point 2nd-order Butterworth low-pass filter.
// The output has mean 0 and std deviation approximately 0.53
// (4342 or so in mantissa).
//
// integer types here are annotated with comments of the form
// "/* x:y */", where x is the number of significant bits in
// the value and y is the base-2 exponent (negated, actually).
// 
// The inspiration for the filter (and identification of basic
// parameters and comparison with some other filters) was done
// by Park Hays and described at [0].  Some variations and
// prototypes of the fixed-point version are implemented at [1].
//
// [0] http://inkofpark.wordpress.com/2013/12/23/arduino-flickering-candle/
// [1] https://github.com/mokus0/junkbox/blob/master/Haskell/Math/BiQuad.hs
#define UPDATE_RATE             60 // Hz
#define FILTER_NORMALIZATION    4313.0
static int16_t /* 15:13 */ flicker_filter(int16_t /* 7:5 */ x) {
    const int16_t // TODO: make sure these constants are inlined
        /* 3:3  */ a1 = -7,  // round ((-0.87727063) * (1L << 3 ))
        /* 4:5  */ a2 = 10,  // round (  0.31106039  * (1L << 5 ))
        /* 7:10 */ b0 = 111, // round (  0.10844744  * (1L << 10))
        /* 7:9  */ b1 = 111, // round (  0.21689488  * (1L << 9 ))
        /* 7:10 */ b2 = 111; // round (  0.10844744  * (1L << 10))
    static int16_t
        /* 15:13 */ d1 = 0,
        /* 15:14 */ d2 = 0;
    
    int16_t /* 15:13 */ y;
    
    // take advantage of fact that b's are all equal with the 
    // chosen exponents (this is a general feature of digital
    // butterworth filters, actually)
    int16_t /* 15:14 */ bx = b0 * x >> 1;
    y  = (bx >> 1)                   + (d1 >> 0);
    d1 = (bx >> 0) - (a1 * (y >> 3)) + (d2 >> 1);
    d2 = (bx >> 0) - (a2 * (y >> 4));
    
    return y;
}

#else

// another scipy-designed filter with 4Hz cutoff
// and 100Hz sample rate.
//
// The higher sampling rate puts the nyquist frequency at
// 50 Hz (digital butterworth filters appear to roll off
// to -inf a bit below there), which probably doesn't make
// that much difference visually.
// 
// SciPy's butterworth parameter designer seems to put the
// cutoff frequency higher than advertised, so I pulled it
// down to something that looked more like the right place
// (using signal.freqz to plot response), yielding the
// following filter.
//
// TODO: other improvements to both performance and accuracy
// specifically:
//  * factor b0 out of b's and into normalization
//  * try other forms
//
// >>> rate=100
// >>> cutoff = 4/(rate/2.)
// >>> b, a = signal.butter(2, cutoff/2, 'low', analog=False)
#define UPDATE_RATE             100 // Hz
#define FILTER_NORMALIZATION    10e3
static int16_t /* 15:13 */ flicker_filter(int16_t /* 7:5 */ x) {
    const int16_t // TODO: make sure these constants are inlined
        /* 7:6  */ a1 = -117, // round ((-1.82269493) * (1L << 6 ))
        /* 7:7  */ a2 =  107, // round (  0.83718165  * (1L << 7 ))
        /* 7:15 */ b0 =  119, // round (  0.00362168  * (1L << 15))
        /* 7:14 */ b1 =  119, // round (  0.00724336  * (1L << 14))
        /* 7:15 */ b2 =  119; // round (  0.00362168  * (1L << 15))
    static int16_t
        /* 15:14 */ d1 = 0,
        /* 15:14 */ d2 = 0;
    
    int16_t /* 15:14 */ y;
    
    y  = ((b0 * x) >> 6)                        + (d1 >> 0);
    d1 = ((b1 * x) >> 5) - (a1 * (y >> 8) << 2) + (d2 >> 0);
    d2 = ((b2 * x) >> 6) - (a2 * (y >> 8) << 1);
    
    return y;
}

#endif

static uint8_t next_intensity() {
    const uint8_t wind = 84, m = 255 - wind;
    const int16_t scale = (2 * FILTER_NORMALIZATION) / wind;
    
    int16_t x = m + flicker_filter(normal()) / scale;
    return x < 0 ? 0 : x > 255 ? 255 : x;
}

// use timer so update rate isn't
// affected by calculation time
static volatile bool tick = false;
ISR(TIM0_OVF_vect) {
    static uint8_t cycles = 0;
    if (!cycles--) {
        tick = true;
        cycles = TIMER0_OVF_RATE / UPDATE_RATE;
    }
}

int main(void)
{
    init_rand();
    init_pwm();
    
    // enable timer overflow interrupt
    TIMSK0 = 1 << TOIE0;
    sei();
    
    while(1)
    {
        // sleep till next update, then perform it.
        while (!tick) sleep_mode();
        tick = false;
        
        OCR0A = next_intensity();
    }
}