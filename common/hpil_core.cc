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
 *
 * hpil_core.cpp - HP-IL state machine implantation
 *
 *****************************************************************************/


#include <stddef.h>
#if defined(_MSC_VER) || defined(__ANDROID__)
	#include <stdio.h>
	#define tprintf sprintf
#else
	#include "tprintf.h"
#endif

#include "hpil_core.h"
#include "shell.h"

uint16_t debugLevel = 0xff;
uint8_t StateChanged;
uint16_t cycles;

// State decoding
static const char R_ist[5][5] = {"    ","REIS","RSYS","RITS","RCDS"};
static const char D_ist[5][5] = {"    ","DIDS","DACS","DSCS","DTRS"};
static const char A_ist[5][5] = {"    ","AIDS","ACDS","ANRS","ACRS"};
static const char S_ist[7][5] = {"    ","SIDS","SGNS","SDYS","STRS","SCSS","SCHS"};
static const char C_ist[5][5] = {"    ","CIDS","CACS","CSBS","CTRS"};
static const char CS_ist[3][5] = {"    ","CSNS","CSRS"};
static const char CE_ist[3][5] = {"    ","CEIS","CEMS"};
static const char T_ist[9][5] = {"    ","TIDS","TADS","TACS","SPAS","DIAS","AIAS","TERS","TAHS"};
static const char L_ist[3][5] = {"    ","LIDS","LACS"};
static const char NR_ist[5][5] = {"    ","NIDS","NENS","NWRS","NACS"};
static const char SR_ist[6][5] = {"    ","SRIS","SRSS","SRHS","SRAS","ARSS"};
static const char AR_ist[3][5] = {"    ","ARIS","ARAS"};
static const char RL_ist[5][5] = {"    ","LOCS","REMS","RWLS","LWLS"};
static const char RS_ist[3][5] = {"    ","RIDS","RACS"};
static const char AA_ist[4][5] = {"    ","AAUS","AAIS","AACS"};
static const char PD_ist[6][5] = {"    ","POFS","PONS","PUPS","PDAS","PDHS"};
static const char PP_ist[4][5] = {"    ","PPIS","PPSS","PPAS"};
static const char DC_ist[3][5] = {"    ","DCIS","DCAS"};
static const char DT_ist[3][5] = {"    ","DTIS","DTAS"};
static const char DD_ist[3][5] = {"    ","DDIS","DDAS"};

HPIL_Core::HPIL_Core(void)
{
}

void HPIL_Core::beginCore(void)
{
	Pseudo_Init();
	R_State = 0;
	D_State = 0;
	A_State = 0;
	S_State = 0;
	C_State = 0;
	CS_State = 0;
	CE_State = 0;
	T_State = 0;
	L_State = 0;
	NR_State = 0;
	SR_State = 0;
	AR_State = 0;
	RL_State = 0;
	RS_State = 0;
	AA_State = 0;
	PD_State = 0;
	PP_State = 0;
	DC_State = 0;
	DT_State = 0;
	DD_State = 0;
	cycles=0;
	Pseudo_Set(pof);
	process();
	Pseudo_Clr(pof);
	Pseudo_Set(pon);
	process();
	Pseudo_Clr(pon);
	process();
}

uint8_t HPIL_Core::process(void)
{
char buf[100] = "";
char opt[5] = "Std ";
	StateChanged = 0;
	//tprintf(buf,"\n{Input %04x, %s, %s, %s, %s, %s, %s , %s, %s , pseudo %08x, chng %01x, %s} ",cycles,R_ist[R_State],D_ist[D_State],A_ist[A_State],S_ist[S_State],C_ist[C_State],T_ist[T_State],L_ist[L_State],AA_ist[AA_State],_pseudoMsg,StateChanged,opt);
	//shell_write_console(buf);
	if ((!(debugLevel & DebugMonitorNoFast)) && (dab() && lacs() && dids() && !(nens() && Pseudo_Tst(hlt)) && !Pseudo_Tst(rsv))) {
		// DOE Lacs optimized cycle
		strcpy(opt, "LACS"); 
		Do_R_State();
		Do_R_State();
		Do_A_State();
		Do_R_State();
		Do_A_State();
		Do_A_State();
		Do_D_State();
		Do_A_State();
		// last D state in normal loop
	}
	else if ((!(debugLevel & DebugMonitorNoFast)) && (dab() && (tacs() || dias()) && dids() && !Pseudo_Tst(rsv | lfs | fre))) {
		// DOE Tacs optimized cycle
		strcpy(opt, "TACS");
		Do_R_State();
		Do_R_State();
		Do_A_State();
		Do_S_State();
		Do_R_State();
		Do_A_State();
		Do_S_State(); 
		//if (Pseudo_Tst(lfs) || Pseudo_Tst(fre)) {
		//	return(1);
		//}
		Do_D_State();
		Do_S_State();
		// last D state in normal loop
	}
	else {
		// full cycle
		Do_R_State();
		Do_D_State();
		Do_A_State();
		Do_S_State();
		Do_C_State();
		Do_T_State();
		Do_L_State();
		Do_SR_State();
		Do_RL_State();
		Do_AA_State();
		Do_PD_State();
		Do_PP_State();
		Do_DC_State();
		Do_DT_State();
		Do_DD_State();
	}
	/*if (StateChanged && (debugLevel & DebugMonitorStates)) {
		cycles++;
		tprintf(buf,"\n{Cycle# %04x, %s, %s, %s, %s, %s, %s , %s, %s , pseudo %08x, chng %01x, %s} ",cycles,R_ist[R_State],D_ist[D_State],A_ist[A_State],S_ist[S_State],C_ist[C_State],T_ist[T_State],L_ist[L_State],AA_ist[AA_State],_pseudoMsg,StateChanged,opt);
		shell_write_console(buf);
	}*/
	return (StateChanged);
}

/*
 * hp-il state machines
 */
void HPIL_Core::Do_R_State(void)
{
// 25/12/2014, safely ignore some state change
uint8_t echo;
uint8_t hold;
	if (pons()) {
		R_State = REIS;
		StateChanged = 1;
	}
	else {
		switch (R_State) {
			case REIS :
				if (Pseudo_Tcl(sync)) {
					R_State = RSYS;
					StateChanged = 1;
				}
				break;
			case RSYS :
				echo = (doe() && !tacs() && !spas() && !dias() && !aias() && !tahs() && !ters() && !lacs() && !nrws() && !cacs())
					|| ((cmd() || idy()) && !cacs() && !csbs())
					|| (arg() && !tads() && !tacs() && !spas() && !dias() && !aias() && !tahs() && !ters() && !lacs() && !cacs() && !csbs())
					|| (aad() && !(aaus() || cacs()))	// add cacs to disable echo of aad when controller
					|| (iaa() && !cacs());
				hold = (doe() && (tacs() || spas() || dias() || aias() || tahs() || ters() || lacs() || nrws() || cacs()))
					|| (rdy() && !arg() && !aag())
					|| ((cmd() || idy()) && (cacs() || csbs()))
					|| (arg() && (tads() || tacs() || spas() || dias() || aias() || tahs() || ters() || lacs() || cacs() || csbs()))
					|| (aad() && (aaus() || cacs()));	// add cacs to hold aad when controler
				if (echo && hold) {
					shell_write_console("Echo / Hold conflicts !!!");
				}
				if (echo && !dtrs()) {
					R_State = RITS;
					StateChanged = 1;
				}
				if (hold) {
					R_State = RCDS;
					StateChanged = 1;
				}
				break;
			case RCDS :
				if (acds() || (nrd() && nrws() && acrs())) {
					R_State = REIS;
					//StateChanged = 1;
				}
				break;
			case RITS :
				if (dtrs()) {
					if (cmd()) {
						R_State = RCDS;
						StateChanged = 1;
					}
					else {
						R_State = REIS;
						// StateChanged = 1;
					}
				}
				break;
		}
	}
}

void HPIL_Core::Do_D_State(void)
{
// 25/12/2014, safely ignore some state change
	if (pons()) {
		D_State = DIDS;
		//StateChanged = 1;
	}
	else {
		switch (D_State) {
			case DIDS :
				if (rits()) {
					D_State = DTRS;
					StateChanged = 1;
					Pseudo_Clr(frtc);
					Pseudo_Set(outf);
				}
				else if (acrs()) {
					D_State = DACS;
					StateChanged = 1;
					_hpil = _repeatHpil;
					Pseudo_Clr(frtc);
					Pseudo_Set(outf);
				}
				else if (sdys() || schs()) {
					D_State = DSCS;
					StateChanged = 1;
					Pseudo_Clr(frtc);
					Pseudo_Set(outf);
				}
				break;
			case DACS :
			case DSCS :
			case DTRS :
				if (Pseudo_Tcl(frtc)) {
					D_State = DIDS;
					//StateChanged = 1;
				}
				break;
		}
	}
}

void HPIL_Core::Do_A_State(void)
{
uint8_t repeat;
uint8_t norepeat;
	if (pons()) {
		A_State = AIDS;
		//StateChanged = 1;
	}
	else {
		switch (A_State) {
			case AIDS :
				if (rcds()) {
					A_State = ACDS;
					StateChanged = 1;
					Pseudo_Set(frav);
					if (doe() && lacs()) {
						if (_deviceListen) {
							if (!(_deviceListen)(_hpil)) {
								// Device not ready, persistent state
								A_State = AIDS;
							}
						}
					}
				}
				break;
			case ACDS :
				repeat = (doe() && (lacs() || nrws() || cacs()))
					|| (rdy() && !sot() && !aag() && !cacs() && !csbs())
					|| (sot() && !tads() && !tacs() && !spas() && !dias() && !aias() && !tahs() && !ters())
					|| (nrd() && !cacs());
				norepeat=(doe() && (tacs() || spas() || dias() || aias() || tahs() || ters()))
					|| cmd() || idy() || aag()
					|| (rdy() && !nrd() && (cacs() || csbs()))
					|| (sot() && (tads() || tacs() || spas() || dias() || aias() || tahs() || ters()))
					|| (nrd() && cacs());
				// troubleshooting
				if (repeat && norepeat) {
					shell_write_console("Repeat conflicts !!!");
					/*Serial.print("Repeat : (doe() && (lacs() || nrws() || cacs()))");
					Serial.println((doe() && (lacs() || nrws() || cacs())),DEC);
					Serial.print("Repeat : (rdy() && !sot() && !aag() && !cacs() && !csbs())");
					Serial.println((rdy() && !sot() && !aag() && !cacs() && !csbs()),DEC);
					Serial.print("Repeat : (sot() && !tads() && !tacs() && !spas() && !dias() && !aias() && !tahs() && !ters())");
					Serial.println((sot() && !tads() && !tacs() && !spas() && !dias() && !aias() && !tahs() && !ters()),DEC);
					Serial.print("Repeat : (nrd() && !cacs())");
					Serial.println((nrd() && !cacs()),DEC);
					Serial.print("norepeat : (doe() && (tacs() || spas() || dias() || aias() || tahs() || ters()))");
					Serial.println((doe() && (tacs() || spas() || dias() || aias() || tahs() || ters())),DEC);
					Serial.print("norepeat : cmd() || idy() || aag()");
					Serial.println(cmd() || idy() || aag(),DEC);
					Serial.print("norepeat : (rdy() && !nrd() && (cacs() || csbs()))");
					Serial.println((rdy() && !nrd() && (cacs() || csbs())),DEC);
					Serial.print("norepeat : (sot() && (tads() || tacs() || spas() || dias() || aias() || tahs() || ters()))");
					Serial.println((sot() && (tads() || tacs() || spas() || dias() || aias() || tahs() || ters())),DEC);
					Serial.print("norepeat : (nrd() && cacs())");
					Serial.println((nrd() && cacs()),DEC);*/
				}
				
				// case repeat && norepeat, do not repeat to avoid endless loop !!! 
				if (norepeat) {
					A_State = AIDS;
					StateChanged = 1;
				}
				else if (repeat) {
					if (!nrws()) {
						A_State = ANRS;
					}
					StateChanged = 1;
					_repeatHpil = _hpil;
				}
				break;
			case ANRS :
				if (ifc() && rcds()) {
					A_State = ACDS;
					StateChanged = 1;
				}
				if (!dacs() && !nacs()) {
					A_State = ACRS;
					StateChanged = 1;
				}
				break;
				case ACRS :
				if (dacs()) {
					A_State = AIDS;
					StateChanged = 1;
				}
				break;
		}
	}
}

void HPIL_Core::Do_S_State(void)
{
uint8_t source;
uint8_t frmrtn;
uint16_t tmp;
	if (pons()) {
		S_State = SIDS;
		StateChanged = 1;
	}
	else {
		source = tacs() || spas() || dias() || aias() || tahs() || ters() || cacs()
			|| nacs() || arss() || aais();
		switch (S_State) {
			case SIDS :
				if (source) {
					S_State = SGNS;
					StateChanged = 1;
				}
				break;
			case SGNS :
				if (!dscs()) {
					// Data
					if (tacs()) {
						if (!_deviceTalk) {
							Pseudo_Set(lfs);
						}
						else {
							tmp = (_deviceTalk)();
							switch (tmp & DeviceMask) {
								case DeviceOK :
									_hpil = tmp;
									Pseudo_Set(nfa);
									break;
								case DeviceLastData :
// end is transmitted in this case
//									_hpil = (tmp & ~DeviceMask) | _END_Mask;
//									Pseudo_Set(nfa);
// end implies ETO in this case
									_hpil = tmp & ~DeviceMask;
									Pseudo_Set(nfa | lfs);
									break;
								case DeviceNoData :
									Pseudo_Set(lfs);
									break;
								default :
									Pseudo_Set(lfs);
							}
						}
					}
					else if (spas() || dias() || aias()) {
						if (!_deviceAltTalk) {
							Pseudo_Set(lfs);
						}
						else {
							tmp = (_deviceAltTalk)();
							switch (tmp & DeviceMask) {
								case DeviceOK :
									_hpil = tmp;
									Pseudo_Set(nfa);
									break;
								case DeviceLastData :
									_hpil = tmp & ~DeviceMask;
									Pseudo_Set(nfa | lfs);
									break;
								case DeviceNoData :
									Pseudo_Set(lfs);
									break;
								default :
									Pseudo_Set(lfs);
							}
						}
					}
					// Internal command
					else if (Pseudo_Tcl(icmd)) {
						_hpil = _internalFrame;
						Pseudo_Set(nfa);
					}
					// Controller command
					else if (Pseudo_Tcl(ccmd)) {
						_hpil = _controllerFrame;
						Pseudo_Set(nfa);
					}
					else if (arss()) {
						_hpil = _IDY_Val | _SRQ_Bit;
						Pseudo_Set(nfa);
					}
					if (Pseudo_Tst(nfa)) {
						S_State = SDYS;
						StateChanged = 1;
						_lastHpil = _hpil;
						if ((doe() || idy()) && (srq() || srss())) {
							_hpil |= _SRQ_Bit;
							Pseudo_Clr(fre);
						}
					}
				}
				if (!source) {
					S_State = SIDS;
					StateChanged = 1;
				}
				break;
			case SDYS :
				if (dscs()) {
					S_State = STRS;
					StateChanged = 1;
					Pseudo_Clr(hshk);
					Pseudo_Clr(nfa);
				}
				break;
			case STRS :
				frmrtn = doe() && acds() && (tacs() || spas() || dias() || aias() || tahs() || ters());
				// doe case, handle it first
				if (frmrtn) {
					if ((_hpil & ~_SRQ_Bit) != (_lastHpil & ~_SRQ_Bit)) {
						Pseudo_Set(fre);
					}
				}
				// should be controller
				else if (cacs() && acds()) {
					frmrtn = rdy() || idy();
					if (_hpil != _lastHpil) {
						// return frame in error
						Pseudo_Set(fre);
						// idy  and aad may be # (optimize to xor + and ?)
						if (((_hpil & _IDY_Mask) == (_lastHpil & _IDY_Mask)) || ((_hpil & _AAD_Mask) == (_lastHpil & _AAD_Mask))) {
							Pseudo_Clr(fre);
						}
					}
					// CMD
					if (!frmrtn) {
						 S_State = SCSS;
						 StateChanged = 1;
					}
					// IDY / RDY, set handshake
					else {
						Pseudo_Set(hshk);
					}
				}
				if (!source) {
					S_State = SIDS;
					StateChanged = 1;
				}
				if ((!Pseudo_Tst(nfa) && frmrtn) || Pseudo_Tcl(lab)){
					S_State = SGNS;
					StateChanged = 1;
				}
				break;
			case SCSS :
				// auto RFC, generate RFC message
				S_State = SCHS;
				StateChanged = 1;
				_hpil = _RFC_Val;
				_lastHpil = _hpil;
				Pseudo_Set(nfa);
				break;
			case SCHS :
				if (dscs() && rfc()) {
					S_State = STRS;
					StateChanged = 1;
					Pseudo_Clr(nfa);
				}
				break;
		}
	}
}

void HPIL_Core::Do_C_State(void)
{
	if (pons() || (ifc() && !Pseudo_Tst(scl) && acds())) {
		C_State = CIDS;
		StateChanged = 1;
	}
	else {
		switch (C_State) {
			case CIDS :
				if ((Pseudo_Tcl(sic) && Pseudo_Tst(scl)) || (tct() && tads() && acds())) {
					C_State = CACS;
					StateChanged = 1;
				}
				break;
			case CACS :
				// modification, add tacs() state transition
				if ((sot() && !tct() && strs()) || Pseudo_Tcl(gts) || tacs()) {
					C_State = CSBS;
					StateChanged = 1;
				}
				if (tct() && strs()) {
					C_State = CTRS;
					StateChanged = 1;
				}
				break;
			case CSBS :
				// modification, add test to remove returning SOT frame step back to CACS when talker !
				//if (sot() && acds() && (tacs() || spas() || dias() || aias()) || (eot() && acds()) || Pseudo_Tcl(gta)) {
				if (((sot() || eot()) && acds()) || Pseudo_Tcl(gta)) {
					C_State = CACS;
					StateChanged = 1;
					Pseudo_Set(hshk);
				}
				break;
			case CTRS :
				if (tct() && acds()) {
					C_State = CACS;
					StateChanged = 1;
				}
				if (!tct() && acds()) {
					C_State = CIDS;
					StateChanged = 1;
				}
				break;
		}
	}
	if (pons()) {
		CS_State = CSNS;
		CE_State = CEIS;
		StateChanged = 1;
	}
	else {
		switch (CS_State) {
			case CSNS :
				if (srq() && (rits() || rcds()) && (cacs() || csbs())) {
					CS_State = CSRS;
					StateChanged = 1;
				}
				break;
			case CSRS :
				if ((doe() || idy()) && !srq() && (rits() || rcds())) {
					CS_State = CSNS;
					StateChanged = 1;
				}
				break;
		}
		switch (CE_State) {
			case CEIS :
				if (ete() && acds() && (cacs() || csbs())) {
					CE_State = CEMS;
					StateChanged = 1;
				}
				break;
			case  CEMS :
				if (cmd() && strs()) {
					CE_State = CEIS;
					StateChanged = 1;
				}
				break;
		}
	}
}


void HPIL_Core::Do_T_State(void)
{
	if (pons() || (ifc() && acds())) {
		T_State = TIDS;
		StateChanged = 1;
	}
	else {
		switch (T_State) {
			case TIDS :
				if ((acds() && mta()) || (cacs() && Pseudo_Tst(tlk))) {
					T_State = TADS;
					StateChanged = 1;
					// reset fre flag in case of past nrd
					Pseudo_Clr(fre);
				}
				break;
			case TADS :
				if (acds()) {
					if (sda()) {
						T_State = TACS;
						StateChanged = 1;
						if (_deviceCmd) {
							(_deviceCmd)(_SDA_Val);
						}
					}
					else if (sdst()) {
						T_State = SPAS;
						StateChanged = 1;
						if (_deviceCmd) {
							(_deviceCmd)(_SST_Val);
						}
					}
					if (sdi()) {
						T_State = DIAS;
						StateChanged = 1;
						if (_deviceCmd) {
							(_deviceCmd)(_SDI_Val);
						}
					}
					if (sai()) {
						T_State = AIAS;
						StateChanged = 1;
						if (_deviceCmd) {
							(_deviceCmd)(_SAI_Val);
						}
					}
					// Exit TDS state
					else if (unt() || ota() || mla()) {
						T_State = TIDS;
						StateChanged = 1;
					}
				}
				// controller local talk
				else if (cacs() && Pseudo_Tcl(tlk)) {
					T_State = TACS;
					StateChanged = 1;
					if (_deviceCmd) {
						(_deviceCmd)(_SDA_Val);
					}
				}
				break;
			case TACS :
			case SPAS :
			case DIAS :
			case AIAS :
				if (Pseudo_Tcl(fre)) {
					T_State = TERS;
					StateChanged = 1;
					_internalFrame = _ETE_Val;
					Pseudo_Set(icmd);
				}
				if (Pseudo_Tcl(lfs) || (nrd() && acds())) {
					T_State = TAHS;
					StateChanged = 1;
					_internalFrame = _ETO_Val;
					Pseudo_Set(icmd);
				}
				break;
			case TAHS :
				if (eto() && strs()) {
					T_State = TADS;
					StateChanged = 1;
				}
				break;
			case TERS :
				if (ete() && strs()) {
					T_State = TADS;
					StateChanged = 1;
				}
				break;
		}
	}
}

void HPIL_Core::Do_L_State(void)
{
	if (pons()) {
		L_State = LIDS;
		StateChanged = 1;
	}
	else {
		switch (L_State) {
			case LIDS :
				if ((acds() && mla()) || (cacs() && Pseudo_Tcl(ltn))) {
					L_State = LACS;
					StateChanged = 1;
					if (_deviceCmd) {
						(_deviceCmd)(_LAD_Val);
					}
				}
				break;
			case LACS :
				if ((acds() && (unl() || ifc() || mta())) || (cacs() && Pseudo_Tcl(lun))) {
					L_State = LIDS;
					StateChanged = 1;
				}
				break;
			}
		}
	if (pons() || (ifc() && acds())) {
		NR_State = NIDS;
		StateChanged = 1;
	}
	else {
		switch (NR_State) {
			case NIDS :
				if ((acds() && eln() && lacs()) || csbs()) {
					NR_State = NENS;
					StateChanged = 1;
				}
				// always clear hlt flg
				Pseudo_Clr(hlt);
				break;
			case NENS :
				if (Pseudo_Tcl(hlt)) {
					NR_State = NRWS;
					StateChanged = 1;
					_internalFrame = _NRD_Val;
					Pseudo_Set(icmd);
				}
				else if (acds() && !eln() && cmd()) {
					NR_State = NIDS;
					StateChanged = 1;
				}
				break;
			case NRWS :
				if (acds() && doe()) {
					NR_State = NACS;
					StateChanged = 1;
				}
				if ((dacs() && doe()) || (acds() && (cmd() || nrd()))) {
					NR_State = NENS;
					StateChanged = 1;
				}
				break;
			case NACS :
				if (rcds() && nrd()) {
					NR_State = NRWS;
					StateChanged = 1;
				}
				break;
		}
	}
}

void HPIL_Core::Do_SR_State(void)
{
	if (pons()) {
		SR_State = SRIS;
		AR_State = ARIS;
		StateChanged = 1;
	}
	else {
		switch (SR_State) {
			case SRIS :
				if (Pseudo_Tst(rsv) && !spas()) {
					SR_State = SRSS;
					StateChanged = 1;
				}
				break;
			case SRSS :
				if (spas()) {
					SR_State = SRHS;
					StateChanged = 1;
				}
				if ((doe() || idy()) && (rits() || acds())) {
					SR_State = SRAS;
					StateChanged = 1;
					_hpil |= _SRQ_Bit;
				}
				if (!Pseudo_Tst(rsv)) {
					SR_State = SRIS;
					StateChanged = 1;
				}
				if (Pseudo_Tcl(arq) && aras()) {
					SR_State = ARSS;
					StateChanged = 1;
				}
				break;
			case SRHS :
				if (!Pseudo_Tst(rsv) && !spas()) {
					SR_State = SRIS;
					StateChanged = 1;
				}
				break;
			case SRAS :
				// Automatic return from this state !
				SR_State = SRSS;
				StateChanged = 1;
				break;
			case ARSS :
				if (idy() && strs()) {
					SR_State = SRSS;
					StateChanged = 1;
					Pseudo_Clr(arq);
				}
				break;
		}
		switch (AR_State) {
			case  ARIS :
				if (ear() && acds()) {
					AR_State = ARAS;
					StateChanged = 1;
				}
				break;
			case ARAS :
				if (ucg() && !ear() && !lpd() && acds()) {
					AR_State = ARIS;
					StateChanged = 1;
				}
				break;
		}
	}
}

void HPIL_Core::Do_RL_State(void)
{
	if (pons()) {
		RL_State = LOCS;
		RS_State = RIDS;
		StateChanged = 1;
	}
	else {
		switch (RL_State) {
			case LOCS :
				if (!Pseudo_Tst(rtl) && mla() && racs() && acds() ) {
					RL_State = REMS;
					StateChanged = 1;
				}
				if (llo() && racs() && acds()) {
					RL_State = LWLS;
					StateChanged = 1;
				}
				break;
			case REMS :
				if (llo() && acds()) {
					RL_State = RWLS;
					StateChanged = 1;
				}
				if (rids() || (gtl() && lacs() && acds()) || Pseudo_Tst(rtl)) {
					RL_State = LOCS;
					StateChanged = 1;
				}
				break;
			case RWLS :
				if (rids()) {
					RL_State = LOCS;
					StateChanged = 1;
				}
				if (gtl()&& lacs() && acds()) {
					RL_State = LWLS;
					StateChanged = 1;
				}
				break;
			case LWLS :
				if (rids()) {
					RL_State = LOCS;
					StateChanged = 1;
				}
				if (mla() && acds()) {
					RL_State = RWLS;
					StateChanged = 1;
				}
				break;
		}
		switch (RS_State) {
			case RIDS :
				if (ren() && acds()) {
					RS_State = RACS;
					StateChanged = 1;
				}
				break;
			case RACS :
				if (nre() && acds()) {
					RS_State = RIDS;
					StateChanged = 1;
				}
				break;
		}
	}
}

void HPIL_Core::Do_AA_State(void)
{
	if (pons() || (aau() && acds() && !cacs())) {
		AA_State = AAUS;
		StateChanged = 1;
		DeviceAddress = _MaxDeviceAddress;
	}
	else {
		switch (AA_State) {
			case AAUS :
				if (aad() && acds()) {
					AA_State = AAIS;
					StateChanged = 1;
					DeviceAddress = _hpil & _MaxDeviceAddress;
					_internalFrame = _hpil == _IAA_Val ? _IAA_Val : _hpil+1;
					Pseudo_Set(icmd);
				}
				break;
			case AAIS :
				if (naa() && strs()) {
					AA_State = AACS;
					StateChanged = 1;
				}
				break;
		}
	}
}

void HPIL_Core::Do_PD_State(void)
{
	if (Pseudo_Tst(pof)) {
		PD_State = POFS;
		StateChanged = 1;
	}
	else {
		switch (PD_State) {
			case POFS :
				if (Pseudo_Tcl(pon) || Pseudo_Tcl(edge)) {
					PD_State = PONS;
					StateChanged = 1;
				}
				break;
			case PONS :
				if (!Pseudo_Tcl(pon) && !Pseudo_Tcl(edge)) {
					PD_State = PUPS;
					StateChanged = 1;
				}
				break;
			case PUPS :
				if (lpd() && !cacs() && acds()) {
					PD_State = PDAS;
					StateChanged = 1;
				}
				break;
			case PDAS :
				if (!lpd() && !rfc() && acds()) {
					PD_State = PUPS;
					StateChanged = 1;
				}
				if (rfc() && acds()) {
					PD_State = PDHS;
					StateChanged = 1;
				}
				break;
			case PDHS :
				if (aids() && dids()) {
					PD_State = POFS;
					StateChanged = 1;
					Pseudo_Clr(edge);
				}
				break;
		}
	}
}

void HPIL_Core::Do_PP_State(void)
{
	if (pons()) {
		PP_State = PPIS;
		StateChanged = 1;
	}
	else {
		switch (PP_State) {
			case PPIS :
				if (ppe() && lacs() && acds()) {
					PP_State = PPSS;
					StateChanged = 1;
					DevicePrq = _hpil & (_PPE_Mask_s || _PPE_Mask_b);
				}
				break;
			case PPSS :
				if ((ppu() || (ppd() && lacs())) && acds()) {
					PP_State = PPIS;
					StateChanged = 1;
				}
				if (idy() && rits()) {
					PP_State = PPAS;
					StateChanged = 1;
					if (Pseudo_Tst(prq) && (DevicePrq & _PPE_Mask_s)) {
						_hpil |= 1 << (DevicePrq,_PPE_Mask_b);
					}
					if (!Pseudo_Tst(prq) && !(DevicePrq & _PPE_Mask_s)) {
						_hpil |= 1 << (DevicePrq,_PPE_Mask_b);
					}
				}
				if (ppe() && lacs() && acds()) {
					DevicePrq = _hpil & (_PPE_Mask_s || _PPE_Mask_b);
				}
				break;
			case PPAS :
				// Automatic return from this state !
				PP_State = PPSS;
				StateChanged = 1;
				break;
		}
	}
}

void HPIL_Core::Do_DC_State(void)
{
	if (pons() ) {
		DC_State = DCIS;
		StateChanged = 1;
	}
	else {
		switch (DC_State) {
			case DCIS :
				if ((dcl() || (sdc() && lacs())) && acds()) {
					DC_State = DCAS;
					StateChanged = 1;
					if (_deviceCmd) {
						(_deviceCmd)(_DCL_Val);
					}
				}
				break;
			case DCAS :
				if (!acds()) {
					DC_State = DCIS;
					StateChanged = 1;
				}
				break;
		}
	}
}

void HPIL_Core::Do_DT_State(void)
{
	if (pons()) {
		DT_State = DTIS;
		StateChanged = 1;
	}
	else {
		switch (DT_State) {
			case DTIS :
				if (get() && lacs() && acds()) {
					DT_State = DTAS;
					StateChanged = 1;
					if (_deviceCmd) {
						(_deviceCmd)(_GET_Val);
					}
				}
				break;
			case DTAS :
				if (!acds()) {
					DT_State = DTIS;
					StateChanged = 1;
				}
				break;
		}
	}
}

void HPIL_Core::Do_DD_State(void)
{
	if (pons()) {
		DD_State = DDIS;
		StateChanged = 1;
	}
	else {
		switch (DD_State) {
			case DDIS :
				if (ddl() && lacs() && acds()) {
					DD_State = DDAS;
					StateChanged = 1;
					if (_deviceCmd) {
						(_deviceCmd)(_hpil);
					}
				}
				else if (ddt() && tads() && acds()) {
					DD_State = DDAS;
					StateChanged = 1;
					if (_deviceCmd) {
						(_deviceCmd)(_hpil);
					}
				}
				break;
			case DDAS :
				if (!acds()) {
					DD_State = DDIS;
					StateChanged = 1;
				}
				break;
		}
	}
}

void HPIL_Core::frameRx(uint16_t rx)
{
	Pseudo_Set(edge | sync);
	_hpil = rx;
}

uint16_t HPIL_Core::frameTx(void)
{
unsigned int tx = -1;
	if (!Pseudo_Tst(frtc)) {
		if (dacs() || dscs() || dtrs()) {
			Pseudo_Set(frtc);
			tx = _hpil;
		}
		else {
			// HP_IL serial out inactivated by driver state machine
		}
	}
	else {
		// HP_IL serial out error, output frame overrun
	}
	return(tx);
}

uint32_t HPIL_Core::pseudoTst(uint32_t msg)
{
	return ((_pseudoMsg & msg) != 0);
}

uint32_t HPIL_Core::pseudoTcl(uint32_t msg)
{
	return (((_pseudoMsg & msg) != 0) ? (_pseudoMsg&=~msg)||1:0);
}

void HPIL_Core::pseudoSet(uint32_t msg)
{
	_pseudoMsg |= msg;
}

void HPIL_Core::pseudoClr(uint32_t msg)
{
	_pseudoMsg &= ~msg;
}
