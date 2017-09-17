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

/* plotter module simulation
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include "core_commands6.h"
#include "core_globals.h"
#include "core_helpers.h"
#include "core_main.h"
#include "core_variables.h"
#include "hpil_common.h"
#include "hpil_controller.h"
#include "hpil_plotter.h"
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

/* 
 * plotter struct
 */
struct {
	PLOTTER_IOBuf ioBuf;
	// some variables for plregx
	vartype * plReg;
	int plRegFrom, plRegTo;
} plotterData;

// status information
#define Status plotterData.ioBuf.BR[0]
// Lower left-hand and upper right-hand corners of graphic limits in APUs
#define P1_x plotterData.ioBuf.BR[1]
#define P2_x plotterData.ioBuf.BR[2]
#define P1_y plotterData.ioBuf.BR[3]
#define P2_y plotterData.ioBuf.BR[4]
// Lower left-hand and upper right-hand corners of plot bounds in APUs
#define x1 plotterData.ioBuf.BR[5]
#define x2 plotterData.ioBuf.BR[6]
#define y1 plotterData.ioBuf.BR[7]
#define y2 plotterData.ioBuf.BR[8]
// Intended pen position in APUs after most recent plotting or [LABEL] command
#define Last_x plotterData.ioBuf.BR[9]
#define Last_y plotterData.ioBuf.BR[10]
// Miscellaneous storage
#define Misc_x1 plotterData.ioBuf.BR[11]
#define Misc_y1 plotterData.ioBuf.BR[12]
// Actual pen position in APUs after most recent plotting or [LABEL] command
#define Last_x_prime plotterData.ioBuf.BR[13]
#define Last_y_prime plotterData.ioBuf.BR[14]
// Miscellaneous storage
#define Misc_x2 plotterData.ioBuf.BR[15]
#define Misc_y2 plotterData.ioBuf.BR[16]
// GU Scaling Factors (GUs > APUs)
#define Factor1_x plotterData.ioBuf.BR[17]
#define Factor2_x plotterData.ioBuf.BR[18]
#define Factor1_y plotterData.ioBuf.BR[19]
#define Factor2_y plotterData.ioBuf.BR[20]
// UU Scaling Factors (UUs > APUs)
#define Factor1_x_prime plotterData.ioBuf.BR[21]
#define Factor2_x_prime plotterData.ioBuf.BR[22]
#define Factor1_y_prime plotterData.ioBuf.BR[23]
#define Factor2_y_prime plotterData.ioBuf.BR[24]
// [BCSIZE] Parameters
#define BcSize plotterData.ioBuf.BR[25]


/* io Buf explicit flags */
#define PLOTTER_STATUS_OUTBOUND		0x01
#define PLOTTER_STATUS_PEN_DOWN		0x02
#define PLOTTER_STATUS_MODE_UU		0x04
/* Truth table :
 * Set pen status
 * | Set pen down
 * v v
 * 0 0  issue PD or PU to reflect Pen status, set Pen satus to down and issue a final PD
 * 0 1  issue PD only is Pen status is down, , set Pen satus to down and issue a final PD
 * 1 0  issue PU and force pen status to up 
 * 1 1  issue PD, set Pen satus to down and issue a final PD
 */
#define PLOTTER_FLAG_SET_PEN_STATUS	0x0010	// set pen status according to following flag. If both flags cleared, issue only pen down 
#define PLOTTER_FLAG_SET_PEN_DOWN	0x0020	// pen down flag or copy pen status flag, if set enforce pu or pd following pen status
#define PLOTTER_FLAG_INCREMENT		0x0040	// incremental move
#define PLOTTER_FLAG_RELATIVE		0x0080	// relative move, do not update initial position
#define PLOTTER_FLAG_PLOT_REG		0x0100	// plot register data
#define PLOTTER_FLAG_PLOT_AXIS		0x0200	// plot an axis
#define PLOTTER_FLAG_AXIS_Y			0x0400	// X or Y axis
#define PLOTTER_FLAG_AXIS_LABEL		0x0800	// plot axis label
#define PLOTTER_FLAG_AXIS_TICK		0x1000	// plot axis tick

extern AltBuf hpil_controllerAltBuf;
extern DataBuf hpil_controllerDataBuf;

static int hpil_plotter_cmd_completion(int);
static int hpil_plotter_label_completion(int);
static int hpil_plotter_axis_completion(int);
static int hpil_plotter_limit_pinit_completion(int);
static int hpil_plotter_plot_generic_completion(int);

static int hpil_plotterSelect_sub(int);
static int hpil_plotterSend_sub(int);
static int hpil_plotterSendGet_sub(int);
static void hpil_plotter_rescale(phloat *, phloat *);
static int hpil_parse(char*, int, phloat*);

int hpil_plotter_init() {
	plotterData.ioBuf.pinit_done = 0;
	return ERR_NONE;
}

int hpil_plotter_check() {
	if (plotterData.ioBuf.pinit_done) {
		return hpil_check();
	}
	return ERR_PLOTTER_INIT;
}

int docmd_clipuu(arg_struct *arg) {
	int err;
	err = hpil_plotter_check();
	if (err != ERR_NONE) {
		return err;
	}
	if ((reg_x->type == TYPE_REAL) && (reg_y->type == TYPE_REAL) && (reg_z->type == TYPE_REAL) && (reg_t->type == TYPE_REAL)) {
		x1 = Factor2_x_prime + ((vartype_real *)reg_t)->x * Factor1_x_prime;
		y1 = Factor2_y_prime + ((vartype_real *)reg_y)->x * Factor1_y_prime;
		x2 = Factor2_x_prime + ((vartype_real *)reg_z)->x * Factor1_x_prime;
		y2 = Factor2_y_prime + ((vartype_real *)reg_x)->x * Factor1_y_prime;
		sprintf((char *)hpil_controllerDataBuf.data, "IW %u,%u,%u,%u;",
			to_int(x1), to_int(y1), to_int(x2), to_int(y2));
		ILCMD_AAU;
		hpil_step = 0;
		hpil_completion = hpil_plotter_cmd_completion;
		mode_interruptible = hpil_worker;
		err = ERR_INTERRUPTIBLE;
	}
	else if ((reg_x->type == TYPE_STRING) || (reg_y->type == TYPE_STRING) || (reg_z->type == TYPE_STRING) || (reg_t->type == TYPE_STRING)) {
		return ERR_ALPHA_DATA_IS_INVALID;
	}
	else {
		err = ERR_INVALID_TYPE;
	}
	return err;
}

int docmd_csize(arg_struct *arg) {
	int err, i;
	char save_decimal_point;
	phloat x;
	phloat dx, dy, r, h, w;
	err = hpil_plotter_check();
	if (err != ERR_NONE) {
		return err;
	}
	if (reg_x->type == TYPE_REAL) {
		x = ((vartype_real *) reg_x)->x;
		// get absolute value
		if (x < 0) {
			x = -x;
		}
		// calculate ratio
		dx = P2_x - P1_x;
		dy = P2_y - P1_y;
		if ((dx == 0) || (dy == 0)) {
			return ERR_PLOTTER_DATA_ERR;
		}
		r = dx / dy;
		if (r < 0) {
			r = -r;
		}
		// calculate h & w :
		// if ratio >= 1 : h = x * 0.5 & w = h * 0.7 * ratio 
		// if ratio < 1 : w = x * 0.35 & h = w * ratio / 0.7
		if (r < 1) {
			w = x * 0.35;
			h = w * r / 0.7;
		}
		else {
			h = x * 0.5;
			w = h * 0.7 / r;
		}
		// force decimal point
		save_decimal_point = flags.f.decimal_point;
		flags.f.decimal_point = 1;
		i = sprintf((char *)hpil_controllerDataBuf.data, "SR ");
		i += phloat2string(w, (char *)&hpil_controllerDataBuf.data[i], 10, 0, 3, 0, 0, 6);
		hpil_controllerDataBuf.data[i] = 0;
		i += sprintf((char *)&hpil_controllerDataBuf.data[i], ",");
		i += phloat2string(h, (char *)&hpil_controllerDataBuf.data[i], 10, 0, 3, 0, 0, 6);
		flags.f.decimal_point = save_decimal_point;
		hpil_controllerDataBuf.data[i] = 0;
		i += sprintf((char *)&hpil_controllerDataBuf.data[i], ";SL 0;");
		ILCMD_AAU;
		hpil_step = 0;
		hpil_completion = hpil_plotter_cmd_completion;
		mode_interruptible = hpil_worker;
		err = ERR_INTERRUPTIBLE;
	}
	else if (reg_x->type == TYPE_STRING) {
		return ERR_ALPHA_DATA_IS_INVALID;
	}
	else {
		err = ERR_INVALID_TYPE;
	}
return err;
}

int docmd_csizeo(arg_struct *arg) {
	int err, i;
	char save_decimal_point;
	phloat x, y, z;
	phloat dx, dy, r, h, w, s;
	err = hpil_plotter_check();
	if (err != ERR_NONE) {
		return err;
	}
	if ((reg_x->type == TYPE_REAL) && (reg_y->type == TYPE_REAL) && (reg_z->type == TYPE_REAL)) {
		x = ((vartype_real *) reg_x)->x;
		y = ((vartype_real *) reg_y)->x;
		z = ((vartype_real *) reg_z)->x;
		// get absolute values
		if (x < 0) {
			x = -x;
		}
		if (y < 0) {
			y = -y;
		}
		err = mappable_tan_r(z, &s);
		if (err != ERR_NONE) {
			return err;
		}
		// calculate ratio
		dx = P2_x - P1_x;
		dy = P2_y - P1_y;
		if ((dx == 0) || (dy == 0)) {
			return ERR_PLOTTER_DATA_ERR;
		}
		r = dx / dy;
		if (r < 0) {
			r = -r;
		}
		// calculate h & w :
		// if ratio >= 1 : h = x * 0.5 & w = h * 0.7 * ratio 
		// if ratio < 1 : w = x * 0.35 & h = w * ratio / 0.7
		if (r < 1) {
			w = x * 0.35;
			h = w * r / 0.7;
		}
		else {
			h = x * 0.5;
			w = h * 0.7 / r;
		}
		// force decimal point
		save_decimal_point = flags.f.decimal_point;
		flags.f.decimal_point = 1;
		i = sprintf((char *)hpil_controllerDataBuf.data, "SR ");
		i += phloat2string(w, (char *)&hpil_controllerDataBuf.data[i], 10, 0, 3, 0, 0, 6);
		hpil_controllerDataBuf.data[i] = 0;
		i += sprintf((char *)&hpil_controllerDataBuf.data[i], ",");
		i += phloat2string(h, (char *)&hpil_controllerDataBuf.data[i], 10, 0, 3, 0, 0, 6);
		hpil_controllerDataBuf.data[i] = 0;
		i += sprintf((char *)&hpil_controllerDataBuf.data[i], ";SL ");
		i += phloat2string(s, (char *)&hpil_controllerDataBuf.data[i], 10, 0, 3, 0, 0, 6);
		i += sprintf((char *)&hpil_controllerDataBuf.data[i], ";");
		flags.f.decimal_point = save_decimal_point;
		ILCMD_AAU;
		hpil_step = 0;
		hpil_completion = hpil_plotter_cmd_completion;
		mode_interruptible = hpil_worker;
		err = ERR_INTERRUPTIBLE;
	}
	else if ((reg_x->type == TYPE_STRING) || (reg_y->type == TYPE_STRING) || (reg_z->type == TYPE_STRING)) {
		return ERR_ALPHA_DATA_IS_INVALID;
	}
	else {
		err = ERR_INVALID_TYPE;
	}
return err;
}

int docmd_draw(arg_struct *arg) {
	int err;
	err = hpil_plotter_check();
	if (err != ERR_NONE) {
		return err;
	}
	if ((reg_x->type == TYPE_REAL) && (reg_y->type == TYPE_REAL)) {
		plotterData.ioBuf.plotting_status = (plotterData.ioBuf.plotting_status & 0x000f) | (PLOTTER_FLAG_SET_PEN_STATUS | PLOTTER_FLAG_SET_PEN_DOWN);
		ILCMD_AAU;
		hpil_step = 0;
		hpil_completion = hpil_plotter_plot_generic_completion;
		mode_interruptible = hpil_worker;
		err = ERR_INTERRUPTIBLE;
	}
	else if ((reg_x->type == TYPE_STRING) || (reg_y->type == TYPE_STRING)) {
		return ERR_ALPHA_DATA_IS_INVALID;
	}
	else {
		err = ERR_INVALID_TYPE;
	}
	return err;
}

int docmd_frame(arg_struct *arg) {
	int err;
	err = hpil_plotter_check();
	if (err != ERR_NONE) {
		return err;
	}
	if (plotterData.ioBuf.plotting_status && PLOTTER_STATUS_MODE_UU) {
		sprintf((char *)hpil_controllerDataBuf.data, "PU;PA %u,%u;PD; PA %u,%u,%u,%u,%u,%u,%u,%u;PU;",
					to_int(x1), to_int(y1),
					to_int(x1), to_int(y2), to_int(x2), to_int(y2), to_int(x2), to_int(y1), to_int(x1), to_int(y1));
	}
	else {
		sprintf((char *)hpil_controllerDataBuf.data, "PU;PA %u,%u;PD; PA %u,%u,%u,%u,%u,%u,%u,%u;PU;",
					to_int(P1_x), to_int(P1_x),
					to_int(P1_x), to_int(P2_y), to_int(P2_x), to_int(P2_y), to_int(P2_x), to_int(P1_y), to_int(P1_x), to_int(P1_y));
	}
	ILCMD_AAU;
	hpil_step = 0;
	hpil_completion = hpil_plotter_cmd_completion;
	mode_interruptible = hpil_worker;
	return ERR_INTERRUPTIBLE;
}

int docmd_gclear(arg_struct *arg) {
	int err;
	err = hpil_plotter_check();
	if (err != ERR_NONE) {
		return err;
	}
	sprintf((char *)hpil_controllerDataBuf.data, "AF;");
	ILCMD_AAU;
	hpil_step = 0;
	hpil_completion = hpil_plotter_cmd_completion;
	mode_interruptible = hpil_worker;
	return ERR_INTERRUPTIBLE;
}

int docmd_idraw(arg_struct *arg) {
	int err;
	err = hpil_plotter_check();
	if (err != ERR_NONE) {
		return err;
	}
	if ((reg_x->type == TYPE_REAL) && (reg_y->type == TYPE_REAL)) {
		plotterData.ioBuf.plotting_status = (plotterData.ioBuf.plotting_status & 0x000f) | (PLOTTER_FLAG_SET_PEN_STATUS | PLOTTER_FLAG_SET_PEN_DOWN | PLOTTER_FLAG_INCREMENT);
		ILCMD_AAU;
		hpil_step = 0;
		hpil_completion = hpil_plotter_plot_generic_completion;
		mode_interruptible = hpil_worker;
		err = ERR_INTERRUPTIBLE;
	}
	else if ((reg_x->type == TYPE_STRING) || (reg_y->type == TYPE_STRING)) {
		return ERR_ALPHA_DATA_IS_INVALID;
	}
	else {
		err = ERR_INVALID_TYPE;
	}
	return err;
}

int docmd_imove(arg_struct *arg) {
	int err;
	err = hpil_plotter_check();
	if (err != ERR_NONE) {
		return err;
	}
	if ((reg_x->type == TYPE_REAL) && (reg_y->type == TYPE_REAL)) {
		plotterData.ioBuf.plotting_status = (plotterData.ioBuf.plotting_status & 0x000f) | (PLOTTER_FLAG_SET_PEN_STATUS | PLOTTER_FLAG_INCREMENT);
		ILCMD_AAU;
		hpil_step = 0;
		hpil_completion = hpil_plotter_plot_generic_completion;
		mode_interruptible = hpil_worker;
		err = ERR_INTERRUPTIBLE;
	}
	else if ((reg_x->type == TYPE_STRING) || (reg_y->type == TYPE_STRING)) {
		return ERR_ALPHA_DATA_IS_INVALID;
	}
	else {
		err = ERR_INVALID_TYPE;
	}
	return err;
}

int docmd_iplot(arg_struct *arg) {
	int err;
	err = hpil_plotter_check();
	if (err != ERR_NONE) {
		return err;
	}
	if ((reg_x->type == TYPE_REAL) && (reg_y->type == TYPE_REAL)) {
		plotterData.ioBuf.plotting_status = (plotterData.ioBuf.plotting_status & 0x000f) | (PLOTTER_FLAG_SET_PEN_DOWN | PLOTTER_FLAG_INCREMENT);
		ILCMD_AAU;
		hpil_step = 0;
		hpil_completion = hpil_plotter_plot_generic_completion;
		mode_interruptible = hpil_worker;
		err = ERR_INTERRUPTIBLE;
	}
	else if ((reg_x->type == TYPE_STRING) || (reg_y->type == TYPE_STRING)) {
		return ERR_ALPHA_DATA_IS_INVALID;
	}
	else {
		err = ERR_INVALID_TYPE;
	}
	return err;
}

int docmd_label(arg_struct *arg) {
	int err;
	phloat x, res;
	err = hpil_plotter_check();
	if (err != ERR_NONE) {
		return err;
	}
	ILCMD_AAU;
	hpil_step = 0;
	hpil_completion = hpil_plotter_label_completion;
	mode_interruptible = hpil_worker;
	return ERR_INTERRUPTIBLE;
}

int docmd_ldir(arg_struct *arg) {
	int err, i;
	char save_decimal_point;
	phloat x, run, rise;
    if (reg_x->type == TYPE_REAL) {
		x = ((vartype_real *)reg_x)->x;
		mappable_cos_r(x, &run);
		mappable_sin_r(x, &rise);
		i = sprintf((char *)hpil_controllerDataBuf.data, "DI ");
		save_decimal_point = flags.f.decimal_point;
		flags.f.decimal_point = 1;
		i += phloat2string(run, (char *)&hpil_controllerDataBuf.data[i], 10, 0, 3, 0, 0, 6);
		i += sprintf((char *)&hpil_controllerDataBuf.data[i], ",");
		i += phloat2string(rise, (char *)&hpil_controllerDataBuf.data[i], 10, 0, 3, 0, 0, 6);
		flags.f.decimal_point = save_decimal_point;
		i += sprintf((char *)&hpil_controllerDataBuf.data[i], ";");
		ILCMD_AAU;
		hpil_step = 0;
		hpil_completion = hpil_plotter_cmd_completion;
		mode_interruptible = hpil_worker;
		err = ERR_INTERRUPTIBLE;
    }
	else if (reg_x->type == TYPE_STRING) {
        return ERR_ALPHA_DATA_IS_INVALID;
	}
	else {
		return ERR_INVALID_TYPE;
	}
	return err;
}

int docmd_limit(arg_struct *arg) {
	int err;
	err = hpil_plotter_check();
	if (err != ERR_NONE) {
		return err;
	}
	if ((reg_x->type == TYPE_REAL) && (reg_y->type == TYPE_REAL) && (reg_z->type == TYPE_REAL) && (reg_t->type == TYPE_REAL)) {
		x1 = ((vartype_real *)reg_t)->x * 40;
		y1 = ((vartype_real *)reg_y)->x * 40;
		x2 = ((vartype_real *)reg_z)->x * 40;
		y2 = ((vartype_real *)reg_x)->x * 40;
		sprintf((char *)hpil_controllerDataBuf.data, "IP %u,%u,%u,%u;",
			to_int(x1), to_int(y1), to_int(x2), to_int(y2));
		ILCMD_AAU;
		hpil_step = 0;
		hpil_completion = hpil_plotter_limit_pinit_completion;
		mode_interruptible = hpil_worker;
		err = ERR_INTERRUPTIBLE;
	}
	else if ((reg_x->type == TYPE_STRING) || (reg_y->type == TYPE_STRING) || (reg_z->type == TYPE_STRING) || (reg_t->type == TYPE_STRING)) {
		err = ERR_ALPHA_DATA_IS_INVALID;
	}
	else {
		err = ERR_INVALID_TYPE;
	}
	return err;
}

int docmd_locate(arg_struct *arg) {
	int err;
	err = hpil_plotter_check();
	if (err != ERR_NONE) {
		return err;
	}
	if ((reg_x->type == TYPE_REAL) && (reg_y->type == TYPE_REAL) && (reg_z->type == TYPE_REAL) && (reg_t->type == TYPE_REAL)) {
		x1 = Factor2_x + ((vartype_real *)reg_t)->x * Factor1_x;
		y1 = Factor2_y + ((vartype_real *)reg_y)->x * Factor1_y;
		x2 = Factor2_x + ((vartype_real *)reg_z)->x * Factor1_x;
		y2 = Factor2_y + ((vartype_real *)reg_x)->x * Factor1_y;
		sprintf((char *)hpil_controllerDataBuf.data, "IW %u,%u,%u,%u;",
			to_int(x1), to_int(y1), to_int(x2), to_int(y2));
		ILCMD_AAU;
		hpil_step = 0;
		hpil_completion = hpil_plotter_cmd_completion;
		mode_interruptible = hpil_worker;
		err = ERR_INTERRUPTIBLE;
	}
	else if ((reg_x->type == TYPE_STRING) || (reg_y->type == TYPE_STRING) || (reg_z->type == TYPE_STRING) || (reg_t->type == TYPE_STRING)) {
		return ERR_ALPHA_DATA_IS_INVALID;
	}
	else {
		err = ERR_INVALID_TYPE;
	}
	return err;
}

int docmd_lorg(arg_struct *arg) {
	int err;
	phloat x, res;
	err = hpil_plotter_check();
	if (err != ERR_NONE) {
		return err;
	}
	if (reg_x->type == TYPE_REAL) {
		x = ((vartype_real *) reg_x)->x;
		if (x < 0) {
			x = -x;
		}
		x = floor(x);
		res = fmod(x, 10);
		if (res == 0) {
			res = 1;
        }
		plotterData.ioBuf.lorg = to_int(res);
		err = ERR_NONE;
	}
	else if (reg_x->type == TYPE_STRING) {
		err = ERR_ALPHA_DATA_IS_INVALID;
	}
	else {
		err = ERR_INVALID_TYPE;
	}
	return err;
}

int docmd_ltype(arg_struct *arg) {
	int err;
	phloat x;
	err = hpil_plotter_check();
	if (err != ERR_NONE) {
		return err;
	}
	if (reg_x->type == TYPE_REAL) {
		x = ((vartype_real *) reg_x)->x;
		x = floor(x);
		if (x < 2) {
			sprintf((char *)hpil_controllerDataBuf.data, "LT;");
		}
		else if (x < 9) {
			x = x - 2;
			sprintf((char *)hpil_controllerDataBuf.data, "LT %u;", to_int(x));
		}
		else {
			return ERR_NONE;
		}
		ILCMD_AAU;
		hpil_step = 0;
		hpil_completion = hpil_plotter_cmd_completion;
		mode_interruptible = hpil_worker;
		err = ERR_INTERRUPTIBLE;
	}
	else if (reg_x->type == TYPE_STRING) {
		err = ERR_ALPHA_DATA_IS_INVALID;
	}
	else {
		err = ERR_INVALID_TYPE;
	}
	return err;
}

int docmd_ltypeo(arg_struct *arg) {
	int err;
	phloat x,y;
	err = hpil_plotter_check();
	if (err != ERR_NONE) {
		return err;
	}
	if ((reg_x->type == TYPE_REAL) && (reg_y->type == TYPE_REAL)) {
		x = ((vartype_real *) reg_x)->x;
		y = ((vartype_real *) reg_y)->x;
		x = floor(x);
		y = floor(y);
		if ((y < 0) || (y > 127)) {
			return ERR_NONE;
		}
		if (x < 2) {
			sprintf((char *)hpil_controllerDataBuf.data, "LT;");
		}
		else if (x < 9) {
			x = x - 2;
			sprintf((char *)hpil_controllerDataBuf.data, "LT %u,%u;", to_int(x), to_int(y));
		}
		else {
			return ERR_NONE;
		}
		ILCMD_AAU;
		hpil_step = 0;
		hpil_completion = hpil_plotter_cmd_completion;
		mode_interruptible = hpil_worker;
		err = ERR_INTERRUPTIBLE;
	}
	else if ((reg_x->type == TYPE_STRING) || (reg_y->type == TYPE_STRING)) {
		err = ERR_ALPHA_DATA_IS_INVALID;
	}
	else {
		err = ERR_INVALID_TYPE;
	}
	return err;
}

int docmd_lxaxis(arg_struct *arg) {
	int err;
	err = hpil_plotter_check();
	if (err != ERR_NONE) {
		return err;
	}
	if ((reg_x->type == TYPE_REAL) && (reg_y->type == TYPE_REAL) && (reg_z->type == TYPE_REAL) && (reg_t->type == TYPE_REAL)) {
		if ((((vartype_real *)reg_t)->x <= ((vartype_real *)reg_z)->x) || (((vartype_real *)reg_y)->x == 0)) {
			return ERR_PLOTTER_RANGE_ERR;
		}
		ILCMD_AAU;
		hpil_step = 0;
		plotterData.ioBuf.plotting_status = (plotterData.ioBuf.plotting_status & 0x000f) | (PLOTTER_FLAG_AXIS_LABEL | PLOTTER_FLAG_AXIS_TICK);
		hpil_completion = hpil_plotter_axis_completion;
		mode_interruptible = hpil_worker;
		err = ERR_INTERRUPTIBLE;
	}
	else if ((reg_x->type == TYPE_STRING) || (reg_y->type == TYPE_STRING) || (reg_z->type == TYPE_STRING) || (reg_t->type == TYPE_STRING)) {
		return ERR_ALPHA_DATA_IS_INVALID;
	}
	else {
		err = ERR_INVALID_TYPE;
	}
	return err;
}

int docmd_lyaxis(arg_struct *arg) {
	int err;
	err = hpil_plotter_check();
	if (err != ERR_NONE) {
		return err;
	}
	if ((reg_x->type == TYPE_REAL) && (reg_y->type == TYPE_REAL) && (reg_z->type == TYPE_REAL) && (reg_t->type == TYPE_REAL)) {
		if ((((vartype_real *)reg_t)->x <= ((vartype_real *)reg_z)->x) || (((vartype_real *)reg_y)->x == 0)) {
			return ERR_PLOTTER_RANGE_ERR;
		}
		ILCMD_AAU;
		hpil_step = 0;
		plotterData.ioBuf.plotting_status = (plotterData.ioBuf.plotting_status & 0x000f) | (PLOTTER_FLAG_AXIS_Y | PLOTTER_FLAG_AXIS_LABEL | PLOTTER_FLAG_AXIS_TICK);
		hpil_completion = hpil_plotter_axis_completion;
		mode_interruptible = hpil_worker;
		err = ERR_INTERRUPTIBLE;
	}
	else if ((reg_x->type == TYPE_STRING) || (reg_y->type == TYPE_STRING) || (reg_z->type == TYPE_STRING) || (reg_t->type == TYPE_STRING)) {
		return ERR_ALPHA_DATA_IS_INVALID;
	}
	else {
		err = ERR_INVALID_TYPE;
	}
	return err;
}

int docmd_move(arg_struct *arg) {
	int err;
	err = hpil_plotter_check();
	if (err != ERR_NONE) {
		return err;
	}
	if ((reg_x->type == TYPE_REAL) && (reg_y->type == TYPE_REAL)) {
		plotterData.ioBuf.plotting_status = (plotterData.ioBuf.plotting_status & 0x000f) | PLOTTER_FLAG_SET_PEN_STATUS;
		ILCMD_AAU;
		hpil_step = 0;
		hpil_completion = hpil_plotter_plot_generic_completion;
		mode_interruptible = hpil_worker;
		err = ERR_INTERRUPTIBLE;
	}
	else if ((reg_x->type == TYPE_STRING) || (reg_y->type == TYPE_STRING)) {
		err = ERR_ALPHA_DATA_IS_INVALID;
	}
	else {
		err = ERR_INVALID_TYPE;
	}
	return err;
}

int docmd_pen(arg_struct *arg) {
	int err;
	phloat x,y;
	err = hpil_plotter_check();
	if (err != ERR_NONE) {
		return err;
	}
	if (reg_x->type == TYPE_REAL) {
		x = ((vartype_real *) reg_x)->x;
		x = floor(x);
		sprintf((char *)hpil_controllerDataBuf.data, "SP %i;", to_int(x));
		ILCMD_AAU;
		hpil_step = 0;
		hpil_completion = hpil_plotter_cmd_completion;
		mode_interruptible = hpil_worker;
		err = ERR_INTERRUPTIBLE;
	}
	else if (reg_x->type == TYPE_STRING) {
		err = ERR_ALPHA_DATA_IS_INVALID;
	}
	else {
		err = ERR_INVALID_TYPE;
	}
	return err;
}

int docmd_pendn(arg_struct *arg) {
	int err;
	err = hpil_plotter_check();
	if (err != ERR_NONE) {
		return err;
	}
	plotterData.ioBuf.plotting_status |= PLOTTER_STATUS_PEN_DOWN;
	sprintf((char *)hpil_controllerDataBuf.data, "PD;");
	ILCMD_AAU;
	hpil_step = 0;
	hpil_completion = hpil_plotter_cmd_completion;
	mode_interruptible = hpil_worker;
	return ERR_INTERRUPTIBLE;
}

int docmd_penup(arg_struct *arg) {
	int err;
	err = hpil_plotter_check();
	if (err != ERR_NONE) {
		return err;
	}
	plotterData.ioBuf.plotting_status &= ~PLOTTER_STATUS_PEN_DOWN;
	sprintf((char *)hpil_controllerDataBuf.data, "PU;");
	ILCMD_AAU;
	hpil_step = 0;
	hpil_completion = hpil_plotter_cmd_completion;
	mode_interruptible = hpil_worker;
	return ERR_INTERRUPTIBLE;
}

int docmd_pinit(arg_struct *arg) {
	int err;
	err = hpil_check();
	if (err != ERR_NONE) {
		return err;
	}
	ILCMD_AAU;
	hpil_step = 4;
	hpil_completion = hpil_plotter_limit_pinit_completion;
	mode_interruptible = hpil_worker;
	return ERR_INTERRUPTIBLE;
}

int docmd_plot(arg_struct *arg) {
	int err;
	err = hpil_plotter_check();
	if (err != ERR_NONE) {
		return err;
	}
	if ((reg_x->type == TYPE_REAL) && (reg_y->type == TYPE_REAL)) {
		plotterData.ioBuf.plotting_status = (plotterData.ioBuf.plotting_status & 0x000f) | PLOTTER_FLAG_SET_PEN_DOWN;
		ILCMD_AAU;
		hpil_step = 0;
		hpil_completion = hpil_plotter_plot_generic_completion;
		mode_interruptible = hpil_worker;
		err = ERR_INTERRUPTIBLE;
	}
	else if ((reg_x->type == TYPE_STRING) || (reg_y->type == TYPE_STRING)) {
		return ERR_ALPHA_DATA_IS_INVALID;
	}
	else {
		err = ERR_INVALID_TYPE;
	}
	return err;
}

int docmd_plregx(arg_struct *arg) {
	int err;
	phloat x;
	int regSize;
	err = hpil_plotter_check();
	if (err != ERR_NONE) {
		return err;
	}
	plotterData.ioBuf.plotting_status = (plotterData.ioBuf.plotting_status & 0x000f) | PLOTTER_FLAG_PLOT_REG;
	if (reg_x->type == TYPE_REAL) {			// use regs
		plotterData.plReg = recall_var("REGS", 4);
		if (plotterData.plReg == NULL) {
			return ERR_NONEXISTENT;
		}
		if (plotterData.plReg->type != TYPE_REALMATRIX) {
			return ERR_INVALID_TYPE;
		}
		if (((vartype_realmatrix *)plotterData.plReg)->columns != 1) {
			return ERR_DIMENSION_ERROR;
		}
		regSize = ((vartype_realmatrix*)plotterData.plReg)->rows - 1;
		x = ((vartype_real *)reg_x)->x;
		if (x < 0) {
	        x = -x;
		}
	    plotterData.plRegFrom = to_int(x);
	    x = (x - plotterData.plRegFrom) * 1000;
	    // cf generic_loop_helper for the 0.0005 trick.
	    #ifndef BCD_MATH
			x = x + 0.0005;
		#endif
		plotterData.plRegTo = to_int(x);
		if ((plotterData.plRegFrom > regSize) || (plotterData.plRegTo > regSize) || (plotterData.plRegTo < plotterData.plRegFrom)) {
			return ERR_OUT_OF_RANGE;
		}
	}
	else if (reg_x->type == TYPE_STRING) {	// use matrix, real (x,y if vector else colums 1 and 2 for x,y) or complex. 
		plotterData.plReg = recall_var(((vartype_string *)reg_x)->text, ((vartype_string *)reg_x)->length);
		if (plotterData.plReg == NULL) {
			return ERR_NONEXISTENT;
		}
		if (plotterData.plReg->type == TYPE_REALMATRIX) {
			plotterData.plRegFrom = 0;
			if (((vartype_realmatrix *)plotterData.plReg)->rows == 1) {
				plotterData.plRegTo = ((vartype_realmatrix *)plotterData.plReg)->rows - 1;
			}
			else {
				plotterData.plRegTo = (((vartype_realmatrix *)plotterData.plReg)->rows - 1) * 2;
			}
		}
		else if (plotterData.plReg->type == TYPE_COMPLEXMATRIX) {
			plotterData.plRegFrom = 0;
			plotterData.plRegTo = (((vartype_complexmatrix *)plotterData.plReg)->rows - 1) * 2;
		}
		else {
			return ERR_INVALID_TYPE;
		}
	}
	else {
		return  ERR_INVALID_TYPE;
	}
	ILCMD_AAU;
	hpil_step = 0;
	hpil_completion = hpil_plotter_plot_generic_completion;
	mode_interruptible = hpil_worker;
	return ERR_INTERRUPTIBLE;
}

int docmd_ratio(arg_struct *arg) {
	int err;
	phloat dx, dy, ratio;
	vartype *v;
	err = hpil_plotter_check();
	if (err != ERR_NONE) {
		return err;
	}
	dx = P2_x - P1_x;
	dy = P2_y - P1_y;
	if ((dx == 0) || (dy == 0)) {
		err = ERR_PLOTTER_DATA_ERR;
	}
	else {
		ratio = dx / dy;
		if (ratio < 0) {
			ratio = -ratio;
		}
		ratio = floor(ratio * 10000) / 10000;
	}
	v = new_real(ratio);
	if (v == NULL) {
	    return ERR_INSUFFICIENT_MEMORY;
	}
	recall_result(v);
	return ERR_NONE;
}

int docmd_rplot(arg_struct *arg) {
	int err;
	err = hpil_plotter_check();
	if (err != ERR_NONE) {
		return err;
	}
	if ((reg_x->type == TYPE_REAL) && (reg_y->type == TYPE_REAL)) {
		plotterData.ioBuf.plotting_status = (plotterData.ioBuf.plotting_status & 0x000f) | (PLOTTER_FLAG_SET_PEN_DOWN | PLOTTER_FLAG_INCREMENT | PLOTTER_FLAG_RELATIVE);
		ILCMD_AAU;
		hpil_step = 0;
		hpil_completion = hpil_plotter_plot_generic_completion;
		mode_interruptible = hpil_worker;
		err = ERR_INTERRUPTIBLE;
	}
	else if ((reg_x->type == TYPE_STRING) || (reg_y->type == TYPE_STRING)) {
		return ERR_ALPHA_DATA_IS_INVALID;
	}
	else {
		err = ERR_INVALID_TYPE;
	}
	return err;
}

int docmd_scale(arg_struct *arg) {
	int err;
	phloat dx, dy;
	err = hpil_plotter_check();
	if (err != ERR_NONE) {
		return err;
	}
	if ((reg_x->type == TYPE_REAL) && (reg_y->type == TYPE_REAL) && (reg_z->type == TYPE_REAL) && (reg_t->type == TYPE_REAL)) {
		dx = ((vartype_real *)reg_z)->x - ((vartype_real *)reg_t)->x;
		dy = ((vartype_real *)reg_x)->x - ((vartype_real *)reg_y)->x;
		if ((dx == 0) || (dy == 0)) {
			return ERR_INVALID_DATA;
		}
		Factor1_x_prime = (x2 - x1) / dx;
		Factor2_x_prime	= x1 - ((vartype_real *)reg_t)->x * Factor1_x_prime;
		Factor1_y_prime	= (y2 - y1) / dy;
		Factor2_y_prime	= y1 - ((vartype_real *)reg_y)->x * Factor1_y_prime;
		err = ERR_NONE;
	}
	else if ((reg_x->type == TYPE_STRING) || (reg_y->type == TYPE_STRING) || (reg_z->type == TYPE_STRING) || (reg_t->type == TYPE_STRING)) {
		return ERR_ALPHA_DATA_IS_INVALID;
	}
	else {
		err = ERR_INVALID_TYPE;
	}
	return ERR_NONE;
}

int docmd_setgu(arg_struct *arg) {
	int err;
	err = hpil_plotter_check();
	if (err != ERR_NONE) {
		return err;
	}
	plotterData.ioBuf.plotting_status &= ~PLOTTER_STATUS_MODE_UU;
	sprintf((char *)hpil_controllerDataBuf.data, "IW %u,%u,%u,%u;", to_int(P1_x), to_int(P1_y), to_int(P2_x), to_int(P2_y));
	ILCMD_AAU;
	hpil_step = 0;
	hpil_completion = hpil_plotter_cmd_completion;
	mode_interruptible = hpil_worker;
	return ERR_INTERRUPTIBLE;
}

int docmd_setuu(arg_struct *arg) {
	int err;
	err = hpil_plotter_check();
	if (err != ERR_NONE) {
		return err;
	}
	plotterData.ioBuf.plotting_status |= PLOTTER_STATUS_MODE_UU;
	sprintf((char *)hpil_controllerDataBuf.data, "IW %u,%u,%u,%u;", to_int(x1), to_int(y1), to_int(x2), to_int(y2));
	ILCMD_AAU;
	hpil_step = 0;
	hpil_completion = hpil_plotter_cmd_completion;
	mode_interruptible = hpil_worker;
	return ERR_INTERRUPTIBLE;
}

int docmd_ticlen(arg_struct *arg) {
	int err;
	phloat x;
	err = hpil_plotter_check();
	if (err != ERR_NONE) {
		return err;
	}
	if (reg_x->type == TYPE_REAL) {
		x = ((vartype_real *) reg_x)->x;
		// get absolute value
		if (x < 0) {
			x = -x;
		}
		sprintf((char *)hpil_controllerDataBuf.data, "TL %u,%u;", to_int(x), to_int(x));
		ILCMD_AAU;
		hpil_step = 0;
		hpil_completion = hpil_plotter_cmd_completion;
		mode_interruptible = hpil_worker;
		err = ERR_INTERRUPTIBLE;
	}
	else if (reg_x->type == TYPE_STRING) {
		return ERR_ALPHA_DATA_IS_INVALID;
	}
	else {
		err = ERR_INVALID_TYPE;
	}
return err;
}

int docmd_unclip(arg_struct *arg) {
	int err;
	phloat x;
	err = hpil_plotter_check();
	if (err != ERR_NONE) {
		return err;
	}
	x1 = P1_x;
	x2 = P2_x;
	y1 = P1_y;
	y2 = P2_y;
	sprintf((char *)hpil_controllerDataBuf.data, "IW %u,%u,%u,%u;",
		to_int(x1), to_int(y1), to_int(x2), to_int(y2));
	ILCMD_AAU;
	hpil_step = 0;
	hpil_completion = hpil_plotter_cmd_completion;
	mode_interruptible = hpil_worker;
	return ERR_INTERRUPTIBLE;
}

int docmd_xaxis(arg_struct *arg) {
	int err;
	err = hpil_plotter_check();
	if (err != ERR_NONE) {
		return err;
	}
	if (reg_x->type == TYPE_REAL) {
		ILCMD_AAU;
		hpil_step = 0;
		plotterData.ioBuf.plotting_status = (plotterData.ioBuf.plotting_status & 0x000f) | PLOTTER_FLAG_PLOT_AXIS;
		hpil_completion = hpil_plotter_plot_generic_completion;
		mode_interruptible = hpil_worker;
		err = ERR_INTERRUPTIBLE;
	}
	else if (reg_x->type == TYPE_STRING) {
		return ERR_ALPHA_DATA_IS_INVALID;
	}
	else {
		err = ERR_INVALID_TYPE;
	}
	return err;
}

int docmd_xaxiso(arg_struct *arg) {
	int err;
	err = hpil_plotter_check();
	if (err != ERR_NONE) {
		return err;
	}
	if ((reg_x->type == TYPE_REAL) && (reg_y->type == TYPE_REAL) && (reg_z->type == TYPE_REAL) && (reg_t->type == TYPE_REAL)) {
		if ((((vartype_real *)reg_t)->x <= ((vartype_real *)reg_z)->x) || (((vartype_real *)reg_y)->x == 0)) {
			return ERR_PLOTTER_RANGE_ERR;
		}
		ILCMD_AAU;
		hpil_step = 0;
		plotterData.ioBuf.plotting_status = (plotterData.ioBuf.plotting_status & 0x000f) | PLOTTER_FLAG_AXIS_TICK;
		hpil_completion = hpil_plotter_axis_completion;
		mode_interruptible = hpil_worker;
		err = ERR_INTERRUPTIBLE;
	}
	else if ((reg_x->type == TYPE_STRING) || (reg_y->type == TYPE_STRING) || (reg_z->type == TYPE_STRING) || (reg_t->type == TYPE_STRING)) {
		return ERR_ALPHA_DATA_IS_INVALID;
	}
	else {
		err = ERR_INVALID_TYPE;
	}
	return err;
}

int docmd_yaxis(arg_struct *arg) {
	int err;
	err = hpil_plotter_check();
	if (err != ERR_NONE) {
		return err;
	}
	if (reg_x->type == TYPE_REAL) {
		ILCMD_AAU;
		hpil_step = 0;
		plotterData.ioBuf.plotting_status = (plotterData.ioBuf.plotting_status & 0x000f) | (PLOTTER_FLAG_PLOT_AXIS | PLOTTER_FLAG_AXIS_Y);
		hpil_completion = hpil_plotter_plot_generic_completion;
		mode_interruptible = hpil_worker;
		err = ERR_INTERRUPTIBLE;
	}
	else if (reg_x->type == TYPE_STRING) {
		return ERR_ALPHA_DATA_IS_INVALID;
	}
	else {
		err = ERR_INVALID_TYPE;
	}
	return err;
}

int docmd_yaxiso(arg_struct *arg) {
	int err;
	err = hpil_plotter_check();
	if (err != ERR_NONE) {
		return err;
	}
	if ((reg_x->type == TYPE_REAL) && (reg_y->type == TYPE_REAL) && (reg_z->type == TYPE_REAL) && (reg_t->type == TYPE_REAL)) {
		if ((((vartype_real *)reg_t)->x <= ((vartype_real *)reg_z)->x) || (((vartype_real *)reg_y)->x == 0)) {
			return ERR_PLOTTER_RANGE_ERR;
		}
		ILCMD_AAU;
		hpil_step = 0;
		plotterData.ioBuf.plotting_status = (plotterData.ioBuf.plotting_status & 0x000f) | (PLOTTER_FLAG_AXIS_Y | PLOTTER_FLAG_AXIS_TICK);
		hpil_completion = hpil_plotter_axis_completion;
		mode_interruptible = hpil_worker;
		err = ERR_INTERRUPTIBLE;
	}
	else if ((reg_x->type == TYPE_STRING) || (reg_y->type == TYPE_STRING) || (reg_z->type == TYPE_STRING) || (reg_t->type == TYPE_STRING)) {
		return ERR_ALPHA_DATA_IS_INVALID;
	}
	else {
		err = ERR_INVALID_TYPE;
	}
	return err;
}

int docmd_pclbuf(arg_struct *arg) {
	plotterData.ioBuf.pinit_done = 0;
	return ERR_NONE;
}

int docmd_pdir(arg_struct *arg) {
	phloat x;
    if (reg_x->type == TYPE_REAL) {
		x = ((vartype_real *)reg_x)->x;
		mappable_sin_r(x, &plotterData.ioBuf.pdir_sin);
		mappable_cos_r(x, &plotterData.ioBuf.pdir_cos);
    }
	else if (reg_x->type == TYPE_STRING) {
        return ERR_ALPHA_DATA_IS_INVALID;
	}
	else {
		return ERR_INVALID_TYPE;
	}
	return ERR_NONE;
}

int docmd_prcl(arg_struct *arg) {
	int err, i;
	phloat t, u;
	vartype *v;
	err = hpil_plotter_check();
	if (err != ERR_NONE) {
		return err;
	}
	err = mappable_x_hpil(25,&i);
	if (err != ERR_NONE) {
		return err;
	}
	if (i == 0) {
		// rebuild BR00
		// start with cos
		t = (plotterData.ioBuf.pdir_cos * 1000);
		if (t < 0) {
			t = floor(-t);
		}
		else {
			t = floor(t);
		}
		t = t / 100000;
		// encode quadrant
		if (plotterData.ioBuf.pdir_cos < 0) {
			t += 0.1;
		}
		if (plotterData.ioBuf.pdir_sin < 0) {
			t += 0.2;
		}
		// lorg
		t += plotterData.ioBuf.lorg;
		// rplot
		if (plotterData.ioBuf.plotting_status & PLOTTER_FLAG_RELATIVE) {
			t += 0.000002;
		}
		// plotting status
		u = plotterData.ioBuf.plotting_status & 0x000f;
		t += u / 10000000;
		v = new_real(t);
	}
	else {
		t = plotterData.ioBuf.BR[i];
	}
		v = new_real(t);
	if (v == NULL) {
	    return ERR_INSUFFICIENT_MEMORY;
	}
	recall_result(v);
	return ERR_NONE;
}

static int hpil_plotter_cmd_completion(int error) {
	if (error == ERR_NONE) {
		error = ERR_INTERRUPTIBLE;
		switch (hpil_step) {
			case 0 :		// Select Plotter
				hpilXCore.buf = hpil_controllerAltBuf.data;
				hpilXCore.bufPtr = 0;
				hpilXCore.bufSize = 2;
				ILCMD_nop;
				hpil_step++;
				error = call_ilCompletion(hpil_plotterSelect_sub);
				break;
			case 1 :		// Send command
				hpilXCore.buf = hpil_controllerDataBuf.data;
				hpilXCore.bufPtr = 0;
				hpilXCore.bufSize = strlen((char *)hpilXCore.buf);
				ILCMD_nop;
				hpil_step++;
				error = call_ilCompletion(hpil_plotterSend_sub);
				break;
			case 2 :		// Send OE command
				hpilXCore.bufPtr = 0;
				hpilXCore.bufSize = sprintf((char*)hpilXCore.buf, "\nOE;");
				ILCMD_nop;
				hpil_step++;
				error = call_ilCompletion(hpil_plotterSendGet_sub);
				break;
			case 3 :		// Get error;
				if (hpilXCore.buf[0] == '0'){
					error = ERR_NONE;
				}
				else {
					error = ERR_PLOTTER_ERR;
				}
			default :
				error = ERR_NONE;
		}
	}
	return error;
}

static int hpil_plotter_label_completion(int error) {
	int i, j;
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
			case 1 :		// Pen Up
				hpilXCore.bufPtr = 0;
				hpilXCore.bufSize = sprintf((char*)hpilXCore.buf, ";PU;");
				ILCMD_nop;
				hpil_step++;
				error = call_ilCompletion(hpil_plotterSend_sub);
				break;
			case 2 :		// Label command
				i = sprintf((char *)hpil_controllerDataBuf.data, "IW %u,%u,%u,%u;", to_int(P1_x), to_int(P1_y), to_int(P2_x), to_int(P2_y));
				i += sprintf((char *)&hpil_controllerDataBuf.data[i], "PA %u,%u;", to_int(Last_x), to_int(Last_y));
				i += sprintf((char *)&hpil_controllerDataBuf.data[i], "CP");
				switch ((plotterData.ioBuf.lorg - 1) / 3) {
					case 0 :	// 
						i += sprintf((char *)&hpil_controllerDataBuf.data[i], "0,");
						break;
					case 1 :
						if (reg_alpha_length % 2) {
							i += sprintf((char *)&hpil_controllerDataBuf.data[i], "-%u.5,", reg_alpha_length/2);
						}
						else {
							i += sprintf((char *)&hpil_controllerDataBuf.data[i], "-%u,", reg_alpha_length/2);
						}
						break;
					case 2 :
						i += sprintf((char *)&hpil_controllerDataBuf.data[i], "-%u;", reg_alpha_length);
						break;
					default:
						// should not occur
						return ERR_INTERNAL_ERROR;
				}
				switch (plotterData.ioBuf.lorg % 3) {
					case 0 :	// 
						i += sprintf((char *)&hpil_controllerDataBuf.data[i], "-.5;");
						break;
					case 1 :
						i += sprintf((char *)&hpil_controllerDataBuf.data[i], "0;");
						break;
					case 2 :
						i += sprintf((char *)&hpil_controllerDataBuf.data[i], "-.25;");
						break;
					default:
						// should not occur
						return ERR_INTERNAL_ERROR;
				}
				i += sprintf((char *)&hpil_controllerDataBuf.data[i], "LB");
				for (j = 0; j < reg_alpha_length; j++) {
					hpil_controllerDataBuf.data[i++] = reg_alpha[j];
				}
				i += sprintf((char *)&hpil_controllerDataBuf.data[i], "\3;");
				if (!flags.f.hpil_ina_err) {			// same flag used to enable next label on same line
					i += sprintf((char *)&hpil_controllerDataBuf.data[i], "CP;");
				}
				hpilXCore.bufPtr = 0;
				hpilXCore.bufSize = i;
				ILCMD_nop;
				hpil_step++;
				error = call_ilCompletion(hpil_plotterSend_sub);
				break;
			case 3 :		// OC command
				hpilXCore.bufPtr = 0;
				hpilXCore.bufSize = sprintf((char *)hpil_controllerDataBuf.data, "OC;");
				ILCMD_nop;
				hpil_step++;
				error = call_ilCompletion(hpil_plotterSendGet_sub);
				break;
			case 4 :		// Get pen position, restore IW
				i = hpil_parse((char*)hpilXCore.buf, hpilXCore.bufPtr, &Last_x_prime);
				Last_x = Last_x_prime;
				i += hpil_parse((char*)&hpilXCore.buf[i], hpilXCore.bufPtr - i, &Last_y_prime);
				Last_y = Last_y_prime;
				hpilXCore.bufPtr = 0;
				if (plotterData.ioBuf.plotting_status & PLOTTER_STATUS_MODE_UU) {
					hpilXCore.bufSize = sprintf((char *)hpil_controllerDataBuf.data, "IW %u,%u,%u,%u;", to_int(x1), to_int(y1), to_int(x2), to_int(y2));
				}
				else {
					hpilXCore.bufSize = sprintf((char *)hpil_controllerDataBuf.data, "IW %u,%u,%u,%u;", to_int(P1_x), to_int(P1_y), to_int(P2_x), to_int(P2_y));
				}
				ILCMD_nop;
				hpil_step++;
				error = call_ilCompletion(hpil_plotterSend_sub);
				break;
			case 5 :
				hpilXCore.bufSize = sprintf((char*)hpilXCore.buf, "\nOE;");
				ILCMD_nop;
				hpil_step++;
				error = call_ilCompletion(hpil_plotterSendGet_sub);
				break;
			case 6 :		// Get error;
				if (hpilXCore.buf[0] == '0'){
					error = ERR_NONE;
				}
				else {
					error = ERR_PLOTTER_ERR;
				}
			default :
				error = ERR_NONE;
		}
	}
	return error;
}

static int hpil_plotter_axis_completion(int error) {
	int i, j;
    int dispmode, digits;
	phloat x, y;
	if (error == ERR_NONE) {
		error = ERR_INTERRUPTIBLE;
		i = 0;
		switch (hpil_step) {
			case 0 :		// Select Plotter
				hpilXCore.buf = hpil_controllerDataBuf.data;
				hpilXCore.bufPtr = 0;
				hpilXCore.bufSize = 2;
				ILCMD_nop;
				hpil_step++;
				error = call_ilCompletion(hpil_plotterSelect_sub);
				break;
			case 1 :		// Label orientation, Pen up, move to start of axis, Pen Down, prepare first segment 
				// Label orientation for x axis
				if (plotterData.ioBuf.plotting_status & PLOTTER_FLAG_AXIS_LABEL) {
					if (!(plotterData.ioBuf.plotting_status & PLOTTER_FLAG_AXIS_Y) && (((vartype_real *)reg_y)->x > 0)) {
						i += sprintf((char *)&hpil_controllerDataBuf.data[i], "DI0,1;");
					}
					else {
						i += sprintf((char *)&hpil_controllerDataBuf.data[i], "DI");
					}
				}
				// Misc_1 -> last tick
				if (plotterData.ioBuf.plotting_status & PLOTTER_FLAG_AXIS_Y) {
					Misc_x1 = ((vartype_real *)reg_x)->x;
					Misc_y1 = ((vartype_real *)reg_z)->x;
					Misc_x2 = 0;
					Misc_y2 = ((vartype_real *)reg_y)->x;
				}
				else {
					Misc_x1 = ((vartype_real *)reg_z)->x;
					Misc_y1 = ((vartype_real *)reg_x)->x;
					Misc_x2 = ((vartype_real *)reg_y)->x;
					Misc_y2 = 0;
				}
				x = Misc_x1;
				y = Misc_y1;
				hpil_plotter_rescale(&x, &y);
				i += sprintf((char *)&hpil_controllerDataBuf.data[i], "PU;PA %u,%u;PD;", to_int(x), to_int(y));
				// Misc2 -> tick increment + deal with negative values
				if (plotterData.ioBuf.plotting_status & PLOTTER_FLAG_AXIS_Y) {
					if (Misc_y2 < 0) {
						Misc_y2 = -Misc_y2;
						Misc_y1 = Misc_y1 + fmod(((vartype_real *)reg_t)->x - ((vartype_real *)reg_z)->x, Misc_y2);
					}
				}
				else {
					if (Misc_x2 < 0) {
						Misc_x2 = -Misc_x2;
						Misc_x1 = Misc_x1 + fmod(((vartype_real *)reg_t)->x - ((vartype_real *)reg_z)->x, Misc_x2);
					}
				}
				x = Misc_x1;
				y = Misc_y1;
				hpil_plotter_rescale(&x, &y);
				i += sprintf((char *)&hpil_controllerDataBuf.data[i], "PA %u,%u;", to_int(x), to_int(y));
				hpil_step++;
			case 2 :		// Draw segments and ticks
				if (plotterData.ioBuf.plotting_status & PLOTTER_FLAG_AXIS_Y) {
					i += sprintf((char *)&hpil_controllerDataBuf.data[i], "YT;");
					Misc_y1 += Misc_y2;
					if (Misc_y1 > ((vartype_real *)reg_t)->x) {
						Misc_y1 = ((vartype_real *)reg_t)->x;
						hpil_step++;
					}
				}
				else {
					i += sprintf((char *)&hpil_controllerDataBuf.data[i], "XT;");
					Misc_x1 += Misc_x2;
					if (Misc_x1 > ((vartype_real *)reg_t)->x) {
						Misc_x1 = ((vartype_real *)reg_t)->x;
						hpil_step++;
					}
				}
				x = Misc_x1;
				y = Misc_y1;
				hpil_plotter_rescale(&x, &y);
				i += sprintf((char *)&hpil_controllerDataBuf.data[i], "PA %u,%u;", to_int(x), to_int(y));
				hpilXCore.bufPtr = 0;
				hpilXCore.bufSize = i;
				i = 0;
				ILCMD_nop;
				error = call_ilCompletion(hpil_plotterSend_sub);
				break;
			case 3 :		// store actual / prepare label
				Last_x_prime = Last_x;
				Last_y_prime = Last_y;
				if (plotterData.ioBuf.plotting_status & PLOTTER_FLAG_AXIS_LABEL) {
					// Labels are in GU's
					i = sprintf((char *)hpil_controllerDataBuf.data, "PU;IW %u,%u,%u,%u;", to_int(P1_x), to_int(P1_y), to_int(P2_x), to_int(P2_y));
					if (plotterData.ioBuf.plotting_status & PLOTTER_FLAG_AXIS_Y) {
						if (((vartype_real *)reg_y)->x < 0) {
							Misc_y1 = ((vartype_real *)reg_z)->x + fmod(((vartype_real *)reg_t)->x - ((vartype_real *)reg_z)->x, Misc_y2);
						}
						else {
							Misc_y1 = ((vartype_real *)reg_z)->x;
						}
					}
					else {
						if (((vartype_real *)reg_y)->x < 0) {
							Misc_x1 = ((vartype_real *)reg_z)->x + fmod(((vartype_real *)reg_t)->x - ((vartype_real *)reg_z)->x, Misc_x2);
						}
						else {
							Misc_x1 = ((vartype_real *)reg_z)->x;
						}
					}
				}
				else {
					// skip label
					i = sprintf((char *)hpil_controllerDataBuf.data, "PU;\nOE;");
					hpil_step = 6;
				}
				hpilXCore.bufPtr = 0;
				hpilXCore.bufSize = i;
				ILCMD_nop;
				hpil_step++;
				error = call_ilCompletion(hpil_plotterSend_sub);
				break;
			case 4 :	// Process labels
				// move to label position
				x = Misc_x1;
				y = Misc_y1;
				hpil_plotter_rescale(&x, &y);
				if (plotterData.ioBuf.plotting_status & PLOTTER_FLAG_AXIS_Y) {
					x = x1 - 220;
				}
				else {
					y = y1 - 220;
				}
				i = sprintf((char *)hpil_controllerDataBuf.data, "PA %u,%u;PU;", to_int(x), to_int(y));
				// write label to alpha_reg
				digits = 0;
				if (flags.f.fix_or_all) {
					dispmode = flags.f.eng_or_all ? 3 : 0;
				}
				else {
					dispmode = flags.f.eng_or_all ? 2 : 1;
				}
				if (flags.f.digits_bit3) {
					digits += 8;
				}
				if (flags.f.digits_bit2) {
					digits += 4;
				}
				if (flags.f.digits_bit1) {
					digits += 2;
				}
				if (flags.f.digits_bit0) {
					digits += 1;
				}
				if (plotterData.ioBuf.plotting_status & PLOTTER_FLAG_AXIS_Y) {
					reg_alpha_length = phloat2string(Misc_y1, reg_alpha, 44,
                                 1, digits, dispmode, flags.f.thousands_separators, 12);
				}
				else {
					reg_alpha_length = phloat2string(Misc_x1, reg_alpha, 44,
                                 1, digits, dispmode, flags.f.thousands_separators, 12);
				}
				// label in bounds ?
				if (((plotterData.ioBuf.plotting_status & PLOTTER_FLAG_AXIS_Y) && ((y >= y1) && (y <= y2))) ||
					(((plotterData.ioBuf.plotting_status & PLOTTER_FLAG_AXIS_Y) == 0) && ((x >= x1) && (x <= x2)))) {
					// set label origin
					if (!(plotterData.ioBuf.plotting_status & PLOTTER_FLAG_AXIS_Y) && (((vartype_real *)reg_y)->x < 0)) {
						if (reg_alpha_length % 2) {
							i += sprintf((char *)&hpil_controllerDataBuf.data[i], "CP -%u.5,-.25;", reg_alpha_length/2);
						}
						else {
							i += sprintf((char *)&hpil_controllerDataBuf.data[i], "CP -%u,-.25;", reg_alpha_length/2);
						}
					}
					else {
						i += sprintf((char *)&hpil_controllerDataBuf.data[i], "CP -%u,-.25;", reg_alpha_length);
					}
					i += sprintf((char *)&hpil_controllerDataBuf.data[i], "LB");
					for (j = 0; j < reg_alpha_length; j++) {
						hpil_controllerDataBuf.data[i++] = reg_alpha[j];
					}
					i += sprintf((char *)&hpil_controllerDataBuf.data[i], "\3;");
				}
				// Prepare next label or next exit
				if (plotterData.ioBuf.plotting_status & PLOTTER_FLAG_AXIS_Y) {
					Misc_y1 += Misc_y2;
					if (Misc_y1 > ((vartype_real *)reg_t)->x) {
						hpil_step++;
					}
				}
				else {
					Misc_x1 += Misc_x2;
					if (Misc_x1 > ((vartype_real *)reg_t)->x) {
						hpil_step++;
					}
				}
				hpilXCore.bufPtr = 0;
				hpilXCore.bufSize = i;
				ILCMD_nop;
				error = call_ilCompletion(hpil_plotterSend_sub);
				break;
			case 5 :		// end of label loop
				hpilXCore.bufPtr = 0;
				hpilXCore.bufSize = sprintf((char*)hpilXCore.buf, "PA %u,%u;PU;DI;\nOA;", to_int(Last_x_prime), to_int(Last_y_prime));
				ILCMD_nop;
				hpil_step++;
				error = call_ilCompletion(hpil_plotterSendGet_sub);
				break;
			case 6 :		// get position
				i = hpil_parse((char*)hpilXCore.buf, hpilXCore.bufPtr, &Last_x_prime);
				i += hpil_parse((char*)&hpilXCore.buf[i], hpilXCore.bufPtr - i, &Last_y_prime);
				Last_x = Last_x_prime;
				Last_y = Last_y_prime;
				plotterData.ioBuf.plotting_status &= ~PLOTTER_STATUS_PEN_DOWN;
				i = 0;
				if (plotterData.ioBuf.plotting_status &PLOTTER_STATUS_MODE_UU) {
					i += sprintf((char *)&hpil_controllerDataBuf.data[i], "IW %u,%u,%u,%u;", to_int(x1), to_int(y1), to_int(x2), to_int(y2));
				}
				else {
					i += sprintf((char *)&hpil_controllerDataBuf.data[i], "IW %u,%u,%u,%u;", to_int(P1_x), to_int(P1_y), to_int(P2_x), to_int(P2_y));
				}
				hpil_step++;
			case 7 :	// continue with get error
				i += sprintf((char *)&hpil_controllerDataBuf.data[i], "\nOE;");
				hpilXCore.bufPtr = 0;
				hpilXCore.bufSize = i;
				ILCMD_nop;
				hpil_step++;
				error = call_ilCompletion(hpil_plotterSendGet_sub);
				break;
			case 8 :		// Get error;
				if ((hpilXCore.buf[0] == '0') ||(hpilXCore.buf[0] == '6')) {
					if (hpilXCore.buf[0] == '6') {
						plotterData.ioBuf.plotting_status |= PLOTTER_STATUS_OUTBOUND;
					}
					else {
						plotterData.ioBuf.plotting_status &= ~PLOTTER_STATUS_OUTBOUND;
					}
					error = ERR_NONE;
				}
				else {
					error = ERR_PLOTTER_ERR;
				}
			default :
				error = ERR_NONE;
		}
	}
	return error;
}

static int hpil_plotter_limit_pinit_completion(int error) {
	int i;
	phloat dx, dy, ratio;
	if (error == ERR_NONE) {
		error = ERR_INTERRUPTIBLE;
		switch (hpil_step) {
			case 0 :		// Limit entry point  - Select Plotter
				hpilXCore.buf = hpil_controllerAltBuf.data;
				hpilXCore.bufPtr = 0;
				hpilXCore.bufSize = 2;
				ILCMD_nop;
				hpil_step++;
				error = call_ilCompletion(hpil_plotterSelect_sub);
				break;
			case 1 :		// set limit
				hpilXCore.buf = hpil_controllerDataBuf.data;
				hpilXCore.bufPtr = 0;
				hpilXCore.bufSize = strlen((char *)hpilXCore.buf);
				ILCMD_nop;
				hpil_step++;
				error = call_ilCompletion(hpil_plotterSend_sub);
				break;
			case 2 :		// Send OE command
				hpilXCore.bufPtr = 0;
				hpilXCore.bufSize = sprintf((char*)hpilXCore.buf, "\nOE;");
				ILCMD_nop;
				hpil_step++;
				error = call_ilCompletion(hpil_plotterSendGet_sub);
				break;
			case 3 :		// Get error;
				if (hpilXCore.buf[0] == '0'){
					ILCMD_AAU;
					hpil_step += 2;
				}
				else {
					error = ERR_PLOTTER_ERR;
				}
				break;
			case 4 :		// Pinitit entry point  - Select Plotter
				hpilXCore.buf = hpil_controllerAltBuf.data;
				hpilXCore.bufPtr = 0;
				hpilXCore.bufSize = 2;
				ILCMD_nop;
				hpil_step++;
				error = call_ilCompletion(hpil_plotterSelect_sub);
				break;
			case 5 :		// Pinit - first stage send DF;DI;SP1;OP;
				hpilXCore.bufPtr = 0;
				hpilXCore.bufSize = sprintf((char*)hpilXCore.buf, "DF;DI;SP1;OP;");
				ILCMD_nop;
				hpil_step++;
				error = call_ilCompletion(hpil_plotterSendGet_sub);
				break;
			case 6 :		// Pinit - first stage process P1,P2, second stage get error status send \nOE;
				// graphics limits in APU
				i = hpil_parse((char*)hpilXCore.buf, hpilXCore.bufPtr, &P1_x);
				i += hpil_parse((char*)&hpilXCore.buf[i], hpilXCore.bufPtr - i, &P1_y);
				i += hpil_parse((char*)&hpilXCore.buf[i], hpilXCore.bufPtr - i, &P2_x);
				i += hpil_parse((char*)&hpilXCore.buf[i], hpilXCore.bufPtr - i, &P2_y);
				// plot bounds in APU
				x1 = P1_x;
				y1 = P1_y;
				x2 = P2_x;
				y2 = P2_y;
				// Calculate GU scaling factors (GU > APU)
				Factor2_x = P1_x;
				Factor2_y = P1_y;
				dx = P2_x - P1_x;
				dy = P2_y - P1_y;
				if ((dx == 0) || (dy == 0)) {
					error = ERR_PLOTTER_DATA_ERR;
				}
				else {
					ratio = dx / dy;
					if (ratio < 0) {
						ratio = -ratio;
					}
					ratio = floor(ratio * 10000) / 10000;
					if (ratio > 1) {
						// y is the shortest axis, set yscale for 100 units
						Factor1_y = dy / 100;
						Factor1_x = 100 / ratio;
					}
					else {
						// x is the shortest axis, set xscale for 100 units 
						Factor1_y = 100 / ratio;
						Factor1_x = dx /100;
					}
				}
				// Set UU scaling factors (GU > APU)
				Factor1_x_prime = Factor1_x;
				Factor2_x_prime	= Factor2_x;
				Factor1_y_prime	= Factor1_y;
				Factor2_y_prime	= Factor2_y;
				// Get plotter status
				hpilXCore.bufPtr = 0;
				hpilXCore.bufSize = sprintf((char*)hpilXCore.buf, "\nOE;");
				ILCMD_nop;
				hpil_step++;
				error = call_ilCompletion(hpil_plotterSendGet_sub);
				break;
			case 7 :		// Pinit - second stage process error code, third stage send IW
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
			case 8 :		// Pinit - fourth stage, get pen position and status send OA
				hpilXCore.bufPtr = 0;
				hpilXCore.bufSize = sprintf((char*)hpilXCore.buf, "\nOA;");
				ILCMD_nop;
				hpil_step++;
				error = call_ilCompletion(hpil_plotterSendGet_sub);
				break;
			case 9 :		// Pinit - fourth stage, get pen status;
				i = hpil_parse((char*)hpilXCore.buf, hpilXCore.bufPtr, &Last_x_prime);
				i += hpil_parse((char*)&hpilXCore.buf[i], hpilXCore.bufPtr - i, &Last_y_prime);
				plotterData.ioBuf.lorg = 1;
				plotterData.ioBuf.pdir_cos = 1;
				plotterData.ioBuf.pdir_sin = 0;
				plotterData.ioBuf.plotting_status = PLOTTER_STATUS_MODE_UU + (hpilXCore.buf[i] == '0' ? 0 : PLOTTER_STATUS_PEN_DOWN);	// set mode to UU + pen status
				plotterData.ioBuf.pinit_done = 1;
			default :
				error = ERR_NONE;
		}
	}
	return error;
}

static int hpil_plotter_plot_generic_completion(int error) {
	int i, plotDone;
	vartype_realmatrix * rm;
	vartype_complexmatrix * cm;
	int xIndex,yIndex;
	phloat x, y;
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
			case 1 :		// Segment to plot
				// pen command to issue
				i = 0;
				if (plotterData.ioBuf.plotting_status & PLOTTER_FLAG_PLOT_AXIS) {
					i = sprintf((char *)hpil_controllerDataBuf.data, "PU;");
				}
				else if (!(plotterData.ioBuf.plotting_status & PLOTTER_FLAG_SET_PEN_STATUS)) {
					if (plotterData.ioBuf.plotting_status & PLOTTER_STATUS_PEN_DOWN) {
						// always issue pen down command
						i = sprintf((char *)hpil_controllerDataBuf.data, "PD;");
					}
					else if (plotterData.ioBuf.plotting_status & PLOTTER_FLAG_SET_PEN_DOWN) {
						// or issue pen up command too ?
						i = sprintf((char *)hpil_controllerDataBuf.data, "PU;");
					}
					plotterData.ioBuf.plotting_status |= PLOTTER_STATUS_PEN_DOWN;
				}
				else {
					// always set pen status
					if (plotterData.ioBuf.plotting_status & PLOTTER_FLAG_SET_PEN_DOWN) {
						i = sprintf((char *)hpil_controllerDataBuf.data, "PD;");
						plotterData.ioBuf.plotting_status |= PLOTTER_STATUS_PEN_DOWN;
					}
					else {
						i = sprintf((char *)hpil_controllerDataBuf.data, "PU;");
						plotterData.ioBuf.plotting_status &= ~PLOTTER_STATUS_PEN_DOWN;
					}
				}
				// calculates move
				plotDone = 1;
				if (plotterData.ioBuf.plotting_status & PLOTTER_FLAG_PLOT_AXIS) {
					if (reg_x->type == TYPE_REAL) {
						if (plotterData.ioBuf.plotting_status & PLOTTER_FLAG_AXIS_Y) {
							x = ((vartype_real *)reg_x)->x;
							y = 0;
							hpil_plotter_rescale(&x, &y);
							y = (plotterData.ioBuf.plotting_status & PLOTTER_STATUS_MODE_UU) ? y1 : P1_y;
							i += sprintf((char *)(&hpil_controllerDataBuf.data[i]),"PA %u,%u;PD;", to_int(x), to_int(y));
							y = (plotterData.ioBuf.plotting_status & PLOTTER_STATUS_MODE_UU) ? y2 : P2_y;
							i += sprintf((char *)(&hpil_controllerDataBuf.data[i]),"PA %u,%u;PU;", to_int(x), to_int(y));																					
						}
						else {
							x = 0;
							y = ((vartype_real *)reg_x)->x;
							hpil_plotter_rescale(&x, &y);
							x = (plotterData.ioBuf.plotting_status & PLOTTER_STATUS_MODE_UU) ? x1 : P1_x;
							i += sprintf((char *)(&hpil_controllerDataBuf.data[i]),"PA %u,%u;PD;", to_int(x), to_int(y));
							x = (plotterData.ioBuf.plotting_status & PLOTTER_STATUS_MODE_UU) ? x2 : P2_x;
							i += sprintf((char *)(&hpil_controllerDataBuf.data[i]),"PA %u,%u;PU;", to_int(x), to_int(y));																					
						}
					}
					else {
						error = ERR_INTERNAL_ERROR;
					}
				}
				else if (plotterData.ioBuf.plotting_status & PLOTTER_FLAG_PLOT_REG) {
					// processes real vectors, real matrix, complex matrix...
					if (plotterData.plReg->type == TYPE_REALMATRIX) {
						rm = (vartype_realmatrix *)plotterData.plReg;
						if ((rm->columns == 1) && (plotterData.plRegFrom < plotterData.plRegTo)) {	// first row X, second row y
							xIndex = plotterData.plRegFrom++;
							yIndex = plotterData.plRegFrom++;
							plotDone = 0;
						}
						else if (plotterData.plRegFrom <= plotterData.plRegTo) {	// column 0 X, column 1 Y
							xIndex = plotterData.plRegFrom;
							yIndex = plotterData.plRegFrom++ + rm->rows;
							plotDone = 0;
						}
						if (plotDone || rm->array->is_string[xIndex] || rm->array->is_string[yIndex]) {
							i += sprintf((char *)(&hpil_controllerDataBuf.data[i]), "PU;");
						}
						else {
							x = rm->array->data[xIndex];
							y = rm->array->data[yIndex];
							hpil_plotter_rescale(&x, &y);
							i += sprintf((char *)(&hpil_controllerDataBuf.data[i]), "PA %u,%u;PD;", to_int(x), to_int(y));
						}
					}
					else if (plotterData.plReg->type == TYPE_COMPLEXMATRIX) {
						cm = (vartype_complexmatrix *)plotterData.plReg;
						if (plotterData.plRegFrom < plotterData.plRegTo) {		// real X, imaginary Y
							xIndex = plotterData.plRegFrom++;
							yIndex = plotterData.plRegFrom++;
							x = cm->array->data[xIndex];
							y = cm->array->data[yIndex];
							hpil_plotter_rescale(&x, &y);
							i += sprintf((char *)(&hpil_controllerDataBuf.data[i]), "PA %u,%u;PD;", to_int(x), to_int(y));
							plotDone = 0;
						}
						else {
							i += sprintf((char *)(&hpil_controllerDataBuf.data[i]), "PU;");
						}
					}
					else {
						// should not occur
						error = ERR_INTERNAL_ERROR;
					}
				}
				else {
					// process stack
					if (reg_x->type == TYPE_REAL && reg_y->type == TYPE_REAL) {
						x = ((vartype_real *)reg_x)->x;
						y = ((vartype_real *)reg_y)->x;
						hpil_plotter_rescale(&x, &y);
						i += sprintf((char *)(&hpil_controllerDataBuf.data[i]), "PA %u,%u;", to_int(x), to_int(y));
						if (plotterData.ioBuf.plotting_status & PLOTTER_STATUS_PEN_DOWN) {
							i += sprintf((char *)(&hpil_controllerDataBuf.data[i]), "PD;");
						}
						// always move pen up after a draw
						i += sprintf((char *)(&hpil_controllerDataBuf.data[i]), "PU;");
					}
					else {
						error = ERR_INTERNAL_ERROR;
					}
				}
				if (error == ERR_INTERRUPTIBLE) {
					hpilXCore.buf = hpil_controllerDataBuf.data;
					hpilXCore.bufPtr = 0;
					hpilXCore.bufSize = strlen((char *)hpilXCore.buf);
					ILCMD_nop;
					if (plotDone) {
						hpil_step++;
					}
					error = call_ilCompletion(hpil_plotterSend_sub);
				}
				break;
			case 2 :		// Send OE command
				hpilXCore.bufPtr = 0;
				hpilXCore.bufSize = sprintf((char*)hpilXCore.buf, "\nOE;");
				ILCMD_nop;
				hpil_step++;
				error = call_ilCompletion(hpil_plotterSendGet_sub);
				break;
			case 3 :		// Get error, Send OA command
				if ((hpilXCore.buf[0] == '0') ||(hpilXCore.buf[0] == '6')) {
					if (hpilXCore.buf[0] == '6') {
						plotterData.ioBuf.plotting_status |= PLOTTER_STATUS_OUTBOUND;
					}
					else {
						plotterData.ioBuf.plotting_status &= ~PLOTTER_STATUS_OUTBOUND;
					}
					hpilXCore.bufPtr = 0;
					hpilXCore.bufSize = sprintf((char*)hpilXCore.buf, "\nOA;");
					ILCMD_nop;
					hpil_step++;
					error = call_ilCompletion(hpil_plotterSendGet_sub);
				}
				else {
					error = ERR_PLOTTER_ERR;
				}
				break;
			case 4 :		// Get pen position
				if (!(plotterData.ioBuf.plotting_status & PLOTTER_FLAG_RELATIVE)) {
					i = hpil_parse((char*)hpilXCore.buf, hpilXCore.bufPtr, &Last_x_prime);
					i += hpil_parse((char*)&hpilXCore.buf[i], hpilXCore.bufPtr - i, &Last_y_prime);
				}
				error = ERR_NONE;
				break;
			default :
				error = ERR_NONE;
		}
	}
	return error;
}

static int hpil_plotterSelect_sub(int error) {
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

static int hpil_plotterSend_sub(int error) {
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

static int hpil_plotterSendGet_sub(int error) {
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

static void hpil_plotter_rescale(phloat *x, phloat *y) {
	phloat X1, Y1, X2, Y2;
	X1 = *x;
	Y1 = *y;
	if (plotterData.ioBuf.plotting_status & PLOTTER_STATUS_MODE_UU) {
		X1 = Factor1_x_prime * X1;
		Y1 = Factor1_y_prime * Y1;
		if (plotterData.ioBuf.plotting_status & PLOTTER_FLAG_INCREMENT) {
			X2 = plotterData.ioBuf.pdir_cos * X1 - plotterData.ioBuf.pdir_sin * Y1;
			Y2 = plotterData.ioBuf.pdir_cos * Y1 + plotterData.ioBuf.pdir_sin * X1;
			X1 = X2 + Last_x;
			Y1 = Y2 + Last_y;
		}
		else {
			X1 = Factor2_x_prime + X1;
			Y1 = Factor2_y_prime + Y1;
		}
	}
	else {
		X1 = Factor1_x * X1;
		Y1 = Factor1_y * Y1;
		if (plotterData.ioBuf.plotting_status & PLOTTER_FLAG_INCREMENT) {
			X2 = plotterData.ioBuf.pdir_cos * X1 - plotterData.ioBuf.pdir_sin * Y1;
			Y2 = plotterData.ioBuf.pdir_cos * Y1 + plotterData.ioBuf.pdir_sin * X1;
			X1 = X2 + Last_x;
			Y1 = Y2 + Last_y;
		}
		else {
			X1 = Factor2_x + X1;
			Y1 = Factor2_y + Y1;
		}
	}
	if (X1 < 0) {
		X1 = 0;
	}
	if (Y1 < 0) {
		Y1 = 0;
	}
	if (X1 > 32000) {
		X1 = 32000;
	}
	if (Y1 > 32000) {
		Y1 = 32000;
	}
	if (!(plotterData.ioBuf.plotting_status & PLOTTER_FLAG_RELATIVE)) {
		Last_x = X1;
		Last_y = Y1;
	}
	*x = X1;
	*y = Y1;
}

static int hpil_parse(char *str, int len, phloat *p) {
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
