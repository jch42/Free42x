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

#include "hpil_controller.h"

HPIL_Controller::HPIL_Controller(void)
{
}

void HPIL_Controller::begin(HpilXController* p)
{
	_hpilXController = p;
	_deviceCmd = NULL;
	_deviceTalk = MakeDelegate(this, &HPIL_Controller::dataTalk);
	_deviceListen = MakeDelegate(this, &HPIL_Controller::dataListen);
	_deviceAltTalk = NULL;
	beginCore();
	Pseudo_Set(scl | sic);
	AA_State = AACS;
	DeviceAddress = 0;
	while(process());
}

void HPIL_Controller::controllerCmd(uint16_t cmd)
{
	if (cmd & Local) {
		Pseudo_Set( cmd & ~Local);
	}
	else {
		Pseudo_Set(ccmd);
		_controllerFrame = cmd;
	}
}

// fillup receive buffer
uint16_t HPIL_Controller::dataListen(uint16_t data)
{
uint16_t rdy = 1;	// set for non blocking loop operation
int bufTrim = 0;
	if (_hpilXController->bufPtr < _hpilXController->bufSize) {
		_hpilXController->buf[_hpilXController->bufPtr++] = (uint8_t)data;
		if ((_hpilXController->statusFlags & ListenTilEnd) && (data & _END_Val)) {
				Pseudo_Set(hlt);
				_hpilXController->statusFlags &= ~ListenTilEnd;
				_hpilXController->statusFlags |= FullListenBuf;
		}
		if (_hpilXController->statusFlags & ListenTilCrLf) {
			if (_hpilXController->buf[_hpilXController->bufPtr - 1] == 0x0d) {
				bufTrim++;
			}
			else if (_hpilXController->buf[_hpilXController->bufPtr - 1] == 0x0a) {
				Pseudo_Set(hlt);
				bufTrim++;
				_hpilXController->statusFlags &= ~ListenTilCrLf;
				_hpilXController->statusFlags |= FullListenBuf;
			}
		}
		if (_hpilXController->statusFlags & ListenTilChar) {
			if (_hpilXController->buf[_hpilXController->bufPtr - 1] == _hpilXController->endChar) {
				Pseudo_Set(hlt);
				bufTrim++;
				_hpilXController->statusFlags &= ~ListenTilChar;
				_hpilXController->statusFlags |= FullListenBuf;
			}
		}
		if (bufTrim) {
			_hpilXController->bufPtr--;
		}
		if ((_hpilXController->bufPtr == _hpilXController->bufSize)) {
			if (!(_hpilXController->statusFlags & RunAgainListenBuf)) {
				Pseudo_Set(hlt);
			}
			_hpilXController->statusFlags |= FullListenBuf;
		}
	}
	else {
		Pseudo_Set(hlt);
		_hpilXController->statusFlags |= OverListenBuf;
	}
	return (rdy);
}

// transmit buffer
uint16_t HPIL_Controller::dataTalk(void)
{
uint16_t data = DeviceNoData;;
	if (_hpilXController->bufPtr < _hpilXController->bufSize) {
		data = _hpilXController->buf[_hpilXController->bufPtr++];
		if (_hpilXController->bufPtr == _hpilXController->bufSize) {
			if (!(_hpilXController->statusFlags & RunAgainTalkBuf)) {
				if (_hpilXController->statusFlags & LastIsEndTalkBuf) {
					_hpilXController->statusFlags &= ~LastIsEndTalkBuf;
					data |= _END_Val;
				}
			data |= DeviceLastData;
			}
			_hpilXController->statusFlags |= EmptyTalkBuf;
		}
	}
	else {
		_hpilXController->statusFlags |= UnderTalkBuf;
	}
	return(data);
}
