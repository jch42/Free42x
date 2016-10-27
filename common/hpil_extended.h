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

#ifndef HPIL_EXTENDED_H
#define HPIL_EXTENDED_H 1

// commands
int docmd_anumdel(arg_struct *arg);
int docmd_clrdev(arg_struct *arg);
int docmd_clrloop(arg_struct *arg);
int docmd_inac(arg_struct *arg);
int docmd_inacl(arg_struct *arg);
int docmd_inae(arg_struct *arg);
int docmd_inan(arg_struct *arg);
int docmd_inxb(arg_struct *arg);
int docmd_inaccl(arg_struct *arg);
int docmd_outac(arg_struct *arg);
int docmd_outacl(arg_struct *arg);
int docmd_outae(arg_struct *arg);
int docmd_outan(arg_struct *arg);
int docmd_outxb(arg_struct *arg);
#endif