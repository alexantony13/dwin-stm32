/*
 * app.c
 *
 *  Created on: May 24, 2025
 *      Author: Alex Antony
 */

#include "app.h"

#include "main.h"
#include "dwin.h"
#include "defines.h"

typedef struct tp_status_t {
	uint8_t updated;
	uint8_t status;
	uint16_t xpos, ypos;
} tp_status_t;

typedef struct sys_param_t {
	uint16_t tick[2];
	uint16_t led_status;
	uint8_t led_status_updated :1;
	tp_status_t tp_status;
} sys_param_t;

sys_param_t sys_param;
extern UART_HandleTypeDef huart3;
dwin_t dwin;

static void display_led_button_pressed_cb(uint8_t *data_ptr,
		uint8_t data16_len) {
	UNUSED(data16_len);
	sys_param.led_status = (data_ptr[0] << 8) | data_ptr[1];
	sys_param.led_status_updated = 1;
}

static void display_touch_update_callback(uint8_t *data_ptr, uint8_t data16_len) {
	if (data16_len == 0x03) {
		sys_param.tp_status.updated = data_ptr[0];
		sys_param.tp_status.status= data_ptr[1];
		sys_param.tp_status.xpos = (data_ptr[2] << 8) | data_ptr[3];
		sys_param.tp_status.ypos = (data_ptr[4] << 8) | data_ptr[5];
	}
}

void app_init() {
	dwin_init(&dwin, &huart3, 32);
	dwin_reg_cb(&dwin, 0x1004, display_led_button_pressed_cb);
	dwin_reg_cb(&dwin, 0x0016, display_touch_update_callback);
}

void app_process() {
	uint32_t prev_tick = HAL_GetTick();
	while (1) {
		uint32_t ctick = HAL_GetTick();

		if ((ctick - prev_tick) >= 50) {
			if (dwin_is_tx_idle(&dwin)) {
				prev_tick = ctick;
				dwin_read_vp(&dwin, 0x0016, 3, ctick);
			}
		} else {
			if (dwin_is_tx_idle(&dwin)) {
				sys_param.tick[0] = ctick >> 16;
				sys_param.tick[1] = ctick;
				dwin_write_vp(&dwin, 0x1000, sys_param.tick, 2, ctick);
			}
		}

		if (sys_param.led_status_updated == 1) {
			sys_param.led_status_updated = 0;

			HAL_GPIO_WritePin(LED0_GPIO_Port, LED0_Pin,
					BIT_CHECK(sys_param.led_status, 0));
			HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin,
					BIT_CHECK(sys_param.led_status, 1));
			HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin,
					BIT_CHECK(sys_param.led_status, 2));
			HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin,
					BIT_CHECK(sys_param.led_status, 3));
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
