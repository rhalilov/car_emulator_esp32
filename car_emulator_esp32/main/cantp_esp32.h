/*
 * cantp_esp32.h
 *
 *  Created on: Jun 9, 2022
 *      Author: refo
 */

#ifndef __CANTP_ESP32_H_
#define __CANTP_ESP32_H_

#include "can-tp.h"

static inline void cantp_rcvr_t_cb(void *args)
{
	cantp_rcvr_timer_cb((cantp_rxtx_status_t *)args);
}

static inline void cantp_sndr_t_cb(void *args)
{
	cantp_sndr_timer_cb((cantp_rxtx_status_t *)args);
}

#endif /* __CANTP_ESP32_H_ */
