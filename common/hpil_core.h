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

#ifndef HPIL_Core_h
#define HPIL_Core_h

#if defined(_MSC_VER) || defined (__ANDROID__)
	#ifndef _XINT_XX
		#define _XINT_XX
		typedef unsigned char  uint8_t;
		typedef unsigned short uint16_t;
		typedef unsigned int uint32_t;
	#endif
#else
	// Defaults to Arduino
	#include "Arduino.h"
#endif

#include "FastDelegate.h"
using namespace fastdelegate;

// function pointer to self
// command processing
typedef FastDelegate1<uint16_t, void>			DeviceCmdDelegate;
// send data from device, listen state
typedef FastDelegate1<uint16_t, uint16_t>		DeviceListenDelegate;
// get data from device
typedef FastDelegate0<uint16_t>					DeviceTalkDelegate;

class HPIL_Core
{
public:
	HPIL_Core(void);
	// init core & power on
	void beginCore(void);
	// main loop
	uint8_t process(void);
	// i/o methods
	void frameRx(uint16_t);
	uint16_t frameTx(void);
	// access to pseudomsg
	uint32_t pseudoTst(uint32_t);
	uint32_t pseudoTcl(uint32_t);
	void pseudoSet(uint32_t);
	void pseudoClr(uint32_t);
protected:
	// device functions calls pointer
	DeviceCmdDelegate _deviceCmd;
	DeviceListenDelegate _deviceListen;
	DeviceTalkDelegate _deviceTalk;
	DeviceTalkDelegate _deviceAltTalk;
	// frame local storage
	uint16_t _hpil;						// current frame
	uint16_t _lastHpil;					// last frame for error checking
	uint16_t _repeatHpil;				// hold frame for nrd
	uint16_t _internalFrame;			// internal generated command
	uint16_t _controllerFrame;			// command from controller
	// Pseudo Messages
	uint32_t _pseudoMsg;
	// Receiver states
	uint8_t R_State;
	void Do_R_State(void);
	// Driver states
	uint8_t D_State;
	void Do_D_State(void);
	// Acceptor Handshake states
	uint8_t A_State;
	void Do_A_State(void);
	// Source Handshake states
	uint8_t S_State;
	void Do_S_State(void);
	// Controller states
	uint8_t C_State;
	uint8_t CS_State;
	uint8_t CE_State;
	void Do_C_State(void);
	// Talker states
	uint8_t T_State;
	void Do_T_State(void);
	// Listener states
	uint8_t L_State;
	uint8_t NR_State;
	void Do_L_State(void);
	// Service Request states
	uint8_t SR_State;
	uint8_t AR_State;
	void Do_SR_State(void);
	// Remote Local states
	uint8_t RL_State;
	uint8_t RS_State;
	void Do_RL_State(void);
	// Automatic Address states
	uint8_t AA_State;
	uint8_t DeviceAddress;
	void Do_AA_State(void);
	// Power Down states
	uint8_t PD_State;
	void Do_PD_State(void);
	// Parallel Poll states
	uint8_t PP_State;
	uint8_t DevicePrq;
	void Do_PP_State(void);
	// Device Clear states
	uint8_t DC_State;
	void Do_DC_State(void);
	// Device Trigger states
	uint8_t DT_State;
	void Do_DT_State(void);
	// Device Dependent Commands states
	uint8_t DD_State;
	void Do_DD_State(void);
};

/*
 * masks for device signaling
 */
#define DeviceMask		0xc000
#define DeviceOK		0x0000
#define DeviceNoData	0x4000
#define DeviceLastData	0x8000

/*
 * Pseudo Messages - unsigned 32 bits integer
 */
// power on & power off
#define pon		0x00000001
#define pof		0x00000002
// hp-il input, first edge, sync bit received, last bit received
#define edge	0x00000004
#define sync	0x00000008
#define load	0x00000010
// hp-il output, outgoing frame available, frame transmission complete, frame transmission error
#define outf	0x00000020
#define frtc	0x00000040
#define fre		0x00000080
// device interaction, device ready, received frame available, new frame available, last frame send
#define Rdy		0x00000100
#define frav	0x00000200
#define nfa		0x00000400
#define lfs		0x00000800
// device interaction, halt data transfert
#define hlt		0x00001000
// parallel poll request, serial poll request, asynchronous request
#define prq		0x00002000
#define rsv		0x00004000
#define arq		0x00008000
// system controler, listen, talk, send interface clear, go to active,  go to standby, local abort, internal command pending, controller command pending, set local ready, go to local
#define scl		0x00010000
#define ltn		0x00020000
#define lun		0x00040000
#define tlk		0x00080000
#define sic		0x00100000
#define gta		0x00200000
#define gts		0x00400000
#define lab		0x00800000
#define icmd	0x01000000
#define ccmd	0x02000000
#define srdy	0x04000000
#define rtl		0x08000000
// special variable rfc hanshake completed
#define hshk	0x40000000

/*
 * Pseudo macro utilities
 */
#define Pseudo_Init() _pseudoMsg = 0
#define Pseudo_Tst(msg) ((_pseudoMsg & (msg)) != 0)
#define Pseudo_Tcl(msg) ((_pseudoMsg & (msg)) != 0) ? (_pseudoMsg&=(~msg))||1:0
#define Pseudo_Set(msg) _pseudoMsg |= (msg)
#define Pseudo_Clr(msg) _pseudoMsg &= (~msg)

/*
 * HP-IL messages
 */
// Decoding patterns
// DOE
#define _DOE_Mask	0x0400
#define _DOE_Val	0x0000
#define _DAR_Mask	0x0500
#define _DAR_Val	0x0100	// Service request via DOE			0x1 xxxx xxxx
#define _DAB_Mask	0x00ff
#define _END_Val	0x0200
#define _END_Mask	0x0200
// IDY
#define _IDY_Mask	0x0600	// Service request via IDY			111 xxxx xxxx
#define _IDY_Val	0x0600
#define _IDR_Mask	0x0700
#define _IDR_Val	0x0700	// Service request via IDY			111 xxxx xxxx
// CMD
#define _CMD_Mask	0x0700
#define _CMD_Val	0x0400
// ... ACG
// ...... ACG frames
#define _NUL_Val	0x0400		// NULl command						100 0000 0000
#define _GTL_Val	0x0401		// Go To Local						100 0000 0001
#define _SDC_Val	0x0404		// Selected Device Clear			100 0000 0100
#define _PPD_Val	0x0405		// Parallel Poll Disable			100 0000 0101
#define _GET_Val	0x0408		// Group Execute Trigger			100 0000 1000
#define _ELN_Val	0x040f		// Enable Listener Nor ready		100 0000 1111
#define _PPE_Mask	0x07f0
#define _PPE_Mask_s	0x0008
#define _PPE_Mask_b	0x0007
#define _PPE_Val	0x0480		// Parallel Poll Enable				100 1000 sbbb
#define _DDL_Mask	0x07e0
#define _DDL_Val	0x04a0		// Device Dependant Listen			100 101x xxxx
#define _DDT_Mask	0x07e0
#define _DDT_Val	0x04c0		// Device Dependant Talker			100 110x xxxx
// ... UCG
#define _UCG_Mask	0x0770
#define _UCG_Val	0x0410		// Universal Command Group			100 x001 xxxx
// ...... UCG frames
#define _NOP_Val	0x0410		// No Operation						100 0001 0000
#define _LLO_Val	0x0411		// Local LOckout					100 0001 0001
#define _DCL_Val	0x0414		// Device CLear						100 0001 0100
#define _PPU_Val	0x0415		// Parallel Poll Unconfigure		100 0001 0101
#define _EAR_Val	0x0418		// Enable Asynchronous Requests		100 0001 1000
#define _IFC_Val	0x0490		// InterFace Clear					100 1001 0000
#define _REN_Val	0x0492		// Remote ENable					100 1001 0010
#define _NRE_Val	0x0493		// Not Remote Enable				100 1001 0011
#define _AAU_Val	0x049a		// Auto Address Unconfigure			100 1001 1010
#define _LPD_Val	0x049b		// Loop Power Down					100 1001 1011
// ... LAG
// ...... LAG frames
#define _LAD_Val	0x0420		// Listen ADdress					100	0010 aaaa
#define _MLA_Mask	0x07e0
#define _MLA_Val	0x0420		// My Listen Address				100 001m mmmm
#define _UNL_Val	0x043f		// UNListen							100 0011 1111
// ... TAG
// ...... TAG frames
#define _TAD_Val	0x0440		// Talk ADdress						100 010x xxxx
#define _MTA_Mask	0x07e0
#define _MTA_Val	0x0440		// My Talk Address					100 010m mmmm
#define _UNT_Val	0x045f		// UNTalk							100 0101 1111
// RDY
#define _RDY_Mask	0x0700
#define _RDY_Val	0x0500
// ... RFC frame
#define _RFC_Val	0x0500		// Ready For Command				101 0000 0000
// ... ARG
#define _ARG_Mask	0x07c0
#define _ARG_Val	0x0540		// Addressed Ready Group			101 01xx xxxx
// ...... EOT
#define _EOT_Mask	0x07fe
#define _EOT_Val	0x0540		// End Of Transmission				101 0100 000x
// ......... EOT frames
#define _ETE_Val	0x0541		// End of Transmission, Error		101 0100 0001
#define _ETO_Val	0x0540		// End of Transmission, Ok			101	0100 0000
// ...... NRD frame
#define _NRD_Val	0x0542		// Not Ready for Data				101 0100 0010
// ...... SOT
#define _SOT_Mask	0x07f8
#define _SOT_Val	0x0560		// Start Of Transmission			101 0110 0xxx
// ......... SOT frames
#define _SDA_Val	0x0560		// Send DAta						101 0110 0000
#define _SST_Val	0x0561		// Send Status						101 0110 0001
#define _SDI_Val	0x0562		// Send Device Id					101 0110 0010
#define _SAI_Val	0x0563		// Send Accessory Id				101 0110 0011
#define _TCT_Val	0x0564		// Take ConTrol						101 0110 0100
// ... AAG
#define _AAG_Mask	0x0780
#define _AAG_Val	0x0580		// Auto Address Group				101 1xxx xxxx
// ...... AAG frames
#define _AAD_Mask	0x07e0
#define _AAD_Val	0x0580		// Auto ADdress						101 100a aaaa
#define _NAA_Val	0x0580		// Next Auto Address				101 100n nnnn
#define _IAA_Val	0x059f		// Illegal Address d4..d0			101 1001 1111

// other internal defs
#define _SRQ_Bit	0x0100
#define _MaxDeviceAddress 0x001f

// message testing
//sst > sdst, collision with sst key
#define aad() ((_hpil & _AAD_Mask) == _AAD_Val)
#define aag() ((_hpil & _AAD_Mask) == _AAG_Val)
#define aau() (_hpil == _AAU_Val)
#define arg() ((_hpil & _ARG_Mask) == _ARG_Val)
#define cmd() ((_hpil & _CMD_Mask) == _CMD_Val)
#define dab() ((_hpil & _CMD_Mask) == _DOE_Val)	// data only, nor end or srq !!!
#define dcl() (_hpil ==_DCL_Val)
#define ddl() ((_hpil & _DDL_Mask) == _DDL_Val)
#define ddt() ((_hpil & _DDT_Mask) == _DDT_Val)
#define doe() ((_hpil & _DOE_Mask) == _DOE_Val)
#define ear() (_hpil == _EAR_Val)
#define eln() (_hpil == _ELN_Val)
#define eot() ((_hpil & _EOT_Mask) == _EOT_Val)
#define ete() (_hpil == _ETE_Val)
#define eto() (_hpil == _ETO_Val)
#define get() (_hpil == _GET_Val)
#define gtl() (_hpil == _GTL_Val)
#define iaa() (_hpil == _IAA_Val)
#define idy() ((_hpil & _IDY_Mask) == _IDY_Val)
#define ifc() (_hpil == _IFC_Val)
#define llo() (_hpil == _LLO_Val)
#define lpd() (_hpil == _LPD_Val)
#define mla() ((_hpil != _UNL_Val) && (_hpil == (DeviceAddress | _MLA_Val)))
#define mta() ((_hpil != _UNT_Val) && (_hpil == (DeviceAddress | _MTA_Val)))
#define naa() (_hpil == (_NAA_Val | ((DeviceAddress == _MaxDeviceAddress) ? _MaxDeviceAddress : DeviceAddress + 1)))
#define nrd() (_hpil == _NRD_Val)
#define nre() (_hpil == _NRE_Val)
#define ota() (((_hpil & _MTA_Mask) == _MTA_Val) && !mta())
#define ppd() (_hpil == _PPD_Val)
#define ppe() ((_hpil & _PPE_Mask) == _PPE_Val)
#define ppu() (_hpil == _PPU_Val)
#define rdy() ((_hpil & _RDY_Mask) == _RDY_Val)
#define ren() (_hpil == _REN_Val)
#define rfc() (_hpil == _RFC_Val)
#define sai() (_hpil == _SAI_Val)
#define sda() (_hpil == _SDA_Val)
#define sdc() (_hpil == _SDC_Val)
#define sdi() (_hpil == _SDI_Val)
#define sot() ((_hpil & _SOT_Mask) == _SOT_Val)
#define srq() (((_hpil & _DAR_Mask) == _DAR_Val) || ((_hpil & _IDR_Mask) == _IDR_Val))
#define sdst() (_hpil == _SST_Val)
#define tct() (_hpil == _TCT_Val)
#define ucg() ((_hpil & _UCG_Mask) == _UCG_Val)
#define unl() (_hpil == _UNL_Val)
#define unt() (_hpil == _UNT_Val)

// HP-IL states encoding
// Receiver
#define REIS		0x0001		// REceiver Idle State
#define RSYS		0x0002		// Reveiver SYnc State
#define RITS		0x0003	 	// Receiver Immediate Transfer State
#define RCDS		0x0004		// ReCeiver Data State
// state testing
#define rits() (R_State == RITS)
#define rcds() (R_State == RCDS)

// Driver
#define DIDS		0x0001		// Driver IDle State
#define DACS		0x0002		// Driver transmit from ACceptor State
#define DSCS		0x0003		// Driver transmit from SourCe State
#define DTRS		0x0004		// Driver TRansfer State
// state testing
#define dids() (D_State == DIDS)
#define dacs() (D_State == DACS)
#define dscs() (D_State == DSCS)
#define dtrs() (D_State == DTRS)

// Acceptor handshake
#define AIDS		0x0001		// Acceptor IDle State
#define ACDS		0x0002		// ACceptor Data State
#define ANRS		0x0003		// Acceptor Not Ready State
#define ACRS		0x0004		// ACceptor Ready State
// state testing
#define aids() (A_State == AIDS)
#define acds() (A_State == ACDS)
#define acrs() (A_State == ACRS)

// Source hanshake
#define SIDS		0x0001		// Source IDle State
#define SGNS		0x0002		// Source GeNerate State
#define SDYS		0x0003		// Source DelaY State
#define STRS		0x0004		// Source TRansfer State
#define SCSS		0x0005		// intermediate state (Rfc auto)
#define SCHS		0x0006		// Source Command Handshake State
// state testing
#define sgns() (S_State == SGNS)
#define sdys() (S_State == SDYS)
#define strs() (S_State == STRS)
#define schs() (S_State == SCHS)

// Controller - main
#define CIDS	0x0001			// Controller IDle State
#define CACS	0x0002			// Controller ACtive State
#define CSBS	0x0003			// Controller StandBy State
#define CTRS	0x0004			// Controller TRansfer State
// state testing
#define cids() (C_State == CIDS)
#define cacs() (C_State == CACS)
#define csbs() (C_State == CSBS)
// Controller - service request
#define CSNS	0x0001			// Controller Service Not requested State
#define CSRS	0x0002			// Controller Service Requested State
// Controller - error
#define CEIS	0x0001			// Controller Error Idle State
#define CEMS	0x0002			// Controller Error Mode State

// Talker
#define TIDS	0x0001			// Talket IDle State
#define TADS	0x0002			// Talker ADdressed State
#define TACS	0x0003			// Talker ACtive State
#define SPAS	0x0004			// Serial Poll Active State
#define DIAS	0x0005			// Device Id Active State
#define AIAS	0x0006			// Accessory Id Active State
#define TERS	0x0007			// Talker ERror State
#define TAHS	0x0008			// TAlker Hold State
// state testing
#define tads() (T_State == TADS)
#define tacs() (T_State == TACS)
#define spas() (T_State == SPAS)
#define dias() (T_State == DIAS)
#define aias() (T_State == AIAS)
#define ters() (T_State == TERS)
#define tahs() (T_State == TAHS)

// Listener - main
#define LIDS	0x0001			// Listen IDle State
#define LACS	0x0002			// Listen ACtive State
// state testing
#define lacs() (L_State == LACS)
// Listener - not ready
#define NIDS	0x0001			// Not ready IDle State
#define NENS	0x0002			// Not ready ENabled State
#define NRWS	0x0003			// Not Ready Wait State
#define NACS	0x0004			// Not Ready Active State
// state testing
#define nens() (NR_State == NENS)
#define nrws() (NR_State == NRWS)
#define nacs() (NR_State == NACS)

// Service request - main
#define SRIS	0x0001			// Service Request Idle State
#define SRSS	0x0002			// Service Request Standby State
#define SRHS	0x0003			// Service Request Hold State
#define SRAS	0x0004			// Service Request Active State
#define ARSS	0x0005			// Asynchronous Request Service State
// state testing
#define srss() (SR_State == SRSS)
#define sras() (SR_State == SRAS)
#define arss() (SR_State == ARSS)
// Service request - asynchronous
#define ARIS	0x0001			// Asynchronous Request Idle State
#define ARAS	0x0002			// Asynchronous Request Active State
// state testing
#define aras() (AR_State == ARAS)

// Remote local - main
#define LOCS	0x0001			// LOCal State
#define REMS	0x0002			// REMote State
#define RWLS	0x0003			// Remote With Lockout State
#define LWLS	0x0004			// Local With Lockout State
// Remote local - remote enabled
#define RIDS	0x0001			// Remote IDle State
#define RACS	0x0002			// Remote ACtive State
// state testing
#define rids() (RS_State == RIDS)
#define racs() (RS_State == RACS)

// Auto address
#define AAUS	0x0001			// Auto Address Unconfigured State
#define AAIS	0x0002			// Auto Address Increment State
#define AACS	0x0003			// Auto Address Configured State
// state testing
#define aaus() (AA_State == AAUS)
#define aais() (AA_State == AAIS)

// Power down
#define POFS	0x0001			// Power OFf State
#define PONS	0x0002			// Power ON State
#define PUPS	0x0003			// Power UP State
#define PDAS	0x0004			// Power Down Active State
#define PDHS	0x0005			// Power Down Hold State
// state testing
#define pons() (PD_State == PONS)

// Parallel poll
#define PPIS	0x0001			// Parallel Poll Idle State
#define PPSS	0x0002			// Parallel Poll Standby State
#define PPAS	0x0003			// Parallel Poll Active State
// state testing
#define ppas() (PP_State == PPAS)

// Device clear
#define DCIS	0x0001			// Device Clear Idle State
#define DCAS	0x0002			// Device Clear Active State

// Device trigger #definetion definitions
#define DTIS	0x0001			// Device Trigger Idle State
#define DTAS	0x0002			// Device Trigger Active State

// Device dependant
#define DDIS	0x0001			// Device Dependant Idle State
#define DDAS	0x0002			// Device Dependant Active State

// debug options
#define DebugMonitorFrame	0x01	// Display HPIL frames i/o
#define DebugMonitorStates	0x02	// Display HPIL core states
#define DebugMonitorNoFast	0x04	// Disable DOE LACS/TACS optimized transaction
#define DebugLatencyTiming	0x10	// Display latency btw frame input / output
#define DebugProcessTiming	0x20	// Display global process timing
#define DebugLoopTiming		0x40	// Display one full loop timing (frame input, process dones)
#define DebugStateTiming	0x80	// Display individual state processing timing

#endif