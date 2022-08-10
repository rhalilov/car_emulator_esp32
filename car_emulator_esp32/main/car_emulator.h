/*
 * car_emulator.h
 *
 *  Created on: Jun 12, 2022
 *      Author: refo
 */

#ifndef __CAR_EMULATOR_H_
#define __CAR_EMULATOR_H_

#define ESP32_IDF_CAN_HAL	1

typedef enum {
	CFG_250KBPS = 0,
	CFG_500KBPS
} cfg_boad_rate_t;

typedef enum {
	CFG_STANDARD_ID = 0,
	CFG_EXTENDED_ID
} cfg_can_idt_t;

typedef struct emulator_cfg_s {
	cfg_boad_rate_t boadrate;
	cfg_can_idt_t id_type;
} emulator_cfg_t;

typedef struct emulator_ctx_c {
	emulator_cfg_t *cfg;
	cantp_rxtx_status_t *cantp_ctx;
	SemaphoreHandle_t sem;
	uint32_t id;
	uint8_t idt;
	uint16_t len;
	uint8_t *data;
} emulator_ctx_t;

void can_check_rx_frame(emulator_ctx_t *ectx);

#endif /* __CAR_EMULATOR_H_ */
