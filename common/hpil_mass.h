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

#ifndef HPIL_MASS_H
#define HPIL_MASS_H 1

// commands
int docmd_create(arg_struct *arg);
int docmd_dir(arg_struct *arg);
int docmd_newm(arg_struct *arg);
int docmd_purge(arg_struct *arg);
int docmd_readp(arg_struct *arg);
int docmd_readr(arg_struct *arg);
int docmd_rename(arg_struct *arg);
int docmd_sec(arg_struct *arg);
int docmd_unsec(arg_struct *arg);
int docmd_wrtp(arg_struct *arg);
int docmd_wrtr(arg_struct *arg);
int docmd_zero(arg_struct * arg);
#endif