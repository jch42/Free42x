/*****************************************************************************
 * Free42 -- an HP-42S calculator simulator
 * Copyright (C) 2004-2017  Thomas Okken
 * EBML state file format
 * Copyright (C) 2018       Jean-Christophe Hessemann
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

#ifndef CORE_EBML_H
#define CORE_EBML_H 1

/* base elements */
#define EL_ID_COUNT			0x11	/* objects (in variables, programs, params) counter */
									/* id, val */
#define EL_ID_NAME			0x12	/* name of variables and platform specific params */
									/* id, len, val */
#define EL_ID_PHLOAT		0x13	/* a phloat as a binary element */
									/* id, len, val */
#define EL_ID_ALPHA			0x14	/* a string */
									/* id, len, val */
#define EL_ID_PROGRAM		0x15	/* a program as a binary element */
									/* id, len, val */
/* variables objects */
#define EL_ID_VARBASE		0x20	/* based on variable data types + 0x20 */
#define EL_ID_VARNULL		0x20	/* an empty var */
									/* id, EL_ID_NAME */
#define EL_ID_VARREAL		0x21	/* real */
									/* id, EL_ID_NAME, EL_ID_PHLOAT */
#define EL_ID_VARCPX		0x22	/* complex */
									/* id, EL_ID_NAME, EL_ID_PHLOAT, EL_ID_PHLOAT */
#define EL_ID_VARREALMTX	0x23	/* real matrix */
								     /* id, EL_ID_NAME, rows, columns, EL_ID_[PHLOAT | ALPHA]s..  */
#define EL_ID_VARCPXMTX		0x24	/* complex matrix */
									/* id, EL_ID_NAME, rows, columns, EL_ID_PHLOATs.. */
#define EL_ID_VARSTRING		0x25	/* alpha */
									/* id, EL_ID_NAME, EL_ID_ALPHA */

/* Global Parameters
 * 0x2xyyyy		x	 = object type	
 *				yyyy = object index
 * 0x25 Value	String
 * 0x29 Value	Binary
 * 0x21 Value	Phloat
 * 0x28 Value	Int
 * 0x2f Value	Bool
*/
#define EL_mode_sigma_reg		0x281001	// int
#define EL_mode_goose			0x281002	// int
#define EL_mode_time_clktd		0x2F1003	// bool
#define EL_mode_time_clk24		0x2F1004	// bool
#define EL_flags				0x251005	// string

#define EL_current_prgm			0x281010	// int
#define EL_pc					0x281011	// int
#define EL_prgm_highlight_row	0x281012	// int

#define EL_varmenu_label		0x281020	// string
#define EL_varmenu_label0		0x251020	// string
#define EL_varmenu_label1		0x251021	// string
#define EL_varmenu_label2		0x251022	// string
#define EL_varmenu_label3		0x251023	// string
#define EL_varmenu_label4		0x251024	// string
#define EL_varmenu_label5		0x251025	// string
#define EL_varmenu				0x251026	// string
#define EL_varmenu_rows			0x281027	// int
#define EL_varmenu_row			0x281028	// int
#define EL_varmenu_role			0x281029	// int

#define EL_core_matrix_singular		0x2F1030	// bool
#define EL_core_matrix_outofrange	0x2F1031	// bool
#define EL_core_auto_repeat			0x2F1032	// bool
#define EL_core_enable_ext_accel	0x2F1033	// bool
#define EL_core_enable_ext_locat	0x2F1034	// bool
#define EL_core_enable_ext_heading	0x2F1035	// bool
#define EL_core_enable_ext_time		0x2F1036	// bool
#define EL_core_enable_ext_hpil		0x2F1037	// bool

#define EL_mode_clall				0x2F1040	// bool
#define EL_mode_command_entry		0x2F1041	// bool
#define EL_mode_number_entry		0x2F1042	// bool
#define EL_mode_alpha_entry			0x2F1043	// bool
#define EL_mode_shift				0x2F1044	// bool
#define EL_mode_appmenu				0x281045	// int
#define EL_mode_plainmenu			0x281046	// int
#define EL_mode_plainmenu_sticky	0x2F1047	// bool
#define EL_mode_transientmenu		0x281048	// int
#define EL_mode_alphamenu			0x281049	// int
#define EL_mode_commandmenu			0x28104A	// int
#define EL_mode_running				0x2F104B	// bool
#define EL_mode_varmenu				0x2F104C	// bool
#define EL_mode_updown				0x2F104D	// bool
#define EL_mode_getkey				0x2F104E	// bool

#define EL_entered_number			0x211050	// phloat
#define EL_entered_string			0x251051	// string

#define EL_pending_command			0x281060	// int
#define EL_pending_command_arg		0x261061	// arg struct, high level
#define EL_xeq_invisible			0x281062	// int

#define EL_incomplete_command		0x281063	// int
#define EL_incomplete_ind			0x281064	// int
#define EL_incomplete_alpha			0x281065	// int
#define EL_incomplete_length		0x281066	// int
#define EL_incomplete_maxdigits		0x281067	// int
#define EL_incomplete_argtype		0x281068	// int
#define EL_incomplete_num			0x281069	// int
#define EL_incomplete_str			0x25106a	// string
#define EL_incomplete_saved_pc		0x28106b	// int
#define EL_incomplete_saved_highlight_row	0x28106c	// int
#define EL_cmdline					0x25106d	// string
#define EL_cmdline_row				0x28106e	// int

#define EL_matedit_mode				0x281070	// int
#define EL_matedit_name				0x251071	// string
#define EL_matedit_i				0x281072	// int
#define EL_matedit_j				0x281073	// int
#define EL_matedit_prev_appmenu		0x281074	// int

#define EL_input_name				0x251080	// string
#define EL_input_arg				0x261081	// arg struct, high level

#define EL_baseapp					0x281090	// int

#define EL_random_number1			0x281091	// int
#define EL_random_number2			0x281092	// int
#define EL_random_number3			0x281093	// int
#define EL_random_number4			0x281094	// int

#define EL_deferred_print			0x281095	// int

#define EL_keybuf_head				0x281096	// int
#define EL_keybuf_tail				0x281096	// int

#define EL_keybuf					0x2810a0	// int
												// 0x2810ax enough place for 16 keys


#define EL_rtn_sp					0x2800FF	// int
#define EL_rtn_prgm					0x280100	// int
												// 0x2801xx enough place for 'unlimited' stack ?
#define EL_rtn_pc					0x280200	// int
												// 0x2802xx enough place for 'unlimited' stack ?
/*
 * Math
 */

// Solver
#define El_solveVersion				0x282000	// int
#define EL_solvePrgm_name			0x252001	// string
#define EL_solveActive_prgm_name	0x252002	// string
#define EL_solveKeep_running		0x282003	// int
#define EL_solvePrev_prgm			0x282004	// int
#define EL_solvePrev_pc				0x282005	// int
#define EL_solveState				0x282006	// int
#define EL_solveWhich				0x282007	// int
#define EL_solveToggle				0x282009	// int
#define EL_solveRetry_counter		0x28200a	// int
#define EL_solveRetry_value			0x21200b	// phloat
#define EL_solveX1					0x21200c	// phloat
#define EL_solveX2					0x21200d	// phloat
#define EL_solveX3					0x21200e	// phloat
#define EL_solveFx1					0x21200f	// phloat
#define EL_solveFx2					0x212010	// phloat
#define EL_solvePrev_x				0x212011	// phloat
#define EL_solveCurr_x				0x212012	// phloat
#define EL_solveCurr_f				0x212013	// phloat
#define EL_solveXm					0x212014	// phloat
#define EL_solveFxm					0x212015	// phloat
#define EL_solveLast_disp_time		0x282016	// int
#define EL_solveShadow_name			0x252020	// string
												// 0x252020..3f enough place for NUM_SHADOWS ?
#define EL_solveShadow_value		0x212040	// phloat
												// 0x252040..5f enough place for NUM_SHADOWS ?

// Integrator
#define El_integVersion				0x282080	// int
#define EL_integPrgm_name			0x252081	// string
#define EL_integActive_prgm_name	0x252082	// string
#define EL_integVar_name			0x252082	// string
#define EL_integKeep_running		0x282083	// int
#define EL_integPrev_prgm			0x282084	// int
#define EL_integPrev_pc				0x282085	// int
#define EL_integState				0x282086	// int
#define EL_integLlim				0x212087	// phloat
#define EL_integUlim				0x212088	// phloat
#define EL_integAcc					0x212089	// phloat
#define EL_integA					0x21208a	// phloat
#define EL_integB					0x21208b	// phloat
#define EL_integEps					0x21208c	// phloat
#define EL_integN					0x28208e	// int
#define EL_integM					0x28208f	// int
#define EL_integI					0x282090	// int
#define EL_integK					0x282091	// int
#define EL_integH					0x212092	// phloat
#define EL_integSum					0x212093	// phloat
#define EL_integNsteps				0x282094	// int
#define EL_integP					0x212095	// phloat
#define EL_integT					0x212096	// phloat
#define EL_integU					0x212097	// phloat
#define EL_integPrev_int			0x212098	// phloat
#define EL_integPrev_res			0x212099	// phloat
#define EL_integC					0x2120c0	// phloat
												// 0x2520c0..cf enough place for NUM_SHADOWS ?
#define EL_integS					0x2100e0	// phloat
												// 0x2520e0..ef enough place for NUM_SHADOWS ?


#define EL_off_enable_flag		0x230300

/* Static size of elements, don't forget to adjust against header */
#define EbmlPhloatSZ		16
#define EbmlCharSZ			1

bool ebmlWriteReg(vartype *v, char reg);
bool ebmlWriteAlphaReg();
bool ebmlWriteVar(var_struct *v);

bool ebmlWriteElBool(int elId, bool val);
bool ebmlWriteElInt(int elId, int val);
bool ebmlWriteElString(int elId, int len, char *val);
bool ebmlWriteElPhloat(int elId, phloat* p);
bool ebmlWriteElArg(int elId, arg_struct *arg);


#endif
