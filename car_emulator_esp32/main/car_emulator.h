/*
 * car_emulator.h
 *
 *  Created on: Jun 12, 2022
 *      Author: refo
 */

#ifndef __CAR_EMULATOR_H_
#define __CAR_EMULATOR_H_

typedef struct emulator_ctx_c {
	cantp_rxtx_status_t *cantp_ctx;
	SemaphoreHandle_t sem;
	uint32_t id;
	uint8_t idt;
	uint16_t len;
	uint8_t *data;
} emulator_ctx_t;

void can_check_rx_frame(emulator_ctx_t *ectx);

#endif /* __CAR_EMULATOR_H_ */
