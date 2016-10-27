/*****************************************************************************
 * Free42 -- an HP-42S calculator simulator
 * Copyright (C) 2004-2016  Thomas Okken
 * Copyright (C) 2015-2016  Jean-Christophe Hessemann, hpil extensions
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

/* printer code
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include "core_display.h"
#include "core_globals.h"
#include "core_main.h"
#include "core_variables.h"
#include "hpil_common.h"
#include "hpil_controller.h"
#include "hpil_printer.h"
#include "shell.h"

/* common vars from core_commands8
 *
 */
extern HPIL_Settings hpil_settings;
extern HpilXController hpilXCore;
extern int controllerCommand;
extern int (*hpil_completion)(int);
extern int hpil_step;
extern int hpil_sp;
extern int hpil_worker(int interrupted);
extern void (*hpil_emptyListenBuffer)(void);
extern void (*hpil_fillTalkBuffer)(void);
extern int frame;

/* buffers structs
 *
 */
extern AltBuf hpil_controllerAltBuf;
extern DataBuf hpil_controllerDataBuf;

/* Status byte definitions for 82162 
 * First byte
 */
#define PrinterStatusER 0x08	// Error

/* Status byte definitions for 82162 
 * Second byte
 */
#define PrinterStatusLC 0x01	// Lower Case
#define PrinterStatusCO 0x02	// Column mode
#define PrinterStatusDW 0x04	// Double Wide
#define PrinterStatusRJ 0x08	// Right Justify
#define PrinterStatusEB 0x10	// Eight Bits mode
#define PrinterStatusBE 0x20	// Buffer Empty
#define PrinterStatusID 0x40	// printer IDle
#define PrinterStatusEL 0x80	// End Line

#define skip0Column		0xb8	// skip column

/* Printer variables
 *
 */
unsigned char printerStatus;
//char *printText;
//int printLength;
//int printed;
//bool printRightJustify;
unsigned char graphics[144];
unsigned char toPrinter[150];
int rasterLine = 0;

/* Translation table for 82162A printer
 *
 */
const unsigned char TranslateCode[] = {
	0x00, 0x01, 0x3f, 0x53, 0x1f, 0x7e, 0x3f, 0x7b,
	0x3f, 0x3f, 0x0a, 0x3f, 0x1d, 0x0d, 0x07, 0x7d,
	0x03, 0x0c, 0x1e, 0x3f, 0x13, 0x3f, 0x16, 0x7c,
	0x3f, 0x1b, 0x3f, 0x3f, 0x18, 0x1a, 0x1e, 0x3f,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
	0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
	0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
	0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
	0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
	0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
	0x3f, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
	0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
	0x78, 0x79, 0x7a, 0x3f, 0x3f, 0x3f, 0x3f, 0x7f
};

void hpil_printRaster(unsigned char*, int);
int hpil_printerSelect();
int hpil_printerStatus();
int hpil_printText(unsigned char*, int);
int hpil_printerSelect_completion(int);
int hpil_printerStatus_completion(int);
int hpil_printText_completion(int);

/* hpil_printText
 *
 * entry point for print worker
 */
void hpil_printText(const char *text, int length, int left_justified) {
	int i, j;
	int statusUpdate;

	if (hpil_check() != ERR_NONE)
        return;
	if (hpil_printerSelect() != ERR_NONE)
		return;
	if (hpil_printerStatus() != ERR_NONE)
		return;
	if (hpil_controllerAltBuf.data[0] & PrinterStatusER)
		return;
	printerStatus = hpil_controllerAltBuf.data[1];
	j = 0;
	if (!(printerStatus & PrinterStatusEB)) {
		// insert escape sequence to go to 8 bits mode, Esc 1 (thanks to TOS, note on manual)
		toPrinter[j++] = 0x1b;
		toPrinter[j++] = 0x7c;
		printerStatus |= PrinterStatusEB;
	}
	if (!left_justified && !(printerStatus & PrinterStatusRJ)) {
		// set right justify
		toPrinter[j++] = 0xe8;
		printerStatus |= PrinterStatusRJ;
	}
	else if (left_justified && (printerStatus & PrinterStatusRJ)) {
		// set left justify
		toPrinter[j++] = 0xe0;
		printerStatus &= ~PrinterStatusRJ;
	}
	// default to single / char / upper
	statusUpdate = 0;
	toPrinter[j] = 0xd0;
	// define mode
	if (flags.f.double_wide_print && !(printerStatus & PrinterStatusDW)) {
		toPrinter[j] |= 0x04;
		printerStatus |= PrinterStatusDW;
		statusUpdate = 1;
	}
	else if (!flags.f.double_wide_print && (printerStatus & PrinterStatusDW)) {
		printerStatus &= ~PrinterStatusDW;
		statusUpdate = 1;
	}
	if (flags.f.lowercase_print & !(printerStatus & PrinterStatusLC)) {
		toPrinter[j] |= 0x01;
		printerStatus |= PrinterStatusLC;
		statusUpdate = 1;
	}
	else if (!flags.f.lowercase_print & (printerStatus & PrinterStatusLC)) {
		printerStatus &= ~PrinterStatusLC;
		statusUpdate = 1;
	}
	if (statusUpdate) {
		j++;
	}
	i = 0;
	do {
		while ((j < 0x66) && (i < length)) {
			toPrinter[j++] = TranslateCode[text[i++] & 0x7f];
		}
		if ((i == length) && (j < 0x66)) {
			toPrinter[j++] = 0x0d;
			toPrinter[j++] = 0x0a;
			i++;
		}
		hpil_printText(toPrinter, j);
	} while (i <= length);
}

/* hpil_printLCD
 *
 * entry point for print worker
 */
void hpil_printLcd(const char *bits, int bytesperline, int x, int y, int width, int height) {
	int xx,yy;
	char c;

	if (hpil_check() != ERR_NONE)
        return;
	if (hpil_printerSelect() != ERR_NONE)
		return;
	if (hpil_printerStatus() != ERR_NONE)
		return;
	if (hpil_controllerAltBuf.data[0] & PrinterStatusER)
		return;
	printerStatus = hpil_controllerAltBuf.data[1];
	do {
		for (xx = x; xx < width; xx++) {
			c = 0;
			for (yy = 0; (yy + rasterLine < 7) && (yy + y < height); yy++) {
				c |= (bits[(xx >> 3) + (bytesperline * (yy + y))] & (0x01 << (xx & 0x07))) ? 1 << (yy % 7) : 0; 
			}
			graphics[xx] = (rasterLine == 0) ? c : graphics[xx] | (c << rasterLine);
		}
		if (yy + rasterLine < 7) {
			rasterLine += yy;
		}
		else {
			rasterLine = 0;
			hpil_printRaster(graphics, xx);
		}
		y += yy;
	} while(y < height);
}

void hpil_printRaster(unsigned char *buf, int len) {
	int i, j, skipped;

	j = 0;
	skipped = 0;
	if (!(printerStatus & PrinterStatusEB)) {
		// insert escape sequence to go to 8 bits mode, Esc | (erroneous hand written note on TOS manual)
		toPrinter[j++] = 0x1b;
		toPrinter[j++] = 0x7c;
		printerStatus |= PrinterStatusEB;
	}
	if (printerStatus & PrinterStatusRJ) {
		toPrinter[j++] = 0xe0;
		printerStatus &= ~PrinterStatusRJ;
	}
	if (!(printerStatus & PrinterStatusCO)) {
		toPrinter[j++] = 0xd2;
		printerStatus |= PrinterStatusCO;
	}
	for (i = 0; i < len; i++) {
		if (buf[i]) {
			if (skipped) {
				skipped = 0;
				j++;
			}
			toPrinter[j++] = buf[i];
		}
		else if (skipped < 7) {
			skipped ++;
			toPrinter[j] = skip0Column + skipped;
		}
		else {
			skipped = 1;
			j++;
			toPrinter[j] = skip0Column + skipped;
		}
	}
	toPrinter[j++] = 0xd0;
	printerStatus &= ~PrinterStatusCO;
	//hpil_showraster(buf);
	//hpil_showraster(toPrinter);

	toPrinter[j++] = 0x0d;
	hpil_printText(toPrinter, j);
}

void hpil_showraster(unsigned char*buf) {
	char asciiart[250];
	int i, j, k, l;
	for(i=0;i<7;i++) {
		sprintf(asciiart,"%u ",i);
		k = 2;
		for (j=0;j<30;j++) {
			if (buf[j] & 0x80) {
				if ((buf[j] & 0xb8) == 0xb8) {
					l = buf[j] & 0x07;
					do {
					 asciiart[k++] = ' ';  
					} while (--l);
				}
			}
			else {
				asciiart[k++] = (buf[j] & (1 << i)) ? 'X' : ' ';
			}
		}
		asciiart[k++] = 0x0d;
		asciiart[k++] = 0x0a;
		asciiart[k] = 0;
		//shell_write_console(asciiart);
	}
}

/*
int hpil_printText_completion(int error) {
	int i;
	if (error == ERR_NONE) {
		error = ERR_INTERRUPTIBLE;
		switch (hpil_step) {
			case 0 :		// get printer device
				hpilXCore.buf = hpil_controllerAltBuf.data;
				ILCMD_nop;
				hpil_step ++;
				error = call_ilCompletion(hpil_printerSelect_sub);
				break;
			case 1 :		// get status
				ILCMD_TAD(hpil_settings.print);
				hpil_step++;
				error = call_ilCompletion(hpil_printerStatus_sub);
				break;
			case 2 :		// combine flags & status, build string
				if (hpilXCore.buf[0] & PrinterStatusER) {
					error = ERR_PRINTER_ERR;
				}
				//else if (!(hpilXCore.buf[1] & PrinterStatusBE)) {
				//	ILCMD_nop;
				//	hpil_step = 1;
				//}
				else {
					i = 0;
					if (!(hpilXCore.buf[1] & PrinterStatusEB)) {
						// insert escape sequence to go to 8 bits mode, Esc 1 (thanks to TOS, note on manual)
						hpil_controllerDataBuf.data[i++] = 0x1b;
						hpil_controllerDataBuf.data[i++] = 0x7c;
					}
					if (printRightJustify && !(hpilXCore.buf[1] & PrinterStatusRJ)) {
						// set right justify
						hpil_controllerDataBuf.data[i++] = 0xe8;
					}
					else if (!printRightJustify && (hpilXCore.buf[1] & PrinterStatusRJ)) {
						// set left justify
						hpil_controllerDataBuf.data[i++] = 0xe0;
					}
					// default to single / char / upper
					hpil_controllerDataBuf.data[i] = 0xd0;
					// define mode
					if (flags.f.double_wide_print) {
						hpil_controllerDataBuf.data[i] |= 0x04;
					}
					if (flags.f.lowercase_print) {
						hpil_controllerDataBuf.data[i] |= 0x01;
					}
					i++;
					while ((i < 0x66) && (printed < printLength)) {
						hpil_controllerDataBuf.data[i++] = TranslateCode[printText[printed++] & 0x7f];
					}
					if ((printed == printLength) && (i < 0x66)) {
						hpil_controllerDataBuf.data[i++] = 0x0d;
						hpil_controllerDataBuf.data[i++] = 0x0a;
					}
					hpilXCore.buf = hpil_controllerDataBuf.data;
					hpilXCore.bufPtr = 0;
					hpilXCore.bufSize = i;
					ILCMD_LAD(hpil_settings.print);
					hpil_step++;
				}
				break;
			case 3 :
				ILCMD_tlk;
				if (printed <= printLength) {
					hpil_step++;
				}
				else {
					hpil_step = 1;
				}

				break;
			case 4 :
				ILCMD_UNL;
				hpil_step ++;
				break;
			default :
				error = ERR_NONE;

		}
	}
	return error;
}
*/

/* hpil_PrinterSelect
 *
 * select printer using auto / manual mode and dskAid
 */
int hpil_printerSelect() {
	int error = ERR_NONE;
	int (*save_hpil_completion)(int);
	int save_hpil_step;
	int save_hpil_sp;

	// backup current hpil_state
	save_hpil_completion = hpil_completion;
	save_hpil_step = hpil_step;
	save_hpil_sp = hpil_sp;
	hpil_settings.modeTransparent = true;

	hpilXCore.buf = hpil_controllerAltBuf.data;
	ILCMD_AAU;
	hpil_step = 0;
	hpil_completion = hpil_printerSelect_completion;
	// go
	do {
		error = hpil_worker(ERR_NONE);
	} while (error == ERR_INTERRUPTIBLE);

	// restore hpil_state
	hpil_completion = save_hpil_completion;
	hpil_step = save_hpil_step;
	hpil_sp = save_hpil_sp;
	hpil_settings.modeTransparent = false;

	return error;
}

int hpil_printerSelect_completion(int error) {
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
				if (hpil_settings.prtAid) {
					i = hpil_settings.prtAid;
					hpil_step++;
				}
				else if (flags.f.manual_IO_mode) {
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
				if (hpilXCore.bufPtr && ((hpil_controllerAltBuf.data[0] & 0x20) == 0x20)) {
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
					if ((hpil_controllerAltBuf.data[0] & 0x20) == 0x20) {
						hpil_settings.print = i;
						error = ERR_NONE;
					}
					else {
						error = ERR_NO_PRINTER;
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

/* get printer status
 *
 * 2 bytes
 */
int hpil_printerStatus() {
	int error = ERR_NONE;
	int (*save_hpil_completion)(int);
	int save_hpil_step;
	int save_hpil_sp;

	// backup current hpil_state
	save_hpil_completion = hpil_completion;
	save_hpil_step = hpil_step;
	save_hpil_sp = hpil_sp;
	hpil_settings.modeTransparent = true;

	hpilXCore.buf = hpil_controllerAltBuf.data;
	hpilXCore.bufPtr = 0;
	hpilXCore.bufSize = 2;
	ILCMD_ltn;
	hpil_step = 0;
	hpil_completion = hpil_printerStatus_completion;
	// go
	do {
		error = hpil_worker(ERR_NONE);
	} while (error == ERR_INTERRUPTIBLE);

	// restore hpil_state
	hpil_completion = save_hpil_completion;
	hpil_step = save_hpil_step;
	hpil_sp = save_hpil_sp;
	hpil_settings.modeTransparent = false;

	return error;
}

int hpil_printerStatus_completion(int error) {
	if (error == ERR_NONE) {
		error = ERR_INTERRUPTIBLE;
		switch (hpil_step) {
			case 0 :		// > SST
				ILCMD_SST;
				hpil_step++;
				break;
			case 1 :
				ILCMD_lun;
				if (hpilXCore.bufPtr == 0) {
					error = ERR_NO_RESPONSE;
				}
				else {
					hpil_step++;
				}
				break;
			default :
				error = ERR_NONE;
		}
	}
return error;
}

/* output buffer to printer
 *
 * 2 bytes
 */
int hpil_printText(unsigned char *text, int len) {
	int error = ERR_NONE;
	int (*save_hpil_completion)(int);
	int save_hpil_step;
	int save_hpil_sp;

	// backup current hpil_state
	save_hpil_completion = hpil_completion;
	save_hpil_step = hpil_step;
	save_hpil_sp = hpil_sp;
	hpil_settings.modeTransparent = true;

	hpilXCore.buf = text;
	hpilXCore.bufPtr = 0;
	hpilXCore.bufSize = len;
	ILCMD_LAD(hpil_settings.print);
	hpil_step = 0;
	hpil_completion = hpil_printText_completion;
	// go
	do {
		error = hpil_worker(ERR_NONE);
	} while (error == ERR_INTERRUPTIBLE);

	// restore hpil_state
	hpil_completion = save_hpil_completion;
	hpil_step = save_hpil_step;
	hpil_sp = save_hpil_sp;
	hpil_settings.modeTransparent = false;

	return error;
}

int hpil_printText_completion(int error) {

	if (error == ERR_NONE) {
		error = ERR_INTERRUPTIBLE;
		switch (hpil_step) {
			case 0 :
				ILCMD_tlk;
				hpil_step++;
				break;
			case 1 :
				ILCMD_UNL;
				hpil_step ++;
				break;
			default :
				error = ERR_NONE;
		}
	}
	return error;
}
