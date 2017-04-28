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

/* mass storage & file system code
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include "core_display.h"
#include "core_globals.h"
#include "core_main.h"
#include "core_variables.h"
#include "hpil_common.h"
#include "hpil_controller.h"
#include "hpil_mass.h"

#if defined (__ANDROID__)

uint16_t _byteswap_ushort(uint16_t x) {
    return (x << 8) | (x >> 8 );
}

uint32_t _byteswap_ulong(uint32_t x) {
    x = ((x << 8) & 0xff00ff00 ) | ((x >> 8) & 0x00ff00ff );
    return (x << 16) | (x >> 16);
}

#endif

#ifndef BCD_MATH
// We need these locally for BID128<->double conversion
#include "bid_conf.h"
#include "bid_functions.h"
#endif

/* common vars from core_commands8
 *
 */
extern AlphaSplit alphaSplit;
extern HPIL_Settings hpil_settings;
extern HpilXController hpilXCore;
extern int controllerCommand;
extern int hpil_step;
extern int hpil_worker(int interrupted);
extern int (*hpil_completion)(int);
extern void (*hpil_emptyListenBuffer)(void);
extern void (*hpil_fillTalkBuffer)(void);
extern int frame;

#define FlagSecured 0x08			// sec flag

#define ITEM_PAD		0			// padding
#define ITEM_FIRST		1			// first item, isString for real matrix, real part for complex
#define ITEM_SECOND		2			// second item, real part for real matrix, imaginary part for complex

extern char hpil_text[23];		// generic text buffer up to 22 chars !!!

#pragma pack(1)			// compact form

struct il_vheader {
	char nameLen;
	char name[7];
	uint16_t type;
	uint16_t rows;		// real or complex matrix, else 0x0000
	uint16_t columns;	// real or complex matrix, else 0x0000
	uint16_t remaining;	// remaining variables in files (0 if last)
};

/* union to avoid intermediate / ease writing of differents var types.
 *
 */
union Regs {
	vartype *reg;
	vartype_string *s;
	vartype_real *r;
	vartype_complex *c;
	vartype_realmatrix *rm;
	vartype_complexmatrix *cm;
};

#pragma pack()

/* mass storage structs
 *
 */

#define TypeCreate	0x0001	// Create file
#define TypeWrite	0x0002	// write to file
#define TypeRead	0x0004	// read from file
#define TypeUpdate	0x0008	// update dir entry
#define TypePurge	0x0010	// purge dir entry
#define TypeRename	0x0020	// update dir entry
#define TypeZero	0x0040	// zero data file
#define TypePartial	0x0100	// need partial write
#define TypeClose	0x0200	// need close


struct MassStorage {
	// file system
	uint16_t recordsMax;		// drive capacity
	uint16_t recordsLeft;
	uint16_t dirEntriesLeft;
	char volume[6];				// volume name
	uint16_t dirEntryBlock;		// Current dir entry
	uint16_t dirEntryByte;
	uint16_t dirBlocklen;
	uint16_t freeDirEntryBlock;	// Free dir entry found
	uint16_t freeDirEntryByte;
	uint16_t freeFBlocks;		// Free dir entry file blocks size (mark for free dir entry found too)
	// directory entry
	uint16_t firstFreeBlock;	// for data
	char fName[10];				// file name
	char fRename[10];			// new file name for rename
	uint16_t fType;				// file type, hex value
	uint16_t fTypeMask;			// alternate format mask
	char fAttr;					// attributes
	char fAttrMask;				// same, for reset
	int fLength;				// file length
	int fBlocks;				// file size in blocks
	int cmdType;				// create / write / read /purge /ssek
	int pBlocks;				// processed blocks
	// what to do
	void (*hpil_processBuffer)(void);
	// more variables
	//char fattr[11];				// file attributes, type & flags, ascii form
	int jobDone;				// no need to seek directory again
	int differedError;			// differed read / write error status
	// variables
	int namedVar;
	il_vheader vheader;
	Regs r;
	int varIndex;
	int varCount;
	int varSubtype;				// re, im, string ?
	int index;					// linear index in matrix / buffer index when reading prgm
	// programms
	int trace;					// save trace mode printing flag
    int normal;					// save normal mode printing flag
	int saved_prgm;				// save current program
	//rename
	int renFound;				// just to flag 'file to rename was found'
};

MassStorage s;

/* buffers structs
 *
 */

extern AltBuf hpil_controllerAltBuf;
extern DataBuf hpil_controllerDataBuf;

static int hpil_newm_completion(int error);
static int hpil_massGenericReadWrite_completion(int error);

static void hpil_newmRefreshBuf(void);
static void hpil_createRefreshBuf(void);
static void hpil_wrtpRefreshBuf(void);
static void hpil_wrtrRefreshBuf(void);
static void hpil_readrRefreshBuf(void);
static void hpil_readpRefreshBuf(void);

int vHeaderChk(il_vheader *vHeader);
void var2reg(int index, char *buf);
void reg2var(int index, uint8_t *buf);

static int hpil_writeBuffer0_sub(int error);
static int hpil_write_sub(int error);
static int hpil_seek_sub(int error);
static int hpil_setBytePtr_sub(int error);
static int hpil_partialWrite_sub(int error);
static int hpil_close_sub(int error);

static int hpil_writeDirEntry_sub(int error);

static int hpil_readBuffer0_sub(int error);
static int hpil_readBufferx_sub(int error);
static int hpil_read_sub(int error);
static int hpil_getAddress_sub(int error);
static int hpil_getMaxAddress_sub(int error);

static int hpil_diskSelect_sub(int error);
static int hpil_getHeader_sub(int error);
static int hpil_dirHeader_sub(int error);
static int hpil_dirEntry_sub(int error);
static int hpil_dirFooter_sub(int error);

// size of objects
#define NumSize 0x10

/* raw phloat read / write translation
 * for binary & decimal versions
 *
 */
#ifdef BCD_MATH
void fetch_phloat(phloat *val, uint8_t *source) {
	memcpy(&val->val, source, sizeof(phloat));
}
void fetch_phloatOrString(phloat *val, uint8_t *source, char isString) {
	int i;
	if (isString) {
		phloat_length(*val) = source[6];
		for (i = 0; i < source[6]; i++) {
			phloat_text(*val)[i] = source[i];
		}
	}
	else {
		memcpy(&val->val, source, sizeof(phloat));
	}
}

void flush_phloat(uint8_t *target, phloat *val) {
	memcpy(target, &val->val, sizeof(phloat));
}

void flush_phloatOrString(uint8_t *target, phloat *val, char isString) {
	int i;
	if (isString) {
		target[6] = phloat_length(*val);
		for (i = 0; i < target[6]; i++) {
			target[i] = phloat_text(*val)[i];
		}
	}
	else {
		memcpy(target, &val->val, sizeof(phloat));
	}
}

#else
void fetch_phloat(phloat* val, uint8_t *source) {
	bid128_to_binary64(val, (BID_UINT128*)source);
}

void fetch_phloatOrString(phloat* val, uint8_t *source, char isString) {
	int i;
	if (isString) {
		phloat_length(*val) = source[6];
		for (i = 0; i < source[6]; i++) {
			phloat_text(*val)[i] = source[i];
		}
	}
	else {
		bid128_to_binary64(val, (BID_UINT128*)source);
	}
}

void flush_phloat(uint8_t *target, phloat* val) {
	binary64_to_bid128((BID_UINT128*)target, val);
}

void flush_phloatOrString(uint8_t *target, phloat* val, char isString) {
	int i;
	if (isString) {
		target[6] = phloat_length(*val);
		for (i = 0; i < phloat_length(*val); i++) {
			target[i] = phloat_text(*val)[i];
		}
	}
	else {
		binary64_to_bid128((BID_UINT128*)target, val);
	}
}

#endif

/* primary commands
 *
 */

int docmd_create(arg_struct *arg) {
	int i;
	int err;
	err = hpil_check();
	if (err != ERR_NONE) {
		return err;
	}
	if (reg_alpha_length < 1) {
		return ERR_ALPHA_DATA_IS_INVALID;
	}
	if (mappable_x_hpil(65535,&s.fLength) != ERR_NONE || s.fLength == 0) {
		err = ERR_INVALID_DATA;
	}
	s.fBlocks = ((BLOCK_SZ / REGS_SZ) - 1 + s.fLength)  / (BLOCK_SZ / REGS_SZ);
	for (i = 0; i < 10; i++) {
		if (i < reg_alpha_length) {
			s.fName[i] = reg_alpha[i];
		}
		else {
			s.fName[i] = ' ';
		}
	}
	s.hpil_processBuffer = hpil_createRefreshBuf;
	s.cmdType = TypeCreate;
	s.fType = 0xe0d0;
	s.fTypeMask = 0xffff;
	s.fAttr = 0x00;
	s.fAttrMask = 0xff;
	hpil_settings.disk = hpil_settings.selected;
	ILCMD_IDY(1);
	hpil_step = 0;
	hpil_completion = hpil_massGenericReadWrite_completion;
	mode_interruptible = hpil_worker;
	return ERR_INTERRUPTIBLE;
}

int docmd_dir(arg_struct *arg) {
	int err;
	err = hpil_check();
	if (err != ERR_NONE) {
		return err;
	}
	s.cmdType = 0x0000;
	s.fType = 0x0000;
	s.fTypeMask = 0x0000;
	s.fAttr = 0x00;	// do we need to care ?
	hpil_settings.disk = hpil_settings.selected;
	ILCMD_IDY(1);
	hpil_step = 0;
	hpil_completion = hpil_massGenericReadWrite_completion;
	mode_interruptible = hpil_worker;
	return ERR_INTERRUPTIBLE;
}

int docmd_newm(arg_struct *arg) {
	int i;
	int err;
	err = hpil_check();
	if (err != ERR_NONE) {
		return err;
	}
	if (arg->type != ARGTYPE_NUM) {
		return ERR_INVALID_TYPE;
	}
	s.dirBlocklen = (arg->val.num + 8) / 8;
	for (i = 0; i < 6; i++) {
		if (i < reg_alpha_length) {
			s.volume[i] = reg_alpha[i];
		}
		else {
			s.volume[i] = ' ';
		}
	}
	ILCMD_IDY(1);
	hpil_step = 0;
	hpil_completion = hpil_newm_completion;
	mode_interruptible = hpil_worker;
	return ERR_INTERRUPTIBLE;
}

int docmd_purge(arg_struct *arg) {
	int i;
	int err;
	err = hpil_check();
	if (err != ERR_NONE) {
		return err;
	}
	if (reg_alpha_length < 1) {
		return  ERR_ALPHA_DATA_IS_INVALID;
	}
	for (i = 0; i < 10; i++) {
		if (i < reg_alpha_length) {
			s.fName[i] = reg_alpha[i];
		}
		else {
			s.fName[i] = ' ';
		}
	}
	s.hpil_processBuffer = 0;
	s.cmdType = TypeUpdate | TypePurge;
	s.fType = 0x0000;
	s.fTypeMask = 0x0000;
	s.fAttr = 0x00;
	s.fAttrMask = 0xff;
	hpil_settings.disk = hpil_settings.selected;
	ILCMD_IDY(1);
	hpil_step = 0;
	hpil_completion = hpil_massGenericReadWrite_completion;
	mode_interruptible = hpil_worker;
	return ERR_INTERRUPTIBLE;
}

int docmd_readp(arg_struct *arg) {
	int i;
	int err;
	err = hpil_check();
	if (err != ERR_NONE) {
		return err;
	}
	if (reg_alpha_length < 1) {
		return ERR_ALPHA_DATA_IS_INVALID;
	}
	for (i = 0; i < 10; i++) {
		if (i < reg_alpha_length) {
			s.fName[i] = reg_alpha[i];
		}
		else {
			s.fName[i] = ' ';
		}
	}
	// set ftype & mask, cmd
	s.hpil_processBuffer = hpil_readpRefreshBuf;
	s.cmdType = TypeRead;
	s.fType = 0xe080;
	s.fTypeMask = 0xffff;
	// WHY ??? set_running(false);
	// Set print mode to MAN during the import, to prevent store_command()
	// from printing programs as they load, though vey usefull for troubleshooting !
	s.trace = flags.f.trace_print;
	s.normal = flags.f.normal_print;
	flags.f.trace_print = 0;
	flags.f.normal_print = 0;
	ILCMD_IDY(1);
	hpil_step = 0;
	hpil_completion = hpil_massGenericReadWrite_completion;
	mode_interruptible = hpil_worker;
	return ERR_INTERRUPTIBLE;
}

/* docmd_readr
 * input : A > data_filename
 * defaults : read whole real REGS from tape
 *				one il_vheader
 *				one rows * columns phloat array
 *				one rows * columns istring char array
 * input : A > data_filename.var_name
 */
int docmd_readr(arg_struct *arg) {
	int err;
	err = hpil_check();
	if (err != ERR_NONE) {
		return err;
	}
	// check alpha format
	err = hpil_splitAlphaReg(SPLIT_MODE_VAR);
	if (err != ERR_NONE) {
		return err;
	}
	// set ftype & mask, cmd
	s.hpil_processBuffer = hpil_readrRefreshBuf;
	s.cmdType = TypeRead;
	strncpy(s.fName, alphaSplit.str1, alphaSplit.len1);
	s.fType = 0xe0d8;
	s.fTypeMask = 0xfff0;
	s.namedVar = 0;
	// setup against input mode
	if (alphaSplit.len2 == 0) {
		// hp-41c readr style only, to REGS, all checks to be done later
		s.r.reg = recall_var("REGS", 4);
		if (s.r.reg == NULL) {
			return ERR_NONEXISTENT;
		}
		s.fType = 0xe0d0;
		s.fTypeMask = 0xffff;
	}
	else if ((alphaSplit.len2 != 1) || (alphaSplit.str2[0] != '*')) {
		s.r.reg = recall_var(alphaSplit.str2, alphaSplit.len2);
		s.namedVar = 1;
		strncpy(s.vheader.name, alphaSplit.str2, alphaSplit.len2);
		s.vheader.nameLen = alphaSplit.len2;
		s.fTypeMask = 0xfff0;
	}
	ILCMD_IDY(1);
	hpil_step = 0;
	hpil_completion = hpil_massGenericReadWrite_completion;
	mode_interruptible = hpil_worker;
	return ERR_INTERRUPTIBLE;
}

int docmd_rename(arg_struct *arg) {
	int err;
	err = hpil_check();
	if (err != ERR_NONE) {
		return err;
	}
	// check alpha format
	err = hpil_splitAlphaReg(SPLIT_MODE_VAR | SPLIT_MODE_PRGM);
	if (err != ERR_NONE) {
		return err;
	}
	if (alphaSplit.len1 < 1 || alphaSplit.len2 < 1) {
		return ERR_ALPHA_DATA_IS_INVALID;
	}
	s.hpil_processBuffer = 0;
	s.cmdType = TypeUpdate | TypeRename;
	s.fType = 0xe000;
	s.fTypeMask = 0xf000;
	s.fAttr = 0x00;
	s.fAttrMask = 0xff;
	strncpy(s.fName, alphaSplit.str1, 10);
	strncpy(s.fRename, alphaSplit.str2, 10);
	hpil_settings.disk = hpil_settings.selected;
	ILCMD_IDY(1);
	hpil_step = 0;
	hpil_completion = hpil_massGenericReadWrite_completion;
	mode_interruptible = hpil_worker;
	return ERR_INTERRUPTIBLE;
}

int docmd_sec(arg_struct *arg) {
	int i;
	int err;
	err = hpil_check();
	if (err != ERR_NONE) {
		return err;
	}
	if (reg_alpha_length < 1) {
		return ERR_ALPHA_DATA_IS_INVALID;
	}
	for (i = 0; i < 10; i++) {
		if (i < reg_alpha_length) {
			s.fName[i] = reg_alpha[i];
		}
		else {
			s.fName[i] = ' ';
		}
	}
	s.hpil_processBuffer = 0;
	s.cmdType = TypeUpdate;
	s.fType = 0xe000;
	s.fTypeMask = 0xf000;
	s.fAttr = FlagSecured;
	s.fAttrMask = ~FlagSecured;
	hpil_settings.disk = hpil_settings.selected;
	ILCMD_IDY(1);
	hpil_step = 0;
	hpil_completion = hpil_massGenericReadWrite_completion;
	mode_interruptible = hpil_worker;
    return ERR_INTERRUPTIBLE;
}

int docmd_unsec(arg_struct *arg) {
	int i;
	int err;
	err = hpil_check();
	if (err != ERR_NONE) {
		return err;
	}
	if (reg_alpha_length < 1) {
		return ERR_ALPHA_DATA_IS_INVALID;
	}
	for (i = 0; i < 10; i++) {
		if (i < reg_alpha_length) {
			s.fName[i] = reg_alpha[i];
		}
		else {
			s.fName[i] = ' ';
		}
	}
	s.hpil_processBuffer = 0;
	s.cmdType = TypeUpdate;
	s.fType = 0xE000;
	s.fTypeMask = 0xF000;
	s.fAttr = 0x00;
	s.fAttrMask = ~FlagSecured;
	hpil_settings.disk = hpil_settings.selected;
	ILCMD_IDY(1);
	hpil_step = 0;
	hpil_completion = hpil_massGenericReadWrite_completion;
	mode_interruptible = hpil_worker;
	return ERR_INTERRUPTIBLE;
}

int docmd_wrtp(arg_struct *arg) {
	int prgm;
	int4 pc;
	int i, j, err;
	arg_struct arg_s;

	err = hpil_check();
	if (err != ERR_NONE) {
		return err;
	}
	err = hpil_splitAlphaReg(SPLIT_MODE_PRGM);
	if (err != ERR_NONE) {
		return err;
	}
	s.saved_prgm = -1;
	if (alphaSplit.len1 != 0) {
		arg_s.type = TYPE_STRING;
		arg_s.length = (alphaSplit.len1 > 6) ? 6 : alphaSplit.len1;
		for (i = 0; i < arg_s.length; i++) {
			arg_s.val.text[i] = alphaSplit.str1[i];
			s.fName[i] = alphaSplit.str1[i];
		}
		for (i; i < 10; i++) {
			s.fName[i] = ' ';
		}
		if (find_global_label(&arg_s, &prgm, &pc)) {
			s.saved_prgm = current_prgm;
			current_prgm = prgm;
		}
		else {
			return ERR_LABEL_NOT_FOUND;
		}
	}
	if (alphaSplit.len2 != 0) {
		for (i = 0; i < alphaSplit.len2; i++) {
			s.fName[i] = alphaSplit.str2[i];
		}
	}
	else {
		if (alphaSplit.len1 == 0) {
			// no label, nor name, build name from first lable of current prgm
			err = ERR_LABEL_NOT_FOUND;
			i = 0;
			do {
				if (labels[i].prgm == current_prgm) {
					for (j = 0; j < labels[i].length; j++) {
						s.fName[j] = labels[i].name[j];
					}
					for (j; j < 10; j++) {
						s.fName[j] = ' ';
					}
					err = ERR_NONE;
				}
			} while (++i < labels_count && err != ERR_NONE);
		}
	}
	if (err != ERR_NONE) {
		return err;
	}
	s.hpil_processBuffer = hpil_wrtpRefreshBuf;
	s.cmdType = TypeCreate | TypeWrite;
	s.fType = 0xe080;
	s.fTypeMask = 0xffff;
	s.fAttr = (flags.f.auto_exec) ? 0x02 : 0x00;
	s.fAttrMask = 0xff;
	// core_program_size ignore (implicit ?) END...
	s.fLength = core_program_size(current_prgm) + 3;
	// take care of the extra crc byte !
	s.fBlocks = (BLOCK_SZ + s.fLength)  / BLOCK_SZ;
	// go
	ILCMD_IDY(1);
	hpil_step = 0;
	hpil_completion = hpil_massGenericReadWrite_completion;
	mode_interruptible = hpil_worker;
	return ERR_INTERRUPTIBLE;
}

/* docmd_wrtr
 * input : A > data_filename		
 *		save whole real unidimensional REGS storage in HP-41C existing 0xe0d0 type file
 * input : A > data_filename.var_name
 *		if file 0xe0d0 exists & real unidimensional & size Ok var save var in HP-41C 0xe0d0 type
 *		if file 0xe0d0 exists & real unidimensional Ko or size Ko, error
 *		if file 0xe0d8 exists & size Ok, save var in 0xe0d8 type
 *		if file 0xe0d8 exists & size Ko, delete file, create new & save
 *		if file does not exists, create 0xe0d8 type & write
 * input : A > data_filename.*
 *		if file 0xe0d0 exists - error
 *		if file 0xe0d8 exists & size Ok, save var in 0xe0d8 type
 *		if file 0xe0d8 exists & size Ko, delete file, create new & save
 *		if file does not exists, create 0xe0d8 type & write
 */
int docmd_wrtr(arg_struct *arg) {
	int i, j, err;
	Regs r;
	err = hpil_check();
	if (err != ERR_NONE) {
		return err;
	}
	// check alpha format
	err = hpil_splitAlphaReg(SPLIT_MODE_VAR);
	if (err != ERR_NONE) {
		return err;
	}
	if (vars_count == 0) {
		return ERR_NO_VARIABLES;
	}
	// preset ftype & mask, cmd & attributes
	s.hpil_processBuffer = hpil_wrtrRefreshBuf;
	s.cmdType = TypeCreate | TypeWrite;
	strncpy(s.fName, alphaSplit.str1, alphaSplit.len1);
	s.fType = 0xe0d8;
	s.fTypeMask = 0xffff;
	s.fAttr = 0x00;
	s.fAttrMask = 0xff;
	s.fLength = 0;
	// setup against input mode
	if (alphaSplit.len2 == 0) {
		// hp-41c wrtr style only, recover 'REGS' matrix
		s.varIndex = lookup_var("REGS", 4);
		s.varCount = 1;
		s.r.reg = recall_var("REGS", 4);
		if (s.r.reg == NULL) {
			err = ERR_NONEXISTENT;
		}
		else if (s.r.reg->type == TYPE_REALMATRIX) {
			if ((s.r.rm->rows == 1) || (s.r.rm->columns == 1)) {
				s.cmdType = TypeWrite | TypePartial;
				s.fType = 0xe0d0;
				s.vheader.remaining = 1;
			}
			else {
				err = ERR_DIMENSION_ERROR;
			}
		}
		else {
			err = ERR_INVALID_TYPE;
		}
	}
	else if (alphaSplit.str2[0] != '*') {
		// single variable, may enable hp-41c style on unidimensional real matrix
		s.varIndex = lookup_var(alphaSplit.str2, alphaSplit.len2);
		s.varCount = 1;
		r.reg = recall_var(alphaSplit.str2, alphaSplit.len2);
		if (r.reg == NULL) {
			err = ERR_NONEXISTENT;
		}
		else if (r.reg->type == TYPE_REALMATRIX) {
			if ((r.rm->rows == 1) || (r.rm->columns == 1)) {
				s.r.reg = r.reg;
				s.fTypeMask = 0xfff0;
			}
		}
	}
	else {
		// all variables, only free42 format / point to first variable
		s.varIndex = 0;
		s.varCount = vars_count;
		s.fType = 0xe0dc;
	}
	if (err == ERR_NONE) {
		// calculate file size
		j = s.varIndex;
		for (i = 0; i < s.varCount; i++) {
			r.reg = vars[j++].value;
			switch (r.reg->type) {
				case TYPE_NULL :
					break;
				case TYPE_REAL :
					s.fLength += NumSize;
					break;
				case TYPE_COMPLEX :
					s.fLength += 2 * NumSize;
					break;
				case TYPE_REALMATRIX :
					// store along value and string flag
					s.fLength += (NumSize * (r.rm->rows * r.rm->columns)) + (((r.rm->rows * r.rm->columns) + 0x000f) & ~0x000f);
					break;
				case TYPE_COMPLEXMATRIX :
					s.fLength += (2 * NumSize * r.cm->rows * r.cm->columns);
					break;
				case TYPE_STRING :
					s.fLength += NumSize;
					break;
				default :
					return ERR_INTERNAL_ERROR;
			}
			s.fLength += sizeof(il_vheader);
		}
		s.fLength = (s.fLength + REGS_SZ - 1) / REGS_SZ;
		s.fBlocks = ((BLOCK_SZ / REGS_SZ) - 1 + s.fLength)  / (BLOCK_SZ / REGS_SZ);
		// go
		ILCMD_IDY(1);
		hpil_step = 0;
		hpil_completion = hpil_massGenericReadWrite_completion;
		mode_interruptible = hpil_worker;
		err = ERR_INTERRUPTIBLE;
	}
	return err;
}

int docmd_zero(arg_struct *arg) {
	int i;
	int err;
	err = hpil_check();
	if (err != ERR_NONE) {
		return err;
	}
	if (reg_alpha_length < 1) {
		return ERR_ALPHA_DATA_IS_INVALID;
	}
	for (i = 0; i < 10; i++) {
		if (i < reg_alpha_length) {
			s.fName[i] = reg_alpha[i];
		}
		else {
			s.fName[i] = ' ';
		}
	}
	s.hpil_processBuffer = hpil_createRefreshBuf;
	s.cmdType = TypeWrite | TypeZero;
	s.fType = 0xe0d0;
	s.fTypeMask = 0xffff;
	s.fAttr = 0x00;
	s.fAttrMask = 0xff;
	hpil_settings.disk = hpil_settings.selected;
	ILCMD_IDY(1);
	hpil_step = 0;
	hpil_completion = hpil_massGenericReadWrite_completion;
	mode_interruptible = hpil_worker;
	return ERR_INTERRUPTIBLE;
}

/* cooperative commands runtime
 *
 */

static int hpil_newm_completion(int error) {
	if (error == ERR_NONE) {
		error = ERR_INTERRUPTIBLE;
		switch (hpil_step) {
			case 0 :		// > select device
				s.dirEntryBlock = 0;
				s.dirEntryByte = 0;
				hpilXCore.buf = hpil_controllerAltBuf.data;
				hpilXCore.bufSize = ControllerAltBufSize;
				ILCMD_nop;;
				hpil_step++;
				error = call_ilCompletion(hpil_diskSelect_sub);
				break;
			case 1 :		// > get capacity
				ILCMD_TAD(hpil_settings.disk);
				hpil_step++;
				error = call_ilCompletion(hpil_getMaxAddress_sub);
				break;
			case 2 :		// > check allocated blocks vs capacity
				if (hpilXCore.bufPtr != 2) {
					s.recordsMax = 0x01ff;		// assume 82161A tape drive...
				}
				else {
					s.recordsMax = (hpil_controllerAltBuf.data[0] << 8) + hpil_controllerAltBuf.data[1];
				}

				if (s.dirBlocklen > (s.recordsMax / 10)) {
					error = ERR_INVALID_DATA;
				}
				else {
					ILCMD_LAD(hpil_settings.disk);
					hpil_step++;
				}
				break;
			case 3 :		// > format
				ILCMD_DDL(0x05);
				hpil_step++;
				break;
			case 4 :		//  > and wait till end
				ILCMD_TAD(hpil_settings.disk);
				hpil_step++;
				error = call_ilCompletion(hpil_wait_sub);
				break;
			case 5 :		// > seek to start
				hpil_controllerAltBuf.data[0] = 0;
				hpil_controllerAltBuf.data[1] = 0;
				ILCMD_LAD(hpil_settings.disk);
				hpil_step++;
				error = call_ilCompletion(hpil_seek_sub);
				break;
			case 6 :		// > write
				ILCMD_LAD(hpil_settings.disk);
				hpil_step++;
				error = call_ilCompletion(hpil_write_sub);
				break;
			case 7 :		// > prepare header and write buffer 0
				hpilXCore.buf = hpil_controllerDataBuf.data;
				hpilXCore.bufSize = ControllerDataBufSize;
				hpilXCore.bufPtr = 0;
				hpilXCore.statusFlags |= (LastIsEndTalkBuf | RunAgainTalkBuf);
				s.pBlocks = 0;
				hpil_fillTalkBuffer = hpil_newmRefreshBuf;
				hpil_newmRefreshBuf();
				ILCMD_nop;
				hpil_step++;
				error = call_ilCompletion(hpil_writeBuffer0_sub);
				break;
			case 8 :		// check final status
				ILCMD_TAD(hpil_settings.disk);
				hpil_step++;
				error = call_ilCompletion(hpil_wait_sub);
				break;
			default :
				error = ERR_NONE;
		}
	}
	return error;
}

/*
 * mimics hp-41c, no free space grouping, write when full erase file... 
 * but use only one device, I like to know where things are !
 * keep as straightforward as possible...
 */
static int hpil_massGenericReadWrite_completion(int error) {
	int i;
	int ftype, flength;
	if (error == ERR_NONE) {
		error = ERR_INTERRUPTIBLE;
		switch (hpil_step) {
			case 0 :	// > control device's Aid, read and check lif header
				s.freeFBlocks = 0;
				s.jobDone = false;
				s.renFound = false;
				hpilXCore.buf = hpil_controllerAltBuf.data;
				ILCMD_nop;
				if (s.cmdType == 0) {					// dir command
					hpil_step ++;
				}
				else {									// other command, just prepare to read first dir entry
					hpil_step = 2;
				}
				error = call_ilCompletion(hpil_getHeader_sub);
				break;
			case 1 :	// dir command, print header
				ILCMD_nop;
				hpil_step++;
				error = call_ilCompletion(hpil_dirHeader_sub);
				break;
			case 2 :	// read dir entry
				hpilXCore.buf = hpil_controllerAltBuf.data;
				hpilXCore.bufSize = 32;
				ILCMD_TAD(hpil_settings.disk);
				hpil_step++;
				error = call_ilCompletion(hpil_readBuffer0_sub);
				break;
			case 3 :	// main loop, crawl through dir entries
				ILCMD_nop;
				ftype = _byteswap_ushort(hpil_controllerAltBuf.dir.fType);
				switch (ftype) {	
					case 0x0000 :	// erased - check if available entry
						hpil_step--;
						if (s.cmdType != 0) {
							s.dirEntriesLeft--;
							if ((s.cmdType & TypeCreate) && (s.freeFBlocks == 0)) {
								// create mode & no entry found yet, check this
								if ((uint16_t)_byteswap_ulong(hpil_controllerAltBuf.dir.fBlocks) >= s.fBlocks) {
									s.firstFreeBlock = (uint16_t)_byteswap_ulong(hpil_controllerAltBuf.dir.fBStart);
									s.freeFBlocks = _byteswap_ulong(hpil_controllerAltBuf.dir.fBlocks);
									hpil_step = 4;
								}
								else {
									s.firstFreeBlock = (uint16_t)(_byteswap_ulong(hpil_controllerAltBuf.dir.fBStart) + _byteswap_ulong(hpil_controllerAltBuf.dir.fBlocks));
								}
							}
						}
						break;
					case 0xffff :	// last entry encountered
						if (s.cmdType == 0) {
							hpil_step = -1;
							error = call_ilCompletion(hpil_dirFooter_sub);
						}
						else {
							hpil_step = 8;
						}
						break;
					default :		// regular file
						s.dirEntriesLeft--;
						s.recordsLeft -= (uint16_t)_byteswap_ulong(hpil_controllerAltBuf.dir.fBlocks);
						if (s.cmdType == 0) {
							// dir command
							hpil_step--;
							error = call_ilCompletion(hpil_dirEntry_sub);
						}
						else {
							if (s.freeFBlocks == 0) {
								// update anyway, should we correct it after
								s.firstFreeBlock = (uint16_t)(_byteswap_ulong(hpil_controllerAltBuf.dir.fBStart) + _byteswap_ulong(hpil_controllerAltBuf.dir.fBlocks));
							}
							if ((s.cmdType & TypeRename) && (strncmp(hpil_controllerAltBuf.dir.fName, s.fRename, 10) == 0)) {
								error = ERR_FILE_DUP;
							}
							else if (strncmp(hpil_controllerAltBuf.dir.fName, s.fName, 10) == 0) {
								hpil_step = 6;
							}
							else {
								hpil_step--;
							}
						}
				}
				break;
			case 4 :	// empty entry found, get own address
				ILCMD_TAD(hpil_settings.disk);
				hpil_step++;
				error = call_ilCompletion(hpil_getAddress_sub);
				break;
			case 5 :	// and loop back
				s.freeDirEntryBlock = (hpil_controllerAltBuf.data[0] << 8) + hpil_controllerAltBuf.data[1];
				s.freeDirEntryByte = hpil_controllerAltBuf.data[2];
				ILCMD_nop;
				hpil_step = 2;
				break;
			case 6 :	// file name is Ok, check type, attributes & size
				ftype = _byteswap_ushort(hpil_controllerAltBuf.dir.fType);
				flength = (uint16_t)(_byteswap_ushort(hpil_controllerAltBuf.dir.fLength));

				if ((ftype & s.fTypeMask) != (s.fType & s.fTypeMask)) {
					return ERR_FILE_BAD_TYPE;						// same filename but bad type	
				}
				if (s.cmdType == TypeCreate) {
					return ERR_FILE_DUP;
				}

				if (s.cmdType & TypeRead) {							// read or secured ?
					s.fType = ftype;								// get ftype, fattr, flength & firstfreeblock
					s.fAttr = hpil_controllerAltBuf.dir.flags[0];
					s.fLength = flength;
					s.firstFreeBlock = (uint16_t)(_byteswap_ulong(hpil_controllerAltBuf.dir.fBStart));
					ILCMD_NOP;
					hpil_step = 30;
				}
				else if ((hpil_controllerAltBuf.dir.flags[0] & FlagSecured) && (s.fAttrMask != ~FlagSecured)) {
					return ERR_FILE_SECURED;					// secured
				}

				for (i = 0; i < ControllerAltBufSize; i++) {		// save entry for WriteDirEntry
					hpil_controllerDataBuf.data[i] = hpil_controllerAltBuf.data[i];
				}
				s.fType = ftype;

				if (s.cmdType & TypePurge) {							// 'purge, set ftype to 0
					hpil_controllerDataBuf.dir.fType = 0;
				}
				else if (s.cmdType & TypeRename) {						// 'rename, overwrite file name
					strncpy(hpil_controllerDataBuf.dir.fName, s.fRename, 10);
					s.renFound = true;
				}
				else if (s.cmdType & TypeZero) {					// 'zero, get length & blocks
					s.fLength = flength;
					s.fBlocks = (uint16_t)(_byteswap_ushort(hpil_controllerAltBuf.dir.fBlocks));
				}
				else if ((s.cmdType & TypeWrite) && (s.fType == 0xe0d0)) {
					s.fLength = s.r.rm->rows * s.r.rm->columns;
					s.fBlocks = ((BLOCK_SZ / REGS_SZ) - 1 + s.fLength) / (BLOCK_SZ / REGS_SZ);
					if (s.fLength > flength) {
						return ERR_FILE_EOF;				// hp-41c format
					}
					else {
						s.fLength = flength;
						s.fBlocks = (uint16_t)(_byteswap_ushort(hpil_controllerAltBuf.dir.fBlocks));
					}
				}

				if (s.cmdType & TypeUpdate) {				// update dir entry
					hpil_controllerDataBuf.dir.flags[0] = (hpil_controllerDataBuf.dir.flags[0] & s.fAttrMask) | s.fAttr;
					hpil_step = 7;
					ILCMD_TAD(hpil_settings.disk);
					error = call_ilCompletion(hpil_getAddress_sub);
				}
				else if (s.cmdType & TypeWrite) {			// 'writeX
					if (s.fLength == flength) {				// no need to update, go write
						s.firstFreeBlock = (uint16_t)(_byteswap_ulong(hpil_controllerAltBuf.dir.fBStart));
						hpil_step = 13;
						ILCMD_NOP;
					}
					else {
						if ((uint16_t)_byteswap_ulong(hpil_controllerAltBuf.dir.fBlocks) < s.fBlocks) {
							hpil_controllerDataBuf.dir.fType = 0x0000;
						}
						else {									// just update length
							hpil_controllerDataBuf.dir.fLength = _byteswap_ushort(s.fLength);
							s.firstFreeBlock = (uint16_t)(_byteswap_ulong(hpil_controllerAltBuf.dir.fBStart));
							s.jobDone = true;
						}
							hpil_step = 7;
							ILCMD_TAD(hpil_settings.disk);
							error = call_ilCompletion(hpil_getAddress_sub);
					}
				}
				break;
			case 7 :	// recover dir entry
				s.dirEntryBlock = (hpil_controllerAltBuf.data[0] << 8) + hpil_controllerAltBuf.data[1];
				s.dirEntryByte = hpil_controllerAltBuf.data[2];
				ILCMD_nop;
				if (s.cmdType & TypeRename){
					hpil_step = 2;		// loop back, check for dup
				}
				else {
					hpil_step = 11;
				}
				break;
			case 8 :	// end of directory entry
				if (s.renFound) {
					ILCMD_nop;
					hpil_step = 11;
				}
				else if ((s.cmdType & TypeRead) || (s.cmdType & TypeUpdate) || ((s.cmdType & TypeWrite ) && (s.fType == 0xe0d0))) {
					return ERR_FILE_NOT_FOUND;
				}
				else if ((s.freeFBlocks == 0) && (s.dirEntriesLeft != 0)) {
					if ((uint16_t)(s.firstFreeBlock + s.fBlocks) >= s.recordsMax) {
						return ERR_MEDIA_FULL;
					}
					else {
						ILCMD_TAD(hpil_settings.disk);
						hpil_step++;
						error = call_ilCompletion(hpil_getAddress_sub);
					}
				}
				else if (s.freeFBlocks != 0) {
					s.dirEntryBlock = s.freeDirEntryBlock;
					s.dirEntryByte = s.freeDirEntryByte;
					s.fBlocks = s.freeFBlocks;
					ILCMD_nop;
					hpil_step = 10;	// dir entry is already known
				}
				else {
					return ERR_DIR_FULL;
				}
				s.jobDone = true;
				break;
			case 9 :	// recover dir entry
				s.dirEntryBlock = (hpil_controllerAltBuf.data[0] << 8) + hpil_controllerAltBuf.data[1];
				s.dirEntryByte = hpil_controllerAltBuf.data[2];
				hpil_step++;
			case 10 :	// create dir entry
				strncpy(hpil_controllerDataBuf.dir.fName,s.fName,10);
				hpil_controllerDataBuf.dir.fType = _byteswap_ushort(s.fType);
				hpil_controllerDataBuf.dir.fBStart = _byteswap_ulong(s.firstFreeBlock);
				hpil_controllerDataBuf.dir.fBlocks = _byteswap_ulong(s.fBlocks);
				for (i = 0; i < 6; i++) {
					hpil_controllerDataBuf.dir.dateTime[i] = 0;
				}
				hpil_controllerDataBuf.dir.pad[0] = 0x80;
				hpil_controllerDataBuf.dir.pad[1] = 0x01;
				hpil_controllerDataBuf.dir.fLength = _byteswap_ushort(s.fLength);
				hpil_controllerDataBuf.dir.flags[0] = s.fAttr;
				hpil_controllerDataBuf.dir.flags[1] = 0x20;
				hpil_step++;
			case 11 :	// write dir entry
				ILCMD_nop;
				hpil_step++;
				error = call_ilCompletion(hpil_writeDirEntry_sub);
				break;
			case 12 :	// > loop / quit / continue ?
				ILCMD_nop;
				if ((s.cmdType & (TypeCreate | TypeWrite | TypeRead)) == 0) {
					hpil_step = -1;								// no create, write or read, only dir entry updated
				}
				else if (s.jobDone) {
					hpil_step++;
				}
				else {
					hpil_step = 2;	// need to continue dir walk
				}
				break;
			case 13 :		// > seek to first free file block
				hpil_controllerAltBuf.data[0] = s.firstFreeBlock << 8;
				hpil_controllerAltBuf.data[1] = s.firstFreeBlock & 0x00ff;
				ILCMD_LAD(hpil_settings.disk);
				hpil_step++;
				error = call_ilCompletion(hpil_seek_sub);
				break;
			case 14 :		// > write
				ILCMD_LAD(hpil_settings.disk);
				hpil_step++;
				if (!(s.cmdType & TypePartial)) {
					// step over
					hpil_step++;
				}
				error = call_ilCompletion(hpil_write_sub);
				break;
			case 15 :		// > write / partial
				ILCMD_LAD(hpil_settings.disk);
				hpil_step++;
				error = call_ilCompletion(hpil_partialWrite_sub);
				break;
			case 16 :		// > prepare file generation and write
				hpilXCore.buf = hpil_controllerDataBuf.data;
				hpilXCore.bufSize = ControllerDataBufSize;
				hpilXCore.bufPtr = 0;
				hpilXCore.statusFlags |= (LastIsEndTalkBuf | RunAgainTalkBuf);
				s.pBlocks = 0;
				s.differedError = ERR_NONE;
				hpil_fillTalkBuffer = s.hpil_processBuffer;
				if (s.hpil_processBuffer) {
					s.hpil_processBuffer();
				}
				ILCMD_nop;
				hpil_step++;
				if (!(s.cmdType & TypeClose)) {
					// step over
					hpil_step++;
				}
				error = call_ilCompletion(hpil_writeBuffer0_sub);
				break;
			case 17 :		// > close record
				ILCMD_LAD(hpil_settings.disk);
				hpil_step++;
				error = call_ilCompletion(hpil_close_sub);
				break;
			case 18 :		// > check status and exit
				ILCMD_TAD(hpil_settings.disk);
				hpil_step++;
				error = call_ilCompletion(hpil_wait_sub);
				break;
			case 19 :
				hpil_step = -1;
				error = s.differedError;
				break;

			case 30:		// > Check if dest var is Ok and Seek to start of data bloc
				if ((s.fType != 0xe0d0)
					|| ((s.fType == 0xe0d0)
					&& (s.r.reg != NULL)
					&& (s.r.reg->type == TYPE_REALMATRIX)
					&& ((s.r.rm->rows == 1) || (s.r.rm->columns == 1)))) {
					hpil_controllerAltBuf.data[0] = s.firstFreeBlock << 8;
					hpil_controllerAltBuf.data[1] = s.firstFreeBlock & 0x00ff;
					ILCMD_LAD(hpil_settings.disk);
					hpil_step++;
					error = call_ilCompletion(hpil_seek_sub);
				}
				else {
					if (s.r.reg == NULL) {
						error = ERR_NONEXISTENT;
					}
					else if (s.r.reg->type != TYPE_REALMATRIX) {
						error = ERR_INVALID_TYPE;
					}
					else {
						error = ERR_DIMENSION_ERROR;
					}
				}
				break;
			case 31 :		// > read
				ILCMD_TAD(hpil_settings.disk);
				hpil_step++;
				error = call_ilCompletion(hpil_read_sub);
				break;
			case 32 :		// > prepare buffer and read data
				hpilXCore.buf = hpil_controllerDataBuf.data;
				hpilXCore.statusFlags |= RunAgainListenBuf;
				if (s.fType == 0xe080) {
					if ((s.fLength + 1) > 0x100) {
						hpilXCore.bufSize = 0x100;
					}
					else {
						hpilXCore.bufSize = s.fLength + 1;
						hpilXCore.statusFlags &= ~RunAgainListenBuf;
					}
				}
				else if (s.fType == 0xe0d0) {
					hpilXCore.bufSize = REGS_SZ;
					if (s.fLength == 1) {
						hpilXCore.statusFlags &= ~RunAgainListenBuf;
					}
				}
				else {
					hpilXCore.bufSize = sizeof(il_vheader);
				}
				s.pBlocks = 0;
				s.differedError = ERR_NONE;
				if (s.hpil_processBuffer) {
					hpil_emptyListenBuffer = s.hpil_processBuffer;
				}
				ILCMD_TAD(hpil_settings.disk);
				hpil_step++;
				error = call_ilCompletion(hpil_readBufferx_sub);
				break;
			case 33 :
				error = s.differedError;
 				if (error == ERR_NONE && s.fType == 0xe080 && s.fAttr & 0x02 && !mode_running) {
					error = ERR_RUN;
		            pending_command = CMD_NONE;
				}
				break;
			default :
				error = ERR_NONE;
		}
	}
	return error;
}

/* newmRefreshBuf
 *
 */
static void hpil_newmRefreshBuf(void) {
	int i;
	for (i = 0; i < hpilXCore.bufSize; i++) {
		if (s.pBlocks == 0) {
			switch (i) {
				case 0 :
					hpilXCore.buf[i] = 0x80;
					break;
				case 2 :
				case 3 :
				case 4 :
				case 5 :
				case 6 :
				case 7 :
					hpilXCore.buf[i] = s.volume[i - 2];
					break;
				case 11 :
					hpilXCore.buf[i] = 0x02;
					break;
				case 18 :
					hpilXCore.buf[i] = s.dirBlocklen >> 8;
					break;
				case 19 :
					hpilXCore.buf[i] = s.dirBlocklen & 0x00ff;
					break;
				default :
					hpilXCore.buf[i] = 0;
			}
		}
		else {
			hpilXCore.buf[i] = 0;
		}
	}
	hpilXCore.bufSize = BLOCK_SZ;
	if (++s.pBlocks >= 2) {
		hpilXCore.statusFlags &= ~RunAgainTalkBuf;
	}
}

/* createRefreshBuf
 *
 */
static void hpil_createRefreshBuf(void) {
	int i;
	if (s.pBlocks++ == 0) {
		for (i = 0; i < hpilXCore.bufSize; i++) {
			hpilXCore.buf[i] = 0;
		}
	}
	hpilXCore.bufSize = BLOCK_SZ;
	if (s.pBlocks >= s.fBlocks) {
		hpilXCore.statusFlags &= ~RunAgainTalkBuf;
	}
}

/* wrtpRefreshBuf
 *
 * build write buffer to save programs.
 */
static void hpil_wrtpRefreshBuf(void) {
	int i, done;
	static char crc;
	static int4 pc;
	if (s.pBlocks++ == 0) {
		crc = 0;
		pc = 0;
	}
	i = 0;
	do {
		done = core_Free42To42(&pc, hpilXCore.buf, &i);
	} while (i < 0xcd && !done);		// keep enough place for max cmdbuf !
	hpilXCore.bufSize = i;
	i--;
	for (i; i >= 0; i--) {
		crc += hpilXCore.buf[i];
	}
	if (done) {
		if (s.saved_prgm != -1) {
			current_prgm = s.saved_prgm;
		}
		hpilXCore.buf[hpilXCore.bufSize++] = crc;
		hpilXCore.statusFlags &= ~RunAgainTalkBuf;
	}
}

/* wrtrRefreshBuf
 *
 * build write buffer chunks by chunks and update for next buffer
 * real matrix, isString handled using a 16 byte isString preface every 16 cells
 */
static void hpil_wrtrRefreshBuf(void) {
	static int step;
	int error = ERR_NONE, i = 0, j, nextSize = 0;
	bool loopAgain = true;

	if (s.pBlocks++ == 0) {
		if (s.fType == 0xe0d0 ) {
			s.r.reg = vars[s.varIndex].value;
			s.index = 0;
			s.varCount = s.r.rm->columns * s.r.rm->rows;
			step = 2;
		}
		else {
			step = 0;
		}
	}
	do {
		switch (step) {
			case 0:
				// build & write vheader if needed
				j = s.varIndex++;
				s.vheader.nameLen = vars[j].length;
				strncpy(s.vheader.name, vars[j].name, vars[j].length);
				s.vheader.type = vars[j].value->type;
				s.r.reg = vars[j].value;
				s.vheader.remaining = s.varCount--;
				switch (s.r.reg->type) {
					case TYPE_NULL :
						break;
					case TYPE_STRING :
					case TYPE_REAL :
						break;
					case TYPE_COMPLEX :
						break;
					case TYPE_REALMATRIX :
						s.vheader.rows = s.r.rm->rows;
						s.vheader.columns = s.r.rm->columns;
						break;
					case TYPE_COMPLEXMATRIX :
						s.vheader.rows = s.r.cm->rows;
						s.vheader.columns = s.r.cm->columns;
					break;
				}
				if (s.r.reg->type != TYPE_NULL) {
					s.varSubtype = ITEM_FIRST;			// for complex, real & complex matrix
					s.index = 0;						// for matrix
					memcpy(&hpilXCore.buf[i], &s.vheader, sizeof(il_vheader));
					i += sizeof(il_vheader);
					step = 1;
				}
				break;
			case 1 :
				switch (s.r.reg->type) {
					case TYPE_STRING :
						hpilXCore.buf[i++] = (unsigned char)s.r.s->length;
						memcpy(&hpilXCore.buf[i], s.r.s->text, 6);
						i += 7;
						step = 0;
						break;
					case TYPE_REAL :
						flush_phloat(&hpilXCore.buf[i], &s.r.r->x);
						i += NumSize;
						step = 0;
						break;
					case TYPE_COMPLEX :
						if (s.varSubtype == ITEM_FIRST) {
							flush_phloat(&hpilXCore.buf[i], &s.r.c->re);
							i += NumSize;;
							s.varSubtype = ITEM_SECOND;
						}
						else {
							flush_phloat(&hpilXCore.buf[i], &s.r.c->im);
							i += NumSize;
							step = 0;
						}
						break;
					case TYPE_REALMATRIX :
						if (s.varSubtype == ITEM_FIRST) {
							hpilXCore.buf[i++] = s.r.rm->array->is_string[s.index++];
							if (!(s.index & 0x0f)) {
								s.index -= 0x10;
								s.varSubtype = ITEM_SECOND;
							}
							else if (s.index >= (s.r.rm->rows * s.r.rm->columns)) {
								s.varSubtype = ITEM_PAD;
							}
						}
						else if (s.varSubtype == ITEM_PAD) {
							if (s.index & 0x0f) {
								s.index++;
								hpilXCore.buf[i++] = 0;
							}
							else {
								s.index -= 0x10;
								s.varSubtype = ITEM_SECOND;
							}
						}
						else {
							flush_phloatOrString(&hpilXCore.buf[i], &(s.r.rm->array->data[s.index]), s.r.rm->array->is_string[s.index]);
							i += NumSize;
							s.index++;
							if (s.index >= (s.r.rm->rows * s.r.rm->columns)) {
								step = 0;
							}
							else if (!(s.index & 0x0f)) {
								s.varSubtype = ITEM_FIRST;
							}
						}
						break;
					case TYPE_COMPLEXMATRIX :
						flush_phloat(&hpilXCore.buf[i], &(s.r.rm->array->data[s.index++]));
						i += NumSize;
						if (s.index >= (s.r.rm->rows * s.r.rm->columns * 2)) {
							step = 0;
						}
						break;
				}

				break;
			case 2 :
				var2reg(s.index++, (char*)&hpilXCore.buf[i]);
				i += 8;
				s.fLength--;
				s.varCount--;
				if ((s.fLength <= 0 ) || (s.varCount <= 0)) {
					loopAgain = false;
					hpilXCore.statusFlags &= ~RunAgainTalkBuf;
				}
				break;
		}

		// testing for last entry
		if ((s.varCount == 0) && (step == 0)) {
			loopAgain = false;
			hpilXCore.statusFlags &= ~RunAgainTalkBuf;
		}
	} while ((i < (ControllerDataBufSize - NumSize + 1)) && (error == ERR_NONE) && loopAgain);

	// somethings was wrong
	if (error != ERR_NONE) {
		hpilXCore.bufSize = 0;
		hpilXCore.statusFlags &= ~RunAgainTalkBuf;
	}
	hpilXCore.bufSize = i;
	s.differedError = error;
}

/* readrRefreshBuf
 *
 * process read buffer chunks by chunks and update for next buffer
 * real matrix, isString handled using a 16 byte isString preface every 16 cells
 */
static void hpil_readrRefreshBuf(void) {
	static int step;
	int error = ERR_NONE, i = 0, nextSize = NumSize;

	if (s.pBlocks++ == 0) {
		if (s.fType == 0xe0d0 ) {
			s.index = 0;
			s.varCount = s.r.rm->columns * s.r.rm->rows;
			step = 2;
		}
		else {
			step = 0;
		}
	}

	// cleanup after ultimate run
	if (!(hpilXCore.statusFlags & RunAgainListenBuf)) {
		hpil_emptyListenBuffer = NULL;
	}
	
	while ((error == ERR_NONE) && (i < hpilXCore.bufSize)) {
		switch (step) {
			case 0 :
				if (s.namedVar) {
					if (s.fType == 0xe0d8) {
						// fake name & remaining entry to read current var to named var.
						strncpy((char*)&hpilXCore.buf[i], &s.vheader.nameLen, 8);
						s.namedVar = false;		// and loop again...
					}
					else if((s.fType == 0xe0dc) &&
						(hpilXCore.buf[i] == s.vheader.nameLen) &&
						(strncmp((const char*)&hpilXCore.buf[i+1], s.vheader.name, s.vheader.nameLen) == 0)) {
						// read this and only var.
						hpilXCore.buf[i+14] = 1;
						hpilXCore.buf[i+15] = 0;
						s.namedVar = false;		// and loop again...
					}
					else {
						// fake type, dims, remaining
						memcpy((char*)&s.vheader.type, (char*)&hpilXCore.buf[i+8], 8);
						i += sizeof(il_vheader);
						s.varSubtype = ITEM_FIRST;				// for complex matrix
						s.index = 0;							// for matrix
						s.varCount = s.vheader.remaining - 1;
						if (s.vheader.type == TYPE_STRING) {
							nextSize = 8;
						}
						step = 3;
					}
				}
				else {
					error = vHeaderChk((il_vheader*)&hpilXCore.buf[i]);
					if (error == ERR_NONE) {
						s.varSubtype = ITEM_FIRST;				// for complex matrix
						s.index = 0;							// for matrix
						s.varCount = ((il_vheader*)&hpilXCore.buf[i])->remaining - 1;
						if (s.r.reg->type == TYPE_STRING) {
							nextSize = 8;
						}
						i += sizeof(il_vheader);
						step = 1;
					}
				}
				break;
			case 1 :
				switch (s.r.reg->type) {
					case TYPE_STRING :
						s.r.s->length = hpilXCore.buf[i++];
						memcpy(s.r.s->text, &hpilXCore.buf[i], 7);
						i += 7;
						step = 0;
						break;
					case TYPE_REAL :
						fetch_phloat(&s.r.r->x, &hpilXCore.buf[i]);
						i += NumSize;
						step = 0;
						break;
					case TYPE_COMPLEX :
						if (s.varSubtype == ITEM_FIRST) {
							fetch_phloat(&s.r.c->re, &hpilXCore.buf[i]);
							s.varSubtype = ITEM_SECOND;
						}
						else {
							fetch_phloat(&s.r.c->im, &hpilXCore.buf[i]);
							step = 0;
						}
						i += NumSize;
						break;
					case TYPE_REALMATRIX :
						if (s.varSubtype == ITEM_FIRST) {
							s.r.rm->array->is_string[s.index++] = hpilXCore.buf[i++];
							if (!(s.index & 0x0f)) {
								s.index -= 0x10;
								s.varSubtype = ITEM_SECOND;
							}
							else if (s.index >= (s.r.rm->rows * s.r.rm->columns)) {
								s.varSubtype = ITEM_PAD;
							}
						}
						else if (s.varSubtype == ITEM_PAD) {
							s.index++;
							i++;
							if (!(s.index & 0x0f)) {
								s.index -= 0x10;
								s.varSubtype = ITEM_SECOND;
							}
						}
						else {
							fetch_phloatOrString(&s.r.rm->array->data[s.index], &hpilXCore.buf[i], s.r.rm->array->is_string[s.index]);
							i += NumSize;
							s.index ++;
							if (s.index >= (s.r.rm->rows * s.r.rm->columns)) {
								step = 0;
							}
							else if (!(s.index & 0x0f)) {
								s.varSubtype = ITEM_FIRST;
							}
						}
						break;
					case TYPE_COMPLEXMATRIX :
						fetch_phloat(&s.r.cm->array->data[s.index++], &hpilXCore.buf[i]);
						i += NumSize;
						if (s.index >= (s.r.rm->rows * s.r.rm->columns * 2)) {
							step = 0;
						}
						break;
				}
				break;
			case 2 :
				reg2var(s.index++, &hpilXCore.buf[i]);
				i += REGS_SZ;
				nextSize = 8;
				s.fLength--;
				s.varCount--;
				if ((s.fLength <= 1) || (s.varCount <= 1)) {
					hpilXCore.statusFlags &= ~RunAgainListenBuf;
				}
				break;
			case 3 :
				switch (s.vheader.type) {
					case TYPE_STRING :
						i += 8;
						step = 0;
						break;
					case TYPE_REAL :
						i += NumSize;
						step = 0;
						break;
					case TYPE_COMPLEX :
						if (s.varSubtype == ITEM_FIRST) {
							s.varSubtype = ITEM_SECOND;
						}
						else {
							step = 0;
						}
						i += NumSize;
						break;
					case TYPE_REALMATRIX :
						if (s.varSubtype == ITEM_FIRST) {
							i += NumSize;
							s.varSubtype = ITEM_SECOND;
						}
						else {
							i += NumSize;
							s.index ++;
							if (s.index >= (s.vheader.rows * s.vheader.columns)) {
								step = 0;
							}
							else if (!(s.index & 0x0f)) {
								s.varSubtype = ITEM_FIRST;
							}
						}
						break;
					case TYPE_COMPLEXMATRIX :
						i += NumSize;
						s.index++;
						if (s.index >= (s.vheader.rows * s.vheader.columns * 2)) {
							step = 0;
						}
						break;
				}
				break;
			default :
				error = ERR_INTERNAL_ERROR;
		}
	}

	// prepare to get next vHeader
	if (step == 0) {
		nextSize = sizeof(il_vheader);
	}

	// reading last entry
	if ((s.varCount <= 0) && (step == 1)) {
		switch (s.r.reg->type) {
			case TYPE_STRING :
			case TYPE_REAL :
					hpilXCore.statusFlags &= ~RunAgainListenBuf;
				break;
			case TYPE_COMPLEX :
				if (s.varSubtype == ITEM_SECOND) {
					hpilXCore.statusFlags &= ~RunAgainListenBuf;
				}
				break;
			case TYPE_REALMATRIX :
				if ((s.index >= ((s.r.rm->rows * s.r.rm->columns) - 1)) && (s.varSubtype == ITEM_SECOND)) {
					hpilXCore.statusFlags &= ~RunAgainListenBuf;
				}
				break;
			case TYPE_COMPLEXMATRIX :
				if (s.index >= ((s.r.rm->rows * s.r.rm->columns * 2) - 1)) {
					hpilXCore.statusFlags &= ~RunAgainListenBuf;
				}
				break;
		}
	}
	else if ((s.varCount <= 0) && (step == 3)) {
		error = ERR_NO_VARIABLES;
	}
	hpilXCore.bufSize = nextSize;
	// somethings was wrong
	if (error != ERR_NONE) {
		hpilXCore.bufSize = 0;
		hpilXCore.statusFlags &= ~RunAgainListenBuf;
	}
	s.differedError = error;
}

/* readpRefreshBuf
 *
 * process read buffer chunks by chunks and update for next buffer
 */
static void hpil_readpRefreshBuf(void) {
	static char buf[0x133];
	static char crc;
	static int read_prgm;
    int error = ERR_NONE, i, j;
	// first run
	if (s.pBlocks == 0) {
		s.saved_prgm = current_prgm;
		goto_dot_dot();
		read_prgm = current_prgm;
		s.index = 0;
		crc = 0;
	}
	// copy buffer to decode buffer
	s.pBlocks += hpilXCore.bufSize;
	for (i = 0; i < hpilXCore.bufSize;) {
		buf[s.index++] = hpilXCore.buf[i++];
	}
	i = 0;
	// decode loop
	while ((error == ERR_NONE)
  		&& ((i < (s.index - 50))
		|| ((i < (s.index - 1)) && !(hpilXCore.statusFlags & RunAgainListenBuf)))) {
		j = i;
		core_42ToFree42((unsigned char*)buf, &i, 50);
		for (j; j < i; j++) {
			// update Crc
			crc += buf[j];
		}
	}
	// copy end of buffer to beginning
	for (j = 0; i < s.index;) {
		buf[j++] = buf[i++];
	}
	s.index = j;
	if (hpilXCore.statusFlags & RunAgainListenBuf) {
		// read next buffer
		if (s.fLength < (s.pBlocks + 0x100)) {
			hpilXCore.statusFlags &= ~RunAgainListenBuf;
			hpilXCore.bufSize = s.fLength - s.pBlocks + 1;
		}
		else {
			hpilXCore.bufSize = 0x100;
		}
	}
	else {
		if (crc != buf[0]) {
			error = ERR_BAD_CRC;
			clear_prgm_by_index(read_prgm);
		}
		else {
			// update & cleanup after ultimate run
			rebuild_label_table();
			update_catalog();
			// Restore print mode
			flags.f.trace_print = s.trace;
			flags.f.normal_print = s.normal;
			hpil_emptyListenBuffer = NULL;
			if (!mode_running && s.fAttr & 0x02) {
				pc = 00;
				current_prgm = read_prgm;
			}
			else {
				// revert to pgm
				current_prgm = s.saved_prgm;
			}
		}
	}
	// something went wrong
	if (error != ERR_NONE) {
		clear_prgm_by_index(read_prgm);
		hpilXCore.bufSize = 0;
		hpilXCore.statusFlags &= ~RunAgainListenBuf;
		hpil_emptyListenBuffer = NULL;
	}
	s.differedError = error;
}

/* vHeaderChk
 *
 * check vheader vs existing var
 * allocate var space if needed
 * 
 */
int vHeaderChk(il_vheader *vHeader) {
	int error = ERR_NONE;

	// check vHeader vs variable
	if ((vHeader->nameLen > 0) && (vHeader->nameLen <= 7)) {
		s.r.reg = recall_var(vHeader->name, vHeader->nameLen);
		if (s.r.reg != NULL) {
			// variable exists
			if (s.r.reg->type == vHeader->type) {
				// same type ?
				if (s.r.reg->type == TYPE_REALMATRIX) {
					// real matrix same size ?
					if ((s.r.rm->columns != vHeader->columns) || (s.r.rm->rows != vHeader->rows)) {
						error = ERR_DIMENSION_ERROR;
					}
				}
				else if (s.r.reg->type == TYPE_COMPLEXMATRIX) {
					// complex matrix same size ?
					if ((s.r.cm->columns != vHeader->columns) || (s.r.cm->rows != vHeader->rows)) {
						error = ERR_DIMENSION_ERROR;
						}
				}
			}
			else {
				error = ERR_INVALID_TYPE;
			}
		}
	}
	else {
		// no empty vheader allowed
		error = ERR_FILE_BAD_TYPE;
	}
	
	if (error == ERR_NONE) {
		switch (vHeader->type) {
			case TYPE_REAL :
				s.r.reg = new_real((phloat)NULL);
				break;
			case TYPE_COMPLEX :
				s.r.reg = new_complex((phloat)NULL, (phloat)NULL);
				break;
			case TYPE_REALMATRIX :
				s.r.reg = new_realmatrix(vHeader->rows, vHeader->columns);
				break;
			case TYPE_COMPLEXMATRIX :
				s.r.reg = new_complexmatrix(vHeader->rows, vHeader->columns);
				break;
			case TYPE_STRING :
				s.r.reg = new_string("", 0);
				break;
			default :
				error = ERR_INTERNAL_ERROR;
		}
		if (s.r.reg == NULL) {
			error = ERR_INSUFFICIENT_MEMORY;
		}
		else {
			store_var(vHeader->name, vHeader->nameLen, s.r.reg);
		}
	}
	return error;
}

/* var2reg
 *
 * converts current phloat or string data to hp-41c reg format for data file
 * truncate of mantissa last digit, not rounding, two digits exponents 
 */
void var2reg(int index, char *buf) {
	int i, j, k;
	unsigned char c;
	unsigned char l;
	unsigned char regBuf[7];
	phloat pd;
	char bcdBuf[50];
	char bcdExp[3];
    bool seenDot = false;
    bool inLeadingZeroes = true;
	int exponent;
    int expOffset = -1;

	for (i = 0; i < 7; i++) {
		regBuf[i] = 0;
	}

	// string representation
	// size / char 0..5
	if (s.r.rm->array->is_string[index]) {
		j = 0;
		l = phloat_length(s.r.rm->array->data[index]);
		regBuf[0] = 0x10;					// set alpha type
		for (i = 1; i < 7 - l; i++) {
			regBuf[i] = 0;
		}
		for (i ; i < 7; i++) {
			regBuf[i] = phloat_text(s.r.rm->array->data[index])[j++];
		}
	}

	// real representation
	// mantissa sign / mantissa 0..9 / exponent sign / exponent 0..1
	else {
		pd = s.r.rm->array->data[index];
		if (pd == 0) {
			// nothing to do
		}
		else if (p_isnan(pd)) {
			// not a number
			// exponent sign 0xf and exponent non 0
			regBuf[5] = 0x0f;
			regBuf[6] = 0x99;
		}
		else if (p_isinf(pd)) {
			// infinite
			// exponent sign 0xf and exponent 0, set mantissa sign
			regBuf[5] = 0x0f;
			if (pd < 0) {
				regBuf[0] = 0x90;			
			}
		}
		else {
#ifdef BCD_MATH
			bid128_to_string(bcdBuf, &pd.val);
#else
			sprintf(bcdBuf, "%le", pd);
#endif
			i = 0;
			j = 0;
			while (bcdBuf[i] != 0) {
				c = bcdBuf[i++];
				switch (c) {
					case '-' :
						regBuf[j/2] |= 0x09 << (j%2 ? 0 : 4);
					case '+' :
						j++;
						break;
					case '.' :
						seenDot = true;
						break;
					case 'e' :
					case 'E' :
						j = 11;
		                sscanf(&bcdBuf[i], "%d", &exponent);
				        exponent += expOffset;
						if (exponent > 99) {
							// infinite - exponent sign 0xf and exponent 0
							regBuf[5] |= 0x0f;
							regBuf[6] = 0x00;
						}
						else if (exponent < -99) {
							// too near to zero = 0
							for (k = 0; k < 7; k++) {
								regBuf[k] = 0;
							}
						}
						else {
							if (exponent < 0) {
								regBuf[5] |= 0x09;
								exponent += 100;
							}
							sprintf(bcdExp,"%.02d",abs(exponent));
							regBuf[6] = (bcdExp[0] << 4) | (bcdExp[1] & 0x0f);
						}
						// force end of loop
						bcdBuf[i] = 0;
						break;
					case '0' :
						if (inLeadingZeroes) {
							break;
						}
					default :
						inLeadingZeroes = false;
						if (!seenDot) {
							expOffset++;
						}
						if (j < 11 ) {
							regBuf[j/2] |= (c & 0x0f) << (j%2 ? 0 : 4);
							j++;
						}
				}
			}
		}
	}

	// relocate nibbles (scramble) from A.R. Duell / lifutils
	buf[0]=regBuf[6];
	buf[1]=(regBuf[0]>>4)+((regBuf[5]&0xf)<<4);
	buf[2]=0;
	buf[3]=(regBuf[5]>>4)+((regBuf[4]&0xf)<<4);
	buf[4]=(regBuf[4]>>4)+((regBuf[3]&0xf)<<4);
	buf[5]=(regBuf[3]>>4);
	buf[5]+=((regBuf[2]&0xf)<<4);
	buf[6]=(regBuf[2]>>4)+((regBuf[1]&0xf)<<4);
	buf[7]=(regBuf[1]>>4)+((regBuf[0]&0xf)<<4);
}

/* reg2var
 *
 * hp-41c reg format data file to phloat or string data
 */
void reg2var(int index, uint8_t *buf) {
	int i, j;
	unsigned char l;
	unsigned char c;
	unsigned char regBuf[7];
	int skipNull = true;
	char bcdBuf[50];


	// descramble nibbles from A.R. Duell / lifutils
    regBuf[0]=((buf[1]&0xf)<<4)+(buf[7]>>4);
    regBuf[1]=((buf[7]&0xf)<<4)+(buf[6]>>4);
    regBuf[2]=((buf[6]&0xf)<<4)+(buf[5]>>4);
    regBuf[3]=((buf[5]&0xf)<<4)+(buf[4]>>4);
    regBuf[4]=((buf[4]&0xf)<<4)+(buf[3]>>4);
    regBuf[5]=((buf[3]&0xf)<<4)+(buf[1]>>4);
    regBuf[6]=buf[0];
	
	switch (regBuf[0] & 0xf0) {
		case 0x10 :
			// string
			s.r.rm->array->is_string[index] = 1;
			l = 6;
			j = 0;
			for (i = 1; i < 7; i++) {
				c = regBuf[i];
				skipNull = c ? false : skipNull;
				if (!skipNull) {
					phloat_text(s.r.rm->array->data[index])[j++] = c;
				}
				else{
					l--;
				}
			}
			phloat_length(s.r.rm->array->data[index]) = l;
			break;
		case 0x00 :
		case 0x90 :
			// number
			s.r.rm->array->is_string[index] = 0;
			bcdBuf[0] = ((regBuf[0] & 0xf0) == 0x90) ? '-' : '+';
			bcdBuf[1] = (regBuf[0] & 0x0f) + 0x030;
			bcdBuf[2] = '.';
			bcdBuf[3] = (regBuf[1] >> 4) + 0x030;
			bcdBuf[4] = (regBuf[1] & 0x0f) + 0x030;
			bcdBuf[5] = (regBuf[2] >> 4) + 0x030;
			bcdBuf[6] = (regBuf[2] & 0x0f) + 0x030;
			bcdBuf[7] = (regBuf[3] >> 4) + 0x030;
			bcdBuf[8] = (regBuf[3] & 0x0f) + 0x030;
			bcdBuf[9] = (regBuf[4] >> 4) + 0x030;
			bcdBuf[10] = (regBuf[4] & 0x0f) + 0x030;
			bcdBuf[11] = (regBuf[5] >> 4) + 0x030;
			bcdBuf[12] = 'E';
			bcdBuf[13] = '+';
			switch (regBuf[5] & 0x0f) {
				case 0x09 :
					bcdBuf[13] = '-';
					if (regBuf[6] & 0x0f) {
						regBuf[6] = ((0x0a - regBuf[6]) & 0x0f)
								  + ((0xa0 - regBuf[6]) & 0xf0);
					}
					else {
						regBuf[6] = (0xa0 - regBuf[6]) & 0xf0;
					}
				case 0x00 :
					bcdBuf[14] = (regBuf[6] >> 4) + 0x030;
					bcdBuf[15] = (regBuf[6] & 0x0f) + 0x030;
					bcdBuf[16] = 0;
					break;
				case 0x0f :
					// nan or inf ? don't know how to deal nan, set huge exponent
					bcdBuf[14] = '9';
					bcdBuf[15] = '9';
					bcdBuf[16] = '9';
					bcdBuf[17] = '9';
					bcdBuf[18] = 0;
					break;
			}
#ifdef BCD_MATH
			BID_UINT128 val;
			bid128_from_string(&val, bcdBuf);
			memcpy(&s.r.rm->array->data[index], &val, sizeof(phloat));
#else
			sscanf(bcdBuf, "%le", (phloat*)(&s.r.rm->array->data[index]));
#endif
			break;
	}
}

/* Mass storage sub functions
 *
 */

/* Write buffer0
 *
 * send data with Write  Buffer 0 command
 * LAD to be done by caller
 */
static int hpil_writeBuffer0_sub(int error) {
	if (error == ERR_NONE) {
		error = ERR_INTERRUPTIBLE;
		switch (hpil_step) {
			case 0 :		// > DDL 00
				ILCMD_DDL(0x00);
				hpil_step++;
				break;
			case 1 :		// > tlk
				ILCMD_tlk;
				hpil_step++;
				break;
			case 2 :
				ILCMD_TAD(hpil_settings.disk);
				hpil_step++;
				error = call_ilCompletion(hpil_wait_sub);
				break;
			case 3 :
				ILCMD_nop;
				error = rtn_il_completion();
				break;
			default :
				error = ERR_NONE;
		}
	}
	return error;
}

/* write 
 *
 * write data to media
 * LAD to be done by caller
 */
static int hpil_write_sub(int error) {
	if (error == ERR_NONE) {
		error = ERR_INTERRUPTIBLE;
		switch (hpil_step) {
			case 0 :		// > DDL 02
				ILCMD_DDL(0x02);
				error = rtn_il_completion();
				break;
			default :
				error = ERR_NONE;
		}
	}
	return error;
}

/* setBytePtr 
 *
 * position current byte ptr
 * LAD to be done by caller
 */
static int hpil_setBytePtr_sub(int error) {
	if (error == ERR_NONE) {
		error = ERR_INTERRUPTIBLE;
		switch (hpil_step) {
			case 0 :		// > DDL 03
				ILCMD_DDL(0x03);
				hpil_step++;
				break;
			case 1 :		// > tlk
				hpilXCore.bufSize = 1;
				hpilXCore.bufPtr = 0;
				ILCMD_tlk;
				error = rtn_il_completion();
				break;
			default :
				error = ERR_NONE;
		}
	}
	return error;
}

/* seek 
 *
 * seek and wait for result
 * LAD to be done by caller
 */
static int hpil_seek_sub(int error) {
	if (error == ERR_NONE) {
		error = ERR_INTERRUPTIBLE;
		switch (hpil_step) {
			case 0 :		// > DDL 04
				ILCMD_DDL(0x04);
				hpil_step++;
				break;
			case 1 :		// > tlk
				hpilXCore.bufSize = 2;
				hpilXCore.bufPtr = 0;
				ILCMD_tlk;
				hpil_step++;
				break;
			case 2 :
				ILCMD_TAD(hpil_settings.disk);
				hpil_step++;
				error = call_ilCompletion(hpil_wait_sub);
				break;
			case 3 :
				ILCMD_nop;
				error = rtn_il_completion();
				break;
			default :
				error = ERR_NONE;
		}
	}
	return error;
}

/* partial write
 *
 * set mode to partial write
 * LAD to be done by caller
 */
static int hpil_partialWrite_sub(int error) {
	if (error == ERR_NONE) {
		error = ERR_INTERRUPTIBLE;
		switch (hpil_step) {
			case 0 :		// > DDL 06
				ILCMD_DDL(0x06);
				error = rtn_il_completion();
				break;
			default :
				error = ERR_NONE;
		}
	}
	return error;
}

/* close 
 *
 * close record & write data to media
 * LAD to be done by caller
 */
static int hpil_close_sub(int error) {
	if (error == ERR_NONE) {
		error = ERR_INTERRUPTIBLE;
		switch (hpil_step) {
			case 0 :		// > DDL 08
				ILCMD_DDL(0x08);
				error = rtn_il_completion();
				break;
			default :
				error = ERR_NONE;
		}
	}
	return error;
}

/* hpil_writeDirEntry
 *
 * shortcut for writing directory
 * use DataBuf & AltBuf to ease directory rewriting
 */
static int hpil_writeDirEntry_sub(int error) {
	static uint8_t* buf;	// to restore original buf
	if (error == ERR_NONE) {
		error = ERR_INTERRUPTIBLE;
		switch (hpil_step) {
			case 0 :		// > seek to dir entry
				buf = hpilXCore.buf;
				hpilXCore.buf = hpil_controllerAltBuf.data;
				// buffers adjustement, we're at the end of current entry
				if (s.dirEntryByte == 0) {
					s.dirEntryBlock--;
					s.dirEntryByte = 0x0100;
					}
				s.dirEntryByte -= 0x20;
				s.dirEntryBlock--;
				hpil_controllerAltBuf.data[0] = s.dirEntryBlock << 8;
				hpil_controllerAltBuf.data[1] = s.dirEntryBlock & 0x00ff;
				ILCMD_LAD(hpil_settings.disk);
				hpil_step++;
				error = call_ilCompletion(hpil_seek_sub);
				break;
			case 1 :		// > seek done, read directory entry
				ILCMD_TAD(hpil_settings.disk);
				hpil_step++;
				error = call_ilCompletion(hpil_read_sub);
				break;
			case 2 :		// > seek again for write
				hpil_controllerAltBuf.data[0] = s.dirEntryBlock << 8;
				hpil_controllerAltBuf.data[1] = s.dirEntryBlock & 0x00ff;
				ILCMD_LAD(hpil_settings.disk);
				hpil_step++;
				error = call_ilCompletion(hpil_seek_sub);
				break;
			case 3 :		// > write
				ILCMD_LAD(hpil_settings.disk);
				hpil_step++;
				error = call_ilCompletion(hpil_write_sub);
				break;
			case 4 :		// > set byte pointer
				hpil_controllerAltBuf.data[0] = (uint8_t) s.dirEntryByte;
				ILCMD_nop;
				hpil_step++;
				error = call_ilCompletion(hpil_setBytePtr_sub);
				break;
			case 5 :		// > recover directory entry and write it
				hpilXCore.buf = hpil_controllerDataBuf.data;
				hpilXCore.bufPtr = 0;
				hpilXCore.statusFlags |= LastIsEndTalkBuf;
				hpilXCore.bufSize = 32;
				ILCMD_nop;
				hpil_step++;
				error = call_ilCompletion(hpil_writeBuffer0_sub);
				break;
			case 6 :		// > done
				hpilXCore.buf = buf;	// restore original buf
				ILCMD_nop;
				error = rtn_il_completion();
				break;
			default :
				error = ERR_NONE;
		}
	}
	return error;
}

/* readBuffer0
 *
 * read data with Send Buffer 0 command
 * TAD to be done by caller
 */
static int hpil_readBuffer0_sub(int error) {
	if (error == ERR_NONE) {
		error = ERR_INTERRUPTIBLE;
		switch (hpil_step) {
			case 0 :		// > check status
				ILCMD_nop;
				hpil_step++;
				error = call_ilCompletion(hpil_wait_sub);
				break;
			case 1 :
				ILCMD_DDT(0x00);
				hpil_step++;
				break;
			case 2 :		// > ltn
				ILCMD_ltn;
				hpil_step++;
				break;
			case 3 :		// > send data
				hpilXCore.bufPtr = 0;
				ILCMD_SDA;
				hpil_step++;
				break;
			case 4 :		// > lun
				ILCMD_lun;
				error = rtn_il_completion();
				break;
			default :
				error = ERR_NONE;
		}
	}
	return error;
}

/* readBufferx
 *
 * read data with Send Buffer 0 command
 * forework to be done by caller
 */
static int hpil_readBufferx_sub(int error) {
	if (error == ERR_NONE) {
		error = ERR_INTERRUPTIBLE;
		switch (hpil_step) {
			case 0 :		// > check status
				ILCMD_nop;
				hpil_step++;
				//error = call_ilCompletion(hpil_wait_sub);
				break;
			case 1 :
				ILCMD_DDT(0x00);
				hpil_step++;
				break;
			case 2 :		// > ltn
				ILCMD_ltn;
				hpil_step++;
				break;
			case 3 :		// > send data
				hpilXCore.bufPtr = 0;
				ILCMD_SDA;
				hpil_step++;
				break;
			case 4 :		// > lun
				ILCMD_lun;
				error = rtn_il_completion();
				break;
			default :
				error = ERR_NONE;
		}
	}
	return error;
}

/* read 
 *
 * read and wait for result, no transfer
 * TAD to be done by caller
 */
static int hpil_read_sub(int error) {
	if (error == ERR_NONE) {
		error = ERR_INTERRUPTIBLE;
		switch (hpil_step) {
			case 0 :		// > DDT 02
				ILCMD_DDT(0x02);
				hpil_step++;
				error = call_ilCompletion(hpil_wait_sub);
				break;
			case 1 :		// > return
				ILCMD_nop;
				error = rtn_il_completion();
				break;
			default :
				error = ERR_NONE;
		}
	}
	return error;
}

/* getAddress 
 *
 * return current adress
 * TAD to be done by caller
 */
static int hpil_getAddress_sub(int error) {
	if (error == ERR_NONE) {
		error = ERR_INTERRUPTIBLE;
		switch (hpil_step) {
			case 0 :		// > DDT  03
				ILCMD_DDT(0x03);
				hpil_step++;
				break;
			case 1 :		// > listen
				hpilXCore.bufSize = 3 + 1;	// avoid uneeded nrd
				hpilXCore.bufPtr = 0;
				ILCMD_ltn;
				hpil_step++;
				break;
			case 2 :		// > get data
				ILCMD_SDA;
				hpil_step++;
				break;
			case 3 :
				ILCMD_lun;
				error = rtn_il_completion();
				break;
			default :
				error = ERR_NONE;
		}
	}
	return error;
}

/* getMaxAddress 
 *
 * return maximum available adress
 */
static int hpil_getMaxAddress_sub(int error) {
	if (error == ERR_NONE) {
		error = ERR_INTERRUPTIBLE;
		switch (hpil_step) {
			case 0 :		// > DDT  07
				ILCMD_DDT(0x07);
				hpil_step++;
				break;
			case 1 :		// > listen
				ILCMD_ltn;
				hpil_step++;
				break;
			case 2 :		// > get data
				hpilXCore.bufSize = 2 + 1;	// avoid uneeded nrd...
				hpilXCore.bufPtr = 0;
				ILCMD_SDA;
				hpil_step++;
				break;
			case 3 :
				ILCMD_lun;
				error = rtn_il_completion();
				break;
			default :
				error = ERR_NONE;
		}
	}
	return error;
}

/* hpil_diskSelect
 *
 * select disk using auto / manual mode and dskAid
 */
static int hpil_diskSelect_sub(int error) {
	static int i, n;
	if (error == ERR_NONE) {
		error = ERR_INTERRUPTIBLE;
		switch (hpil_step) {
			case 0 :
				ILCMD_AAU;
				hpil_step++;
				break;
			case 1 :
				ILCMD_AAD(0x01);
				hpil_step++;
				break;
			case 2 :
				n = (frame & 0x001f) - 1;
				if (hpil_settings.dskAid) {
					i = hpil_settings.dskAid;
					hpil_step++;
				}
				else if (flags.f.manual_IO_mode) {
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
			case 3:
				if (hpilXCore.bufPtr && ((hpil_controllerAltBuf.data[0] & 0x10) == 0x10)) {
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
			case 4 :
				if (hpilXCore.bufPtr) {
					if ((hpil_controllerAltBuf.data[0] & 0x10) == 0x10) {
						hpil_settings.disk = i;
						ILCMD_nop;
						error = rtn_il_completion();
					}
					else {
						error = ERR_NO_DRIVE;
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

/* getHeader
 *
 * check device's Aid, seek, read check header, get media capacity / dir entries
 */
static int hpil_getHeader_sub(int error) {
	if (error == ERR_NONE) {
		error = ERR_INTERRUPTIBLE;
		switch (hpil_step) {
			case 0 :		// > find device
				s.dirEntryBlock = 0;
				s.dirEntryByte = 0;
				hpilXCore.buf = hpil_controllerAltBuf.data;
				hpilXCore.bufSize = ControllerAltBufSize;
				ILCMD_nop;
				hpil_step++;
				error = call_ilCompletion(hpil_diskSelect_sub);
				break;
			case 1 :		// > check AID done by diskSelect, get max address
				ILCMD_nop;
				hpil_step++;
				error = call_ilCompletion(hpil_getMaxAddress_sub);
				break;
			case 2 :		// > save recordsMax, LAD and seek to start of media
				ILCMD_nop;
				if (hpilXCore.bufPtr != 2) {
					s.recordsMax = 0x01ff;		// assume 82161A tape drive...
				}
				else {
					s.recordsMax = (hpil_controllerAltBuf.data[0] << 8) + hpil_controllerAltBuf.data[1];
				}
				hpil_controllerAltBuf.data[0] = 0;
				hpil_controllerAltBuf.data[1] = 0;
				ILCMD_LAD(hpil_settings.disk);
				hpil_step++;
				error = call_ilCompletion(hpil_seek_sub);
				break;
			case 3 :		// > read header block
				ILCMD_TAD(hpil_settings.disk);
				hpil_step++;
				error = call_ilCompletion(hpil_read_sub);
				break;
			case 4 :		// > transfer header block
				hpilXCore.bufSize = ControllerAltBufSize;
				ILCMD_nop;
				hpil_step++;
				error = call_ilCompletion(hpil_readBuffer0_sub);
				break;
			case 5 :		// > check header data, seek to next first dir entry
				if ((_byteswap_ushort(hpil_controllerAltBuf.header.identifier) != 0x8000)	// no lif identifier found
				 || (_byteswap_ulong(hpil_controllerAltBuf.header.dir_bstart) != 0x0002) 	// wrong dir start
				 || (_byteswap_ulong(hpil_controllerAltBuf.header.dir_blen) < 1)			// incorrect directory block len
				 ||	(_byteswap_ulong(hpil_controllerAltBuf.header.dir_blen) > 1250)) {
					error = ERR_BAD_MEDIA;
					return error;
				}
				s.dirEntryBlock = (uint16_t)_byteswap_ulong(hpil_controllerAltBuf.header.dir_bstart);
				s.dirBlocklen = (uint16_t)_byteswap_ulong(hpil_controllerAltBuf.header.dir_blen);
				s.dirEntriesLeft = (s.dirBlocklen * 8) - 1;
				s.firstFreeBlock = s.dirEntryBlock + s.dirBlocklen;
				hpil_controllerAltBuf.data[0] = s.dirEntryBlock << 8;
				hpil_controllerAltBuf.data[1] = s.dirEntryBlock & 0x00ff;
				s.recordsLeft = s.recordsMax - s.dirBlocklen;
				ILCMD_LAD(hpil_settings.disk);
				hpil_step++;
				error = call_ilCompletion(hpil_seek_sub);
				break;
			case 6 :		// > read 
				ILCMD_TAD(hpil_settings.disk);
				hpil_step++;
				error = call_ilCompletion(hpil_read_sub);
				break;
			case 7 :		// > done
				ILCMD_nop;
				error = rtn_il_completion();
				break;
			default :
				error = ERR_NONE;
		}
	}
	return error;
}

static int hpil_dirHeader_sub(int error) {
	if (error == ERR_NONE) {
		error = ERR_INTERRUPTIBLE;
		switch (hpil_step) {
			case 0 :		// > print volume header
				if (hpil_controllerAltBuf.header.label[0]) {
					sprintf(hpil_text,"Volume: %.6s        ", hpil_controllerAltBuf.header.label);
					//				   0000000000111111111222	
					//				   1234567890123456789012
				}
				else {
					sprintf(hpil_text,"                      ");
					//				   0000000000111111111222	
					//				   1234567890123456789012
				}
				ILCMD_nop;
				hpil_step++;
				error = call_ilCompletion(hpil_display_sub);
				break;
			case 1 :		// > print dir header
				sprintf(hpil_text,"Name     Type Flg Regs");
				//				   0000000000111111111222	
				//				   1234567890123456789012	
				ILCMD_nop;
				hpil_step++;
				error = call_ilCompletion(hpil_pauseAndDisplay_sub);
				break;
			case 2 :
				ILCMD_nop;
				error = rtn_il_completion();
				break;
			default :
				error = ERR_NONE;
		}
	}
	return error;
}

static int hpil_dirEntry_sub(int error) {
	int f;
	char buf[11];
	if (error == ERR_NONE) {
		error = ERR_INTERRUPTIBLE;
		switch (hpil_step) {
			case 0 :		// > print current entry
				f = _byteswap_ushort(hpil_controllerAltBuf.dir.fType);
				if (f == 0x0001) {				// Stf LIF text file
					strcpy(buf,"ASCI    ");
				} else if (f == 0xe020) {		// 41 all calc with extender memory (Hoser ROM)
					strcpy(buf,"CALC    ");
				} else if (f == 0xe030) {		// 41 extender memory (Hoser ROM)
					strcpy(buf,"XMEM    ");
				} else if (f == 0xe040) {		// 41 all calc
					strcpy(buf,"WALL    ");
				} else if (f == 0xe050) {		// 41 user keyboard
					strcpy(buf,"KEYS    ");
				} else if (f ==0xe060) {		// 41 calculator status
					strcpy(buf,"STAT    ");
				} else if (f == 0xe070) {		// 41 ROM file
					strcpy(buf,"ROM     ");
				} else if (f == 0xe080) {		// 41 FOCAL Program
					strcpy(buf,"PRGM    ");
					// length stored as bytes but displayed as regs - correct this here
					hpil_controllerAltBuf.dir.fLength = _byteswap_ushort((_byteswap_ushort(hpil_controllerAltBuf.dir.fLength)+6)/7);
				} else if (f == 0xe0b0) {		// 41 buffer (Hoser ROM)
					strcpy(buf,"BUFF    ");				
				} else if (f == 0xe0d0) {		// Std data file
					strcpy(buf,"DATA    ");
				} else if (f == 0xe0d8) {		// Free42S Extended data file
					strcpy(buf,"EDTA    ");
				} else if (f == 0xe0dc) {		// Free42S All data file
					strcpy(buf,"ADTA    ");
				} else {						// unknown file type	
					strcpy(buf,"????    ");
				}
				if (hpil_controllerAltBuf.dir.flags[0] & 0x01) {
					buf[5] = 'p';
				}
				if (hpil_controllerAltBuf.dir.flags[0] & 0x02) {
					buf[6] = 'a';
				}
				if (hpil_controllerAltBuf.dir.flags[0] & FlagSecured) {
					buf[7] = 's';
				}
				sprintf(hpil_text,"%.9s%-8s%5u",hpil_controllerAltBuf.dir.fName,buf,_byteswap_ushort(hpil_controllerAltBuf.dir.fLength));
				//				   0000000000111111111222	
				//				   1234567890123456789012
				ILCMD_nop;
				hpil_step ++;
				error = call_ilCompletion(hpil_pauseAndDisplay_sub);
				break;
			case 1 :
				ILCMD_nop;
				error = rtn_il_completion();
				break;
			default :
				error = ERR_NONE;
		}
	}
	return error;
}

static int hpil_dirFooter_sub(int error) {
	if (error == ERR_NONE) {
		error = ERR_INTERRUPTIBLE;
		switch (hpil_step) {
			case 0 :		// > print current entry
				sprintf(hpil_text,"Dir entries left %5u",s.dirEntriesLeft);
				//				   0000000000111111111222	
				//				   1234567890123456789012
				ILCMD_nop;
				hpil_step++;
				error = call_ilCompletion(hpil_pauseAndDisplay_sub);
				break;
			case 1 :		// > DDT 07
				sprintf(hpil_text,"Records left     %5u", s.recordsLeft);
				//				   0000000000111111111222	
				//				   1234567890123456789012
				ILCMD_nop;
				hpil_step++;
				error = call_ilCompletion(hpil_pauseAndDisplay_sub);
				break;
			case 2 :
				ILCMD_nop;
				hpil_step++;
				error = call_ilCompletion(hpil_pause_sub);
				break;
			case 3 :
				flags.f.two_line_message = 0;
				flags.f.message = 0;
				redisplay();
				ILCMD_nop;
				error = rtn_il_completion();
				break;
			default :
				error = ERR_NONE;
		}
	}
	return error;
}
