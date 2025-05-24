/*
 * app.c
 *
 *  Created on: May 24, 2025
 *      Author: Alex Antony
 */

#include "app.h"

#include "main.h"
#include "dwin.h"

extern UART_HandleTypeDef huart3;
dwin_t dwin;

void app_init() {
	dwin.huart = &huart3;
	dwin.rx_ring_buffer.size = 32;
	dwin_init(&dwin);
}

void app_process() {
	uint32_t ctick = HAL_GetTick();
	dwin_process(&dwin, ctick);
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
	if (huart == dwin.huart) {
		dwin_uart_tx_callback(&dwin);
	}
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size) {
	if (huart == dwin.huart) {
		dwin_uart_rx_callback(&dwin, (Size - 1));
	}
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
	if (huart == dwin.huart) {
		dwin_uart_error_callback(&dwin);
	}
}
