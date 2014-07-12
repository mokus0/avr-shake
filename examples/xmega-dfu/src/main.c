#include <avr/io.h>
#include <board.h>
#include <pmic.h>
#include <sysclk.h>
#include <udc.h>
#include <nvm.h>
#include <reset_cause.h>

#define ISP_PORT_DIR      PORTB_DIR
#define ISP_PORT_PINCTRL  PORTB_PIN0CTRL
#define ISP_PORT_IN       PORTB_IN
#define ISP_PORT_PIN      0

FUSES =
{
    .FUSEBYTE1 = FUSE1_DEFAULT,
    .FUSEBYTE2 = FUSE2_DEFAULT & FUSE_BOOTRST,
    .FUSEBYTE4 = FUSE4_DEFAULT,
    .FUSEBYTE5 = FUSE5_DEFAULT,
};

static bool flash_is_empty(void)
{
    return nvm_flash_read_word(0) == 0xFFFF;
}

static bool check_hardware_condition(void)
{
    ISP_PORT_DIR        = 0;
    ISP_PORT_PINCTRL    = PORT_OPC_PULLUP_gc;
    
    volatile uint8_t spin = 0xFF;
    while (--spin);
    
    return (ISP_PORT_IN & (1 << ISP_PORT_PIN)) == 0;
}

extern uint16_t start_app_key;
static void start_app(void)
{
    ISP_PORT_PINCTRL = 0;
    
    if (start_app_key == 0x55AA)
    {
        // resetting due to DFU reset-to-app operation, make sure we get there.
        start_app_key = 0;
        reset_cause_clear_causes(CHIP_RESET_CAUSE_SOFT);
    }
    
    // TODO: figure out what else needs reset... need to reset stack?
    // probably not, the application boot code probably already initializes
    // a new one.
    EIND = 0;
    void (*zero)(void) = 0;
    zero();
}

static void start_boot(void)
{
    pmic_init();
    pmic_set_vector_location(PMIC_VEC_BOOT);
    
    sysclk_init();
    board_init();
    cpu_irq_enable();
    
    udc_start();
    udc_attach();
    
    while (true);
}

int main(void)
{
    if (reset_cause_is_software_reset()) start_app();
    if (check_hardware_condition() || flash_is_empty()) start_boot();
    
    start_app();
}
