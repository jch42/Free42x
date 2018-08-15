/*****************************************************************************
 * FreeIL -- an HP-IL loop emulation
 * Copyright (C) 2014-2016 Jean-Christophe HESSEMANN
 *
 * This program is free software// you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY// without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program// if not, see http://www.gnu.org/licenses/.
 *****************************************************************************/

#ifndef HPIL_Common_h
#define HPIL_Common_h 1

#include "hpil_core.h"

/* common tools for hpil devices
 *
 */
#define ControllerAltBufSize	0x0020
#define ControllerDataBufSize	0x0100

#define REGS_SZ 8					// size of a register in bytes
#define BLOCK_SZ 256				// size of a block

// choose if we pad first or second part with blanks...
#define SPLIT_MODE_VAR		0x01	// in this mode, first part is for filename
#define SPLIT_MODE_PRGM		0x02	// and in this the first is for filename and the second for label

typedef struct {
	int selected;					// selected device
	int print;						// first print / display device in autoio mode
	int disk;						// first drive in autoio mode
	int plotter;					// first plotter in autoio mode
	int prtAid;						// aid of printer
	int dskAid;						// aid of disk
	bool modeEnabled;				// hpil enabled...
	bool modeTransparent;			// background loop processing
	bool modePIL_Box;				// use PIL_Box
} HPIL_Settings;

#pragma pack (1)

typedef struct {
	uint16_t identifier;
	char label[6];
	uint32_t dir_bstart;		// in blocks
	char pad[4];
	uint32_t dir_blen;			// in blocks
} LIF_header;

typedef struct {
	char fName[10];
	uint16_t fType;
	uint32_t fBStart;		// in blocks
	uint32_t fBlocks;		// in blocks
	char dateTime[6];		// unused
	char pad[2];			// must be 8001
	uint16_t fLength;		// file len in 8 bytes or in byte for focal programs
	char flags[2];
}  dir_entry;

#pragma pack ()

typedef union {
	uint8_t data[ControllerAltBufSize];
	LIF_header header;
	dir_entry dir;
} AltBuf;

typedef union {
	uint8_t data[ControllerDataBufSize];
	LIF_header header;
	dir_entry dir;
} DataBuf;

typedef struct {
	char str1[10];
	unsigned char len1;
	char str2[10];
	unsigned char len2;
} AlphaSplit;

void hpil_init(bool modeEnabled, bool modePil_Box);
void hpil_close(bool modeEnabled, bool modePil_Box);
bool persist_hpil(void);
bool unpersist_hpil(int ver);
int hpil_check(void);
int hpil_worker(int interrupted);
int call_ilCompletion(int (*hpil_completion_call)(int));
int rtn_il_completion();

int hpil_aid_sub(int error);
int hpil_wait_sub(int error);

int hpil_display_sub(int error);
int hpil_pause_sub(int error);
int hpil_pauseAndDisplay_sub(int error);

int mappable_x_hpil(int max, int *cmd);
int mappable_x_char(uint8_t *c);
int hpil_splitAlphaReg(int mode);

#endif

