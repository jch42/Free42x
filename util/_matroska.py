# EBML/Matroska parser
# Copyright (C) 2010, 2015  Johannes Sasongko <sasongko@gmail.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
#
# The developers of the Exaile media player hereby grant permission
# for non-GPL compatible GStreamer and Exaile plugins to be used and
# distributed together with GStreamer and Exaile. This permission is
# above and beyond the permissions granted by the GPL license by which
# Exaile is covered. If you modify this code, you may extend this
# exception to your version of the code, but you are not obligated to
# do so. If you do not wish to do so, delete this exception statement
# from your version.


# This code is heavily based on public domain code by "Omion" (from the
# Hydrogenaudio forums), as obtained from Matroska's Subversion repository at
# revision 858 (2004-10-03), under "/trunk/Perl.Parser/MatroskaParser.pm".


from __future__ import print_function


import sys
from struct import pack, unpack
from warnings import warn

MASTER, VINT, INT, STRING, PHLOAT, BOOL, BINARY, SIZE, ARG, MASTER_SIZED = range(10)


class EbmlException(Exception):
    pass


class EbmlWarning(Warning):
    pass


class BinaryData(bytes):
    def __repr__(self):
        return "<BinaryData>"


def bchr(n):
    """chr() that always returns bytes in Python 2 and 3"""
    return pack('B', n)


class Ebml:
    """EBML parser.
    Usage: Ebml(location, tags).parse()
    where `tags` is a dictionary of the form {id: (name, type)}.
    """

    ## Constructor and destructor

    def __init__(self, location, tags):
        self.tags = tags
        self.open(location)

    def __del__(self):
        self.close()

    ## File access.
    ## These can be overridden to provide network support.

    def open(self, location):
        """Open a location and set self.size."""
        self.file = f = open(location, 'rb')
        f = self.file
        f.seek(0, 2)
        self.size = f.tell()
        print("Size is ",self.size)
        f.seek(0, 0)

    def seek(self, offset, mode):
        self.file.seek(offset, mode)

    def tell(self):
        return self.file.tell()

    def read(self, length):
        return self.file.read(length)

    def close(self):
        self.file.close()

    ## Element reading

    def readID(self):
        b = self.read(1)
        b1 = ord(b)
        if b1 & 0b10000000:  # 1 byte
            print("1 byte")
            return b1 & 0b01111111
        elif b1 & 0b01000000:  # 2 bytes
            print("2 bytes")
            return unpack(">H", bchr(b1 & 0b00111111) + self.read(1))[0]
        elif b1 & 0b00100000:  # 3 bytes
            print("3 bytes")
            return unpack(">L", b"\0" + bchr(b1 & 0b00011111) + self.read(2))[0]
        elif b1 & 0b00010000:  # 4 bytes
            print("4 bytes")
            return unpack(">L", bchr(b1 & 0b00001111) + self.read(3))[0]
        elif b1 & 0b00001000:  # 5 bytes
            print("5 bytes")
            return unpack(">Q", b"\0\0\0" + bchr(b1 & 0b00000111) + self.read(4))[0]
        else:
            raise EbmlException("invalid element ID (leading byte 0x%02X)" % b1)

    def readSize(self):
        b1 = ord(self.read(1))
        if b1 & 0b10000000:  # 1 byte
            return b1 & 0b01111111
        elif b1 & 0b01000000:  # 2 bytes
            return unpack(">H", bchr(b1 & 0b00111111) + self.read(1))[0]
        elif b1 & 0b00100000:  # 3 bytes
            return unpack(">L", b"\0" + bchr(b1 & 0b00011111) + self.read(2))[0]
        elif b1 & 0b00010000:  # 4 bytes
            return unpack(">L", bchr(b1 & 0b00001111) + self.read(3))[0]
        elif b1 & 0x00001000:  # 5 bytes
            return unpack(">Q", b"\0\0\0" + bchr(b1 & 0b00000111) + self.read(4))[0]
        elif b1 & 0b00000100:  # 6 bytes
            return unpack(">Q", b"\0\0" + bchr(b1 & 0b0000011) + self.read(5))[0]
        elif b1 & 0b00000010:  # 7 bytes
            return unpack(">Q", b"\0" + bchr(b1 & 0b00000001) + self.read(6))[0]
        elif b1 & 0b00000001:  # 8 bytes
            return unpack(">Q", b"\0" + self.read(7))[0]
        else:
            assert b1 == 0
            raise EbmlException("undefined element size")

    def readInteger(self, length, signed):
        if length == 1:
            value = ord(self.read(1))
        elif length == 2:
            value = unpack(">H", self.read(2))[0]
        elif length == 3:
            value = unpack(">L", b"\0" + self.read(3))[0]
        elif length == 4:
            value = unpack(">L", self.read(4))[0]
        elif length == 5:
            value = unpack(">Q", b"\0\0\0" + self.read(5))[0]
        elif length == 6:
            value = unpack(">Q", b"\0\0" + self.read(6))[0]
        elif length == 7:
            value = unpack(">Q", b"\0" + (self.read(7)))[0]
        elif length == 8:
            value = unpack(">Q", self.read(8))[0]
        else:
            raise EbmlException("don't know how to read %r-byte integer" % length)
        if signed:
            nbits = (8 - length) + 8 * (length - 1)
            if value >= (1 << (nbits - 1)):
                value -= 1 << nbits
        return value

    def readFloat(self, length):
        if length == 4:
            return unpack('>f', self.read(4))[0]
        elif length == 8:
            return unpack('>d', self.read(8))[0]
        else:
            raise EbmlException("don't know how to read %r-byte float" % length)

    ## Parsing

    def parse(self, from_=0, to=None):
        """Parses EBML from `from_` (inclusive) to `to` (exclusive).
        Note that not all streams support seeking backwards, so prepare to handle
        an exception if you try to parse from arbitrary position.
        """
        print("parsing")
        if to is None:
            to = self.size
        self.seek(from_, 0)
        node = {}
        # Iterate over current node's children.
        while self.tell() < to:
            print("Loop")
            try:
                id = self.readID()
                print("Id=",id)
            except EbmlException as e:
                # Invalid EBML header. We can't reliably get any more data from
                # this level, so just return anything we have.
                warn(EbmlWarning(e))
                return node
            try:
                key, type_ = self.tags[id]
                print("Key type=", type_)
                prevId = id
                count = 0
            except KeyError:
                print("Key Error append", id, "PrevId", prevId)
                count += 1
                print("Count is ", count)
                if id == prevId + (count << 4):
                    print("Key series, count", count)
                    key, type_ = self.tags[prevId]
                else:
#                   self.seek(size, 1)
                    print("Not a serie @", self.tell())
                    continue
            try:
                if type_ is MASTER:
                    tell = self.tell()
                    value = self.parse(tell)
                elif type_ is MASTER_SIZED:
                    size = self.readSize()
                    print("Master, size=", size)
                    tell = self.tell()
                    value = self.parse(tell)
                elif type_ is VINT:
                    value = self.readSize()
                    print("Vint =", value)
                elif type_ is INT:
                    size = self.readSize()
                    print("Int, size=", size)
                    value = self.readInteger(size, True)
                elif type_ is STRING:
                    size = self.readSize()
                    print("String, size=", size)
                    value = self.read(size).decode('ascii')
                elif type_ is PHLOAT:
                    size = self.readSize()
                    print("Phloat, size=",size)
                    value = BinaryData(self.read(size))
                elif type_ is BOOL:
                    size = self.readSize()
                    print("Boolean size=", size)
                    value = self.read(size).decode('ascii')
                elif type_ is BINARY:
                    size = self.readSize()
                    print("Binary size=", size)
                    value = BinaryData(self.read(size))
                elif type_ is ARG:
                    size = self.readSize()
                    print("Arg size=", size)
                    tell = self.tell()
                    upto = tell + size
                    while self.tell() < upto:
                        value = self.parse(tell)
                else:
                    assert False, type_
            except (EbmlException, UnicodeDecodeError) as e:
                warn(EbmlWarning(e))
            else:
                try:
                    parentval = node[key]
                except KeyError:
                    parentval = node[key] = []
                parentval.append(value)
        return node


## Matroska-specific code

# Interesting Matroska tags.
# Tags not defined here are skipped while parsing.
MatroskaTags = {
	# Generic
	0x2121: ('EBMLFree42ArgType', VINT),
	0x2131: ('EBMLFree42ArgLength', VINT),
	0x2142: ('EBMLFree42ArgTarget', INT),
	0x2152: ('EBMLFree42ArgVal', INT),
	0x2153: ('EBMLFree42ArgVal', STRING),
	0x2154: ('EBMLFree42ArgVal', PHLOAT),

	# Free42 master
	0x4672EE42: ('EBMLFree42', MASTER),
	0x13: ('EBMLFree42Desc', STRING),
	0x21: ('EBMLFree42Version', VINT),
	0x31: ('EBMLFree42ReadVersion', VINT),

	# Free42 Shell
	0x3000: ('EBMLFree42Shell', MASTER),
	0x3011: ('EBMLFree42ShellVersion', VINT),
	0x3021: ('EBMLFree42ShellReadVersion', VINT),
	0x3033: ('EBMLFree42ShellOS', STRING),
	0x3046: ('EBMLFree42ShellState', BINARY),

	# Free42 Core State
	0x2000: ('EBMLFree42Core', MASTER),
	0x2011: ('EBMLFree42CoreVersion', VINT),
	0x2021: ('EBMLFree42CoreReadVersion', VINT),

	0x21012: ('EL_mode_sigma_reg', INT),
	0x21022: ('EL_mode_goose', INT),
	0x21035: ('EL_mode_time_clktd', BOOL),
	0x21045: ('EL_mode_time_clk24', BOOL),
	0x21053: ('EL_flags', STRING),
	0x21082: ('EL_current_prgm', INT),
	0x21092: ('EL_pc', INT),
	0x210a2: ('EL_prgm_higlight_row', INT),
	0x21103: ('EL_varmenu_label', STRING),
	0x21183: ('EL_varmenu', STRING),
	0x21192: ('EL_varmenu_rows', INT),
	0x211a2: ('EL_varmenu_row', INT),
	0x211b2: ('EL_varmenu_role', INT),
	0x21205: ('EL_core_matrix_singular', BOOL),
	0x21215: ('EL_core_matrix_outofrange', BOOL),
	0x21225: ('EL_core_auto_repeat', BOOL),
	0x21235: ('EL_core_ext_accel', BOOL),
	0x21245: ('EL_core_ext_local', BOOL),
	0x21255: ('EL_core_ext_heading', BOOL),
	0x21265: ('EL_core_ext_time', BOOL),
	0x21275: ('EL_core_ext_hpil', BOOL),
	0x21305: ('EL_mode_clall', BOOL),
	0x21315: ('EL_mode_command_entry', BOOL),
    0x21325: ('EL_mode_number_entry', BOOL),
    0x21335: ('EL_mode_alpha_entry', BOOL),
    0x21345: ('EL_mode_shift', BOOL),
    0x21352: ('EL_mode_appmenu', INT),
    0x21362: ('EL_mode_plainmenu', INT),
    0x21375: ('EL_mode_plainmenu_sticky', BOOL),
    0x21382: ('EL_mode_transientmenu', INT),
    0x21392: ('EL_mode_alphamenu', INT),
    0x213a2: ('EL_mode_commandmenu', INT),
    0x213b5: ('EL_mode_running', BOOL),
	0x213c5: ('EL_mode_varmenu', BOOL),
    0x213d5: ('EL_mode_updown', BOOL),
	0x213e5: ('EL_mode_getkey', BOOL),
	0x21404: ('EL_entered_number', PHLOAT),
	0x21413: ('EL_entered_string', STRING),
	0x21482: ('EL_pending_command', INT),
	0x21490: ('EL_pending_command_arg', ARG),
	0x214a2: ('EL_xeq_invisible', INT),
	0x21502: ('EL_incomplete_command', INT),
	0x21512: ('EL_incomplete_ind', INT),
    0x21522: ('EL_incomplete_alpha', INT),
    0x21532: ('EL_incomplete_length', INT),
    0x21542: ('EL_incomplete_maxdigits', INT),
    0x21552: ('EL_incomplete_argtype', INT),
    0x21562: ('EL_incomplete_num', INT),
    0x21573: ('EL_incomplete_str', STRING),
    0x21582: ('EL_incomplete_saved_pc', INT),
    0x21592: ('EL_incomplete_saved_highlight_row', INT),
    0x215a3: ('EL_cmdline', STRING),
	0x215b2: ('EL_cmdline_row', INT),
	0x21602: ('EL_matedit_mode', INT),
    0x21613: ('EL_matedit_name', STRING),
    0x21622: ('EL_matedit_i', INT),
    0x21632: ('EL_matedit_j', INT),
    0x21642: ('EL_matedit_prev_appmenu', INT),
    0x21683: ('EL_input_name', STRING),
    0x21690: ('EL_input_arg', ARG),
	0x21702: ('EL_baseapp', INT),
	0x21742: ('EL_random_number1', INT),
	0x21752: ('EL_random_number2', INT),
	0x21762: ('EL_random_number3', INT),
	0x21772: ('EL_random_number4', INT),
	0x21802: ('EL_deferred_print', INT),
	0x21882: ('EL_keybuf_head', INT),
	0x21892: ('EL_keybuf_tail', INT),
	0x218a2: ('EL_rtn_sp', INT),
	0x21902: ('EL_keybuf', INT),
	0x21a02: ('EL_rtn_prgm', INT),
	0x21c02: ('EL_rtn_pc', INT),
	0x2fff4: ('EL_off_enable_flag', BOOL),

	# Math solver
	0x23002: ('EL_solveVersion', INT),
	0x23013: ('EL_solvePrgm_name', STRING),
	0x23023: ('EL_solveActive_prgm_name', STRING),
	0x23032: ('EL_solveKeep_running', INT),
	0x23042: ('EL_solvePrev_prgm', INT),
	0x23052: ('EL_solvePrev_pc', INT),
	0x23062: ('EL_solveState', INT),
	0x23072: ('EL_solveWhich', INT),
	0x23082: ('EL_solveToggle', INT),
	0x23092: ('EL_solveRetry_counter', INT),
	0x230a4: ('EL_solveRetry_value', PHLOAT),
	0x230b4: ('EL_solveX1', PHLOAT),
	0x230c4: ('EL_solveX2', PHLOAT),
	0x230d4: ('EL_solveX3', PHLOAT),
	0x230e4: ('EL_solveFx1', PHLOAT),
	0x230f4: ('EL_solveFx2', PHLOAT),
	0x23104: ('EL_solvePrev_x', PHLOAT),
	0x23124: ('EL_solveCurr_x', PHLOAT),
	0x23134: ('EL_solveCurr_f', PHLOAT),
	0x23144: ('EL_solveXm', PHLOAT),
	0x23154: ('EL_solveFxm', PHLOAT),
	0x23162: ('EL_solveLast_disp_time', INT),
	0x23403: ('EL_solveShadow_name', STRING),
	0x23804: ('EL_solveShadow_value', PHLOAT),

	# Math integrator
	0x24002: ('El_integVersion', INT),
	0x24013: ('EL_integPrgm_name', STRING),
	0x24023: ('EL_integActive_prgm_name', STRING),
	0x24033: ('EL_integVar_name', STRING),
	0x24042: ('EL_integKeep_running', INT),
	0x24052: ('EL_integPrev_prgm', INT),
	0x24062: ('EL_integPrev_pc', INT),
	0x24072: ('EL_integState', INT),
	0x24084: ('EL_integLlim', PHLOAT),
	0x24094: ('EL_integUlim', PHLOAT),
	0x240a4: ('EL_integAcc', PHLOAT),
	0x240b4: ('EL_integA', PHLOAT),
	0x240c4: ('EL_integB', PHLOAT),
	0x240d4: ('EL_integEps', PHLOAT),
	0x240e2: ('EL_integN', INT),
	0x240f2: ('EL_integM', INT),
	0x24102: ('EL_integI', INT),
	0x24112: ('EL_integK', INT),
	0x24124: ('EL_integH', PHLOAT),
	0x24134: ('EL_integSum', PHLOAT),
	0x24142: ('EL_integNsteps', INT),
	0x24154: ('EL_integP', PHLOAT),
	0x24164: ('EL_integT', PHLOAT),
	0x24174: ('EL_integU', PHLOAT),
	0x24184: ('EL_integPrev_int', PHLOAT),
	0x24194: ('EL_integPrev_res', PHLOAT),
	0x24404: ('EL_integC', PHLOAT),
	0x24804: ('EL_integS', PHLOAT),

	# Display
	0x22002: ('EL_display_catalogmenu_section', INT),
	0x22102: ('EL_display_catalogmenu_rows', INT),
	0x22202: ('EL_display_catalogmenu_row', INT),
	0x22402: ('EL_display_catalogmenu_item', INT),
	0x22803: ('EL_display_custommenu', STRING),
	# Arg not as a serie...
	0x22c00: ('EL_display_progmenu_arg0', ARG),
	0x22c10: ('EL_display_progmenu_arg1', ARG),
	0x22c20: ('EL_display_progmenu_arg2', ARG),
	0x22c30: ('EL_display_progmenu_arg3', ARG),
	0x22c40: ('EL_display_progmenu_arg4', ARG),
	0x22c50: ('EL_display_progmenu_arg5', ARG),
	0x22c60: ('EL_display_progmenu_arg6', ARG),
	0x22c70: ('EL_display_progmenu_arg7', ARG),
	0x22c80: ('EL_display_progmenu_arg8', ARG),
	0x22c90: ('EL_display_progmenu_arg9', ARG),
	0x22d02: ('EL_display_progmenu_is_gto', INT),
	0x22e03: ('EL_display_progmenu', STRING),
	0x22f03: ('EL_display', STRING),
	0x22f12: ('EL_display_appmenu_exitcallback', INT),

	# HPIL
	0x2e002: ('EL_hpil_selected', INT),
	0x2e012: ('EL_hpil_print', INT),
	0x2e022: ('EL_hpil_disk', INT),
	0x2e032: ('EL_hpil_plotter', INT),
	0x2e042: ('EL_hpil_prtAid', INT),
	0x2e052: ('EL_hpil_dskAid', INT),
	0x2e065: ('EL_hpil_modeEnabled', BOOL),
	0x2e075: ('EL_hpil_modeTransparent', BOOL),
	0x2e085: ('EL_hpil_modePIL_Box', BOOL),

	# A variable
	0x400: ('EBMLFree42VarNull', MASTER_SIZED),
	0x410: ('EBMLFree42VarReal', MASTER_SIZED),
	0x420: ('EBMLFree42VarCpx', MASTER_SIZED),
	0x430: ('EBMLFree42VarRMtx', MASTER_SIZED),
	0x440: ('EBMLFree42VarCMtx', MASTER_SIZED),
	0x450: ('EBMLFree42VarStr', MASTER_SIZED),
	0x523: ('EBMLFree42VarName', STRING),
	0x532: ('EBMLFree42VarRows', INT),
	0x542: ('EBMLFree42VarColumns', INT),
# achtung, potential bug, VarNoType !!!
	0x580: ('EBMLFree42VarNoType', INT),
	0x583: ('EBMLFree42VarString', STRING),
	0x584: ('EBMLFree42VarPhloat', PHLOAT),

	# Variables
	0x4000: ('EBMLFree42Vars', MASTER),
	0x4011: ('EBMLFree42VarsVersion', VINT),
	0x4021: ('EBMLFree42VarsReadVersion', VINT),
	0x4031: ('EBMLFree42VarsCount', VINT),

	# A program
	0x600: ('EBMLFree42Prog', MASTER_SIZED),
	0x623: ('EBMLFree42ProgName', STRING),
	0x633: ('EBMLFree42ProgData', STRING),

	# Programs
	0x6000: ('EBMLFree42Progs', MASTER),
	0x6011: ('EBMLFree42ProgsVersion', VINT),
	0x6021: ('EBMLFree42ProgsReadVersion', VINT),
	0x6031: ('EBMLFree42ProgsCount', VINT),

}

def parse(location):
    return GioEbml(location, MatroskaTags).parse()


def dump(location):
    from pprint import pprint

    pprint(parse(location))


def dump_tags(location):
    from pprint import pprint

    mka = parse(location)
    segment = mka['Segment'][0]
    info = segment['Info'][0]
    try:
        timecodescale = info['TimecodeScale'][0]
    except KeyError:
        timecodescale = 1000000
    length = info['Duration'][0] * timecodescale / 1e9
    print("Length = %s seconds" % length)
    pprint(segment['Tags'][0]['Tag'])


def gio_location(location):
    """Convert location to GIO-compatible location.
    This works around broken behaviour in the Win32 GIO port (it converts paths
    into UTF-8 and requires them to be specified in UTF-8 as well).
    :type location: str
    :rtype: bytes
    """
    if sys.platform == 'win32' and '://' not in location:
        if isinstance(location, bytes):
            # Decode the path according to the FS encoding to get the Unicode
            # representation first. If the path is in a different encoding,
            # this step will fail.
            location = location.decode(sys.getfilesystemencoding())
        location = location.encode('utf-8')
    return location


if __name__ == '__main__':
    location = gio_location(sys.argv[1])
    dump_tags(location)


# vi: et sts=4 sw=4 ts=4