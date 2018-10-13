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

/* schema (kindof...)
 *
 * Header
 *
 *	<element name="EBMLFree42" path=1*1(\EBMLFree42) id="0x0x4672ee420" minOccurs="1" maxOccurs="1" type="master element"/>
 *	<documentation This element is the Free42 top master element/>
 *
 *		<element name="EBMLFree42Desc" path=1*1(\EBMLFree42\EBMLFree42Desc) id="0x1013" minOccurs="1" maxOccurs="1" type="string"/>
 *		<documentation This element is the Free42 description/>
 *
 *		<element name="EBMLFree42Version" path=1*1(\EBMLFree42\EBMLFree42Version) id="0x1021" minOccurs="1" maxOccurs="1" type="vinteger"/>
 *		<documentation This element is the Free42 specs used to create the document/>
 *
 *		<element name="EBMLFree42ReadVersion" path=1*1(\EBMLFree42\EBMLFree42ReadVersion) id="0x1022" minOccurs="1" maxOccurs="1" type="vinteger"/>
 *		<documentation This element is the minimum version required to be able to read this document/>
 *
 * Shell
 *
 *		<element name="EBMLFree42Shell" path=1*1(\EBMLFree42\EBMLFree42Shell) id="0x200000" minOccurs="1" maxOccurs="1" type="master element"/>
 *		<documentation This element is the shell state master element and contains shell specific elements/>
 *
 *			<element name="EBMLFree42ShellOS" path=1*1(\EBMLFree42\EBMLFree42Shell\EBMLFree42ShellOS) id="0x200013" minOccurs="1" maxOccurs="1" type="string"/>
 *			<documentation This element is the shell operating system generic short name/>
 *
 *			<element name="EBMLFree42ShellVersion" path=1*1(\EBMLFree42\EBMLFree42Shell\EBMLFree42ShellVersion) id="0x200021" minOccurs="1" maxOccurs="1" type="vinteger"/>
 *			<documentation This element is the shell state specs used to create the shell state master element/>
 *
 *			<element name="EBMLFree42ShellReadVersion" path=1*1(\EBMLFree42\EBMLFree42Shell\EBMLFree42ShellReadVersion) id="0x200031" minOccurs="1" maxOccurs="1" type="vinteger"/>
 *			<documentation This element is the minimum version to read the shell state master element/>
 *
 * Core
 *
 *		<element name="EBMLFree42Core" path=1*1(\EBMLFree42\EBMLFree42Core) id="0x300000" minOccurs="1" maxOccurs="1" type="master element"/>
 *		<documentation This element contains all core state elements excluding variables and programs/>
 *
 *			<element name="EBMLFree42CoreVersion" path=1*1(\EBMLFree42\EBMLFree42Core\EBMLFree42CoreVersion) id="0x300011" minOccurs="1" maxOccurs="1" type="vinteger"/>
 *			<documentation This element is the core state specs used to create the core state master element/>
 *
 *			<element name="EBMLFree42CoreReadVersion" path=1*1(\EBMLFree42\EBMLFree42Core\EBMLFree42CoreReadVersion) id="0x300021" minOccurs="1" maxOccurs="1" type="vinteger"/>
 *			<documentation This element is the minimum version to read the core state master element/>
 *
 * Variables
 *
 *		<element name="EBMLFree42Vars" path=1*1(\EBMLFree42\EBMLFree42Vars) id="0x4000" minOccurs="1" maxOccurs="1" type="master element"/>
 *		<documentation This element contains all explicit variables/>
 *
 *			<element name="EBMLFree42VarsVersion" path=1*1(\EBMLFree42\EBMLFree42Vars\EBMLFree42VarsVersion) id="0x4011" minOccurs="1" maxOccurs="1" type="vinteger"/>
 *			<documentation This element is the vars specs used to create the vars master element/>
 *
 *			<element name="EBMLFree42VarsReadVersion" path=1*1(\EBMLFree42\EBMLFree42Vars\EBMLFree42VarsReadVersion) id="0x4021" minOccurs="1" maxOccurs="1" type="vinteger"/>
 *			<documentation This element is the minimum version to read the vars master element/>
 *
 *			<element name="EBMLFree42VarsCount" path=1*1(\EBMLFree42\EBMLFree42Vars\EBMLFree42VarsCount) id="0x4031" minOccurs="1" maxOccurs="1" type="vinteger"/>
 *			<documentation This element is the number of variables in the vars master element/>
 *
 * Programs
 *
 *		<element name="EBMLFree42Progs" path=1*1(\EBMLFree42\EBMLFree42Progs) id="0x6000" minOccurs="1" maxOccurs="1" type="master element"/>
 *		<documentation This element contains all explicit variables/>
 *
 *			<element name="EBMLFree42ProgsVersion" path=1*1(\EBMLFree42\EBMLFree42Progs\EBMLFree42ProgsVersion) id="0x6011" minOccurs="1" maxOccurs="1" type="vinteger"/>
 *			<documentation This element is the vats specs used to create the Progs master element/>
 *
 *			<element name="EBMLFree42ProgsReadVersion" path=1*1(\EBMLFree42\EBMLFree42Progs\EBMLFree42ProgsReadVersion) id="0x6021" minOccurs="1" maxOccurs="1" type="vinteger"/>
 *			<documentation This element is the minimum version to read the Progs master element/>
 *
 *			<element name="EBMLFree42ProgsCount" path=1*1(\EBMLFree42\EBMLFree42Progs\EBMLFree42ProgsCount) id="0x6031" minOccurs="1" maxOccurs="1" type="vinteger"/>
 *			<documentation This element is the number of variables in the Progs master element/>
 *
 * Elements coding
 *
 *	0 > Master element
 *	1 > vint
 *	2 > integer
 *	3 > string
 *	4 > binary
 *	5 > boolean
 *
 * Variables master elements
 *
 *	<element name=EBMLFree42VarNull path=*(\EBMLFree42\EBMLFree42Core\EBMLFree42VarNull
 *										/ \EBMLFree42\EBMLFree42Vars\EBMLFree42VarNull) id="0x400" type="master element"/>
 *
 *	<element name=EBMLFree42VarReal path=*(\EBMLFree42\EBMLFree42Core\EBMLFree42VarReal
 *										/ \EBMLFree42\EBMLFree42Vars\EBMLFree42VarReal) id="0x410" type="master element"/>
 *
 *	<element name=EBMLFree42VarCpx  path=*(\EBMLFree42\EBMLFree42Core\EBMLFree42VarCpx
 *										/ \EBMLFree42\EBMLFree42Vars\EBMLFree42VarCpx ) id="0x420" type="master element"/>
 *
 *	<element name=EBMLFree42VarRMtx path=*(\EBMLFree42\EBMLFree42Core\EBMLFree42VarRMtx
 *										/ \EBMLFree42\EBMLFree42Vars\EBMLFree42VarRMtx) id="0x430" type="master element"/>
 *
 *	<element name=EBMLFree42VarCMtx path=*(\EBMLFree42\EBMLFree42Core\EBMLFree42VarCMtx
 *										/ \EBMLFree42\EBMLFree42Vars\EBMLFree42VarCMtx) id="0x440" type="master element"/>
 *
 *	<element name=EBMLFree42VarStr  path=*(\EBMLFree42\EBMLFree42Core\EBMLFree42VarStr
 *										/ \EBMLFree42\EBMLFree42Vars\EBMLFree42VarStr ) id="0x450" type="master element"/>
 *
 * variables global elements
 *
 *	<element name=EBMLFree42VarSize path=1*1(EBMLFree42VarNull\EBMLFree42VarSize
 *										   / EBMLFree42VarReal\EBMLFree42VarSize
 *										   / EBMLFree42VarCpx\EBMLFree42VarSize
 *										   / EBMLFree42VarRMtx\EBMLFree42VarSize
 *										   / EBMLFree42VarCMtx\EBMLFree42VarSize
 *										   / EBMLFree42VarStr\EBMLFree42VarSize ) id ="0x511" type="vint"/>
 *
 *	<element name=EBMLFree42VarName path=1*1(EBMLFree42VarNull\EBMLFree42VarName
 *										   / EBMLFree42VarReal\EBMLFree42VarName
 *										   / EBMLFree42VarCpx\EBMLFree42VarName
 *										   / EBMLFree42VarRMtx\EBMLFree42VarName
 *										   / EBMLFree42VarCMtx\EBMLFree42VarName
 *										   / EBMLFree42VarStr\EBMLFree42VarName ) id ="0x523" type="string"/>
 *
 *	<element name=EBMLFree42VarRows path=1*1(EBMLFree42VarRMtx\EBMLFree42VarRows
 *										   / EBMLFree42VarCMtx\EBMLFree42VarRows ) id ="0x532" type="integer"/>
 *
 *	<element name=EBMLFree42VarColumns path=1*1(EBMLFree42VarRMtx\EBMLFree42VarColumns
 *											  / EBMLFree42VarCMtx\EBMLFree42VarColumns ) id ="0x542" type="integer"/>
 *
 *	<element name=EBMLFree42VarPhloat path=1*1(EBMLFree42VarReal\EBMLFree42VarPhloat
 *											 / EBMLFree42VarCpx\EBMLFree42VarPhloat
 *											 / EBMLFree42VarRMtx\EBMLFree42VarPhloat
 *											 / EBMLFree42VarCMtx\EBMLFree42VarPhloat ) id ="0x584" type="binary"/>
 *
 *	<element name=EBMLFree42VarStr path=1*1(EBMLFree42VarRMtx\EBMLFree42VarStr
 *										  / EBMLFree42VarString\EBMLFree42VarStr ) id ="0x583" type="string"/>
 *
 * programs elements
 *
 *	<element name="EBMLFree42Prog" path=1*1(\EBMLFree42\EBMLFree42Progs\EBMLFree42Prog) id="0x600" minOccurs="1" maxOccurs="1" type="master element"/>
 *
 *		<element name="EBMLFree42ProgSize" path=1*1(EBMLFree42Prog\EBMLFree42ProgSize) id="0x611" minOccurs="1" maxOccurs="1" type="vint"/>
 *
 *		<element name="EBMLFree42ProgName" path=1*1(EBMLFree42Progs\EBMLFree42ProgName) id="0x623" minOccurs="1" maxOccurs="1" type="string"/>
 *
 *		<element name="EBMLFree42ProgData" path=1*1(EBMLFree42Progs\EBMLFree42ProgData) id="0x634" minOccurs="1" maxOccurs="1" type="binary"/>
 *
 */


#define _EBMLFree42Desc					"Free42 ebml state"
#define _EBMLFree42Version				1
#define _EBMLFree42ReadVersion			1

#define _EBMLFree42CoreVersion			1
#define _EBMLFree42CoreReadVersion		1

#define _EBMLFree42VarsVersion			1
#define _EBMLFree42VarsReadVersion		1

#define _EBMLFree42ProgsVersion			1
#define _EBMLFree42ProgsReadVersion		1

#define _EBMLFree42ShellVersion			1
#define _EBMLFree42ShellReadVersion		1


/*
 * types for base elements Ids
 * combine with each element Id's (4 lsb)
 */
#define EBMLFree42MasterElement			0x00			/* master, no value														*/
#define EBMLFree42VIntElement			0x01			/* vint, value encoded as variable size integer							*/
#define EBMLFree42IntElement			0x02			/* signed int, size in bytes as vint, value stored in little endianness */  
#define EBMLFree42StringElement			0x03			/* string, size in bytes as vint, ascii	string							*/
#define EBMLFree42PhloatElement			0x04			/* phloat as binary, size in bytes as vint, value in little endianess	*/
#define EBMLFree42BooleanElement		0x05			/* boolean, value encoded as variable size integer (0 or 1)				*/
#define EBMLFree42BinaryElement			0x06			/* binary, size in bytes as vint, binary undefined						*/

/*
 * Master document header
 */
#define EBMLFree42						0x4672ee42		/* Free42 top master element, vint		*/
#define EBMLFree42Desc					0x0013			/* Free42 document description, string	*/
#define EBMLFree42Version				0x0021			/* specs used to create the document	*/
#define EBMLFree42ReadVersion			0x0031			/* minimum version to read the document	*/

/*
 * variables sub documents
 * All types derived from EBMLFree42VarNull + vartype << 4
 */
#define EBMLFree42VarNull				0x400			/* master elements */
#define EBMLFree42VarReal				0x410
#define EBMLFree42VarCpx				0x420
#define EBMLFree42VarRMtx				0x430
#define EBMLFree42VarCMtx				0x440
#define EBMLFree42VarStr				0x450

/*
 * variables common elements
 */
#define EBMLFree42VarSize				0x511			/* size of subdocument, vint	*/
#define EBMLFree42VarName				0x523			/* variable's name, string		*/

/*
 * matrix variables common elements
 */
#define	EBMLFree42VarRows				0x532			/* matrix's rows, has to be signed int to cope with shadows matrix int	*/
#define EBMLFree42VarColumns			0x542			/* matrix's columns, signed int to keep same type as rows, int			*/ 

/*
 * variables elements Id's
 */
#define EBMLFree42VarNoType				0x580			/* untyped						*/
#define EBMLFree42VarString				0x583			/* type EBMLFree42StringElement	*/
#define EBMLFree42VarPhloat				0x584			/* type EBMLFree42PhloatElement	*/

/*
 * variables document
 */
#define EBMLFree42Vars					0x4000			/* master element, document containing explicit variables	*/
#define EBMLFree42VarsVersion			0x4011			/* element, version used to create this document, vint		*/
#define EBMLFree42VarsReadVersion		0x4021			/* element, minimum version to read this document, vint		*/
#define EBMLFree42VarsCount				0x4031			/* element, number of objects in variables document, vint	*/

/*
 * core document
 */
#define EBMLFree42Core					0x2000			/* Master element, document containing core state		*/
#define EBMLFree42CoreVersion			0x2011			/* Element, version used to create this document, vint	*/
#define EBMLFree42CoreReadVersion		0x2021			/* Element, minimum version to read this document, vint	*/

/*
 * args common elements
 */
#define EBMLFree42ArgSize				0x2111			/* size of argument, vint						*/
#define EBMLFree42ArgType				0x2121			/* type of argument, vint						*/
#define EBMLFree42ArgLength				0x2131			/* length in arg struct, vint					*/
#define EBMLFree42ArgTarget				0x2142			/* target in arg struct, int					*/
#define EBMLFree42ArgVal				0x2150			/* val as union in arg struct, variable type	*/

/*
 * program element
 */
#define EBMLFree42Prog					0x600			/* master program subdocument	*/
#define EBMLFree42ProgSize				0x611			/* size of subdocument			*/
#define EBMLFree42ProgName				0x623			/* program name					*/
#define EBMLFree42ProgData				0x633			/* program, as string			*/

/*
 * programs document
 */
#define EBMLFree42Progs					0x6000			/* Master element, document containing programs				*/
#define EBMLFree42ProgsVersion			0x6011			/* Element, version used to create this document, vint		*/
#define EBMLFree42ProgsReadVersion		0x6021			/* Element, minimum version to read this document, vint		*/
#define EBMLFree42ProgsCount			0x6031			/* Element, number of programs in document, vint			*/

/*
 * shell document
 */
#define EBMLFree42Shell					0x3000			/* Master element, document containing shel state		*/
#define EBMLFree42ShellVersion			0x3011			/* Element, version used to create this document, vint	*/
#define EBMLFree42ShellReadVersion		0x3021			/* Element, minimum version to read this document, vint	*/
#define EBMLFree42ShellOS				0x3033			/* Element, OS name, string								*/
#define EBMLFree42ShellState			0x3046			/* Element, binary										*/

/*
 * end of document for unsized documents
 */
#define EBMLFree42EOD					0x7def			/* Tag for end of unsized document						*/

/* 
 * core parameters
 */
#define EL_mode_sigma_reg		0x21010		// int
#define EL_mode_goose			0x21020		// int
#define EL_mode_time_clktd		0x21030		// bool
#define EL_mode_time_clk24		0x21040		// bool
#define EL_flags				0x21050		// string

#define EL_current_prgm			0x21080		// int
#define EL_pc					0x21090		// int
#define EL_prgm_highlight_row	0x210a0		// int

#define EL_varmenu_label		0x21100		// string
#define EL_varmenu_label0		0x21100		// string
#define EL_varmenu_label1		0x21110		// string
#define EL_varmenu_label2		0x21120		// string
#define EL_varmenu_label3		0x21130		// string
#define EL_varmenu_label4		0x21140		// string
#define EL_varmenu_label5		0x21150		// string
#define EL_varmenu				0x21180		// string
#define EL_varmenu_rows			0x21190		// int
#define EL_varmenu_row			0x211a0		// int
#define EL_varmenu_role			0x211b0		// int

#define EL_core_matrix_singular		0x21200		// bool
#define EL_core_matrix_outofrange	0x21210		// bool
#define EL_core_auto_repeat			0x21220		// bool
#define EL_core_enable_ext_accel	0x21230		// bool
#define EL_core_enable_ext_locat	0x21240		// bool
#define EL_core_enable_ext_heading	0x21250		// bool
#define EL_core_enable_ext_time		0x21260		// bool
#define EL_core_enable_ext_hpil		0x21270		// bool

#define EL_mode_clall				0x21300		// bool
#define EL_mode_command_entry		0x21310		// bool
#define EL_mode_number_entry		0x21320		// bool
#define EL_mode_alpha_entry			0x21330		// bool
#define EL_mode_shift				0x21340		// bool
#define EL_mode_appmenu				0x21350		// int
#define EL_mode_plainmenu			0x21360		// int
#define EL_mode_plainmenu_sticky	0x21370		// bool
#define EL_mode_transientmenu		0x21380		// int
#define EL_mode_alphamenu			0x21390		// int
#define EL_mode_commandmenu			0x213a0		// int
#define EL_mode_running				0x213b0		// bool
#define EL_mode_varmenu				0x213c0		// bool
#define EL_mode_updown				0x213d0		// bool
#define EL_mode_getkey				0x213e0		// bool

#define EL_entered_number			0x21400		// phloat
#define EL_entered_string			0x21410		// string

#define EL_pending_command			0x21480		// int - argh!! check if should be converted to pgm
#define EL_pending_command_arg		0x21490		// arg struct, high level
#define EL_xeq_invisible			0x214a0		// int - check again what it is...

#define EL_incomplete_command				0x21500		// int
#define EL_incomplete_ind					0x21510		// int
#define EL_incomplete_alpha					0x21520		// int
#define EL_incomplete_length				0x21530		// int
#define EL_incomplete_maxdigits				0x21540		// int
#define EL_incomplete_argtype				0x21550		// int
#define EL_incomplete_num					0x21560		// int
#define EL_incomplete_str					0x21570		// string
#define EL_incomplete_saved_pc				0x21580		// int
#define EL_incomplete_saved_highlight_row	0x21590		// int
#define EL_cmdline							0x215a0		// string
#define EL_cmdline_row						0x215b0	// int

#define EL_matedit_mode						0x21600		// int
#define EL_matedit_name						0x21610		// string
#define EL_matedit_i						0x21620		// int
#define EL_matedit_j						0x21630		// int
#define EL_matedit_prev_appmenu				0x21640		// int

#define EL_input_name						0x21680		// string
#define EL_input_arg						0x21690		// arg struct, high level

#define EL_baseapp							0x21700		// int

#define EL_random_number1					0x21740		// int
#define EL_random_number2					0x21750		// int
#define EL_random_number3					0x21760		// int
#define EL_random_number4					0x21770		// int

#define EL_deferred_print					0x21800		// int

#define EL_keybuf_head						0x21880		// int
#define EL_keybuf_tail						0x21890		// int
#define EL_rtn_sp							0x218a0		// int, moved here to keep contiguous space for stacks

#define EL_keybuf							0x21900		// int
														// 0x219x0 enough place for 16 keys

#define EL_rtn_prgm							0x21a00		// int
														// 0x21ax0 enough place for upto 32 level stack ?
#define EL_rtn_pc							0x21c00		// int
														// 0x21bx0 enough place for upto 32 level rtn zddresses ?

/*
 * display
 */
#define El_display_catalogmenu_section		0x22000		// int
														// 0x22000 to 0x220f0 enough place for up to 16 entries
#define El_display_catalogmenu_rows			0x22100		// int
														// 0x22100 to 0x221f0 enough place for up to 16 entries
#define El_display_catalogmenu_row			0x22200		// int
														// 0x22200 to 0x222f0 enough place for up to 16 entries
#define El_display_catalogmenu_item			0x22400		// int
														// 0x22400 to 0x227f0 enough place for up to 64 entries
#define El_display_custommenu				0x22800		// string
														// 0x22800 to 0x22bf0 enough place for up to 64 entries
#define El_display_progmenu_arg				0x22c00		// arg
														// 0x22c00 to 0x22cf0 enough place for up to 16 entries
#define El_display_progmenu_is_gto			0x22d00		// int
														// 0x22d00 to 0x22df0 enough place for up to 16 entries
#define El_display_progmenu					0x22e00		// string
														// 0x22e00 to 0x22ef0 enough place for up to 16 entries
#define El_display							0x22f00		// string
#define El_display_appmenu_exitcallback		0x22f10		// int

														
/*
 * Math
 */

// Solver
#define EL_solveVersion				0x23000		// int
#define EL_solvePrgm_name			0x23010		// string
#define EL_solveActive_prgm_name	0x23020		// string
#define EL_solveKeep_running		0x23030		// int
#define EL_solvePrev_prgm			0x23040		// int
#define EL_solvePrev_pc				0x23050		// int
#define EL_solveState				0x23060		// int
#define EL_solveWhich				0x23070		// int
#define EL_solveToggle				0x23080		// int
#define EL_solveRetry_counter		0x23090		// int
#define EL_solveRetry_value			0x230a0		// phloat
#define EL_solveX1					0x230b0		// phloat
#define EL_solveX2					0x230c0		// phloat
#define EL_solveX3					0x230d0		// phloat
#define EL_solveFx1					0x230e0		// phloat
#define EL_solveFx2					0x230f0		// phloat
#define EL_solvePrev_x				0x23100		// phloat
#define EL_solveCurr_x				0x23120		// phloat
#define EL_solveCurr_f				0x23130		// phloat
#define EL_solveXm					0x23140		// phloat
#define EL_solveFxm					0x23150		// phloat
#define EL_solveLast_disp_time		0x23160		// int
#define EL_solveShadow_name			0x23400		// string
												// 0x234x0 enough place for upto 32 NUM_SHADOWS ?
#define EL_solveShadow_value		0x23800		// phloat
												// 0x238x0 enough place for upto 32 NUM_SHADOWS ?

// Integrator
#define EL_integVersion				0x24000		// int
#define EL_integPrgm_name			0x24010		// string
#define EL_integActive_prgm_name	0x24020		// string
#define EL_integVar_name			0x24030		// string
#define EL_integKeep_running		0x24040		// int
#define EL_integPrev_prgm			0x24050		// int
#define EL_integPrev_pc				0x24060		// int
#define EL_integState				0x24070		// int
#define EL_integLlim				0x24080		// phloat
#define EL_integUlim				0x24090		// phloat
#define EL_integAcc					0x240a0		// phloat
#define EL_integA					0x240b0		// phloat
#define EL_integB					0x240c0		// phloat
#define EL_integEps					0x240d0		// phloat
#define EL_integN					0x240e0		// int
#define EL_integM					0x240f0		// int
#define EL_integI					0x24100		// int
#define EL_integK					0x24110		// int
#define EL_integH					0x24120		// phloat
#define EL_integSum					0x24130		// phloat
#define EL_integNsteps				0x24140		// int
#define EL_integP					0x24150		// phloat
#define EL_integT					0x24160		// phloat
#define EL_integU					0x24170		// phloat
#define EL_integPrev_int			0x24180		// phloat
#define EL_integPrev_res			0x24190		// phloat
#define EL_integC					0x24400		// phloat
												// 0x244x0 enough place for upto 32 ROMBK ?
#define EL_integS					0x24800		// phloat
												// 0x248x0 enough place for upto 32 ROMBK ?

// hpil eXtensions
#define EL_hpil_selected			0x2e000		// int
#define EL_hpil_print				0x2e010		// int
#define EL_hpil_disk				0x2e020		// int
#define EL_hpil_plotter				0x2e030		// int,
#define EL_hpil_prtAid				0x2e040		// int
#define EL_hpil_dskAid				0x2e050		// int
#define EL_hpil_modeEnabled			0x2e060		// bool
#define EL_hpil_modeTransparent		0x2e070		// bool
#define EL_hpil_modePIL_Box			0x2e080		// bool


#define EL_off_enable_flag			0x2fff0		// only for iphone 


/*
 * static size of variables elements, don't forget to adjust against header
 */
#define EbmlPhloatSZ		19
#define EbmlStringSZ		3

bool ebmlWriteReg(vartype *v, char reg);
bool ebmlWriteAlphaReg();
bool ebmlWriteVar(var_struct *v);
bool ebmlWriteProgram(int prgm_index, prgm_struct *p);

bool ebmlWriteElBool(unsigned int elId, bool val);
bool ebmlWriteElInt(unsigned int elId, int val);
bool ebmlWriteElString(unsigned int elId, int len, char *val);
bool ebmlWriteElPhloat(unsigned int elId, phloat* p);
bool ebmlWriteElBinary(unsigned int elId, unsigned int l, void * b);
bool ebmlWriteElArg(unsigned int elId, arg_struct *arg);

bool ebmlWriteMasterHeader();
bool ebmlWriteCoreDocument();
bool ebmlWriteVarsDocument(unsigned int count);
bool ebmlWriteProgsDocument(unsigned int count);
bool ebmlWriteShellDocument(unsigned int version, unsigned int readVersion, unsigned int len, char * OsVersion);
bool ebmlWriteEndOfDocument();


#endif
