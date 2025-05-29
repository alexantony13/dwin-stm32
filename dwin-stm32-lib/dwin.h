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
	DWIN_STATUS_INIT, DWIN_STATUS_OK, DWIN_STATUS_UART_ERROR,
} dwin_status_t;

typedef enum dwin_rx_state_t {
	DWIN_RX_STATUS_WAITING_HEADER,
	DWIN_RX_STATUS_WAITING_FRAME_LEN,
	DWIN_RX_STATUS_WAITING_FN_CODE,
	DWIN_RX_STATUS_WAITING_DATA,
	DWIN_RX_STATUS_DATA_RECEIVED,
} dwin_rx_state_t;

typedef enum dwin_tx_state_t {
	DWIN_TX_STATUS_IDLE,
	DWIN_TX_STATUS_TX_BUSY_WRITE_VP,
	DWIN_TX_STATUS_TX_BUSY_READ_VP,
	DWIN_TX_STATUS_VP_WRITE_TX_CMPLT,
	DWIN_TX_STATUS_VP_READ_TX_CMPLT,
	DWIN_TX_STATUS_VP_WRITE_ACK_WAITING,
	DWIN_TX_STATUS_VP_READ_RESPONSE_WAITING,
	DWIN_TX_STATUS_VP_WRITE_ACK,
	DWIN_TX_STATUS_VP_READ_RESPONSE,
} dwin_tx_state_t;

typedef enum dwin_error_t {
	DWIN_ERROR_NOERR,
	DWIN_ERROR_ERR,
	DWIN_ERROR_BUSY,
	DWIN_ERROR_TIMEOUT,
	DWIN_ERROR_MEM_ALLOC,
	DWIN_ERROR_PARAM,
	DWIN_ERROR_QUEUE,
} dwin_error_t;

typedef struct dwin_ring_buffer_t {
	uint8_t *buf_ptr;
	uint8_t size;
	int8_t head_index, tail_index;
} dwin_ring_buffer_t;

typedef void (*dwin_event_cb_fn_t)(uint8_t *data8_ptr, uint8_t data16_count);

typedef struct dwin_t {
	void *huart;
	dwin_ring_buffer_t rx_ring_buffer;

	dwin_status_t status;

	dwin_rx_state_t rx_state;
	uint8_t rx_frame_buffer[DWIN_RX_FRAME_MAX_LEN];
	uint16_t rx_num_data_bytes_received, rx_data_bytes_len;
	uint32_t rx_frame_start_tick, rx_frame_timeout_ticks;

	dwin_tx_state_t tx_state;
	uint8_t tx_frame_buffer[DWIN_TX_FRAME_MAX_LEN];
	uint32_t tx_last_sent_tick, tx_timeout_ticks;

	uint16_t cb_address[DWIN_CALLBACK_ADDR_MAX_COUNT];
	dwin_event_cb_fn_t cb_fn[DWIN_CALLBACK_ADDR_MAX_COUNT];
} dwin_t;

/**
 * @brief 		DWIN lib init function
 * 				Should be called once on application start.
 *
 * @param dwin	dwin_t hanle
 * @return
 */
dwin_error_t dwin_init(dwin_t *dwin, void *huart, uint8_t ring_buffer_size);

/**
 * @brief			DWIN lib process function.
 * 					Should be called from the main loop.
 *
 * @param dwin		dwin_t hanle
 * @param c_tick	current tick value
 * @return
 */
dwin_error_t dwin_process(dwin_t *dwin, uint32_t c_tick);

/**
 * @brief 					Function to write data to DWIN display VP address
 *
 * @param dwin				dwin_t hanle
 * @param vp_start_addr		VP start address to which data is to be written
 * @param vp_data_buff		pointer to data
 * @param data_len			data length
 * @param ctick				current tick value for checking timeout
 * @return
 */
dwin_error_t dwin_write_vp(dwin_t *dwin, uint16_t vp_start_addr,
		uint16_t *vp_data_buff, uint8_t data_len, uint32_t ctick);

/**
 * @brief 					Function to read data from DWIN display VP address
 *
 * @param dwin				dwin_t hanle
 * @param vp_start_addr		VP start address from which data is to be read
 * @param vp_data_buff		pointer to data
 * @param data_len			data length
 * @param ctick				current tick value for checking timeout
 * @return
 */
dwin_error_t dwin_read_vp(dwin_t *dwin, uint16_t vp_start_addr,
		uint16_t data_len, uint32_t ctick);

/**
 * @brief 					Function to register user callbacks on VP data update from display.
 *
 * @param dwin				dwin_t hanle
 * @param watch_address		VP address to check for update, upon which the callback function is called.
 * @param cb_fn				Function pointer to the user callback function
 * @return
 */
dwin_error_t dwin_reg_cb(dwin_t *dwin, uint16_t watch_address,
		dwin_event_cb_fn_t cb_fn);

/**
 * @brief 			Function to check if UART is ready for new transmit.
 *
 * @param dwin		dwin_t hanle
 * @return
 */
uint8_t dwin_is_tx_idle(dwin_t *dwin);

/**
 * @brief	DWIN UART callback to be called after receiving data
 *			Can be called from:
 *				void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size){}
 *
 * @param dwin						dwin_t hanle
 * @param last_byte_pos_in_buffer	position of the last byte received in the uart rx buffer
 * @attention 						"HAL_UARTEx_RxEventCallback" gives the size.
 * 									"last_byte_pos_in_buffer" should be (size-1)
 */
inline void dwin_uart_rx_callback(dwin_t *dwin,
		uint16_t last_byte_pos_in_buffer) {
	dwin->rx_ring_buffer.head_index = last_byte_pos_in_buffer;
}

/**
 * @brief		DWIN UART callback to be called after data is transmitted.
 * 				Can be called from:
 * 					void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart){}
 *
 * @param dwin	dwin_t hanle
 */
void dwin_uart_tx_callback(dwin_t *dwin);

/**
 * @brief 		DWIN UART error callback
 * 				Can be called from:
 * 					void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart){}
 *
 * @param dwin	dwin_t hanle
 */
inline void dwin_uart_error_callback(dwin_t *dwin) {
	dwin->status = DWIN_STATUS_UART_ERROR;
}

#ifdef __cplusplus
}
#endif

#endif /* INC_DWIN_H_ */
