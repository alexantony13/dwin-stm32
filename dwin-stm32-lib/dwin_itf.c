/*
 * dwin_itf.c
 *
 *  Created on: May 24, 2025
 *      Author: Alex Antony
 */

#include "dwin_itf.h"
#include "main.h"

dwin_error_t dwin_itf_uart_abort(dwin_t *dwin) {
	dwin_error_t error = HAL_UART_Abort(dwin->huart);
	return error;
}

dwin_error_t dwin_itf_uart_receive_to_idle_dma(dwin_t *dwin) {
	dwin_error_t error = HAL_UARTEx_ReceiveToIdle_DMA(dwin->huart,
			dwin->rx_ring_buffer.buf_ptr, dwin->rx_ring_buffer.size);
	return error;
}

dwin_error_t dwin_itf_uart_transmit_dma(dwin_t *dwin, uint16_t tx_len) {
	dwin_error_t error = HAL_UART_Transmit_DMA(dwin->huart,
			dwin->tx_frame_buffer, tx_len);
	return error;
}
