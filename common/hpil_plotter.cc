/*****************************************************************************
 * Free42 -- an HP-42S calculator simulator
 * Copyright (C) 2004-2017  Thomas Okken
 * Copyright (C) 2015-2017  Jean-Christophe Hessemann, hpil extensions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see http://www.gnu.org/licenses/.
 *****************************************************************************/

/* plotter module
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include "core_globals.h"
#include "core_helpers.h"
#include "core_main.h"
#include "core_variables.h"
#include "hpil_common.h"
#include "hpil_controller.h"
#include "hpil_extended.h"
#include "string.h"

extern HPIL_Settings hpil_settings;
extern HpilXController hpilXCore;
extern int controllerCommand;
extern int hpil_step;
extern int hpil_worker(int interrupted);
extern int (*hpil_completion)(int);
extern void (*hpil_emptyListenBuffer)(void);
extern void (*hpil_fillTalkBuffer)(void);
extern int frame;

/* buffers structs
 *
 */
phloat BR[26];
// status information
#define Status BR[0]
// Lower left-hand and upper right-hand corners of graphic limits in APUs
#define P1_x BR[1]
#define P2_x BR[2]
#define P1_y BR[3]
#define P2_y BR[4]
// Lower left-hand and upper right-hand corners of plot bounds in APUs
#define x1 BR[5]
#define x2 BR[6]
#define y1 BR[7]
#define y2 BR[8]
// Intended pen position in APUs after most recent plotting or [LABEL] command
#define Last_x BR[9]
#define Last_y BR[10]
// Miscellaneous storage
#define Misc_x BR[11]
#define Misc_y BR[12]
// Actual pen position in APUs after most recent plotting or [LABEL] command
#define Last_x_prime BR[13]
#define Last_y_prime BR[14]
// Miscellaneous storage
#define Misc_x_prime BR[15]
#define Misc_y_prime BR[16]
// GU Scaling Factors (GUs > APUs)
#define Factor1_x BR[17]
#define Factor2_x BR[18]
#define Factor1_y BR[19]
#define Factor2_y BR[20]
// UU Scaling Factors (GUs > APUs)
#define Factor1_x_prime BR[21]
#define Factor2_x_prime BR[22]
#define Factor1_y_prime BR[23]
#define Factor2_y_prime BR[24]
// [BCSIZE] Parameters
#define BcSize BR[25]

// status elements
int n7;

extern AltBuf hpil_controllerAltBuf;
extern DataBuf hpil_controllerDataBuf;

int hpil_pinit_completion(int);
int hpil_plotterSelect_sub(int);
int hpil_plotterSend_sub(int);
int hpil_plotterSendGet_sub(int);
int hpil_parse(char*, int, phloat*);

int docmd_pinit(arg_struct *arg) {
	int err;
	err = hpil_check();
	if (err != ERR_NONE) {
		return err;
	}
	ILCMD_AAU;
	hpil_step = 0;
	hpil_completion = hpil_pinit_completion;
	mode_interruptible = hpil_worker;
	return ERR_INTERRUPTIBLE;
}

int hpil_pinit_completion(int error) {
	int i;
	if (error == ERR_NONE) {
		error = ERR_INTERRUPTIBLE;
		switch (hpil_step) {
			case 0 :		// Select Plotter
				hpilXCore.buf = hpil_controllerDataBuf.data;
				hpilXCore.bufPtr = 0;
				hpilXCore.bufSize = 2;
				ILCMD_nop;
				hpil_step++;
				error = call_ilCompletion(hpil_plotterSelect_sub);
				break;
			case 1 :		// Pinit - first stage send DF;DI;SP1;OP;
				strncpy((char*)hpilXCore.buf, "DF;DI;SP1;OP;", 13);
				hpilXCore.bufPtr = 0;
				hpilXCore.bufSize = 13;
				ILCMD_nop;
				hpil_step++;
				error = call_ilCompletion(hpil_plotterSendGet_sub);
				break;
			case 2 :		// Pinit - first stage process P1,P2, second stage get error status send \nOE;
				i = hpil_parse((char*)hpilXCore.buf, hpilXCore.bufPtr, &P1_x);
				i += hpil_parse((char*)&hpilXCore.buf[i], hpilXCore.bufPtr - i, &P1_y);
				i += hpil_parse((char*)&hpilXCore.buf[i], hpilXCore.bufPtr - i, &P2_x);
				i += hpil_parse((char*)&hpilXCore.buf[i], hpilXCore.bufPtr - i, &P2_y);
				x1 = P1_x;
				y1 = P1_y;
				x2 = P2_x;
				y2 = P2_y;
				strncpy((char*)hpilXCore.buf, "\nOE;", 5);
				hpilXCore.bufPtr = 0;
				hpilXCore.bufSize = 5;
				ILCMD_nop;
				hpil_step++;
				error = call_ilCompletion(hpil_plotterSendGet_sub);
				break;
			case 3 :		// Pinit - second stage process error code, third stage send IW
				if ((hpilXCore.bufPtr == 1) && (hpilXCore.buf[0] == '0')) {
					hpilXCore.bufPtr = 0;
					hpilXCore.bufSize = sprintf((char *)hpilXCore.buf, "IW %u,%u,%u,%u;",
						to_int(x1), to_int(y1), to_int(x2), to_int(y2));
					ILCMD_nop;
					hpil_step++;
					error = call_ilCompletion(hpil_plotterSend_sub);
				}
				else {
					error = ERR_PLOTTER_ERR;
				}
				break;
			case 4 :		// Pinit - fourth stage, get pen position and status send OA
				strncpy((char*)hpilXCore.buf, "\nOA;", 5);
				hpilXCore.bufPtr = 0;
				hpilXCore.bufSize = 5;
				ILCMD_nop;
				hpil_step++;
				error = call_ilCompletion(hpil_plotterSendGet_sub);
				break;
			case 5 :		// Pinit - fourth stage, get pen status;
				i = hpil_parse((char*)hpilXCore.buf, hpilXCore.bufPtr, &Last_x_prime);
				i += hpil_parse((char*)&hpilXCore.buf[i], hpilXCore.bufPtr - i, &Last_y_prime);
				n7 = hpilXCore.buf[i] == '0' ? 0 : 2;
			default :
				error = ERR_NONE;
		}
	}
	return error;
}

int hpil_plotterSelect_sub(int error) {
	static int i, n;
	if (error == ERR_NONE) {
		error = ERR_INTERRUPTIBLE;
		switch (hpil_step) {
			case 0 :
				ILCMD_AAD(0x01);
				hpil_step++;
				break;
			case 1 :
				n = (frame & 0x001f) - 1;
				if (flags.f.manual_IO_mode) {
					i = hpil_settings.selected;
					hpil_step++;
				}
				else if (hpil_settings.selected) {
					i = hpil_settings.selected;
				}
				else {
					i = 1;
				}
				ILCMD_TAD(i);
				hpil_step++;
				error = call_ilCompletion(hpil_aid_sub);
				break;
			case 2:
				if (hpilXCore.bufPtr && ((hpilXCore.buf[0] & 0xf0) == 0x60)) {
					ILCMD_nop;
					hpil_step++;
				}
				else {
					if (i < n) {
						ILCMD_TAD(++i);
						error = call_ilCompletion(hpil_aid_sub);
					}
					else {
						ILCMD_nop;
						hpil_step++;
					}
				}
				break;
			case 3 :
				if (hpilXCore.bufPtr) {
					if ((hpilXCore.buf[0] & 0xf0) == 0x60) {
						hpil_settings.plotter = i;
						ILCMD_nop;
						error = rtn_il_completion();
					}
					else {
						error = ERR_NO_PLOTTER;
					}
				}
				else {
					error = ERR_NO_RESPONSE;
				}
				break;
			default :
				error = ERR_NONE;
		}
	}
	return error;
}

int hpil_plotterSend_sub(int error) {
	if (error == ERR_NONE) {
		error = ERR_INTERRUPTIBLE;
		switch (hpil_step) {
			case 0 :		// AAU > AAD
				ILCMD_AAD(0x01);
				hpil_step++;
				break;
			case 1 :		// AAD > LAD Selected
				ILCMD_LAD(hpil_settings.plotter);
				hpil_step++;
				break;
			case 2 :		// LAD > tlk
				ILCMD_tlk;
				hpilXCore.bufPtr = 0;
				hpil_step++;
				break;
			case 3 :		// LAD > UNL
				ILCMD_UNL;
				error = rtn_il_completion();
				break;
			default :
				error = ERR_NONE;
		}
	}
	return error;
}

int hpil_plotterSendGet_sub(int error) {
	if (error == ERR_NONE) {
		error = ERR_INTERRUPTIBLE;
		switch (hpil_step) {
			case 0 :		// AAU > AAD
				ILCMD_AAD(0x01);
				hpil_step++;
				break;
			case 1 :		// AAD > LAD Selected
				ILCMD_LAD(hpil_settings.plotter);
				hpil_step++;
				break;
			case 2 :		// LAD > tlk
				ILCMD_tlk;
				hpilXCore.bufPtr = 0;
				hpil_step++;
				break;
			case 3 :		// LAD > UNL
				ILCMD_UNL;
				hpil_step++;
				break;
			case 4 :		// UNL > TAD Selected
				ILCMD_TAD(hpil_settings.plotter);
				hpil_step++;
				break;
			case 5 :		// TAD > ltn & prepare buffer
				ILCMD_ltn;
				hpilXCore.bufPtr = 0;
				hpilXCore.bufSize = 0x100;
				hpilXCore.statusFlags |= ListenTilCrLf;
				hpil_step++;
				break;
			case 6 :		// Local listen > SDA
				ILCMD_SDA;
				hpil_step++;
				break;
			case 7 :		// SDA > lun
				ILCMD_lun;
				hpil_step++;
				break;
			case 8 :		// lun > UNT;
				ILCMD_UNT;
				hpil_step++;
				break;
			case 9 :		// get result
				if (hpilXCore.bufPtr == 0) {
					error = ERR_NO_RESPONSE;
				}
				else if (!(hpilXCore.statusFlags & ListenTilCrLf)) {
					ILCMD_NOP;
					error = rtn_il_completion();
				}
				else {
					error = ERR_TRANSMIT_ERROR;
				}
				break;
			default :
				error = ERR_INTERNAL_ERROR;
		}
	}
	if (error != ERR_INTERRUPTIBLE) {
		hpilXCore.statusFlags &= ~ListenTilCrLf;
	}
	return error;
}

int hpil_parse(char *str, int len, phloat *p) {
    int i = 0, j = 0, l = 0;
	char c;
    while (i < len) {
        c = str[i++];
        if (c < '0' || c > '9') {
			if (l == 0) {
				j++;
			}
			else {
				break;
			}
		}
		else {
			l++;
		}
	}
	if (l != 0) {
		parse_phloat(&str[j], l, p);
	}		
	return i;	
}