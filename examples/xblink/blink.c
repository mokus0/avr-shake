#define HAVE_XTAL       0
#define BLINK_DELAY_MS  500

#include <avr/io.h>
#include <util/delay.h>

int main(void)
{
    PORTC.DIRSET = 0b00000111;
    
    if (HAVE_XTAL) {
        // ========= System Clock configuration =========
        // Set to external 16Mhz crystal, using the PLL at *2
        // set it to be a 12-16Mhz crystal with a slow start-up time.
        OSC.XOSCCTRL = OSC_FRQRANGE_12TO16_gc | OSC_XOSCSEL_XTAL_16KCLK_gc ;
        OSC.CTRL |= OSC_XOSCEN_bm ; // enable it
        while( (OSC.STATUS & OSC_XOSCRDY_bm) == 0 ){} // wait until it's stable

        // The external crystal is now running and stable.
        // (Note that it's not yet selected as the clock source)
        // Now configure the PLL to be eXternal oscillator * 8
        OSC.PLLCTRL = OSC_PLLSRC_XOSC_gc | 8;

        // now enable the PLL...
        OSC.CTRL |= OSC_PLLEN_bm ; // enable the PLL...
        while( (OSC.STATUS & OSC_PLLRDY_bm) == 0 ){} // wait until it's stable

        // set peripheral prescalers.
        // should end up with 
        CCP = CCP_IOREG_gc;   // protected write follows
        CLK.PSCTRL = CLK_PSADIV_1_gc | CLK_PSBCDIV_2_2_gc;
        
        // And now, *finally*, we can switch from the internal 2Mhz clock to the PLL
        CCP = CCP_IOREG_gc;   // protected write follows
        CLK.CTRL = CLK_SCLKSEL_PLL_gc;   // The System clock is now  PLL (16Mhz * 2)
    } else {
        CCP = CCP_IOREG_gc;              // disable register security for oscillator update
        OSC.CTRL = OSC_RC32MEN_bm;       // enable 32MHz oscillator
        while(!(OSC.STATUS & OSC_RC32MRDY_bm)); // wait for oscillator to be ready
        CCP = CCP_IOREG_gc;              // disable register security for clock update
        CLK.CTRL = CLK_SCLKSEL_RC32M_gc; // switch to 32MHz clock
    }
    
    //CCP = CCP_IOREG_gc;
    //CLK.LOCK = 1;
    
    // Set up 3-channel 16-bit dual-slope pwm on PORTC 0-2
    TCC0.PER = 0xFFFF;
    
    TCC0.CTRLB = TC_WGMODE_DSBOTH_gc | TC0_CCAEN_bm | TC0_CCBEN_bm | TC0_CCCEN_bm;
    TCC0.CTRLC = 0;
    TCC0.CTRLD = 0;
    TCC0.CTRLE = 0;
    
    TCC0.CCA = 0;
    TCC0.CCB = 0;
    TCC0.CCC = 0;
    
    // start the timer
    TCC0.CTRLA = TC_CLKSEL_DIV1_gc;
    
    PORTC.DIRSET = 0b00000111;
    
    while(1){
        _delay_ms( BLINK_DELAY_MS ) ;
        TCC0.CCABUF = TCC0.CCBBUF = TCC0.CCCBUF = 0x4000;
        _delay_ms( BLINK_DELAY_MS ) ;
        TCC0.CCABUF = TCC0.CCBBUF = TCC0.CCCBUF = 0x1000;
    }}