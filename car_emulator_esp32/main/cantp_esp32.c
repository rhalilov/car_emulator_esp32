/*
 * cantp_esp32.c
 *
 *  Created on: Jun 8, 2022
 *      Author: refo
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_log.h"

#include "driver/twai.h"
#include "hal/twai_ll.h"

#include "can_drv_esp32.h"
#include "can-tp.h"
#include "obd.h"
#include "car_emulator.h"

#define TAG             "CANTP_ESP32"

static const char *cantp_frame_t_enum_str[] = {
		FOREACH_CANTP_N_PCI_TYPE(GENERATE_STRING)
};

static const char *cantp_fc_flow_status_enum_str[] = {
		FOREACH_CANTP_FC_FLOW_STATUS(GENERATE_STRING)
};

void print_cantp_frame(cantp_frame_t cantp_frame)
{
	cantp_logi("\033[0;32m%s frame\033[0m ",
					cantp_frame_t_enum_str[cantp_frame.n_pci_t]);
	uint8_t datalen = 0;
	uint8_t *dataptr = {0};
	switch (cantp_frame.n_pci_t) {
	case CANTP_SINGLE_FRAME: {
			cantp_logi("len=%d ", cantp_frame.sf.len);
			datalen = cantp_frame.sf.len;
			dataptr = cantp_frame.sf.d;
		} break;
	case CANTP_FIRST_FRAME: {
			cantp_logi("len=%d ", cantp_ff_len_get(&cantp_frame));
			datalen = CANTP_FF_NUM_DATA_BYTES;
			dataptr = cantp_frame.ff.d;
		} break;
	case CANTP_CONSEC_FRAME: {
			cantp_logi("SN=%x ", cantp_frame.cf.sn);
			datalen = CANTP_CF_NUM_DATA_BYTES;
			dataptr = cantp_frame.cf.d;
		} break;
	case CANTP_FLOW_CONTROLL: {
			cantp_logi("FlowStatus=\033[0;33m%s\033[0m BlockSize=%d STmin=%x ",
					cantp_fc_flow_status_enum_str[cantp_frame.fc.fs],
					cantp_frame.fc.bs,
					cantp_frame.fc.st);
			datalen = 0;
			dataptr = NULL;
		} break;
	}
	for (uint8_t i=0; i < datalen; i++) {
		cantp_logi("0x%02x ", dataptr[i]);
	}
	cantp_logi("--\n");fflush(0);
}

void cantp_usleep(uint32_t tout_us)
{
//	usleep(tout_us);
//	ets_delay_us(tout_us);
	vTaskDelay(10);
}

int cantp_rcvr_params_init(cantp_rxtx_status_t *ctx, cantp_params_t *par, char *name)
{
	ctx->params = par;
	ctx->params->st_min = 0;
	ctx->params->block_size = 0;
	return 0;
}

void cantp_received_cb(cantp_rxtx_status_t *ctx,
		uint32_t id, uint8_t idt, uint8_t *data, uint8_t len)
{
	emulator_ctx_t *ectx;
	ectx = (emulator_ctx_t *)ctx->cb_ctx;

	ectx->id = id;
	ectx->idt = idt;
	ectx->len = len;

	cantp_logi("\033[0;36mCAN-SL Receiver Received "
			"from ID=0x%06x IDT=%d :\033[0m ", id, idt);
	ectx->data = malloc(len);
	for (uint16_t i = 0; i < len; i++) {
		cantp_logi("0x%02x ", data[i]);
		ectx->data[i] = data[i];
	}
	cantp_logi("\n"); fflush(0);
	can_check_rx_frame(ectx);
}

int cantp_timer_start(void *timer, char *name, long tout_us)
{
	cantp_logd("Starting timer (%p) %s %ldμs\n", timer, name, tout_us);
	esp_err_t res = esp_timer_start_once((esp_timer_handle_t)timer, tout_us);
    switch (res) {
    	case ESP_ERR_INVALID_ARG:
			printf("ERROR (%d) ESP_ERR_INVALID_ARG (the handle is invalid)\n", res);
			fflush(0);
			break;
    	case ESP_ERR_INVALID_STATE:
    		printf("ERROR (%d) ESP_ERR_INVALID_STATE (timer is already running)\n", res);
    		fflush(0);
    		break;
    }
	return (res == ESP_OK)?0:-1;
}

void cantp_timer_stop(void *timer)
{
//	esp_timer_handle_t t;
//	t = (esp_timer_handle_t)timer;
	cantp_logv("Timer (%p) ", timer); fflush(0);
	esp_timer_stop((esp_timer_handle_t)timer);
	cantp_logv("Stopped\n");
}

int cantp_can_rx(cantp_can_frame_t *rx_frame, uint32_t tout_us)
{
	TickType_t ticks_to_wait;
	if (tout_us == 0) {
		ticks_to_wait = portMAX_DELAY;
	} else {
		ticks_to_wait = (tout_us/1000)/portTICK_RATE_MS;
	}
#if ESP32_IDF_CAN_HAL
	twai_message_t rx_msg;

	twai_receive(&rx_msg, ticks_to_wait);

	rx_frame->dlc = rx_msg.data_length_code;
	rx_frame->rtr = rx_msg.extd;
	rx_frame->id = rx_msg.identifier;

	cantp_logv("\t\tTWAI received %d bytes: ", rx_frame->dlc);
	for (uint8_t i=0; i < rx_frame->dlc; i++) {
		rx_frame->data_u8[i] = rx_msg.data[i];
		cantp_logv("0x%02x ", rx_frame->data_u8[i]);
	}
	cantp_logv("\n");
	return 0;

#else
	//casting to (can_frame_esp32_t *) is just for this implementation
	//can driver of esp32
	esp_err_t err = can_drv_esp32_rx((can_frame_esp32_t *)rx_frame, ticks_to_wait);
	if (err != ESP_OK) {
		ESP_LOGI(TAG, "ERROR twai_receive: %s", esp_err_to_name(err));
		return -1;
	}
	cantp_logv("\t\tCAN DRV received %d bytes: ", rx_frame->dlc);
	for (uint8_t i=0; i < rx_frame->dlc; i++) {
		cantp_logv("0x%02x ", rx_frame->data_u8[i]);
	}
	cantp_logv("\n");
	return 0;

#endif
}

int cantp_can_tx_nb(uint32_t id, uint8_t idt, uint8_t dlc, uint8_t *data)
{
	cantp_logv("\t\tTWAI Sending NB from ID=0x%06x IDt=%d DLC=%d :", id, idt, dlc);
#if ESP32_IDF_CAN_HAL
	twai_message_t tx_msg = {
			.identifier = id,
			.data_length_code = dlc,
			.extd = idt,
			.ss = 1,
			.self = 0,
			.rtr = 0
	};

	for (uint8_t i=0; i < dlc; i++) {
		cantp_logv("0x%02x ", data[i]);
		tx_msg.data[i] = data[i];
	}
	cantp_logv("\n"); fflush(0);
	return (twai_transmit(&tx_msg, 1) == ESP_OK)?0:-1;
#else
	can_frame_esp32_t tx_frame = { 0 };
	tx_frame.id = id;
	tx_frame.idt = idt;
	tx_frame.rtr = 0;
	tx_frame.dlc = dlc;
	for (uint8_t i=0; i < dlc; i++) {
		cantp_logv("0x%02x ", data[i]);
		tx_frame.data_u8[i] = data[i];
	}
	cantp_logv("\n"); fflush(0);
	return (can_drv_esp32_tx(&tx_frame) == ESP_OK)?0:-1;
#endif
}

int cantp_sndr_wait_tx_done(cantp_rxtx_status_t *ctx, uint32_t tout_us)
{
#if ESP32_IDF_CAN_HAL
	uint32_t alerts;
	twai_reconfigure_alerts(TWAI_ALERT_TX_SUCCESS, NULL);

	if (twai_read_alerts(&alerts, (tout_us/1000)/portTICK_PERIOD_MS) != ESP_OK) {
		return -1;
	}
	printf("--%d--", alerts);fflush(0);
	if (alerts & TWAI_ALERT_TX_SUCCESS) {
		return 0;
	}
	return -1;
#else
	esp_err_t res = can_drv_esp32_wait_tx_end((tout_us/1000)/portTICK_RATE_MS);
	return (res == ESP_OK)?0:-1;
#endif
}

int cantp_can_tx(uint32_t id, uint8_t idt, uint8_t dlc, uint8_t *data, long tout_us)
{
	cantp_logv("\t\tTWAI Sending from ID=0x%06x IDt=%d DLC=%d :", id, idt, dlc);
#if ESP32_IDF_CAN_HAL
	twai_message_t tx_msg = {
			.identifier = id,
			.data_length_code = dlc,
			.extd = idt,
			.ss = 1,
			.self = 0,
			.rtr = 0
	};

	for (uint8_t i=0; i < dlc; i++) {
		cantp_logv("0x%02x ", data[i]);
		tx_msg.data[i] = data[i];
	}
	cantp_logv("\n"); fflush(0);
	esp_err_t res = twai_transmit(&tx_msg, 1);
	if (res != ESP_OK) {
		return -1;
	}

	uint32_t alerts;
	twai_reconfigure_alerts(TWAI_ALERT_TX_SUCCESS, NULL);

	if (twai_read_alerts(&alerts, (tout_us/1000)/portTICK_PERIOD_MS) != ESP_OK) {
		return -1;
	}
	printf("-----%d-----", alerts);fflush(0);
	if (alerts & TWAI_ALERT_TX_SUCCESS) {
		return 0;
	}
	return -1;
#else
	can_frame_esp32_t tx_frame;
	tx_frame.id = id;
	tx_frame.idt = idt;
	tx_frame.rtr = 0;
	tx_frame.dlc = dlc;

	for (uint8_t i=0; i < dlc; i++) {
		cantp_logv("0x%02x ", data[i]);
		tx_frame.data_u8[i] = data[i];
	}
	cantp_logv("\n"); fflush(0);

	can_drv_esp32_tx(&tx_frame);

	esp_err_t res = can_drv_esp32_wait_tx_end((tout_us/1000)/portTICK_RATE_MS);
	return (res == ESP_OK)?0:-1;

#endif
}

int cantp_sndr_state_sem_take(cantp_rxtx_status_t *ctx, uint32_t tout_us)
{
	if (tout_us == 0) {
		//Wait forever
		return (xSemaphoreTake((SemaphoreHandle_t)ctx->sndr.state_sem,
												portMAX_DELAY) == pdTRUE)?0:-1;
	}
	return (xSemaphoreTake((SemaphoreHandle_t)ctx->sndr.state_sem,
								(tout_us/1000)/portTICK_RATE_MS) == pdTRUE)?0:-1;
}

void cantp_sndr_state_sem_give(cantp_rxtx_status_t *ctx)
{
	xSemaphoreGive((SemaphoreHandle_t)ctx->sndr.state_sem);
}

void cantp_sndr_tx_done_cb(void)
{
	printf("CAN-SL Sender: TX Done \n"); fflush(0);
}
