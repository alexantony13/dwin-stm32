/*
 * dwin-itf.h
 *
 *  Created on: May 24, 2025
 *      Author: Alex Antony
 */

#ifndef DWIN_STM32_LIB_DWIN_ITF_H_
#define DWIN_STM32_LIB_DWIN_ITF_H_

#include "dwin.h"

dwin_error_t dwin_itf_uart_abort(dwin_t *dwin);
dwin_error_t dwin_itf_uart_receive_to_idle_dma(dwin_t *dwin);
dwin_error_t dwin_itf_uart_transmit_dma(dwin_t *dwin, uint16_t tx_len);

#endif /* DWIN_STM32_LIB_DWIN_ITF_H_ */
