/*****************************************************************************
 * FreeIL -- an HP-IL loop emulation
 * Copyright (C) 2014-2016 Jean-Christophe HESSEMANN
 *
 * This program is free software// you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY// without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program// if not, see http://www.gnu.org/licenses/.
 *****************************************************************************/

#include <stdio.h>

#include "core_display.h"
#include "core_ebml.h"
#include "core_globals.h"
#include "core_helpers.h"
#include "core_main.h"
#include "hpil_common.h"
#include "hpil_controller.h"
#include "hpil_plotter.h"
#include "shell.h"
#include "string.h"

HPIL_Controller hpil_core;
HpilXController hpilXCore;
int controllerCommand;
int frame;
uint4 loopTimeout;
AltBuf hpil_controllerAltBuf;
DataBuf hpil_controllerDataBuf;


HPIL_Settings hpil_settings;
void (*hpil_emptyListenBuffer)(void);
void (*hpil_fillTalkBuffer)(void);

// hpil completion nano machine & stack
int (*hpil_completion)(int);
int hpil_step;
#define Hpil_stack_depth 10
int (*hpil_completion_st[Hpil_stack_depth])(int);
int hpil_step_st[Hpil_stack_depth];
int hpil_sp;

int IFCRunning, IFCCountdown;	// IFC timing

char hpil_text[23];		// generic test buffer
uint4 lastTimeCheck, timeCheck;

AlphaSplit alphaSplit;

void clear_il_st();
int hpil_write_frame(void);

/* restart
 *
 * performs power up sequence
 * and controller state initialization
 * but does not clear stack
 */
void hpil_restart(void) {
	hpilXCore.buf = hpil_controllerDataBuf.data;
	hpilXCore.bufSize = ControllerDataBufSize;
	hpilXCore.bufPtr = 0;
	hpilXCore.statusFlags = 0;
	hpil_core.begin(&hpilXCore);
	hpil_settings.modeTransparent = false;
}

/* start
 *
 * same as above
 * but reset stack and completion
 *
 */
void hpil_start(void) {
	hpil_restart();
	hpil_completion = NULL;
	clear_il_st();
}

/* IL init
 *
 */
void hpil_init(bool modeEnabled, bool modePIL_Box) {
	int err = ERR_BROKEN_LOOP;
	hpil_settings.modeEnabled = modeEnabled;
	hpil_settings.modePIL_Box = modePIL_Box;
	hpil_start();
	// should init all hpil modules
	hpil_plotter_init();
	if (hpil_settings.modePIL_Box) {
		shell_write_frame(M_CON);
		loopTimeout = 250;			// 500 ms
		frame = M_NOP;
		while (!shell_read_frame(&frame) && loopTimeout--);
		if (frame != M_CON) {
			hpil_settings.modeEnabled = false;
		}
	}
	if (hpil_settings.modeEnabled) {
		ILCMD_IFC;
		IFCRunning = 1;
		IFCCountdown = 15;
		hpil_settings.modeTransparent = true;
		do {
			err = hpil_worker(ERR_NONE);
		} 
		while (err == ERR_INTERRUPTIBLE);
		hpil_settings.modeTransparent = false;
	}
	if (err != ERR_NONE) {
			hpil_settings.modeEnabled = false;
	}
	else {
		flags.f.VIRTUAL_input = hpil_settings.modeEnabled;
	}
}

void hpil_close(bool modeEnabled, bool modePil_Box) {
	if (hpil_settings.modePIL_Box) {
		shell_write_frame(M_COFF);
		loopTimeout = 250;			// 500 ms
		frame = M_NOP;
		while (!shell_read_frame(&frame) && loopTimeout--);
	}
}

/* persist_hpil
 *
 * save parts of hpil settings
 */
bool persist_hpil(void) {
	if (!ebmlWriteElInt(EL_hpil_selected, hpil_settings.selected)) {
		return false;
	}
	if (!ebmlWriteElInt(EL_hpil_print, hpil_settings.print)) {
		return false;
	}
	if (!ebmlWriteElInt(EL_hpil_disk, hpil_settings.disk)) {
		return false;
	}
	if (!ebmlWriteElInt(EL_hpil_plotter, hpil_settings.plotter)) {
		return false;
	}
	if (!ebmlWriteElInt(EL_hpil_prtAid, hpil_settings.prtAid)) {
		return false;
	}
	if (!ebmlWriteElInt(EL_hpil_dskAid, hpil_settings.dskAid)) {
		return false;
	}
	if (!ebmlWriteElBool(EL_hpil_modeEnabled, hpil_settings.modeEnabled)) {
		return false;
	}
	if (!ebmlWriteElBool(EL_hpil_modeTransparent, hpil_settings.modeTransparent)) {
		return false;
	}
	if (!ebmlWriteElBool(EL_hpil_modePIL_Box, hpil_settings.modePIL_Box)) {
		return false;
	}
	return true;
}

/* unpersist_hpil
 *
 * get back parts of hpil settings
 */
bool unpersist_hpil(int ver) {
	return shell_read_saved_state(&hpil_settings, sizeof(hpil_settings)) == sizeof(hpil_settings);
}

/* hpil_checkup
 *
 * check command validity and hpil loop input / output connected
 */
int hpil_check() {
	int err;
	if (!core_settings.enable_ext_hpil) {
        err = ERR_NONEXISTENT;
	}
	else if (!hpil_settings.modeEnabled) {
		err = ERR_BROKEN_LOOP;
	}
	else if (hpilXCore.statusFlags & (CmdNew | CmdRun)) {
		err = ERR_RESTRICTED_OPERATION;
	}
	else { 
		err = shell_check_connectivity();
	}
	return err;
}

/* hpil_worker
 *
 * main, interruptible loop for processing hpil messages
 */
int hpil_worker(int interrupted) {
	int err = ERR_INTERRUPTIBLE;
	char s[100];

	if (interrupted) {
        err = ERR_STOP;
	}
	// New command ?
	else  if (hpilXCore.statusFlags & CmdNew) {
		//sprintf(s,"New %06X\n",controllerCommand);
		//shell_write_console(s);
		// clear buffers status flags
		hpilXCore.statusFlags &= ~MaskBuf;
		if (controllerCommand == Local) {
			// fake command, next step
			hpilXCore.statusFlags &= ~CmdNew;
			if (hpil_completion) {
				err =  hpil_completion(ERR_NONE);
			}
		}
		else if (controllerCommand & Local) {
			// local controller command
			hpilXCore.statusFlags &= ~CmdNew;
			hpilXCore.statusFlags |= CmdRun;
			hpil_core.pseudoSet(controllerCommand & ~Local);
			if (!(controllerCommand & tlk)) {
				// simulate hshk for local commands
				hpil_core.pseudoSet(hshk);
			}
		}
		else {
			// anything else
			hpilXCore.statusFlags &= ~CmdNew;
			hpilXCore.statusFlags |= CmdRun;
			hpil_core.controllerCmd(controllerCommand);
		}
		while (hpil_core.process()) {
			if (hpil_core.pseudoTcl(outf)) {
				frame = hpil_core.frameTx();
				if (hpil_write_frame() != ERR_NONE) {
					err = ERR_BROKEN_IP;
				}
			}
		}
	}
	else if (IFCRunning) {
		if (shell_read_frame(&frame)) {
			//sprintf(s,"IFC Rx  %06X\n",frame);
			//shell_write_console(s);
			hpil_core.frameRx((uint16_t)frame);			
			switch (IFCRunning) {
				case 1 :		// > IFC has been looped back
					if (frame == M_IFC) {
						// and received - process loop
						while (hpil_core.process()) {
							if (hpil_core.pseudoTcl(outf)) {
								frame = hpil_core.frameTx();
								if (hpil_write_frame() != ERR_NONE) {
									// check on first send only
									err = ERR_BROKEN_IP;
								}
							}
						}
						IFCRunning++;
					}
					break;
				case 2 :	// > IFC looped - wait for RFC
					if (frame == M_RFC) {
						// rfc received, process loop;
						while (hpil_core.process());
						IFCRunning = 0;
					}
					break;
			}
		}
		else if (IFCRunning == 1 && loopTimeout < 2 && (IFCCountdown-- > 0)) {
			// need to restart core state machine
			hpil_core.begin(&hpilXCore);
			hpilXCore.statusFlags = 0;
			ILCMD_IFC;
		}
		else {
			loopTimeout --;
		}
	}
	// Receiving message ?
	else if (shell_read_frame(&frame)) {
		//sprintf(s,"Rx  %06X\n",frame);
		//shell_write_console(s);
		hpil_core.frameRx((uint16_t)frame);
		while (hpil_core.process()) {
			if (hpil_core.pseudoTcl(outf)) {
				frame = hpil_core.frameTx();
				if (hpil_write_frame() != ERR_NONE) {
					err = ERR_BROKEN_IP;
				}
			}
		}
	}
	else {
		//shell_write_console("Loop\n");
		loopTimeout--;
		while (hpil_core.process()) {
			//shell_write_console("Active\n");

			if (hpil_core.pseudoTcl(outf)) {
				frame = hpil_core.frameTx();
				if (hpil_write_frame() != ERR_NONE) {
					err = ERR_BROKEN_IP;
				}
			}
		}
	}
	// check end of current command
	if (hpil_core.pseudoTcl(hshk)){
		hpilXCore.statusFlags &= ~CmdRun;
		hpilXCore.statusFlags |= CmdHshk;
		if (hpil_completion) {
			err = hpil_completion(ERR_NONE);
		}
		else {
			err = ERR_NONE;
		}
	}
	if (loopTimeout == 0) {
		err = ERR_BROKEN_LOOP;
	}
	if (err == ERR_NONE || err == ERR_INTERRUPTIBLE || err == ERR_RUN) {
		// check buffers level and process
		if (hpilXCore.statusFlags & FullListenBuf) {
			if (hpilXCore.statusFlags & RunAgainListenBuf) {
				hpilXCore.statusFlags &= ~FullListenBuf;
			}
			if (hpil_emptyListenBuffer) {
				hpil_emptyListenBuffer();
				hpilXCore.bufPtr = 0;
			}
		}
		else if ((hpilXCore.statusFlags & EmptyTalkBuf) && (hpilXCore.statusFlags & RunAgainTalkBuf)) {
			hpilXCore.statusFlags &= ~EmptyTalkBuf;
			if (hpil_fillTalkBuffer) {
				hpil_fillTalkBuffer();
				hpilXCore.bufPtr = 0;
				if (!(hpilXCore.statusFlags & RunAgainTalkBuf)) {
					hpil_fillTalkBuffer = NULL;
				}
			}
			else {
				hpilXCore.statusFlags &= ~RunAgainTalkBuf;
			}
		}
	}
	else if (hpil_settings.modeTransparent) {
		hpil_restart();
	}
	else {
		hpil_start();
        shell_annunciators(-1, -1, 0, -1, -1, -1);
	}
	return err;
}

/* hpil_write_frame
 * 
 * send frame, prepare for timeout
 */
int hpil_write_frame() {
	if (!shell_write_frame(frame)) {
		return ERR_BROKEN_IP;
	}
	if ((frame & 0x0600) == 0x0600) {	// IDY
		loopTimeout = 125;				// 125 x 2 ms shell timeout -> 250 ms (augmented from initial 50 ms, too much latency for idy)
	}
	else if (frame == 0x0490) {			// IFC - special timing - increase timing at each loop
		loopTimeout = 50 * (16 - IFCCountdown);
	}
	else if (frame & 0x0400) {			// CMD
		loopTimeout = 500;				// 500 x 2 ms shell timeout -> 1000 ms
	}
	else {								// DOE or RDY
		loopTimeout = 1500;				// 1500 x 2 ms shell timeout -> 3000 ms
	}
	return ERR_NONE;
}

/* IL stack
 *
 * to enable il subroutines calls
 */
int call_ilCompletion(int (*hpil_completion_call)(int)) {
    int err = ERR_INTERRUPTIBLE;
    if (hpil_sp == Hpil_stack_depth) {
        err = ERR_IL_ERROR;
    }
	else {
		hpil_completion_st[hpil_sp] = hpil_completion;
		hpil_step_st[hpil_sp] = hpil_step;
		hpil_sp++;
		hpil_completion = hpil_completion_call;
		hpil_step = 0;
	}
    return err;
}

/* IL stack return
 *
 * counterpart for call
 */
int rtn_il_completion() {
    int err = ERR_INTERRUPTIBLE;
    if (hpil_sp == 0) {
        err = ERR_IL_ERROR;
	}
	else {
		hpil_sp--;
		hpil_completion = hpil_completion_st[hpil_sp];
		hpil_step = hpil_step_st[hpil_sp];
    }
	return err;
}

/* IL stack init
 *
 * simply set sp to 0
 */
void clear_il_st() {
    hpil_sp = 0;
}

/* AID internal Command
 *
 * read device ID
 * LAD to be done by caller
 */
int hpil_aid_sub(int error) {
	if (error == ERR_NONE) {
		error = ERR_INTERRUPTIBLE;
		switch (hpil_step) {
			case 0 :		// > buffer clear & ltn
				hpilXCore.bufPtr = 0;
				ILCMD_ltn;
				hpil_step++;
				break;
			case 1 :		// > SAI
				ILCMD_SAI;
				hpil_step++;
				break;
			case 2 :		// lun
				ILCMD_lun;
				error = rtn_il_completion();
				break;
			default :
				error = ERR_NONE;
		}
	}
	return error;
}

/* wait for status not busy
 *
 * wait till status is no more busy
 */
int hpil_wait_sub(int error) {
	if (error == ERR_NONE) {
		error = ERR_INTERRUPTIBLE;
		switch (hpil_step) {
			case 0 :		// > buffer clear & ltn
				hpilXCore.bufPtr = 0;
				ILCMD_ltn;
				hpil_step++;
				break;
			case 1 :		// > SST
				ILCMD_SST;
				hpil_step++;
				break;
			case 2 :		// > Status interpretation
				if (hpilXCore.bufPtr == 0) {
					error = ERR_NO_RESPONSE;
				}
				else {
					switch (hpilXCore.buf[0]) {
						case 0x00 :		// Idle
							ILCMD_lun;
							error = rtn_il_completion();
							break;
						case 0x20:		// > Busy
							hpil_step = 1;
							break;
						default:		// Anything else - same code as idle
							ILCMD_lun;
							error = rtn_il_completion();
							break;
					}
				}
				break;
			default :
				error = ERR_NONE;
		}
	}
	return error;
}

/* hpil_Display
 *
 * display / print
 */
int hpil_display_sub(int error) {
	if (error == ERR_NONE) {
		error = ERR_INTERRUPTIBLE;
        if ((flags.f.trace_print || flags.f.normal_print)
          && flags.f.printer_exists) {
			print_text(hpil_text,22,1);
		}
		scroll_display(1,hpil_text,22);
		error = rtn_il_completion();
	}
	ILCMD_nop;
	return error;
}

/* hpil_pause
 *
 * add a pause
 */
int hpil_pause_sub(int error) {
	if (error == ERR_NONE) {
		error = ERR_INTERRUPTIBLE;
		timeCheck = shell_milliseconds();
		if (timeCheck - 500 > lastTimeCheck) {
			lastTimeCheck = timeCheck;
			error = rtn_il_completion();
		}
	}
	ILCMD_nop;
	return error;
}

/* hpil_pauseAndDisplay
 *
 * add a pause, and display / print
 */
int hpil_pauseAndDisplay_sub(int error) {
	if (error == ERR_NONE) {
		error = ERR_INTERRUPTIBLE;
		timeCheck = shell_milliseconds();
		if (timeCheck - 500 > lastTimeCheck) {
			lastTimeCheck = timeCheck;
	        if ((flags.f.trace_print || flags.f.normal_print)
			  && flags.f.printer_exists) {
				print_text(hpil_text,22,1);
			}
			scroll_display(1,hpil_text,22);
			error = rtn_il_completion();
		}
	}
	ILCMD_nop;
	return error;
}

/* mappable_x_hpil()
 *
 * Validate x value for message assembly
 */
int mappable_x_hpil(int max, int *cmd) {
	int arg;
    if (reg_x->type == TYPE_REAL) {
        phloat x = ((vartype_real *) reg_x)->x;
		if ((x < 0) || (x > 65535 )) {
            return ERR_INVALID_DATA;
		}
        arg = to_int(x);
		if (x > max) {
            return ERR_INVALID_DATA;
		}
	}
	else {
        return ERR_INVALID_TYPE;
	}
	*cmd = (uint16_t) arg;
    return ERR_NONE;
}

/* mappable_x_char()
 *
 * Validate x value for a char
 */
int mappable_x_char(uint8_t *c) {
	phloat x;
    vartype_string *s;
	if (reg_x->type == TYPE_REAL) {
        phloat x = ((vartype_real *) reg_x)->x;
		if (x < 0) {
            x = -x;
		}
		if (x > 255) {
			return ERR_INVALID_DATA;
		}
		*c = (uint8_t)to_int(x);
    }
	else if (reg_x->type == TYPE_STRING) {
		s = (vartype_string *) reg_x;
		if (s->length == 0) {
			return ERR_INVALID_DATA;
		}
		*c = (uint8_t) s->text[0];
	}
	else {
	    return ERR_INVALID_TYPE;
	}
	return ERR_NONE;
}

/* splitAlphaReg
 *
 * extract strings from alpha reg
 */
int hpil_splitAlphaReg(int mode) {
	int i;
	int dotSeen;
	// check alpha format
	alphaSplit.len1 = 0;
	alphaSplit.len2 = 0;
	dotSeen = false;
	if (reg_alpha_length == 0) {
		return ERR_ALPHA_DATA_IS_INVALID;
	}
	for (i = 0; i < reg_alpha_length; i++) {
		if ((reg_alpha[i] == '.') && !dotSeen) {
			dotSeen = true;
		}
		//else if ((reg_alpha[i] == '*') && (i == (reg_alpha_length - 1)) && dotSeen) {
		//	starSeen = true;
		//}
		else if (!dotSeen && (alphaSplit.len1 < 10)) {
			alphaSplit.str1[alphaSplit.len1++] = reg_alpha[i];
		}
		else if (dotSeen && (alphaSplit.len2 < 7)) {
			alphaSplit.str2[alphaSplit.len2++] = reg_alpha[i];
		}
		else {
			return ERR_ALPHA_DATA_IS_INVALID;
		}
	}
	// pad file name
	if (mode & SPLIT_MODE_VAR) {
		for (alphaSplit.len1; alphaSplit.len1 < 10; alphaSplit.len1++) {
			alphaSplit.str1[alphaSplit.len1] = ' ';
		}
	}
	if ((mode & SPLIT_MODE_PRGM) && (alphaSplit.len2 != 0)) {
		for (alphaSplit.len2; alphaSplit.len2 < 10; alphaSplit.len2++) {
			alphaSplit.str2[alphaSplit.len2] = ' ';
		}
	}
	return ERR_NONE;
}
