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

#ifndef HPIL_Controller_h
#define HPIL_Controller_h

#include "hpil_core.h"

typedef struct {
	uint8_t* buf;						// pointer to CURRENT BUF
	uint16_t bufPtr;					// byte pointer
	uint16_t bufSize;					// buffer size
	uint16_t statusFlags;				// status flags
	uint8_t  endChar;					// end char when receiving
} HpilXController;

class HPIL_Controller: public HPIL_Core
{
public:
	HPIL_Controller(void);
	// init device
	void begin(HpilXController*);
	// controller communication
	void controllerCmd(uint16_t);
protected:
	HpilXController* _hpilXController;
	// Controller functions
	uint16_t dataListen(uint16_t);
	uint16_t dataTalk(void);
};

// flags for buffer management
#define MaskBuf				0x00000033
#define MaskListenBuf		0x00000007
#define FullListenBuf		0x00000001		// listen buffer is full
#define OverListenBuf		0x00000002		// listen buffer overflow
#define RunAgainListenBuf	0x00000004		// end of buffer does not mean end of reception
#define MaskTalkBuf			0x00000070
#define EmptyTalkBuf		0x00000010		// talk buffer is swapped
#define UnderTalkBuf		0x00000020		// talk buffer is empty
#define RunAgainTalkBuf		0x00000040		// end of buffer does not mean end of transmission
#define LastIsEndTalkBuf	0x00000080		// last byte will be also be 'end' tagged
#define CmdNew				0x00000100		// a new command is present
#define CmdRun				0x00000200		// last command still running
#define CmdHshk				0x00000400		// last command acknowledged
#define BrokenLoop			0x00000800		// no returning frame
#define ListenTilEnd		0x00001000		// listen until end frame received
#define ListenTilCrLf		0x00002000		// listen until cr/lf frames received
#define ListenTilChar		0x00004000		// listen until char received

// flags for local messages
#define Local				0x00008000		// local functions (combined with core pseudo messages)


// Commands Menomnics
// ACG
#define M_NUL 0x0400
#define M_GTL 0x0401
#define M_SDC 0x0404
#define M_PPD 0x0405
#define M_GET 0x0408
#define M_ELN 0x040f
#define M_PPE 0x0480
#define M_DDL 0x04a0
#define M_DDT 0x04c0
// UGC
#define M_NOP 0x0410
#define M_LLO 0x0411
#define M_DCL 0x0414
#define M_PPU 0x0415
#define M_EAR 0x0418
#define M_IFC 0x0490
#define M_REN 0x0492
#define M_NRE 0x0493
// PIL-Box
#define M_TDIS 0x0494
#define M_COFI 0x0495
#define M_CON  0x0496
#define M_COFF 0x0497
// UGC Continuation
#define M_AAU 0x049a
#define M_LPD 0x049b
// LAG
#define M_LAD 0x0420
#define M_UNL 0x043f
//TAG
#define M_TAD 0x0440
#define M_UNT 0x045f
// IDY
#define M_IDY 0x0600
#define M_ISR 0x0700
// RDY
#define M_RFC 0x0500
#define M_ETO 0x0540
#define M_ETE 0x0541
#define M_NRD 0x0542
#define M_SDA 0x0560
#define M_SST 0x0561
#define M_SDI 0x0562
#define M_SAI 0x0563
#define M_TCT 0x0564
#define M_AAD 0x0580
// Local only
#define M_nop Local
#define M_ltn Local | ltn
#define M_lun Local | lun
#define M_tlk Local | tlk

// commands macros
// ACG
#define ILCMD_NUL     controllerCommand = M_NUL    ; hpilXCore.statusFlags |= CmdNew
#define ILCMD_GTL     controllerCommand = M_GTL    ; hpilXCore.statusFlags |= CmdNew
#define ILCMD_SDC     controllerCommand = M_SDC    ; hpilXCore.statusFlags |= CmdNew
#define ILCMD_PPD     controllerCommand = M_PPD    ; hpilXCore.statusFlags |= CmdNew
#define ILCMD_GET     controllerCommand = M_GET    ; hpilXCore.statusFlags |= CmdNew
#define ILCMD_ELN     controllerCommand = M_ELN    ; hpilXCore.statusFlags |= CmdNew
#define ILCMD_PPE(x)  controllerCommand = M_PPE | x; hpilXCore.statusFlags |= CmdNew
#define ILCMD_DDL(x)  controllerCommand = M_DDL | x; hpilXCore.statusFlags |= CmdNew
#define ILCMD_DDT(x)  controllerCommand = M_DDT | x; hpilXCore.statusFlags |= CmdNew
// UCG
#define ILCMD_NOP     controllerCommand = M_NOP    ; hpilXCore.statusFlags |= CmdNew
#define ILCMD_LLO     controllerCommand = M_LLO    ; hpilXCore.statusFlags |= CmdNew
#define ILCMD_DCL     controllerCommand = M_DCL    ; hpilXCore.statusFlags |= CmdNew
#define ILCMD_PPU     controllerCommand = M_PPU    ; hpilXCore.statusFlags |= CmdNew
#define ILCMD_EAR     controllerCommand = M_EAR    ; hpilXCore.statusFlags |= CmdNew
#define ILCMD_IFC     controllerCommand = M_IFC    ; hpilXCore.statusFlags |= CmdNew
#define ILCMD_REN     controllerCommand = M_REN    ; hpilXCore.statusFlags |= CmdNew
#define ILCMD_NRE     controllerCommand = M_NRE    ; hpilXCore.statusFlags |= CmdNew
// PIL-Box
#define ILCMD_TDIS    controllerCommand = M_TDIS   ; hpilXCore.statusFlags |= CmdNew
#define ILCMD_COFI    controllerCommand = M_COFI   ; hpilXCore.statusFlags |= CmdNew
#define ILCMD_CON     controllerCommand = M_CON    ; hpilXCore.statusFlags |= CmdNew
#define ILCMD_COFF    controllerCommand = M_COFF   ; hpilXCore.statusFlags |= CmdNew
// UGC Continuation
#define ILCMD_AAU     controllerCommand = M_AAU    ; hpilXCore.statusFlags |= CmdNew
#define ILCMD_LPD     controllerCommand = M_LPD    ; hpilXCore.statusFlags |= CmdNew
// LAG
#define ILCMD_LAD(x)  controllerCommand = M_LAD | x; hpilXCore.statusFlags |= CmdNew
#define ILCMD_UNL     controllerCommand = M_UNL    ; hpilXCore.statusFlags |= CmdNew
// TAG
#define ILCMD_TAD(x)  controllerCommand = M_TAD | x; hpilXCore.statusFlags |= CmdNew
#define ILCMD_UNT     controllerCommand = M_UNT    ; hpilXCore.statusFlags |= CmdNew
// IDY
#define ILCMD_IDY(x)  controllerCommand = M_IDY | x; hpilXCore.statusFlags |= CmdNew
#define ILCMD_ISR     controllerCommand = M_ISR    ; hpilXCore.statusFlags |= CmdNew
// RDY
#define ILCMD_RFC     controllerCommand = M_RFC    ; hpilXCore.statusFlags |= CmdNew
#define ILCMD_ETO     controllerCommand = M_ETO    ; hpilXCore.statusFlags |= CmdNew
#define ILCMD_ETE     controllerCommand = M_ETE    ; hpilXCore.statusFlags |= CmdNew
#define ILCMD_NRD     controllerCommand = M_NRD    ; hpilXCore.statusFlags |= CmdNew
#define ILCMD_SDA     controllerCommand = M_SDA    ; hpilXCore.statusFlags |= CmdNew
#define ILCMD_SST     controllerCommand = M_SST    ; hpilXCore.statusFlags |= CmdNew
#define ILCMD_SDI     controllerCommand = M_SDI    ; hpilXCore.statusFlags |= CmdNew
#define ILCMD_SAI     controllerCommand = M_SAI    ; hpilXCore.statusFlags |= CmdNew
#define ILCMD_TCT     controllerCommand = M_TCT    ; hpilXCore.statusFlags |= CmdNew
#define ILCMD_AAD(x)  controllerCommand = M_AAD | x; hpilXCore.statusFlags |= CmdNew
// Local only
#define ILCMD_nop     controllerCommand = M_nop    ; hpilXCore.statusFlags |= CmdNew
#define ILCMD_ltn     controllerCommand = M_ltn    ; hpilXCore.statusFlags |= CmdNew
#define ILCMD_lun     controllerCommand = M_lun    ; hpilXCore.statusFlags |= CmdNew
#define ILCMD_tlk     controllerCommand = M_tlk    ; hpilXCore.statusFlags |= CmdNew

#endif
