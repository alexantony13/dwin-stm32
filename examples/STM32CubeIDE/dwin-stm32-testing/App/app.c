/*
 * app.c
 *
 *  Created on: May 24, 2025
 *      Author: Alex Antony
 */

#include "app.h"

#include "main.h"
#include "dwin.h"

typedef struct sys_param_t {
	uint16_t tick[2];
	uint16_t led_status;
	uint8_t led_status_updated :1;
} sys_param_t;

sys_param_t sys_param;
extern UART_HandleTypeDef huart3;
dwin_t dwin;

static void display_led_button_pressed_cb(uint16_t data) {
	sys_param.led_status = data;
}

void app_init() {
	dwin_init(&dwin, &huart3, 32);
	dwin_reg_cb(&dwin, 0x1004, display_led_button_pressed_cb);
}

void app_process() {
	while (1) {
		uint32_t ctick = HAL_GetTick();

		if (dwin_is_tx_idle(&dwin)) {
			sys_param.tick[0] = ctick >> 16;

			sys_param.tick[1] = ctick;
			dwin_write_vp(&dwin, 0x1000, sys_param.tick, 2, ctick);
		}

		if (sys_param.led_status_updated == 1) {
			sys_param.led_status_updated = 0;

			HAL_GPIO_WritePin(LED0_GPIO_Port, LED0_Pin,
					((sys_param.led_status >> 0) | 0x01));
			HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin,
					((sys_param.led_status >> 1) | 0x01));
			HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin,
					((sys_param.led_status >> 2) | 0x01));
			HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin,
					((sys_param.led_status >> 3) | 0x01));
		}

		dwin_process(&dwin, ctick);
	}
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
