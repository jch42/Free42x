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

#include "core_commands2.h"
#include "core_commands8.h"
#include "core_helpers.h"
#include "core_main.h"
#include "core_variables.h"
#include "hpil_controller.h"
#include "hpil_common.h"
#include "shell.h"

extern int frame;
extern int controllerCommand;
extern int (*hpil_completion)(int);
extern int hpil_step;
extern int IFCRunning, IFCCountdown;	// IFC timing


static int hpil_init_completion(int error);
static int hpil_nloop_completion(int error);
static int hpil_stat_completion(int error);
static int hpil_id_completion(int error);
static int hpil_aid_completion(int error);

extern AltBuf hpil_controllerAltBuf;
extern DataBuf hpil_controllerDataBuf;

extern HPIL_Settings hpil_settings;

extern HPIL_Controller hpil_core;
extern HpilXController hpilXCore;

int docmd_ifc(arg_struct *arg) {
	int err;
	err = hpil_check();
	if (err != ERR_NONE) {
		return err;
	}
	ILCMD_nop;
	hpil_completion = hpil_init_completion;
	mode_interruptible = hpil_worker;
	return ERR_INTERRUPTIBLE;
}

int docmd_nloop(arg_struct *arg) {
	int err;
	err = hpil_check();
	if (err != ERR_NONE) {
		return err;
	}
	ILCMD_AAU;
	hpil_step = 0;
	hpil_completion = hpil_nloop_completion;
	mode_interruptible = hpil_worker;
	return ERR_INTERRUPTIBLE;
}

int docmd_select(arg_struct *arg) {
    if (!core_settings.enable_ext_hpil)
        return ERR_NONEXISTENT;
	if (hpilXCore.statusFlags & (CmdNew | CmdRun))
		return ERR_RESTRICTED_OPERATION;
	return mappable_x_hpil(31,&hpil_settings.selected);
}

int docmd_rclsel(arg_struct *arg) {
	vartype *v;
    if (!core_settings.enable_ext_hpil)
        return ERR_NONEXISTENT;
	if (hpilXCore.statusFlags & (CmdNew | CmdRun))
		return ERR_RESTRICTED_OPERATION;
	v = new_real(hpil_settings.selected);
	if (v == NULL)
	    return ERR_INSUFFICIENT_MEMORY;
	recall_result(v);
    return ERR_NONE;}

int docmd_prtsel(arg_struct *arg) {
    if (!core_settings.enable_ext_hpil)
        return ERR_NONEXISTENT;
	if (hpilXCore.statusFlags & (CmdNew | CmdRun))
		return ERR_RESTRICTED_OPERATION;
	return mappable_x_hpil(31,&hpil_settings.prtAid);
}

int docmd_dsksel(arg_struct *arg) {
    if (!core_settings.enable_ext_hpil)
        return ERR_NONEXISTENT;
	if (hpilXCore.statusFlags & (CmdNew | CmdRun))
		return ERR_RESTRICTED_OPERATION;
	return mappable_x_hpil(31,&hpil_settings.dskAid);
}

int docmd_autoio(arg_struct *arg) {
    if (!core_settings.enable_ext_hpil)
        return ERR_NONEXISTENT;
	if (hpilXCore.statusFlags & (CmdNew | CmdRun))
		return ERR_RESTRICTED_OPERATION;
	flags.f.manual_IO_mode = 0;
    return ERR_NONE;
}

int docmd_manio(arg_struct *arg) {
    if (!core_settings.enable_ext_hpil)
        return ERR_NONEXISTENT;
	if (hpilXCore.statusFlags & (CmdNew | CmdRun))
		return ERR_RESTRICTED_OPERATION;
	flags.f.manual_IO_mode = 1;
    return ERR_NONE;
}

int docmd_stat(arg_struct *arg) {
	int err;
	err = hpil_check();
	if (err != ERR_NONE) {
		return err;
	}
	ILCMD_AAU;
	hpil_step = 0;
	hpil_completion = hpil_stat_completion;
	mode_interruptible = hpil_worker;
	return ERR_INTERRUPTIBLE;
}

int docmd_id(arg_struct *arg) {
	int err;
	err = hpil_check();
	if (err != ERR_NONE) {
		return err;
	}
	ILCMD_AAU;
	hpil_step = 0;
	hpil_completion = hpil_id_completion;
	mode_interruptible = hpil_worker;
	return ERR_INTERRUPTIBLE;
}

int docmd_aid(arg_struct *arg) {
	int err;
	err = hpil_check();
	if (err != ERR_NONE) {
		return err;
	}
	ILCMD_AAU;
	hpil_step = 0;
	hpil_completion = hpil_aid_completion;
	mode_interruptible = hpil_worker;
	return ERR_INTERRUPTIBLE;
}

int docmd_bldspec(arg_struct *arg) {
	int err;
	phloat x;
	if (reg_x->type == TYPE_REAL) {
		x = ((vartype_real *) reg_x)->x;
		if ((x > 0) && (x < 128)) {
			err = ERR_NONE;
		}
		else {
			err = ERR_OUT_OF_RANGE;
		}
	}
	else if (reg_x->type == TYPE_STRING) {
		err = ERR_ALPHA_DATA_IS_INVALID;
	}
	else {
		err = ERR_INVALID_TYPE;
	}
	return err;
}

/* hpil completion routines
 *
 * All high level hpil processing is done there
 */
static int hpil_init_completion(int error) {
	if (error == ERR_NONE) {
		error = ERR_INTERRUPTIBLE;
		switch (hpil_step) {
			case 0 :		// IFC
				ILCMD_IFC;
				IFCRunning = 1;
				IFCCountdown = 15;
				break;
			default :
				error = ERR_NONE;
		}
	}
	hpil_step++;
	return error;
}

static int hpil_nloop_completion(int error) {
	vartype *v;
	if (error == ERR_NONE) {
		error = ERR_INTERRUPTIBLE;
		switch (hpil_step) {
			case 0 :		// AAU > AAD
				ILCMD_AAD(0x01);
				break;
			case 1 :		// AAD get frame
				if ((frame & M_AAD) == M_AAD) {
					v = new_real((frame & 0x001f) - 1);
					error = ERR_NONE;
					if (v == NULL) {
						error = ERR_INSUFFICIENT_MEMORY;
					}
					recall_result(v);
					// no need ?
					//if (flags.f.trace_print && flags.f.printer_exists) {
					//	docmd_prx(NULL);
					//}
				}
				else {
					error = ERR_TRANSMIT_ERROR;
				}
				break;
			default :
				error = ERR_NONE;
		}
	}
	hpil_step++;
	return error;
}

static int hpil_stat_completion(int error) {
	int i = 0;
	if (error == ERR_NONE) {
		error = ERR_INTERRUPTIBLE;
		switch (hpil_step) {
			case 0 :		// AAU > AAD
				ILCMD_AAD(0x01);
				break;
			case 1 :		// AAD > TAD Selected
				ILCMD_TAD(hpil_settings.selected);
				break;
			case 2 :		// TAD > ltn & prepare buffer
				ILCMD_ltn;
				hpilXCore.bufSize = ControllerDataBufSize;
				hpilXCore.bufPtr = 0;
				break;
			case 3 :		// Local listen > SST
				ILCMD_SST;
				break;
			case 4 :		// SST > lun
				ILCMD_lun;
				break;
			case 5 :		// lun > UNT;
				ILCMD_UNT;
				break;
			case 6 :		// Display result
				if (hpilXCore.bufPtr) {
					reg_alpha[0]='s';
					for (i = 0; i < (hpilXCore.bufPtr > 43 ? 43 : hpilXCore.bufPtr); i++) {
						reg_alpha[i+1] = hpilXCore.buf[i];
					}
					reg_alpha_length = i+1;
					if (flags.f.trace_print && flags.f.printer_exists) {
						docmd_pra(NULL);
					}
					error = ERR_NONE;
				}
				else {
					reg_alpha_length = 0;
					error = ERR_NO_RESPONSE;
				}
				break;
			default :
				error = ERR_NONE;
		}
		hpil_step++;
	}
	return error;
}

static int hpil_id_completion(int error) {
	int i = 0;
	int j = 0;
	if (error == ERR_NONE) {
		error = ERR_INTERRUPTIBLE;
		switch (hpil_step) {
			case 0 :		// AAU > AAD
				ILCMD_AAD(0x01);
				break;
			case 1 :		// AAD > TAD Selected
				ILCMD_TAD(hpil_settings.selected);
				break;
			case 2 :		// TAD > ltn & prepare buffer
				ILCMD_ltn;
				hpilXCore.bufSize = ControllerDataBufSize;
				hpilXCore.bufPtr = 0;
				break;
			case 3 :		// Local listen > SDI
				ILCMD_SDI;
				break;
			case 4 :		// SST > lun
				ILCMD_lun;
				break;
			case 5 :		// lun > UNT;
				ILCMD_UNT;
				break;
			case 6 :		// Display result
				if (hpilXCore.bufPtr) {
					for (i = 0; i < (hpilXCore.bufPtr > 43 ? 43 : hpilXCore.bufPtr); i++) {
						if ((hpilXCore.buf[i] == 0x0a) || (hpilXCore.buf[i] == 0x0d)) {
							break;
						}
						else {
							reg_alpha[i] = hpilXCore.buf[i];
						}
					}
					reg_alpha_length = i;
					if (flags.f.trace_print && flags.f.printer_exists) {
						docmd_pra(NULL);
					}
					error = ERR_NONE;
				}
				else {
					reg_alpha_length = 0;
					error = ERR_NO_RESPONSE;
				}
				break;
			default :
				error = ERR_NONE;
		}
		hpil_step++;
	}
	return error;
}

static int hpil_aid_completion(int error) {
	if (error == ERR_NONE) {
		error = ERR_INTERRUPTIBLE;
		switch (hpil_step) {
			case 0 :		// AAU > AAD
				ILCMD_AAD(0x01);
				hpil_step++;
				break;
			case 1 :		// > TAD prepare buffer and call aid sub
				ILCMD_TAD(hpil_settings.selected);
				hpilXCore.buf = hpil_controllerAltBuf.data;
				hpilXCore.bufSize = ControllerAltBufSize;
				hpil_step++;
				error = call_ilCompletion(hpil_aid_sub);
				break;
			case 2 :		// Display result
				if (hpilXCore.bufPtr) {
					vartype *v = new_real(hpilXCore.buf[0] & 0xff);
				    if (v == NULL)
						return ERR_INSUFFICIENT_MEMORY;
					recall_result(v);
					error = ERR_NONE;
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
