/*
 * dwin.c
 *
 *  Created on: Aug 19, 2023
 *      Author: Alex Antony
 */

#include "dwin.h"
#include "dwin_itf.h"
#include <stdlib.h>

/*
 * DWIN serial data write frame:
 *  Request: 5aa5 07 82 1000 0064 0032
 *    Max length: (6 + 2n) bytes
 *  Response: 5aa5 03 82 4f4b
 *    Length: 6
 *
 * DWIN serial data read frame:
 *  Request: 5aa5 04 83 1000 02
 *    Max length: 7 bytes
 *  Reply: 5aa5 08 83 1000 02 data0 data1
 *    Max length: (7 + 2n) bytes
 */

#define DWIN_TX_TIMEOUT_TICKS 1000
#define DWIN_RX_FRAME_TIMEOUT_TICKS 1000

#define DWIN_COMM_FRAME_HEADER_HIGH 0x5a
#define DWIN_COMM_FRAME_HEADER_LOW 0xa5

#define DWIN_COMM_FRAME_CMD_WRITE_VARIABLE 0x82
#define DWIN_COMM_FRAME_CMD_READ_VARIABLE 0x83
#define DWIN_COMM_FRAME_CMD_WRITE_ACK_HIGH 0x4f
#define DWIN_COMM_FRAME_CMD_WRITE_ACK_LOW 0x4b
#define DWIN_VP_READ_TX_FRAME_LEN 7
#if DWIN_VP_READ_TX_FRAME_LEN >= DWIN_TX_FRAME_MAX_LEN
#error DWIN_TX_FRAME_MAX_LEN should be greater than DWIN_VP_READ_TX_FRAME_LEN
#endif

#define DWIN_UINT16_FROM_UINT8(high_byte, low_byte) ((uint16_t)((high_byte<<8)|low_byte))
#define DWIN_VP_WRITE_TX_FRAME_LEN(data_len) ((data_len*2)+6)

enum dwin_frame_names {
	DWIN_FRAME_NAME_HEADER_HIGH,
	DWIN_FRAME_NAME_HEADER_LOW,
	DWIN_FRAME_NAME_LEN,
	DWIN_FRAME_NAME_FUNC_CODE,
	DWIN_FRAME_NAME_DATA_START,
};

static dwin_error_t dwin_ring_buffer_dequeue(dwin_t *dwin, uint8_t *data);

dwin_error_t dwin_init(dwin_t *dwin, void *huart, uint8_t ring_buffer_size) {
	dwin_error_t ret_status = DWIN_ERROR_NOERR;

	if ((NULL == dwin) || (NULL == huart) || (ring_buffer_size == 0)
			|| (dwin->rx_ring_buffer.size >= DWIN_RX_CIRC_BUF_MAX_LEN)) {
		return DWIN_ERROR_PARAM;
	}

	dwin->huart = huart;
	dwin->rx_ring_buffer.size = ring_buffer_size;

	dwin->tx_state = DWIN_TX_STATUS_IDLE;
	dwin->tx_timeout_ticks = DWIN_TX_TIMEOUT_TICKS;

	dwin->rx_state = DWIN_RX_STATUS_WAITING_HEADER;
	dwin->rx_ring_buffer.head_index = -1;
	dwin->rx_ring_buffer.tail_index = -1;
	dwin->rx_frame_timeout_ticks = DWIN_RX_FRAME_TIMEOUT_TICKS;

	for (uint8_t i = 0; i < DWIN_CALLBACK_ADDR_MAX_COUNT; ++i) {
		dwin->cb_fn[i] = NULL;
		dwin->cb_address[i] = 0;
	}

	dwin->rx_ring_buffer.buf_ptr = (uint8_t*) calloc(dwin->rx_ring_buffer.size,
			sizeof(uint8_t));

	if (dwin->rx_ring_buffer.buf_ptr != NULL) {
		ret_status = dwin_itf_uart_receive_to_idle_dma(dwin);
	} else {
		ret_status = DWIN_ERROR_MEM_ALLOC;
	}

	dwin->status = DWIN_STATUS_INIT;
	if (ret_status == DWIN_ERROR_NOERR) {
		dwin->status = DWIN_STATUS_OK;
	}

	return ret_status;
}

static dwin_error_t dwin_ring_buffer_dequeue(dwin_t *dwin, uint8_t *data) {
	if ((dwin == NULL) || (data == NULL)) {
		return DWIN_ERROR_PARAM;
	}

	if (dwin->status == DWIN_STATUS_INIT) {
		return DWIN_ERROR_ERR;
	}

	dwin_error_t ret_status = DWIN_ERROR_NOERR;

	if (dwin->rx_ring_buffer.tail_index >= 0) {
		if (dwin->rx_ring_buffer.tail_index
				!= dwin->rx_ring_buffer.head_index) {
			++dwin->rx_ring_buffer.tail_index;
			dwin->rx_ring_buffer.tail_index %= dwin->rx_ring_buffer.size;
			*data =
					dwin->rx_ring_buffer.buf_ptr[dwin->rx_ring_buffer.tail_index];
		} else {
			ret_status = DWIN_ERROR_QUEUE;
		}
	} else if (dwin->rx_ring_buffer.head_index >= 0) {
		*data = dwin->rx_ring_buffer.buf_ptr[++dwin->rx_ring_buffer.tail_index];
	} else {
		ret_status = DWIN_ERROR_QUEUE;
	}

	return ret_status;
}

dwin_error_t dwin_process(dwin_t *dwin, uint32_t c_tick) {
	if (dwin == NULL) {
		return DWIN_ERROR_PARAM;
	}

	if (dwin->status == DWIN_STATUS_INIT) {
		return DWIN_ERROR_ERR;
	}

	dwin_error_t ret_status = DWIN_ERROR_NOERR;
	uint8_t rx_data;

	if (dwin->status == DWIN_STATUS_UART_ERROR) {
		dwin->rx_state = DWIN_RX_STATUS_WAITING_HEADER;
		dwin->tx_state = DWIN_TX_STATUS_IDLE;
		dwin_itf_uart_abort(dwin);
		dwin->rx_ring_buffer.head_index = -1;
		dwin->rx_ring_buffer.tail_index = -1;
		if (dwin_itf_uart_receive_to_idle_dma(dwin) == DWIN_ERROR_NOERR) {
			dwin->status = DWIN_STATUS_OK;
		}
	}

	if (dwin->rx_state < DWIN_RX_STATUS_DATA_RECEIVED) {
		if (dwin_ring_buffer_dequeue(dwin, &rx_data) == DWIN_ERROR_NOERR) {
			switch (dwin->rx_state) {
			case DWIN_RX_STATUS_WAITING_HEADER:
				if (rx_data == DWIN_COMM_FRAME_HEADER_HIGH) {
					dwin->rx_frame_buffer[DWIN_FRAME_NAME_HEADER_HIGH] =
							rx_data;
				} else if (rx_data == DWIN_COMM_FRAME_HEADER_LOW) {
					dwin->rx_frame_buffer[DWIN_FRAME_NAME_HEADER_LOW] = rx_data;
					if (dwin->rx_frame_buffer[DWIN_FRAME_NAME_HEADER_HIGH]
							== DWIN_COMM_FRAME_HEADER_HIGH) {
						dwin->rx_state = DWIN_RX_STATUS_WAITING_FRAME_LEN;
						dwin->rx_frame_start_tick = c_tick;
					}
				}
				break;
			case DWIN_RX_STATUS_WAITING_FRAME_LEN:
				dwin->rx_frame_buffer[DWIN_FRAME_NAME_LEN] = rx_data;
				dwin->rx_state = DWIN_RX_STATUS_WAITING_FN_CODE;
				break;
			case DWIN_RX_STATUS_WAITING_FN_CODE:
				dwin->rx_frame_buffer[DWIN_FRAME_NAME_FUNC_CODE] = rx_data;
				dwin->rx_data_bytes_len =
						dwin->rx_frame_buffer[DWIN_FRAME_NAME_LEN] - 1;
				dwin->rx_num_data_bytes_received = 0;
				dwin->rx_state = DWIN_RX_STATUS_WAITING_DATA;
				break;
			case DWIN_RX_STATUS_WAITING_DATA:
				if (dwin->rx_num_data_bytes_received
						< dwin->rx_data_bytes_len) {
					dwin->rx_frame_buffer[DWIN_FRAME_NAME_DATA_START
							+ dwin->rx_num_data_bytes_received] = rx_data;
				}
				if (++(dwin->rx_num_data_bytes_received)
						== dwin->rx_data_bytes_len) {
					dwin->rx_state = DWIN_RX_STATUS_DATA_RECEIVED;
				}
				break;
			default:
				break;
			}
		}
	}

	switch (dwin->tx_state) {
	case DWIN_TX_STATUS_IDLE:
	case DWIN_TX_STATUS_TX_BUSY_WRITE_VP:
	case DWIN_TX_STATUS_TX_BUSY_READ_VP:
		break;
	case DWIN_TX_STATUS_VP_WRITE_TX_CMPLT:
		dwin->tx_state = DWIN_TX_STATUS_VP_WRITE_ACK_WAITING;
		break;
	case DWIN_TX_STATUS_VP_READ_TX_CMPLT:
		dwin->tx_state = DWIN_TX_STATUS_VP_READ_RESPONSE_WAITING;
		break;
	case DWIN_TX_STATUS_VP_WRITE_ACK_WAITING:
		break;
	case DWIN_TX_STATUS_VP_READ_RESPONSE_WAITING:
		break;
	case DWIN_TX_STATUS_VP_WRITE_ACK:
	case DWIN_TX_STATUS_VP_READ_RESPONSE:
		dwin->tx_state = DWIN_TX_STATUS_IDLE;
		break;
	default:
		break;
	}

	if (dwin->rx_state == DWIN_RX_STATUS_DATA_RECEIVED) {
		dwin->rx_state = DWIN_RX_STATUS_WAITING_HEADER;

		if (dwin->rx_frame_buffer[DWIN_FRAME_NAME_FUNC_CODE]
				== DWIN_COMM_FRAME_CMD_READ_VARIABLE) {

			if (dwin->tx_state == DWIN_TX_STATUS_VP_READ_RESPONSE_WAITING) {
				dwin->tx_state = DWIN_TX_STATUS_VP_READ_RESPONSE;
			}

			uint16_t address = DWIN_UINT16_FROM_UINT8(
					dwin->rx_frame_buffer[DWIN_FRAME_NAME_DATA_START],
					dwin->rx_frame_buffer[DWIN_FRAME_NAME_DATA_START + 1]);

			uint8_t data_count =
					dwin->rx_frame_buffer[DWIN_FRAME_NAME_DATA_START + 2];

			for (uint8_t i = 0; i < DWIN_CALLBACK_ADDR_MAX_COUNT; ++i) {

				if (dwin->cb_fn[i] == NULL) {
					break;
				} else if (address == dwin->cb_address[i]) {
					(*(dwin->cb_fn[i]))(
							&(dwin->rx_frame_buffer[DWIN_FRAME_NAME_DATA_START
									+ 3]), data_count);
					break;
				}
			}
		} else if (dwin->tx_state == DWIN_TX_STATUS_VP_WRITE_ACK_WAITING) {
			if (dwin->rx_frame_buffer[DWIN_FRAME_NAME_FUNC_CODE]
					== DWIN_COMM_FRAME_CMD_WRITE_VARIABLE) {
				if ((dwin->rx_frame_buffer[DWIN_FRAME_NAME_DATA_START]
						== DWIN_COMM_FRAME_CMD_WRITE_ACK_HIGH)
						&& (dwin->rx_frame_buffer[DWIN_FRAME_NAME_DATA_START + 1]
								== DWIN_COMM_FRAME_CMD_WRITE_ACK_LOW)) {
					dwin->tx_state = DWIN_TX_STATUS_VP_WRITE_ACK;
				}
			}
		}
	}

	if (dwin->tx_state != DWIN_TX_STATUS_IDLE) {
		if ((c_tick - dwin->tx_last_sent_tick) >= dwin->tx_timeout_ticks) {
			dwin->tx_state = DWIN_TX_STATUS_IDLE;
		}
	}

	if (dwin->rx_state != DWIN_RX_STATUS_WAITING_HEADER) {
		if ((c_tick - dwin->rx_frame_start_tick)
				>= dwin->rx_frame_timeout_ticks) {
			dwin->rx_state = DWIN_RX_STATUS_WAITING_HEADER;
		}
	}

	return ret_status;
}

/*
 * DWIN serial data write frame:
 *  Request: 5aa5 07 82 1000 0064 0032
 *    Max length: (6 + 2n) bytes
 *  Response: 5aa5 03 82 4f4b
 *    Length: 6
 */
dwin_error_t dwin_write_vp(dwin_t *dwin, uint16_t vp_start_addr,
		uint16_t *vp_data_buff, uint8_t vp_data_len, uint32_t ctick) {

	if ((dwin == NULL) || (vp_data_buff == NULL) || (vp_data_len == 0)) {
		return DWIN_ERROR_PARAM;
	}

	if (dwin->status == DWIN_STATUS_INIT) {
		return DWIN_ERROR_ERR;
	}
	if (dwin->tx_state != DWIN_TX_STATUS_IDLE) {
		return DWIN_ERROR_BUSY;
	}

	dwin_error_t ret_status = DWIN_ERROR_NOERR;
	uint16_t tx_frame_len = DWIN_VP_WRITE_TX_FRAME_LEN(vp_data_len);

	if (tx_frame_len >= DWIN_TX_FRAME_MAX_LEN) {
		return DWIN_ERROR_ERR;
	}

	dwin->tx_frame_buffer[DWIN_FRAME_NAME_HEADER_HIGH] =
	DWIN_COMM_FRAME_HEADER_HIGH;
	dwin->tx_frame_buffer[DWIN_FRAME_NAME_HEADER_LOW] =
	DWIN_COMM_FRAME_HEADER_LOW;
	dwin->tx_frame_buffer[DWIN_FRAME_NAME_LEN] = tx_frame_len - 3;
	dwin->tx_frame_buffer[DWIN_FRAME_NAME_FUNC_CODE] =
	DWIN_COMM_FRAME_CMD_WRITE_VARIABLE;
	dwin->tx_frame_buffer[DWIN_FRAME_NAME_DATA_START] = vp_start_addr >> 8;
	dwin->tx_frame_buffer[DWIN_FRAME_NAME_DATA_START + 1] = vp_start_addr
			& 0x00ff;

	for (uint8_t i = 0; i < vp_data_len; ++i) {
		dwin->tx_frame_buffer[DWIN_FRAME_NAME_DATA_START + 2 + (2 * i)] =
				vp_data_buff[i] >> 8;
		dwin->tx_frame_buffer[DWIN_FRAME_NAME_DATA_START + 2 + (2 * i) + 1] =
				vp_data_buff[i] & 0x00ff;
	}

	ret_status = dwin_itf_uart_transmit_dma(dwin, tx_frame_len);
	if (ret_status == DWIN_ERROR_NOERR) {
		dwin->tx_last_sent_tick = ctick;
		dwin->tx_state = DWIN_TX_STATUS_TX_BUSY_WRITE_VP;
	}

	return ret_status;
}

/*
 * DWIN serial data read frame:
 *  Request: 5A A5 04 83 1000 01
 *    Length: 7 bytes
 *  Response: 5A A5 06 83 1000 01 0002
 *    Max length: (7 + 2n) bytes
 */
dwin_error_t dwin_read_vp(dwin_t *dwin, uint16_t vp_start_addr,
		uint16_t vp_data_len, uint32_t ctick) {

	if ((dwin == NULL) || (vp_data_len == 0)) {
		return DWIN_ERROR_PARAM;
	}

	if (dwin->status == DWIN_STATUS_INIT) {
		return DWIN_ERROR_ERR;
	}
	if (dwin->tx_state != DWIN_TX_STATUS_IDLE) {
		return DWIN_ERROR_BUSY;
	}

	dwin_error_t ret_status = DWIN_ERROR_NOERR;

	dwin->tx_frame_buffer[DWIN_FRAME_NAME_HEADER_HIGH] =
	DWIN_COMM_FRAME_HEADER_HIGH;
	dwin->tx_frame_buffer[DWIN_FRAME_NAME_HEADER_LOW] =
	DWIN_COMM_FRAME_HEADER_LOW;
	dwin->tx_frame_buffer[DWIN_FRAME_NAME_LEN] = DWIN_VP_READ_TX_FRAME_LEN - 3;
	dwin->tx_frame_buffer[DWIN_FRAME_NAME_FUNC_CODE] =
	DWIN_COMM_FRAME_CMD_READ_VARIABLE;
	dwin->tx_frame_buffer[DWIN_FRAME_NAME_DATA_START] = vp_start_addr >> 8;
	dwin->tx_frame_buffer[DWIN_FRAME_NAME_DATA_START + 1] = vp_start_addr
			& 0x00ff;
	dwin->tx_frame_buffer[DWIN_FRAME_NAME_DATA_START + 2] = vp_data_len;

	ret_status = dwin_itf_uart_transmit_dma(dwin, DWIN_VP_READ_TX_FRAME_LEN);
	if (ret_status == DWIN_ERROR_NOERR) {
		dwin->tx_last_sent_tick = ctick;
		dwin->tx_state = DWIN_TX_STATUS_TX_BUSY_READ_VP;
	}

	return ret_status;
}

dwin_error_t dwin_reg_cb(dwin_t *dwin, uint16_t watch_address,
		dwin_event_cb_fn_t cb_fn) {

	if ((dwin == NULL) || (cb_fn == NULL)) {
		return DWIN_ERROR_PARAM;
	}

	if (dwin->status == DWIN_STATUS_INIT) {
		return DWIN_ERROR_ERR;
	}

	dwin_error_t ret_status = DWIN_ERROR_NOERR;
	uint8_t index = 0;
	for (; index < DWIN_CALLBACK_ADDR_MAX_COUNT; ++index) {
		if (dwin->cb_address[index] == -1) {
			dwin->cb_address[index] = watch_address;
			dwin->cb_fn[index] = cb_fn;
			break;
		}
	}
	if (index == DWIN_CALLBACK_ADDR_MAX_COUNT) {
		ret_status = DWIN_ERROR_ERR;
	}
	return ret_status;
}

uint8_t dwin_is_tx_idle(dwin_t *dwin) {
	return dwin->tx_state == DWIN_TX_STATUS_IDLE ? 1 : 0;
}

extern void dwin_uart_error_callback(dwin_t *dwin);

void dwin_uart_tx_callback(dwin_t *dwin) {
	if (dwin->tx_state == DWIN_TX_STATUS_TX_BUSY_WRITE_VP) {
		dwin->tx_state = DWIN_TX_STATUS_VP_WRITE_TX_CMPLT;
	} else if (dwin->tx_state == DWIN_TX_STATUS_TX_BUSY_READ_VP) {
		dwin->tx_state = DWIN_TX_STATUS_VP_READ_TX_CMPLT;
	}
}

extern void dwin_uart_rx_callback(dwin_t *dwin,
		uint16_t last_byte_pos_in_buffer);

