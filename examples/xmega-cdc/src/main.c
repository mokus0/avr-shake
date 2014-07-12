#include "conf_clock.h"

#include <avr/io.h>
#include <board.h>
#include <pmic.h>
#include <usart.h>
#include <sleepmgr.h>
#include <stdbool.h>
#include <stdint.h>
#include <sysclk.h>
#include <udc.h>
#include <udi_cdc.h>
#include <avr/pgmspace.h>

FUSES =
{
    .FUSEBYTE1 = FUSE1_DEFAULT,
    .FUSEBYTE2 = FUSE2_DEFAULT,
    .FUSEBYTE4 = FUSE4_DEFAULT,
    .FUSEBYTE5 = FUSE5_DEFAULT,
};

static const USART_t * const port_usarts[UDI_CDC_PORT_NB] PROGMEM =
{
    &USARTC0, &USARTC1, &USARTD0, &USARTE0
};
static usart_rs232_options_t port_configurations[UDI_CDC_PORT_NB];

int main(void)
{
    sysclk_init();
    sleepmgr_init();
    pmic_init();
    
    board_init();
    cpu_irq_enable();
    
    // TODO: pull into board-init?
    // TODO: set up pins for RTS/DTR
    PORTC.DIRSET = PIN3_bm | PIN7_bm;
    PORTD.DIRSET = PIN3_bm | PIN7_bm;
    PORTE.DIRSET = PIN3_bm;
    
    // initialize port configurations
    // TODO: save in EEPROM
    const usb_cdc_line_coding_t default_port_config = {
        dwDTERate:      UDI_CDC_DEFAULT_RATE,
        bCharFormat:    UDI_CDC_DEFAULT_STOPBITS,
        bParityType:    UDI_CDC_DEFAULT_PARITY,
        bDataBits:      UDI_CDC_DEFAULT_DATABITS,
    };
    for (uint8_t port = 0; port < UDI_CDC_PORT_NB; port++)
    {
        cdc_uart_config(port, &default_port_config);
    }
    
    udc_start();
    udc_attach();
    
    while (true) {
        sleepmgr_enter_sleep();
    }
}

USART_t *get_port_usart(uint8_t port)
{
    if (port >= UDI_CDC_PORT_NB) return NULL;
    
    USART_t *usart;
    memcpy_P(&usart, port_usarts + port, sizeof usart);
    
    return usart;
}

bool cdc_enable(uint8_t port)
{
    USART_t *usart = get_port_usart(port);
    if (usart == NULL) return false;
    
    if (!usart_init_rs232(usart, port_configurations + port)) return false;
    
    usart->CTRLA = USART_RXCINTLVL_HI_gc | USART_DREINTLVL_HI_gc;
    
    return true;
}

void cdc_disable(uint8_t port)
{
    USART_t *usart = get_port_usart(port);
    if (usart == NULL) return;
    
    usart->CTRLA = USART_RXCINTLVL_OFF_gc | USART_DREINTLVL_OFF_gc;
    
    usart_tx_disable(usart);
    usart_rx_disable(usart);
    sysclk_disable_peripheral_clock(usart);
}

static bool cdc_usart_is_enabled(const USART_t * const usart)
{
    return usart->CTRLA != 0;
}

static USART_CHSIZE_t lookup_charlength(uint8_t bits)
{
    switch (bits)
    {
        case 5:     return USART_CHSIZE_5BIT_gc;
        case 6:     return USART_CHSIZE_6BIT_gc;
        case 7:     return USART_CHSIZE_7BIT_gc;
        default:
        case 8:     return USART_CHSIZE_8BIT_gc;
        case 9:     return USART_CHSIZE_9BIT_gc;
    }
}

static USART_PMODE_t lookup_paritytype(uint8_t parity)
{
    switch (parity)
    {
        case CDC_PAR_EVEN:  return USART_PMODE_EVEN_gc;
        case CDC_PAR_ODD:   return USART_PMODE_ODD_gc;
        default:
        case CDC_PAR_NONE:  return USART_PMODE_DISABLED_gc;
    }
}

void cdc_uart_config(uint8_t port, const usb_cdc_line_coding_t * cfg)
{
    USART_t *usart = get_port_usart(port);
    if (usart == NULL) return;
    
    port_configurations[port] = (usart_rs232_options_t)
    {
        baudrate:   cfg->dwDTERate,
        charlength: lookup_charlength(cfg->bDataBits),
        paritytype: lookup_paritytype(cfg->bParityType),
        stopbits:   cfg->bCharFormat == CDC_STOP_BITS_2,
    };
    
    if (cdc_usart_is_enabled(usart)) usart_init_rs232(usart, port_configurations + port);
}

void cdc_data_received(uint8_t port)
{
    USART_t *usart = get_port_usart(port);
    if (usart == NULL) return;
    
    if (cdc_usart_is_enabled(usart))
    {
        // Enable UART TX interrupt to send values
        usart->CTRLA = USART_RXCINTLVL_HI_gc | USART_DREINTLVL_HI_gc;
    }
}

void cdc_set_dtr(uint8_t port, bool b_enable)
{
    USART_t *usart = get_port_usart(port);
    if (usart == NULL) return;
    
    if (cdc_usart_is_enabled(usart))
    {
        // do something
    }
}

void cdc_set_rts(uint8_t port, bool b_enable)
{
    USART_t *usart = get_port_usart(port);
    if (usart == NULL) return;
    
    if (cdc_usart_is_enabled(usart))
    {
        // do something
    }
}

#define USART_ISRS(port,usart)                                              \
    ISR(usart ## _RXC_vect)                                                 \
    {                                                                       \
        uint8_t in_byte = usart.DATA;                                       \
                                                                            \
        if (usart.STATUS & (USART_FERR_bm | USART_BUFOVF_bm)) {             \
            udi_cdc_multi_signal_framing_error(port);                       \
        }                                                                   \
                                                                            \
        if (udi_cdc_multi_is_tx_ready(port))                                \
        {                                                                   \
            udi_cdc_multi_putc(port, in_byte);                              \
        }                                                                   \
        else                                                                \
        {                                                                   \
            udi_cdc_multi_signal_overrun(port);                             \
        }                                                                   \
    }                                                                       \
    ISR(usart ## _DRE_vect)                                                 \
    {                                                                       \
        if (udi_cdc_multi_is_rx_ready(port)) {                              \
            usart.DATA = udi_cdc_multi_getc(port);                          \
        } else {                                                            \
            usart.CTRLA = USART_RXCINTLVL_HI_gc | USART_DREINTLVL_OFF_gc;   \
        }                                                                   \
    }


USART_ISRS(0, USARTC0)
USART_ISRS(1, USARTC1)
USART_ISRS(2, USARTD0)
USART_ISRS(3, USARTE0)
