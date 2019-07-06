/*****************************************************************************
 * Free42 -- an HP-42S calculator simulator
 * Copyright (C) 2004-2019  Thomas Okken
 * EBML state file format
 * Copyright (C) 2018-2019  Jean-Christophe Hessemann
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

#include "core_globals.h"

typedef struct {
    int docId;              // document Id
    int docLen;             // document length
    int docFirstEl;         // position of first element in the document
    int elId;               // element Id
    int elLen;              // element length
    int elPos;              // element starting position in file
    int elData;             // start of data in element
    int pos;                // current position in file
} ebmlElement_Struct;

/* schema (kindof...)
 *
 * Header
 *
 *  <element name="EBMLFree42" path=1*1(\EBMLFree42) id="0x0x4672ee420" minOccurs="1" maxOccurs="1" type="master element"/>
 *  <documentation This element is the Free42 top master element/>
 *
 *      <element name="EBMLFree42Desc" path=1*1(\EBMLFree42\EBMLFree42Desc) id="0x0013" minOccurs="1" maxOccurs="1" type="string"/>
 *      <documentation This element is the Free42 description/>
 *
 *      <element name="EBMLFree42Version" path=1*1(\EBMLFree42\EBMLFree42Version) id="0x0021" minOccurs="1" maxOccurs="1" type="vinteger"/>
 *      <documentation This element is the Free42 specs used to create the document/>
 *
 *      <element name="EBMLFree42ReadVersion" path=1*1(\EBMLFree42\EBMLFree42ReadVersion) id="0x0031" minOccurs="1" maxOccurs="1" type="vinteger"/>
 *      <documentation This element is the minimum version required to be able to read this document/>
 *
 * Shell
 *
 *      <element name="EBMLFree42Shell" path=1*1(\EBMLFree42\EBMLFree42Shell) id="0x1000" minOccurs="1" maxOccurs="1" type="master element"/>
 *      <documentation This element is the shell state master element and contains shell specific elements/>
 *
 *          <element name="EBMLFree42ShellVersion" path=1*1(\EBMLFree42\EBMLFree42Shell\EBMLFree42ShellVersion) id="0x1011" minOccurs="1" maxOccurs="1" type="vinteger"/>
 *          <documentation This element is the shell state specs used to create the shell state master element/>
 *
 *          <element name="EBMLFree42ShellReadVersion" path=1*1(\EBMLFree42\EBMLFree42Shell\EBMLFree42ShellReadVersion) id="0x1021" minOccurs="1" maxOccurs="1" type="vinteger"/>
 *          <documentation This element is the minimum version to read the shell state master element/>
 *
 *          <element name="EBMLFree42ShellOS" path=1*1(\EBMLFree42\EBMLFree42Shell\EBMLFree42ShellOS) id="0x1033" minOccurs="1" maxOccurs="1" type="string"/>
 *          <documentation This element is the shell operating system generic short name/>
 *
 * Core
 *
 *      <element name="EBMLFree42Core" path=1*1(\EBMLFree42\EBMLFree42Core) id="0x2000" minOccurs="1" maxOccurs="1" type="master element"/>
 *      <documentation This element contains all core state elements excluding variables and programs/>
 *
 *          <element name="EBMLFree42CoreVersion" path=1*1(\EBMLFree42\EBMLFree42Core\EBMLFree42CoreVersion) id="0x2011" minOccurs="1" maxOccurs="1" type="vinteger"/>
 *          <documentation This element is the core state specs used to create the core state master element/>
 *
 *          <element name="EBMLFree42CoreReadVersion" path=1*1(\EBMLFree42\EBMLFree42Core\EBMLFree42CoreReadVersion) id="0x2021" minOccurs="1" maxOccurs="1" type="vinteger"/>
 *          <documentation This element is the minimum version to read the core state master element/>
 *
 * Variables
 *
 *      <element name="EBMLFree42Vars" path=1*1(\EBMLFree42\EBMLFree42Vars) id="0x4000" minOccurs="1" maxOccurs="1" type="master element"/>
 *      <documentation This element contains all explicit variables/>
 *
 *          <element name="EBMLFree42VarsVersion" path=1*1(\EBMLFree42\EBMLFree42Vars\EBMLFree42VarsVersion) id="0x4011" minOccurs="1" maxOccurs="1" type="vinteger"/>
 *          <documentation This element is the vars specs used to create the vars master element/>
 *
 *          <element name="EBMLFree42VarsReadVersion" path=1*1(\EBMLFree42\EBMLFree42Vars\EBMLFree42VarsReadVersion) id="0x4021" minOccurs="1" maxOccurs="1" type="vinteger"/>
 *          <documentation This element is the minimum version to read the vars master element/>
 *
 *          <element name="EBMLFree42VarsCount" path=1*1(\EBMLFree42\EBMLFree42Vars\EBMLFree42VarsCount) id="0x4031" minOccurs="1" maxOccurs="1" type="vinteger"/>
 *          <documentation This element is the number of variables in the vars master element/>
 *
 * Programs
 *
 *      <element name="EBMLFree42Progs" path=1*1(\EBMLFree42\EBMLFree42Progs) id="0x6000" minOccurs="1" maxOccurs="1" type="master element"/>
 *      <documentation This element contains all explicit variables/>
 *
 *          <element name="EBMLFree42ProgsVersion" path=1*1(\EBMLFree42\EBMLFree42Progs\EBMLFree42ProgsVersion) id="0x6011" minOccurs="1" maxOccurs="1" type="vinteger"/>
 *          <documentation This element is the vats specs used to create the Progs master element/>
 *
 *          <element name="EBMLFree42ProgsReadVersion" path=1*1(\EBMLFree42\EBMLFree42Progs\EBMLFree42ProgsReadVersion) id="0x6021" minOccurs="1" maxOccurs="1" type="vinteger"/>
 *          <documentation This element is the minimum version to read the Progs master element/>
 *
 *          <element name="EBMLFree42ProgsCount" path=1*1(\EBMLFree42\EBMLFree42Progs\EBMLFree42ProgsCount) id="0x6031" minOccurs="1" maxOccurs="1" type="vinteger"/>
 *          <documentation This element is the number of variables in the Progs master element/>
 *
 * Elements coding
 *
 *  0 > Master element
 *  1 > vint
 *  2 > integer
 *  3 > string
 *  4 > binary
 *  5 > boolean
 *
 * Variables master elements
 *
 *  <element name=EBMLFree42VarNull path=*(\EBMLFree42\EBMLFree42Core\EBMLFree42VarNull
 *                                      / \EBMLFree42\EBMLFree42Vars\EBMLFree42VarNull) id="0x400" type="master element"/>
 *
 *  <element name=EBMLFree42VarReal path=*(\EBMLFree42\EBMLFree42Core\EBMLFree42VarReal
 *                                      / \EBMLFree42\EBMLFree42Vars\EBMLFree42VarReal) id="0x410" type="master element"/>
 *
 *  <element name=EBMLFree42VarCpx  path=*(\EBMLFree42\EBMLFree42Core\EBMLFree42VarCpx
 *                                      / \EBMLFree42\EBMLFree42Vars\EBMLFree42VarCpx ) id="0x420" type="master element"/>
 *
 *  <element name=EBMLFree42VarRMtx path=*(\EBMLFree42\EBMLFree42Core\EBMLFree42VarRMtx
 *                                      / \EBMLFree42\EBMLFree42Vars\EBMLFree42VarRMtx) id="0x430" type="master element"/>
 *
 *  <element name=EBMLFree42VarCMtx path=*(\EBMLFree42\EBMLFree42Core\EBMLFree42VarCMtx
 *                                      / \EBMLFree42\EBMLFree42Vars\EBMLFree42VarCMtx) id="0x440" type="master element"/>
 *
 *  <element name=EBMLFree42VarStr  path=*(\EBMLFree42\EBMLFree42Core\EBMLFree42VarStr
 *                                      / \EBMLFree42\EBMLFree42Vars\EBMLFree42VarStr ) id="0x450" type="master element"/>
 *
 * variables global elements
 *
 *  <element name=EBMLFree42VarSize path=1*1(EBMLFree42VarNull\EBMLFree42VarSize
 *                                         / EBMLFree42VarReal\EBMLFree42VarSize
 *                                         / EBMLFree42VarCpx\EBMLFree42VarSize
 *                                         / EBMLFree42VarRMtx\EBMLFree42VarSize
 *                                         / EBMLFree42VarCMtx\EBMLFree42VarSize
 *                                         / EBMLFree42VarStr\EBMLFree42VarSize ) id ="0x511" type="vint"/>
 *
 *  <element name=EBMLFree42VarName path=1*1(EBMLFree42VarNull\EBMLFree42VarName
 *                                         / EBMLFree42VarReal\EBMLFree42VarName
 *                                         / EBMLFree42VarCpx\EBMLFree42VarName
 *                                         / EBMLFree42VarRMtx\EBMLFree42VarName
 *                                         / EBMLFree42VarCMtx\EBMLFree42VarName
 *                                         / EBMLFree42VarStr\EBMLFree42VarName ) id ="0x523" type="string"/>
 *
 *  <element name=EBMLFree42VarRows path=1*1(EBMLFree42VarRMtx\EBMLFree42VarRows
 *                                         / EBMLFree42VarCMtx\EBMLFree42VarRows ) id ="0x532" type="integer"/>
 *
 *  <element name=EBMLFree42VarColumns path=1*1(EBMLFree42VarRMtx\EBMLFree42VarColumns
 *                                            / EBMLFree42VarCMtx\EBMLFree42VarColumns ) id ="0x542" type="integer"/>
 *
 *  <element name=EBMLFree42VarPhloat path=1*1(EBMLFree42VarReal\EBMLFree42VarPhloat
 *                                           / EBMLFree42VarCpx\EBMLFree42VarPhloat
 *                                           / EBMLFree42VarRMtx\EBMLFree42VarPhloat
 *                                           / EBMLFree42VarCMtx\EBMLFree42VarPhloat ) id ="0x584" type="binary"/>
 *
 *  <element name=EBMLFree42VarStr path=1*1(EBMLFree42VarRMtx\EBMLFree42VarStr
 *                                        / EBMLFree42VarString\EBMLFree42VarStr ) id ="0x583" type="string"/>
 *
 * programs elements
 *
 *  <element name="EBMLFree42Prog" path=1*1(\EBMLFree42\EBMLFree42Progs\EBMLFree42Prog) id="0x600" minOccurs="1" maxOccurs="1" type="master element"/>
 *
 *      <element name="EBMLFree42ProgSize" path=1*1(EBMLFree42Prog\EBMLFree42ProgSize) id="0x611" minOccurs="1" maxOccurs="1" type="vint"/>
 *
 *      <element name="EBMLFree42ProgName" path=1*1(EBMLFree42Progs\EBMLFree42ProgName) id="0x623" minOccurs="1" maxOccurs="1" type="string"/>
 *
 *      <element name="EBMLFree42ProgData" path=1*1(EBMLFree42Progs\EBMLFree42ProgData) id="0x634" minOccurs="1" maxOccurs="1" type="binary"/>
 *
 */

#define _EBMLFree42Desc                     "Free42 ebml state"
#define _EBMLFree42Version                  1
#define _EBMLFree42ReadVersion              1

#define _EBMLFree42ShellVersion             1
#define _EBMLFree42ShellReadVersion         1

#define _EBMLFree42CoreVersion              1
#define _EBMLFree42CoreReadVersion          1

#define _EBMLFree42DisplayVersion           1
#define _EBMLFree42DisplayReadVersion       1

#define _EBMLFree42VarsVersion              1
#define _EBMLFree42VarsReadVersion          1

#define _EBMLFree42ProgsVersion             1
#define _EBMLFree42ProgsReadVersion         1

/*
 * types for base elements Ids
 * combine with each element Id's (4 lsb)
 */
#define EBMLFree42MasterElement             0x00        // master, no value
#define EBMLFree42VIntElement               0x01        // vint, value encoded as variable size integer
#define EBMLFree42IntElement                0x02        // signed int, size in bytes as vint, value stored in little endianness 
#define EBMLFree42StringElement             0x03        // string, size in bytes as vint, ascii string
#define EBMLFree42PhloatElement             0x04        // phloat as binary, size in bytes as vint, value in little endianess
#define EBMLFree42BooleanElement            0x05        // boolean, value encoded as variable size integer (0 or 1)
#define EBMLFree42BinaryElement             0x06        // binary, size in bytes as vint, binary undefined


/****************************************************************************
 *                                                                          *
 * Master document header                                                   *
 *                                                                          *
 ****************************************************************************/

#define EBMLFree42                          0x4672ee42  // Free42 top master element, vint
#define EBMLFree42Desc                      0x0013      // Free42 document description, string
#define EBMLFree42Version                   0x0021      // specs used to create the document
#define EBMLFree42ReadVersion               0x0031      // minimum version to read the document

/*
 * verbosity
 */
#define EBMLFree42Verbose                   0x7333      // Element, verbosity, string

/*
 * end of document for unsized documents
 */
#define EBMLFree42EOD                       0x7def      // Tag for end of unsized document

/****************************************************************************
 *                                                                          *
 * shell document                                                           *
 *                                                                          *
 ****************************************************************************/

#define EBMLFree42Shell                     0x1000      // Master element, document containing shell state
#define EBMLFree42ShellVersion              0x1011      // Element, version used to create this document, vint
#define EBMLFree42ShellReadVersion          0x1021      // Element, minimum version to read this document, vint
#define EBMLFree42ShellOS                   0x1033      // Element, OS name, string

/*
 * shell state (Windows)
 */
#define EBMLFree42ShellStateVersion         0x11011     // Element, shell state version, vint
#define EBMLFree42ShellState                0x11026     // Element, binary

/*
 * shared shell state (Android)
 *
 * could share with windows but will need to massage filename to convert from Linux to Windows
 */
#define EBMLFree42ShellPrintToGif           0x12015     // Element, boolean
#define EBMLFree42ShellPrintToTxt           0x12025     // Element, boolean
#define EBMLFree42ShellPrintToIr            0x12035     // Element, boolean
#define EBMLFree42ShellPrintToGifFileName   0x12043     // Element, string
#define EBMLFree42ShellPrintMaxGifHeight    0x12052     // Element, int
#define EBMLFree42ShellPrintToTxtFileName   0x12063     // ELement, string

/*
 * shell state (Android)
 */
#define EBMLFree42ShellAndroidSkinName0     0x13013     // Element, string
#define EBMLFree42ShellAndroidSkinName1     0x12023     // Element, string
#define EBMLFree42ShellAndroidExtSkinName0  0x13033     // Element, string
#define EBMLFree42ShellAndroidExtSkinName1  0x13043     // Element, string
#define EBMLFree42ShellAndroidSkinSmooth0   0x13055     // Element, boolean
#define EBMLFree42ShellAndroidSkinSmooth1   0x13065     // Element, boolean
#define EBMLFree42ShellAndroidDispSmooth0   0x13075     // Element, boolean
#define EBMLFree42ShellAndroidDispSmooth1   0x13085     // Element, boolean
#define EBMLFree42ShellAndroidKeyClickEn    0x13095     // Element, boolean
#define EBMLFree42ShellAndroidKeyVibraEn    0x130a5     // Element, boolean
#define EBMLFree42ShellAndroidPrefOrient    0x130b2     // Element, int
#define EBMLFree42ShellAndroidStyle         0x130c2     // Element, int
#define EBMLFree42ShellAndroidAlwaysrepaint 0x130d5     // Element, boolean

/****************************************************************************
 *                                                                          *
 * core document                                                            *
 *                                                                          *
 ****************************************************************************/

#define EBMLFree42Core                      0x2000      // Master element, document containing core state
#define EBMLFree42CoreVersion               0x2011      // Element, version used to create this document, vint
#define EBMLFree42CoreReadVersion           0x2021      // Element, minimum version to read this document, vint

/*
 * args common elements
 */
#define EBMLFree42ArgSize                   0x2111      // size of argument, vint
#define EBMLFree42ArgType                   0x2121      // type of argument, vint
#define EBMLFree42ArgLength                 0x2131      // length in arg struct, vint
#define EBMLFree42ArgTarget                 0x2142      // target in arg struct, int
#define EBMLFree42ArgVal                    0x2150      // val as union in arg struct, variable type

/* 
 * core parameters
 */
#define EL_mode_bin_dec                     0x21015     // bool
#define EL_mode_sigma_reg                   0x21022     // int
#define EL_mode_goose                       0x21032     // int
#define EL_mode_time_clktd                  0x21045     // bool
#define EL_mode_time_clk24                  0x21055     // bool
#define EL_flags                            0x21063     // string

#define EL_current_prgm                     0x21082     // int
#define EL_pc                               0x21092     // int
#define EL_prgm_highlight_row               0x210a2     // int

#define EL_varmenu_label                    0x21103     // string
#define EL_varmenu_label0                   0x21103     // string
#define EL_varmenu_label1                   0x21113     // string
#define EL_varmenu_label2                   0x21123     // string
#define EL_varmenu_label3                   0x21133     // string
#define EL_varmenu_label4                   0x21143     // string
#define EL_varmenu_label5                   0x21153     // string
#define EL_varmenu                          0x21183     // string
#define EL_varmenu_rows                     0x21192     // int
#define EL_varmenu_row                      0x211a2     // int
#define EL_varmenu_role                     0x211b2     // int

#define EL_core_matrix_singular             0x21205     // bool
#define EL_core_matrix_outofrange           0x21215     // bool
#define EL_core_auto_repeat                 0x21225     // bool

#define EL_mode_clall                       0x21305     // bool
#define EL_mode_command_entry               0x21315     // bool
#define EL_mode_number_entry                0x21325     // bool
#define EL_mode_alpha_entry                 0x21335     // bool
#define EL_mode_shift                       0x21345     // bool
#define EL_mode_appmenu                     0x21352     // int
#define EL_mode_plainmenu                   0x21362     // int
#define EL_mode_plainmenu_sticky            0x21375     // bool
#define EL_mode_transientmenu               0x21382     // int
#define EL_mode_alphamenu                   0x21392     // int
#define EL_mode_commandmenu                 0x213a2     // int
#define EL_mode_running                     0x213b5     // bool
#define EL_mode_varmenu                     0x213c5     // bool
#define EL_mode_updown                      0x213d5     // bool
#define EL_mode_getkey                      0x213e5     // bool

#define EL_entered_number                   0x21404     // phloat
#define EL_entered_string                   0x21413     // string

#define EL_pending_command                  0x21482     // int - argh!! check if should be converted to pgm
#define EL_pending_command_arg              0x21490     // arg struct, high level
#define EL_xeq_invisible                    0x214a2     // int - check again what it is...

#define EL_incomplete_command               0x21502     // int
#define EL_incomplete_ind                   0x21512     // int
#define EL_incomplete_alpha                 0x21522     // int
#define EL_incomplete_length                0x21532     // int
#define EL_incomplete_maxdigits             0x21542     // int
#define EL_incomplete_argtype               0x21552     // int
#define EL_incomplete_num                   0x21562     // int
#define EL_incomplete_str                   0x21573     // string
#define EL_incomplete_saved_pc              0x21582     // int
#define EL_incomplete_saved_highlight_row   0x21592     // int
#define EL_cmdline                          0x215a3     // string
#define EL_cmdline_row                      0x215b2     // int

#define EL_matedit_mode                     0x21602     // int
#define EL_matedit_name                     0x21613     // string
#define EL_matedit_i                        0x21622     // int
#define EL_matedit_j                        0x21632     // int
#define EL_matedit_prev_appmenu             0x21642     // int

#define EL_input_name                       0x21683     // string
#define EL_input_arg                        0x21690     // arg struct, high level

#define EL_baseapp                          0x21702     // int

#define EL_random_number1                   0x21742     // int
#define EL_random_number2                   0x21752     // int
#define EL_random_number3                   0x21762     // int
#define EL_random_number4                   0x21772     // int

#define EL_deferred_print                   0x21802     // int

#define EL_keybuf_head                      0x21882     // int
#define EL_keybuf_tail                      0x21892     // int
#define EL_rtn_sp                           0x218a2     // int, moved here to keep contiguous space for stacks

#define EL_keybuf_sz                        0x218b1     // vint
#define EL_rtn_prgm_sz                      0x218c1     // vint
#define EL_rtn_pc_sz                        0x218d1     // vint

#define EL_keybuf                           0x21b02     // int
                                                        // 0x21bx0 enough place for 16 keys (no more needed)
#define EL_rtn_prgm                         0x21c02     // int
                                                        // 0x21cx0/0x21dx0 enough place for upto 32 level stack (no more needed)
#define EL_rtn_pc                           0x21d02     // int
                                                        // 0x21ex0/0x21fx0 enough place for upto 32 level rtn addresses (no more needed)

#define EL_off_enable_flag                  0x21f05     // only for iphone 

/*
 * Math
 */

// Solver
#define EL_solveVersion             0x23002     // int
#define EL_solvePrgm_name           0x23013     // string
#define EL_solveActive_prgm_name    0x23023     // string
#define EL_solveKeep_running        0x23032     // int
#define EL_solvePrev_prgm           0x23042     // int
#define EL_solvePrev_pc             0x23052     // int
#define EL_solveState               0x23062     // int
#define EL_solveWhich               0x23072     // int
#define EL_solveToggle              0x23082     // int
#define EL_solveRetry_counter       0x23092     // int
#define EL_solveRetry_value         0x230a4     // phloat
#define EL_solveX1                  0x230b4     // phloat
#define EL_solveX2                  0x230c4     // phloat
#define EL_solveX3                  0x230d4     // phloat
#define EL_solveFx1                 0x230e4     // phloat
#define EL_solveFx2                 0x230f4     // phloat
#define EL_solvePrev_x              0x23104     // phloat
#define EL_solveCurr_x              0x23124     // phloat
#define EL_solveCurr_f              0x23134     // phloat
#define EL_solveXm                  0x23144     // phloat
#define EL_solveFxm                 0x23154     // phloat
#define EL_solveLast_disp_time      0x23162     // int

#define EL_solveShadow_name_sz      0x23241     // vint 
#define EL_solveShadow_value_sz     0x23281     // vint

#define EL_solveShadow_name         0x23403     // string
                                                // 0x234x0 enough place for upto 32 NUM_SHADOWS (no more needed)
#define EL_solveShadow_value        0x23804     // phloat
                                                // 0x238x0 enough place for upto 32 NUM_SHADOWS (no more needed)

// Integrator
#define EL_integVersion             0x24002     // int
#define EL_integPrgm_name           0x24013     // string
#define EL_integActive_prgm_name    0x24023     // string
#define EL_integVar_name            0x24033     // string
#define EL_integKeep_running        0x24042     // int
#define EL_integPrev_prgm           0x24052     // int
#define EL_integPrev_pc             0x24062     // int
#define EL_integState               0x24072     // int
#define EL_integLlim                0x24084     // phloat
#define EL_integUlim                0x24094     // phloat
#define EL_integAcc                 0x240a4     // phloat
#define EL_integA                   0x240b4     // phloat
#define EL_integB                   0x240c4     // phloat
#define EL_integEps                 0x240d4     // phloat
#define EL_integN                   0x240e2     // int
#define EL_integM                   0x240f2     // int
#define EL_integI                   0x24102     // int
#define EL_integK                   0x24112     // int
#define EL_integH                   0x24124     // phloat
#define EL_integSum                 0x24134     // phloat
#define EL_integNsteps              0x24142     // int
#define EL_integP                   0x24154     // phloat
#define EL_integT                   0x24164     // phloat
#define EL_integU                   0x24174     // phloat
#define EL_integPrev_int            0x24184     // phloat
#define EL_integPrev_res            0x24194     // phloat

#define EL_integC_sz                0x24241     // vint
#define EL_integS_sz                0x24281     // vint

#define EL_integC                   0x24404     // phloat
                                                // 0x244x0 enough place for upto 32 ROMBK (no more needed)
#define EL_integS                   0x24804     // phloat
                                                // 0x248x0 enough place for upto 32 ROMBK (no more needed)

/****************************************************************************
 *                                                                          *
 * display document                                                         *
 *                                                                          *
 ****************************************************************************/

#define EBMLFree42Display                   0x3000      // Master element, document containing display data
#define EBMLFree42DisplayVersion            0x3011      // Element, version used to create this document, vint
#define EBMLFree42DisplayReadVersion        0x3021      // Element, minimum version to read this document, vint

#define El_display_catalogmenu_section_sz   0x31011     // vint
#define El_display_catalogmenu_rows_sz      0x31021     // vint
#define El_display_catalogmenu_row_sz       0x31031     // vint
#define El_display_catalogmenu_item_sz      0x31041     // vint
#define El_display_custommenu_sz            0x31051     // vint
#define El_display_progmenu_arg_sz          0x31061     // vint
#define El_display_progmenu_is_gto_sz       0x31071     // vint
#define El_display_progmenu_sz              0x31081     // vint

#define El_display_catalogmenu_section      0x31102     // int
                                                        // 0x31000 to 0x310f0 enough place for up to 16 entries (no more needed)
#define El_display_catalogmenu_rows         0x31202     // int
                                                        // 0x31100 to 0x311f0 enough place for up to 16 entries (no more needed)
#define El_display_catalogmenu_row          0x31302     // int
                                                        // 0x31200 to 0x312f0 enough place for up to 16 entries (no more needed)
#define El_display_catalogmenu_item         0x31402     // int
                                                        // 0x31400 to 0x317f0 enough place for up to 64 entries (no more needed)
#define El_display_custommenu               0x31503     // string
                                                        // 0x31800 to 0x31bf0 enough place for up to 64 entries (no more needed)
#define El_display_progmenu_arg             0x31600     // arg
                                                        // 0x31c00 to 0x31cf0 enough place for up to 16 entries (no more needed)
#define El_display_progmenu_is_gto          0x31702     // int
                                                        // 0x31d00 to 0x31df0 enough place for up to 16 entries (no more needed)
#define El_display_progmenu                 0x31803     // string
                                                        // 0x31e00 to 0x31ef0 enough place for up to 16 entries (no more needed)
#define El_display                          0x31f03     // string
#define El_display_appmenu_exitcallback     0x31f12     // int

/****************************************************************************
 *                                                                          *
 * variables document                                                       *
 *                                                                          *
 ****************************************************************************/

#define EBMLFree42Vars                      0x4000      // master element, document containing explicit variables
#define EBMLFree42VarsVersion               0x4011      // element, version used to create this document, vint
#define EBMLFree42VarsReadVersion           0x4021      // element, minimum version to read this document, vint
#define EBMLFree42VarsCount                 0x4031      // element, number of objects in variables document, vint

/*
 * variables sub documents
 * All types derived from EBMLFree42VarNull + vartype << 4
 */
#define EBMLFree42VarNull                   0x400       // all master elements
#define EBMLFree42VarReal                   0x410
#define EBMLFree42VarCpx                    0x420
#define EBMLFree42VarRMtx                   0x430
#define EBMLFree42VarCMtx                   0x440
#define EBMLFree42VarStr                    0x450

/*
 * variables common elements
 */
#define EBMLFree42VarName                   0x463       // variable's name, string

/*
 * matrix variables common elements
 */
#define EBMLFree42VarRows                   0x472       // matrix's rows, has to be signed int to cope with shadows matrix, int
#define EBMLFree42VarColumns                0x482       // matrix's columns, keep same type as rows, int 

/*
 * variables elements Id's
 */
#define EBMLFree42VarNoType                 0x490       // untyped
#define EBMLFree42VarString                 0x4a3       // type EBMLFree42StringElement
#define EBMLFree42VarPhloat                 0x4b4       // type EBMLFree42PhloatElement

/****************************************************************************
 *                                                                          *
 * programs document                                                        *
 *                                                                          *
 ****************************************************************************/
 
#define EBMLFree42Progs                     0x6000      // Master element, document containing programs
#define EBMLFree42ProgsVersion              0x6011      // Element, version used to create this document, vint
#define EBMLFree42ProgsReadVersion          0x6021      // Element, minimum version to read this document, vint
#define EBMLFree42ProgsCount                0x6031      // Element, number of programs in document, vint

/*
 * program element
 */
#define EBMLFree42Prog                      0x600       // master program subdocument
#define EBMLFree42ProgSize                  0x611       // size of subdocument, vint
#define EBMLFree42ProgName                  0x623       // program name, string
#define EBMLFree42Prog_size                 0x631       // program size in memory, vint
#define EBMLFree42Prog_lclbl_invalid        0x641       // no need to save? vint
#define EBMLFree42Prog_text                 0x653       // program, as string

/*
 * static size of variables elements, don't forget to adjust against header
 */
#define EbmlPhloatSZ        19
#define EbmlStringSZ        3

bool ebmlWriteReg(vartype *v, char reg);
bool ebmlWriteAlphaReg();
bool ebmlWriteVar(var_struct *v);
bool ebmlWriteProgram(int prgm_index, prgm_struct *p);

bool ebmlWriteElBool(unsigned int elId, bool val);
bool ebmlWriteElVInt(unsigned int elId, unsigned int val);
bool ebmlWriteElInt(unsigned int elId, int val);
bool ebmlWriteElString(unsigned int elId, int len, char *val);
bool ebmlWriteElPhloat(unsigned int elId, phloat *p);
bool ebmlWriteElBinary(unsigned int elId, unsigned int l, void *b);
bool ebmlWriteElArg(unsigned int elId, arg_struct *arg);

bool ebmlWriteMasterHeader();
bool ebmlWriteShellDocument(unsigned int version, unsigned int readVersion, unsigned int len, char * OsVersion);
bool ebmlWriteCoreDocument();
bool ebmlWriteDisplayDocument();
bool ebmlWriteVarsDocument(unsigned int count);
bool ebmlWriteProgsDocument(unsigned int count);
bool ebmlWriteEndOfDocument();

bool ebmlReadVar(ebmlElement_Struct *el, var_struct *var);
bool ebmlReadAlphaReg(ebmlElement_Struct *el, char *valuse, int *length);
bool ebmlReadProgram(ebmlElement_Struct *el, prgm_struct *p);

bool ebmlReadElBool(ebmlElement_Struct *el, bool *value);
bool ebmlReadElInt(ebmlElement_Struct *el, int *value);
bool ebmlReadElString(ebmlElement_Struct *el, char  *value, int *sz);
bool ebmlReadElPhloat(ebmlElement_Struct *el, phloat *p);
bool ebmlReadElArg(ebmlElement_Struct *el, arg_struct *arg);

int ebmlGetNext(ebmlElement_Struct *el);
int ebmlGetEl(ebmlElement_Struct *el);

bool ebmlGetString(ebmlElement_Struct *el, char *value, int len);
bool ebmlGetBinary(ebmlElement_Struct *el, void *value, int len);

#endif
