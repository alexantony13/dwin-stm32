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

enum dwin_frame_names {
	DWIN_FRAME_HEADER_HIGH,
	DWIN_FRAME_HEADER_LOW,
	DWIN_FRAME_LEN,
	DWIN_FRAME_FUNC_CODE,
	DWIN_FRAME_DATA_START,
};

static uint8_t dwin_ring_buffer_dequeue(dwin_t *dwin, uint8_t *data);

uint8_t dwin_init(dwin_t *dwin) {
	uint8_t ret_status = 0;

	dwin->status = DWIN_STATUS_OK;

	dwin->tx_status = DWIN_TX_STATUS_IDLE;
	dwin->tx_timeout_ticks = 1000;

	dwin->rx_status = DWIN_RX_STATUS_WAITING_HEADER;
	dwin->rx_ring_buffer.head_index = -1;
	dwin->rx_ring_buffer.tail_index = -1;
	dwin->rx_frame_timeout_ticks = 1000;

	for (uint8_t i = 0; i < DWIN_CALLBACK_ADDR_MAX_COUNT; ++i) {
		dwin->cb_address[i] = -1;
		dwin->cb_fn[i] = NULL;
	}

	if (dwin->rx_ring_buffer.size != 0) {
		if (dwin->rx_ring_buffer.size < DWIN_RX_CIRC_BUF_MAX_LEN) {
			dwin->rx_ring_buffer.buf_ptr = (uint8_t*) calloc(
					dwin->rx_ring_buffer.size, sizeof(uint8_t));

			if (dwin->rx_ring_buffer.buf_ptr != NULL) {
				if (dwin_itf_uart_receive_to_idle_dma(dwin)
						!= DWIN_ERROR_NOERR) {
					ret_status = 4;
				}
			} else {
				ret_status = 3;
			}
		} else {
			ret_status = 2;
		}
	} else {
		ret_status = 1;
	}

	return ret_status;
}

static uint8_t dwin_ring_buffer_dequeue(dwin_t *dwin, uint8_t *data) {
	uint8_t ret_status = 0;

	if (dwin->rx_ring_buffer.tail_index >= 0) {
		if (dwin->rx_ring_buffer.tail_index
				!= dwin->rx_ring_buffer.head_index) {
			++dwin->rx_ring_buffer.tail_index;
			dwin->rx_ring_buffer.tail_index %= dwin->rx_ring_buffer.size;
			*data =
					dwin->rx_ring_buffer.buf_ptr[dwin->rx_ring_buffer.tail_index];
		} else {
			ret_status = 2;
		}
	} else if (dwin->rx_ring_buffer.head_index >= 0) {
		*data = dwin->rx_ring_buffer.buf_ptr[++dwin->rx_ring_buffer.tail_index];
	} else {
		ret_status = 1;
	}

	return ret_status;
}

uint8_t dwin_process(dwin_t *dwin, uint32_t c_tick) {
	uint8_t ret_status = 0;
	uint8_t rx_data;

	if (dwin->status == DWIN_STATUS_UART_ERROR) {
		dwin->rx_status = DWIN_RX_STATUS_WAITING_HEADER;
		dwin->tx_status = DWIN_TX_STATUS_IDLE;
		dwin_itf_uart_abort(dwin);
		dwin->rx_ring_buffer.head_index = -1;
		dwin->rx_ring_buffer.tail_index = -1;
		if (dwin_itf_uart_receive_to_idle_dma(dwin) == DWIN_ERROR_NOERR) {
			dwin->status = DWIN_STATUS_OK;
		}
	}

	if (dwin->rx_status < DWIN_RX_STATUS_DATA_RECEIVED) {
		if (dwin_ring_buffer_dequeue(dwin, &rx_data) == 0) {
			// HAL_UART_Transmit(dwin->ring_buffer.huart, &rx_data, 1, 100);
			switch (dwin->rx_status) {
			case DWIN_RX_STATUS_WAITING_HEADER:
				if (rx_data == 0x5a) {
					dwin->rx_frame_buffer[DWIN_FRAME_HEADER_HIGH] = rx_data;
				} else if (rx_data == 0xa5) {
					dwin->rx_frame_buffer[DWIN_FRAME_HEADER_LOW] = rx_data;
					if (dwin->rx_frame_buffer[DWIN_FRAME_HEADER_HIGH] == 0x5a) {
						dwin->rx_status = DWIN_RX_STATUS_WAITING_FRAME_LEN;
						dwin->rx_frame_start_tick = c_tick;
					}
				}
				break;
			case DWIN_RX_STATUS_WAITING_FRAME_LEN:
				dwin->rx_frame_buffer[DWIN_FRAME_LEN] = rx_data;
				dwin->rx_status = DWIN_RX_STATUS_WAITING_FN_CODE;
				break;
			case DWIN_RX_STATUS_WAITING_FN_CODE:
				dwin->rx_frame_buffer[DWIN_FRAME_FUNC_CODE] = rx_data;
				dwin->rx_data_byte_len = dwin->rx_frame_buffer[DWIN_FRAME_LEN]
						- 1;
				dwin->rx_data_byte_count = 0;
				dwin->rx_status = DWIN_RX_STATUS_WAITING_DATA;
				break;
			case DWIN_RX_STATUS_WAITING_DATA:
				if (dwin->rx_data_byte_count < dwin->rx_data_byte_len) {
					dwin->rx_frame_buffer[DWIN_FRAME_DATA_START
							+ dwin->rx_data_byte_count] = rx_data;
				}
				if (++(dwin->rx_data_byte_count) == dwin->rx_data_byte_len) {
					dwin->rx_status = DWIN_RX_STATUS_DATA_RECEIVED;
				}
				break;
			default:
				break;
			}
		}
	}

	switch (dwin->tx_status) {
	case DWIN_TX_STATUS_IDLE:
	case DWIN_TX_STATUS_TX_BUSY:
		break;
	case DWIN_TX_STATUS_TX_CMPLT:
		dwin->tx_status = DWIN_TX_STATUS_ACK_WAITING;
		break;
	case DWIN_TX_STATUS_ACK_WAITING:
		break;
	case DWIN_TX_STATUS_ACK:
		dwin->tx_status = DWIN_TX_STATUS_IDLE;
		break;
	default:
		break;
	}

	if (dwin->rx_status == DWIN_RX_STATUS_DATA_RECEIVED) {
		dwin->rx_status = DWIN_RX_STATUS_WAITING_HEADER;

		if (dwin->rx_frame_buffer[DWIN_FRAME_FUNC_CODE] == 0x83) {
			uint16_t address = (dwin->rx_frame_buffer[DWIN_FRAME_DATA_START]
					<< 8) | (dwin->rx_frame_buffer[DWIN_FRAME_DATA_START + 1]);

			uint8_t data_count =
					dwin->rx_frame_buffer[DWIN_FRAME_DATA_START + 2];
			if (data_count == 1) {
				uint16_t data =
						(dwin->rx_frame_buffer[DWIN_FRAME_DATA_START + 3] << 8)
								| (dwin->rx_frame_buffer[DWIN_FRAME_DATA_START
										+ 4]);
				for (uint8_t i = 0; i < DWIN_CALLBACK_ADDR_MAX_COUNT; ++i) {
					if (dwin->cb_address[i] == -1) {
						break;
					} else {
						if (address == dwin->cb_address[i]) {
							(*(dwin->cb_fn[i]))(data);
							break;
						}
					}
				}
			} else {
				/*
				 * TODO:
				 * Handling conditions where data_count is more than one
				 */
			}
		} else {
			if (dwin->tx_status == DWIN_TX_STATUS_ACK_WAITING) {
				if (dwin->rx_frame_buffer[DWIN_FRAME_FUNC_CODE] == 0x82) {
					if ((dwin->rx_frame_buffer[DWIN_FRAME_DATA_START] == 0x4f)
							&& (dwin->rx_frame_buffer[DWIN_FRAME_DATA_START + 1]
									== 0x4b)) {
						dwin->tx_status = DWIN_TX_STATUS_ACK;
					}
				}
			}
		}
	}

	if (dwin->tx_status != DWIN_TX_STATUS_IDLE) {
		if ((c_tick - dwin->tx_last_sent_tick) >= dwin->tx_timeout_ticks) {
			dwin->tx_status = DWIN_TX_STATUS_IDLE;
		}
	}

	if (dwin->rx_status != DWIN_RX_STATUS_WAITING_HEADER) {
		if ((c_tick - dwin->rx_frame_start_tick)
				>= dwin->rx_frame_timeout_ticks) {
			dwin->rx_status = DWIN_RX_STATUS_WAITING_HEADER;
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
uint8_t dwin_write_vp(dwin_t *dwin, uint16_t vp_start_addr, uint16_t *vp_data,
		uint16_t vp_data_len, uint32_t ctick) {
	uint8_t ret_status = 0;
	uint16_t tx_frame_len = (vp_data_len * 2) + 6;
	if (dwin->tx_status != DWIN_TX_STATUS_IDLE) {
		ret_status = 2;
	} else {
		if (tx_frame_len >= DWIN_TX_FRAME_MAX_LEN) {
			ret_status = 1;
		} else {
			dwin->tx_frame_buffer[DWIN_FRAME_HEADER_HIGH] = 0x5a;
			dwin->tx_frame_buffer[DWIN_FRAME_HEADER_LOW] = 0xa5;
			dwin->tx_frame_buffer[DWIN_FRAME_LEN] = (2 * vp_data_len) + 3;
			dwin->tx_frame_buffer[DWIN_FRAME_FUNC_CODE] = 0x82;
			dwin->tx_frame_buffer[DWIN_FRAME_DATA_START] = vp_start_addr >> 8;
			dwin->tx_frame_buffer[DWIN_FRAME_DATA_START + 1] = vp_start_addr
					& 0x00ff;

			for (uint8_t i = 0; i < vp_data_len; ++i) {
				dwin->tx_frame_buffer[DWIN_FRAME_DATA_START + 2 + (2 * i)] =
						vp_data[i] >> 8;
				dwin->tx_frame_buffer[DWIN_FRAME_DATA_START + 2 + (2 * i) + 1] =
						vp_data[i] & 0x00ff;
			}
		}

		if (dwin_itf_uart_transmit_dma(dwin, tx_frame_len)
				== DWIN_ERROR_NOERR) {
			dwin->tx_last_sent_tick = ctick;
			dwin->tx_status = DWIN_TX_STATUS_TX_BUSY;
		} else {
			ret_status = 3;
		}
	}

	return ret_status;
}

uint8_t dwin_reg_cb(dwin_t *dwin, uint16_t watch_address, dwin_cb_fn_t cb_fn) {
	uint8_t ret_status = 0;
	uint8_t index = 0;
	for (; index < DWIN_CALLBACK_ADDR_MAX_COUNT; ++index) {
		if (dwin->cb_address[index] == -1) {
			dwin->cb_address[index] = watch_address;
			dwin->cb_fn[index] = cb_fn;
			break;
		}
	}
	if (index == DWIN_CALLBACK_ADDR_MAX_COUNT) {
		ret_status = 1;
	}
	return ret_status;
}

uint8_t dwin_is_tx_idle(dwin_t *dwin) {
	return dwin->tx_status == DWIN_TX_STATUS_IDLE ? 1 : 0;
}

extern inline void dwin_uart_tx_callback(dwin_t *dwin);
extern inline void dwin_uart_rx_callback(dwin_t *dwin, uint16_t head);
extern inline void dwin_uart_error_callback(dwin_t *dwin);
