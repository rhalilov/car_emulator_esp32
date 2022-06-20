// Original from http://svn.icculus.org/*checkout*/obdgpslogger/trunk/src/obdinfo/obdconvertfunctions.h

/* Copyright 2009 Gary Briggs

This file is part of obdgpslogger.

obdgpslogger is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

obdgpslogger is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with obdgpslogger.  If not, see <http://www.gnu.org/licenses/>.
*/

/** \file
  \brief Functions to convert from OBD output to actual values
  Functions declared in this file are named by their OBD Service command
   which is two hex digits. It's easier than coming up with short unique
   names myself. Please refer to obdservicecommands.h to see what's what.
*/

#ifndef __OBD_H
#define __OBD_H

#ifdef __cplusplus
extern "C" {
#endif //  __cplusplus

// http://en.wikipedia.org/wiki/Table_of_OBD-II_Codes

typedef struct __attribute__((packed)) {
	uint32_t id;			//CAN ID
	uint8_t idt;			//ID type
	uint16_t len;
	union {
		uint8_t obd_data[8];
		struct {
			uint8_t obd2_service;	//obd d0
			uint8_t obd2_pid;		//obd d2
			uint8_t obd2_d[5];		//obd2_d[0]=d3 - obd2_d[4]=d7
		};
	};
} obd2_frame_t;

/// All obd conversion functions adhere to this
typedef float (*OBDConvFunc)(uint8_t  A, uint8_t  B,
	uint8_t  C, uint8_t  D);

/// All obd reverse conversion functions adhere to this
/// return value is number of values [A,B,C,D] filled.
typedef int (*OBDConvRevFunc)(float val, uint8_t  *A, uint8_t  *B,
	uint8_t  *C, uint8_t  *D);

int obdRevConvert_04    (float val, uint8_t  *A, uint8_t  *B, uint8_t  *C, uint8_t  *D);
int obdRevConvert_05    (float val, uint8_t  *A, uint8_t  *B, uint8_t  *C, uint8_t  *D);
int obdRevConvert_06_09 (float val, uint8_t  *A, uint8_t  *B, uint8_t  *C, uint8_t  *D);
int obdRevConvert_0A    (float val, uint8_t  *A, uint8_t  *B, uint8_t  *C, uint8_t  *D);
int obdRevConvert_0B    (float val, uint8_t  *A, uint8_t  *B, uint8_t  *C, uint8_t  *D);
int obdRevConvert_0C(float val, uint8_t *A, uint8_t *B, uint8_t *C, uint8_t *D);
int obdRevConvert_0D    (float val, uint8_t  *A, uint8_t  *B, uint8_t  *C, uint8_t  *D);
int obdRevConvert_0E    (float val, uint8_t  *A, uint8_t  *B, uint8_t  *C, uint8_t  *D);
int obdRevConvert_0F    (float val, uint8_t  *A, uint8_t  *B, uint8_t  *C, uint8_t  *D);
int obdRevConvert_10    (float val, uint8_t  *A, uint8_t  *B, uint8_t  *C, uint8_t  *D);
int obdRevConvert_11    (float val, uint8_t  *A, uint8_t  *B, uint8_t  *C, uint8_t  *D);
int obdRevConvert_14_1B (float val, uint8_t  *A, uint8_t  *B, uint8_t  *C, uint8_t  *D);
int obdRevConvert_1F    (float val, uint8_t  *A, uint8_t  *B, uint8_t  *C, uint8_t  *D);
int obdRevConvert_21    (float val, uint8_t  *A, uint8_t  *B, uint8_t  *C, uint8_t  *D);
int obdRevConvert_22    (float val, uint8_t  *A, uint8_t  *B, uint8_t  *C, uint8_t  *D);
int obdRevConvert_23    (float val, uint8_t  *A, uint8_t  *B, uint8_t  *C, uint8_t  *D);
int obdRevConvert_24_2B (float val, uint8_t  *A, uint8_t  *B, uint8_t  *C, uint8_t  *D);
int obdRevConvert_2C    (float val, uint8_t  *A, uint8_t  *B, uint8_t  *C, uint8_t  *D);
int obdRevConvert_2D    (float val, uint8_t  *A, uint8_t  *B, uint8_t  *C, uint8_t  *D);
int obdRevConvert_2E    (float val, uint8_t  *A, uint8_t  *B, uint8_t  *C, uint8_t  *D);
int obdRevConvert_2F    (float val, uint8_t  *A, uint8_t  *B, uint8_t  *C, uint8_t  *D);
int obdRevConvert_30    (float val, uint8_t  *A, uint8_t  *B, uint8_t  *C, uint8_t  *D);
int obdRevConvert_31    (float val, uint8_t  *A, uint8_t  *B, uint8_t  *C, uint8_t  *D);
int obdRevConvert_32    (float val, uint8_t  *A, uint8_t  *B, uint8_t  *C, uint8_t  *D);
int obdRevConvert_33    (float val, uint8_t  *A, uint8_t  *B, uint8_t  *C, uint8_t  *D);
int obdRevConvert_34_3B (float val, uint8_t  *A, uint8_t  *B, uint8_t  *C, uint8_t  *D);
int obdRevConvert_3C_3F (float val, uint8_t  *A, uint8_t  *B, uint8_t  *C, uint8_t  *D);
int obdRevConvert_42    (float val, uint8_t  *A, uint8_t  *B, uint8_t  *C, uint8_t  *D);
int obdRevConvert_43    (float val, uint8_t  *A, uint8_t  *B, uint8_t  *C, uint8_t  *D);
int obdRevConvert_44    (float val, uint8_t  *A, uint8_t  *B, uint8_t  *C, uint8_t  *D);
int obdRevConvert_45    (float val, uint8_t  *A, uint8_t  *B, uint8_t  *C, uint8_t  *D);
int obdRevConvert_46    (float val, uint8_t  *A, uint8_t  *B, uint8_t  *C, uint8_t  *D);
int obdRevConvert_47_4B (float val, uint8_t  *A, uint8_t  *B, uint8_t  *C, uint8_t  *D);
int obdRevConvert_4C    (float val, uint8_t  *A, uint8_t  *B, uint8_t  *C, uint8_t  *D);
int obdRevConvert_4D    (float val, uint8_t  *A, uint8_t  *B, uint8_t  *C, uint8_t  *D);
int obdRevConvert_4E    (float val, uint8_t  *A, uint8_t  *B, uint8_t  *C, uint8_t  *D);
int obdRevConvert_52    (float val, uint8_t  *A, uint8_t  *B, uint8_t  *C, uint8_t  *D);

#ifdef __cplusplus
}
#endif //  __cplusplus


#endif // __OBD_H
