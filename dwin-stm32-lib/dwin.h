/*
 * dwin.h
 *
 *  Created on: Aug 19, 2023
 *      Author: Alex Antony
 */

#ifndef INC_DWIN_H_
#define INC_DWIN_H_

#include "stdint.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define DWIN_RX_CIRC_BUF_MAX_LEN 64
#define DWIN_RX_FRAME_MAX_LEN 16
#define DWIN_TX_FRAME_MAX_LEN 16
#define DWIN_CALLBACK_ADDR_MAX_COUNT 8

typedef enum dwin_status_t {
	DWIN_STATUS_OK, DWIN_STATUS_UART_ERROR,
} dwin_status_t;

typedef enum dwin_rx_status_t {
	DWIN_RX_STATUS_WAITING_HEADER,
	DWIN_RX_STATUS_WAITING_FRAME_LEN,
	DWIN_RX_STATUS_WAITING_FN_CODE,
	DWIN_RX_STATUS_WAITING_DATA,
	DWIN_RX_STATUS_DATA_RECEIVED,
} dwin_rx_status_t;

typedef enum dwin_tx_status_t {
	DWIN_TX_STATUS_IDLE,
	DWIN_TX_STATUS_TX_BUSY,
	DWIN_TX_STATUS_TX_CMPLT,
	DWIN_TX_STATUS_ACK_WAITING,
	DWIN_TX_STATUS_ACK,
} dwin_tx_status_t;

typedef enum dwin_error_t {
	DWIN_ERROR_NOERR, DWIN_ERROR_ERR, DWIN_ERROR_BUSY, DWIN_ERROR_TIMEOUT
} dwin_error_t;

typedef struct dwin_ring_buffer_t {
	uint8_t *buf_ptr;
	int16_t size;
	int16_t head_index, tail_index;
} dwin_ring_buffer_t;

typedef void (*dwin_cb_fn_t)(uint16_t);

typedef struct dwin_t {
	void *huart;
	dwin_ring_buffer_t rx_ring_buffer;

	dwin_status_t status;

	dwin_rx_status_t rx_status;
	uint8_t rx_frame_buffer[DWIN_RX_FRAME_MAX_LEN];
	uint16_t rx_data_byte_count, rx_data_byte_len;
	uint32_t rx_frame_start_tick, rx_frame_timeout_ticks;

	dwin_tx_status_t tx_status;
	uint8_t tx_frame_buffer[DWIN_TX_FRAME_MAX_LEN];
	uint32_t tx_last_sent_tick, tx_timeout_ticks;

	int16_t cb_address[DWIN_CALLBACK_ADDR_MAX_COUNT];
	dwin_cb_fn_t cb_fn[DWIN_CALLBACK_ADDR_MAX_COUNT];
} dwin_t;

uint8_t dwin_init(dwin_t *dwin);
uint8_t dwin_process(dwin_t *dwin, uint32_t c_tick);
uint8_t dwin_write_vp(dwin_t *dwin, uint16_t vp_start_addr, uint16_t *data,
		uint16_t data_len, uint32_t ctick);
uint8_t dwin_reg_cb(dwin_t *dwin, uint16_t watch_address, dwin_cb_fn_t cb_fn);
uint8_t dwin_is_tx_idle(dwin_t *dwin);

/*
 * Call from HAL_UARTEx_RxEventCallback in main.c
 * void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
 * {}
 */
inline void dwin_uart_rx_callback(dwin_t *dwin, uint16_t head) {
	dwin->rx_ring_buffer.head_index = head;
}

/* Call from HAL_UART_TxCpltCallback handler in main.c
 void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
 {}
 */
inline void dwin_uart_tx_callback(dwin_t *dwin) {
	dwin->_tx_status = DWIN_TX_STATUS_TX_CMPLT;
}

/* Call from HAL_UART_ErrorCallback in main.c
 * void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
 * {}
 */
inline void dwin_uart_error_callback(dwin_t *dwin) {
	dwin->_status = DWIN_STATUS_UART_ERROR;
}

#ifdef __cplusplus
}
#endif

#endif /* INC_DWIN_H_ */
