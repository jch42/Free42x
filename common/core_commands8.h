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

#ifndef CORE_COMMANDS8_H
#define CORE_COMMANDS8_H 1

#include "core_globals.h"

int docmd_ifc(arg_struct *arg);
int docmd_nloop(arg_struct *arg);
int docmd_select(arg_struct *arg);
int docmd_rclsel(arg_struct *arg);
int docmd_prtsel(arg_struct *arg);
int docmd_dsksel(arg_struct *arg);
int docmd_autoio(arg_struct *arg);
int docmd_manio(arg_struct *arg);
int docmd_stat(arg_struct *arg);
int docmd_id(arg_struct *arg);
int docmd_aid(arg_struct *arg);
int docmd_bldspec(arg_struct *arg);

#endif
