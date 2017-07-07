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

#ifndef HPIL_PLOTTER_H
#define HPIL_PLOTTER_H 1

// Plotters persistent struct
struct PLOTTER_IOBuf {
	phloat BR[26];		// original I/O buffer
	// other resources (to rebuild BR0)
	int	lorg;
	int plotting_status;
};

// commands
int docmd_clipuu(arg_struct *arg);
int docmd_lorg(arg_struct *arg);
int docmd_ltype(arg_struct *arg);
int docmd_ltypeo(arg_struct *arg);
int docmd_pen(arg_struct *arg);
int docmd_pendn(arg_struct *arg);
int docmd_penup(arg_struct *arg);
int docmd_pinit(arg_struct *arg);
int docmd_plregx(arg_struct *arg);
int docmd_ratio(arg_struct *arg);
int docmd_scale(arg_struct *arg);
int docmd_prcl(arg_struct *arg);
#endif