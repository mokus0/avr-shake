#ifndef ___n_main_h__
#define ___n_main_h__

#include <usb_protocol_cdc.h>
#include <stdbool.h>
#include <stdint.h>

void usb_suspend(void);
void usb_resume(void);

bool cdc_enable(uint8_t port);
void cdc_disable(uint8_t port);
void cdc_uart_config(uint8_t port, const usb_cdc_line_coding_t * cfg);
void cdc_data_received(uint8_t port);
void cdc_set_dtr(uint8_t port, bool b_enable);
void cdc_set_rts(uint8_t port, bool b_enable);

#endif /* ___n_main_h__ */
