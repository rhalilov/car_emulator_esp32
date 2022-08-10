/*
 * car_emulator.c
 *
 *  Created on: Jun 13, 2022
 *      Author: refo
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "cantp_esp32.h"
#include "can-tp.h"
#include "obd.h"
#include "car_emulator.h"

#define OBD2_VIN_LEN 17
unsigned int vehicle_speed = 100;
float vehicle_rpm = 2500;
float vehicle_throttle = 30;
char vehicle_vin[OBD2_VIN_LEN] = "ESP32OBD2EMULATOR";
static uint8_t resp_vin[20];

void createOBDResponse(	obd2_frame_t *response,
						uint8_t service,
						uint8_t pid,
						cfg_can_idt_t idt)
{
	if (idt == CFG_STANDARD_ID) {
		response->id = 0x7E8;
		response->idt = 0;
	} else {
		response->id = 0x18DAF110;
		response->idt = 1;
	}
	response->len = 2;
	response->obd2_service = 0x40 + service; // Service (+ 0x40)
	response->obd2_pid = pid; // PID
}

void respondToOBD1(uint8_t pid, emulator_ctx_t *ectx)
{
	printf("Responding to Service 1: PID 0x%02x ", pid);

	obd2_frame_t resp;
	createOBDResponse(&resp, 1, pid, ectx->cfg->id_type);

	switch (pid) {
		case 0x00: // Supported PIDs
			printf(": Supported PIDS 01-%x\n", pid+0x1F);
			resp.len += 4; // Number of data bytes
			resp.obd2_d[0] = 0x00; // can_data[3] Data byte 0
			resp.obd2_d[1] = 0x18; // can_data[4] Data byte 1
			resp.obd2_d[2] = 0x80; // can_data[5] Data byte 2
			resp.obd2_d[3] = 0x00; // can_data[6] Data byte 3
			break;
		case 0x0C: // RPM
			printf(": RPM\n");
			resp.len += 2; // Number of data bytes
			obdRevConvert_0C(vehicle_rpm, &resp.obd2_d[0],
										&resp.obd2_d[1], 0, 0);
			break;
		case 0x0D: // Speed
			printf(": Speed\n");
			resp.len += 1; // Number of data bytes
			obdRevConvert_0D(vehicle_speed, &resp.obd2_d[0], 0, 0, 0);
			break;
		case 0x11: // Throttle position
			printf(": Throttle position\n");
			resp.len += 1; // Number of data bytes
			obdRevConvert_11(vehicle_throttle, &resp.obd2_d[0], 0, 0, 0);
			break;
		case 0x20:
		case 0x40:
		case 0x60:
		case 0x80:
		case 0xA0:
		case 0xC0:
		case 0xE0:
			printf(": Supported PIDS 01-%x\n", pid+0x1F);
			resp.len += 4; // Number of data bytes
			resp.obd2_d[0] = 0x00; // can_data[3] Data byte 0
			resp.obd2_d[1] = 0x00; // can_data[4] Data byte 1
			resp.obd2_d[2] = 0x00; // can_data[5] Data byte 2
			resp.obd2_d[3] = 0x00; // can_data[6] Data byte 3
			break;
		default:
			printf(": PID is not supported!\n");
			return;
	}
	cantp_send(ectx->cantp_ctx, resp.id, resp.idt, resp.obd_data, resp.len);
}

void respondToOBD9(uint8_t pid, emulator_ctx_t *ectx)
{
	printf("Responding to Service 9: PID 0x%02x ", pid);

	obd2_frame_t response;
	createOBDResponse(&response, 9, pid, ectx->cfg->id_type);

	switch (pid) {
		case 0x00: {// Supported PIDs
			printf(": Supported PIDS 01-%x\n", pid+0x1F);
			response.len += 4;
			response.obd2_d[0] = 0x40;
			response.obd2_d[1] = 0x00; // Data byte 2
			response.obd2_d[2] = 0x00; // Data byte 3
			response.obd2_d[3] = 0x00; // Data byte 4
			cantp_send(ectx->cantp_ctx, response.id, response.idt,
													response.obd_data, response.len);
		}; break;
		case 0x02: // Vehicle Identification Number (VIN)
			printf(": VIN\n");
//			uint8_t *resp_vin = malloc(20);
			obd2_frame_t *res_vin_p;
			res_vin_p = (obd2_frame_t *)resp_vin;

			createOBDResponse(res_vin_p, 9, pid, ectx->cfg->id_type);

			res_vin_p->obd2_d[0] = 1;

			printf("Copy VIN: ");
			for (uint8_t i = 0; i < OBD2_VIN_LEN; i++) {
				res_vin_p->obd2_d[i+1] = vehicle_vin[i];
				printf("%c(0x%02x) ", vehicle_vin[i], vehicle_vin[i]);
			}
			printf("\n"); fflush(0);

			res_vin_p->len += OBD2_VIN_LEN + 1;
			cantp_send(ectx->cantp_ctx, res_vin_p->id, res_vin_p->idt,
													res_vin_p->obd_data, res_vin_p->len);
//			// Initiate multi-frame message packet
//			response.can_data[0] = 0x10; // FF (First Frame, ISO_15765-2)
//			response.can_data[1] = 0x14; // Length (20 bytes)
//			response.can_data[2] = 0x49; // Service (+ 0x40)
//			response.obd2_d[0] = 0x02; // PID
//			response.obd2_d[1] = 0x01; // Data byte 1
//			response.obd2_d[2] = vehicle_vin[0]; // Data byte 2
//			response.obd2_d[3] = vehicle_vin[1]; // Data byte 3
//			response.obd2_d[4] = vehicle_vin[2]; // Data byte 4
//
//			esp32_twai_send_response(&response);

//			// Clear flow control queue
//			// memset(can_flow_queue, 0, 40);
//
//			// Fill flow control queue
//			// Part 1
//			can_flow_queue[0][0] = 0x21; // CF (Consecutive Frame, ISO_15765-2), sequence number
//			can_flow_queue[0][1] = vehicle_vin[3]; // Data byte 1
//			can_flow_queue[0][2] = vehicle_vin[4]; // Data byte 2
//			can_flow_queue[0][3] = vehicle_vin[5]; // Data byte 3
//			can_flow_queue[0][4] = vehicle_vin[6]; // Data byte 4
//			can_flow_queue[0][5] = vehicle_vin[7]; // Data byte 5
//			can_flow_queue[0][6] = vehicle_vin[8]; // Data byte 6
//			can_flow_queue[0][7] = vehicle_vin[9]; // Data byte 7
//			// Part 2
//			can_flow_queue[1][0] = 0x22; // CF
//			can_flow_queue[1][1] = vehicle_vin[10]; // Data byte 1
//			can_flow_queue[1][2] = vehicle_vin[11]; // Data byte 2
//			can_flow_queue[1][3] = vehicle_vin[12]; // Data byte 3
//			can_flow_queue[1][4] = vehicle_vin[13]; // Data byte 4
//			can_flow_queue[1][5] = vehicle_vin[14]; // Data byte 5
//			can_flow_queue[1][6] = vehicle_vin[15]; // Data byte 6
//			can_flow_queue[1][7] = vehicle_vin[16]; // Data byte 7

			break;
	}
}

void can_check_rx_frame(emulator_ctx_t *ectx)
{
	obd2_frame_t obd_rx_frame = { 0 };

	obd_rx_frame.id = ectx->id;
	obd_rx_frame.idt = ectx->idt;
	if (ectx->len > 7) {
		printf("RX Len %d can not be larger than 7\n", ectx->len);
	}
	for (uint8_t i=0; i < ectx->len; i++) {
		obd_rx_frame.obd_data[i] = ectx->data[i];
	}

	free(ectx->data);

	uint32_t tester_id;
	switch (ectx->cfg->id_type) {
	case CFG_STANDARD_ID:
		tester_id = 0x7df;
		break;
	case CFG_EXTENDED_ID:
		tester_id = 0x18DB33F1;
		break;
	default:
		printf("ERROR: Tester ID Type is not correct: %d", ectx->cfg->id_type);
		tester_id = 0x7df;
		break;
	}

	printf("====================================OBD2======================================\n");
	// Check if frame is OBD query
	if (obd_rx_frame.idt == ectx->cfg->id_type) {
		// Compare the bits ID
		if (obd_rx_frame.id == tester_id) {
			printf("OBD QUERY !\n");
			switch (obd_rx_frame.obd2_service) {
				case 1:
					respondToOBD1(obd_rx_frame.obd2_pid, ectx);
					break;
				case 9:
					respondToOBD9(obd_rx_frame.obd2_pid, ectx);
					break;
				default:
					printf("Unsupported Service %d !\n", obd_rx_frame.obd2_service);
			}
		} //else
//		if (obd_rx_frame.id == 0x7e0) { // Check if frame is addressed to the ECU (us)
//			printf("ECU MSG !\n");
//
//			if (obd_rx_frame.len == 0x30) { // Flow control frame (continue)
//				obd2_can_frame_t response = createOBDResponse(0, 0);
//
//				for (int i = 0; i < 5; i++) {
//					if (can_flow_queue[i][0] == 0) {
//						continue;
//					}
//
//					for (int j = 0; j < 8; j++) {
//						response.can_data[j] = can_flow_queue[i][j];
//					}
//					esp32_twai_send_response(&response);
//				}
//
//				memset(can_flow_queue, 0, 40);
//			}
//		}
	} else {
		//29bit ID

	}
	printf("====================================OBD2 END==================================\n");
}
