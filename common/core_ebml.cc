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

#include "core_globals.h"
#include "core_ebml.h"
#include "core_helpers.h"
#include "core_main.h"
#include "core_tables.h"
#include "core_variables.h"
#include "shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef BCD_MATH
// We need these locally for BID128<->double conversion
#include "bid_conf.h"
#include "bid_functions.h"
#endif

ebmlElement_Struct elCurrent;

/*
 * Track shared matrix arrays
 */
static int array_count;
static int array_list_capacity;
static void **array_list;

static bool array_list_grow() {
void **p;
    if (array_count < array_list_capacity) {
        return true;
    }
        array_list_capacity += 10;
        p = (void **) realloc(array_list, array_list_capacity * sizeof(void *));
    if (p == NULL) {
        return false;
    }
    array_list = p;
    return true;
}

static int array_list_search(void *array) {
int i;
    for (i = 0; i < array_count; i++) {
        if (array_list[i] == array) {
            return i;
        }
    }
    return -1;
}

/*
 * get vint size
 */
int ebmlGetVIntSz(unsigned int i) {
int sz;
    sz = 0;
    if (i < 126) {
        sz = 1;
    }
    else if (i < 16382) {
        sz = 2;
    }
    else if (i < 2097150) {
        sz = 3;
    }
    else if (i < 268435454) {
        sz = 4;
    }
    return sz;
}

/*
 * append vint value to buffer
 */
int ebml2VInt(unsigned int i, unsigned char* b) {
int size=0;
    if (i < 126) {
        size = 1;
        b[0] = (unsigned char)(i | 0x80);
    }
    else if (i < 16382) {
        size = 2;
        b[0] = (unsigned char)((i >> 8) | 0x40);
        b[1] = (unsigned char)(i & 0xff);
    }
    else if (i < 2097150) {
        size = 3;
        b[0] = (unsigned char)((i >> 16) | 0x20);
        b[1] = (unsigned char)((i >> 8) & 0xff);
        b[2] = (unsigned char)(i & 0xff);
    }
    else if (i < 268435454) {
        size = 4;
        b[0] = (unsigned char)((i >> 24) | 0x10);
        b[1] = (unsigned char)((i >> 16) & 0xff);
        b[2] = (unsigned char)((i >> 8) & 0xff);
        b[3] = (unsigned char)(i & 0xff);
    }
    else {
        size = 5;
        b[0] = (unsigned char)(0x08);
        b[1] = (unsigned char)((i >> 24) & 0xff);
        b[2] = (unsigned char)((i >> 16) & 0xff);
        b[3] = (unsigned char)((i >> 8)  & 0xff);
        b[4] = (unsigned char)(i & 0xff);
    }
    return size;
}

/*
 * append vint representation of element to buffer
 */
int ebmlWriteVInt(int elId, unsigned int i, unsigned char *b) {
int j, sz;
    sz = 0;
    /* write Id */
    j = ebml2VInt((elId & ~0x0f) + EBMLFree42VIntElement, &b[0]);
    if (j == 0) {
        return j;
    }
    sz += j;
    /* write value */
    j = ebml2VInt(i, &b[sz]);
    if (j == 0) {
        return j;
    }
    sz += j;
    return sz;
}

/*
 * append integer representation of element to buffer
 */
int ebmlWriteInt(int elId, int i, unsigned char *b) {
int j, sz;
    sz = 0;
    /* write Id */
    j = ebml2VInt((elId & ~0x0f) + EBMLFree42IntElement, &b[sz]);
    if (j == 0) {
        return j;
    }
    sz += j;
    j = ebml2VInt(sizeof(i), &b[sz]);
    if (j == 0) {
        return false;
    }
    sz += j;
    for (j = 0; j < sizeof(i); j++) {
        b[sz++] = (unsigned char)((i >> ((sizeof(i) - j - 1) * 8)) & 0xff);
    }
    return sz;
}

/*
 * append string representation of element to buffer
 */
int ebmlWriteString(int elId, int l, char* a, unsigned char *b) {
int i, j, sz;
    sz = 0;
    /* write Id */
    j = ebml2VInt((elId & ~0x0f) + EBMLFree42StringElement, &b[0]);
    if (j == 0) {
        return j;
    }
    sz += j;
    /* write size */
    j = ebml2VInt((unsigned int)(l), &b[sz]);
    if (j == 0) {
        return j;
    }
    sz += j;
    for (i = 0; i < l; i++) {
        b[sz++] = (unsigned char) a[i];
    }
    return sz;
}

/*
 * append phloat representation of element to buffer
 */
int ebmlWritePhloat(int elId, phloat* p, unsigned char *b) {
int j, sz;
    sz = 0;
    /* write id & size */
    j = ebml2VInt(elId, &b[sz]);
    if (j == 0) {
        return false;
    }
    sz += j;
    j = ebml2VInt(sizeof(phloat), &b[sz]);
    if (j == 0) {
        return false;
    }
    sz += j;
#ifdef BCD_MATH
    memcpy(&b[sz], &p->val, sizeof(phloat));
#else
    binary64_to_bid128((BID_UINT128*)&b[sz], p);
#endif
    sz += sizeof(phloat);
    return sz;
}

/*
 * write sized element header
 * - write element Id
 * - write size value
 * - write buffer
 */
bool ebmlFlushSizedEl(int elId, int elSz, int bufSz, unsigned char *buf) {
unsigned char elBuf[16];
int sz, j;
    j = ebml2VInt(elId, &elBuf[0]);
    if (j == 0) {
        return false;
    }
    sz = j;
    if (elSz < 0) {
        // unsized element
        elBuf[sz] = 0xff;
        j = 1;
    }
    else {
        j = ebml2VInt(elSz, &elBuf[sz]);
    }   
    if (j == 0) {
        return false;
    }
    sz += j;
    if (!shell_write_saved_state(elBuf, sz)) {                      
        return false;
    }
    if (!shell_write_saved_state(buf, bufSz)) {                         
        return false;
    }
    return true; 
}

/*
 * turns an stack variable to a named variable
 */
bool ebmlWriteReg(vartype *v, char reg) {
var_struct namedReg;
vartype dummyVarType;
    namedReg.length = 1;
    namedReg.name[0] = reg;
    if (v == 0) {
        // variable may not have been initalized yet, write Null type
        dummyVarType.type = TYPE_NULL;
        namedReg.value = &dummyVarType;
    }
    else {
        namedReg.value = v;
    }
    return ebmlWriteVar(&namedReg);
}

/*
 * turns alpha reg to a named variable
 */
bool ebmlWriteAlphaReg() {
unsigned char buf[64];
int sz, j;
char reg;
    sz = 0;
    reg = 'A';
    /* write variable name */
    j = ebmlWriteString(EBMLFree42VarName, 1, &reg, &buf[0]);
    if (j == 0) {
        return false;
    }
    sz += j;
    /* write alpha_reg */
    j = ebmlWriteString(EBMLFree42VarString, reg_alpha_length, reg_alpha, &buf[sz]);
    if (j == 0) {
        return false;
    }
    sz += j;
    return ebmlFlushSizedEl(EBMLFree42VarStr, sz, sz, buf);
}

/*
 * stores var as ebml subdocument
 */
bool ebmlWriteVar(var_struct *v) {
unsigned char buf[64];
int sz, szEl, elId;
vartype_real *r;
vartype_complex *c;
vartype_string *s;
vartype_realmatrix *rm;
vartype_complexmatrix *cm;
int n, msz;  
int type, i, j;
    type = v->value->type;
    elId = EBMLFree42VarNull + (type << 4);
    // Variable object - common header: ElId, Size, [ ElName[
    j = ebmlWriteString(EBMLFree42VarName,(unsigned int)v->length, v->name, &buf[0]);
    if (j == 0) {
        return false;
    }
    sz = j;
    // Variables body
    switch (type) {
        case TYPE_NULL :
            // body: ]
            szEl = sz;
            break;
        case TYPE_REAL :
            // body: ElPhloat]
            r = (vartype_real *) v->value;
            j = ebmlWritePhloat(EBMLFree42VarPhloat, &r->x, &buf[sz]);
            if (j == 0) {
                return false;
            }
            sz += j;
            szEl = sz;
            break;
        case TYPE_COMPLEX:
            // body: phloat, phloat]
            c = (vartype_complex *) v->value;
            j = ebmlWritePhloat(EBMLFree42VarPhloat, &c->re, &buf[sz]);
            if (j == 0) {
                return false;
            }
            sz += j;
            j = ebmlWritePhloat(EBMLFree42VarPhloat, &c->im, &buf[sz]);
            if (j == 0) {
                return false;
            }
            sz += j;
            szEl = sz;
            break;
        case TYPE_STRING:
            // body: string length, string]
            s = (vartype_string *) v->value;
            j = ebmlWriteString(EBMLFree42VarString, s->length, s->text, &buf[sz]);
            if (j == 0) {
                return false;
            }
            sz += j;
            szEl = sz;
            break;
        case TYPE_REALMATRIX:
            // body: rows, columns, phloat[], char[]]
            rm = (vartype_realmatrix *) v->value;
            msz = rm->rows * rm->columns;
            if (rm->array->refcount > 1) {
                n = array_list_search(rm->array);
                if (n == -1) {
                    // A negative row count signals a new shared matrix
                    rm->rows = -rm->rows;
                    if (!array_list_grow()) {
                        return false;
                    }
                    array_list[array_count++] = rm->array;
                }
                else {
                    // A zero row count means this matrix shares its data
                    // with a previously written matrix
                    rm->rows = 0;
                    rm->columns = n;
                    msz = 0;
                }
            }
            j = ebmlWriteInt(EBMLFree42VarRows, rm->rows, &buf[sz]);
            if (j == 0) {
                return false;
            }
            sz += j;
            j = ebmlWriteInt(EBMLFree42VarColumns, rm->columns, &buf[sz]);
            if (j == 0) {
                return false;
            }
            sz += j;
            // for real matrix, need to scan all matrix to get variable size
            szEl = sz;
            for (i = 0; i < msz; i++) {
                if (rm->array->is_string[i]) {
                    szEl += EbmlStringSZ + phloat_length(rm->array->data[i]);
                }
                else {
                    szEl += EbmlPhloatSZ;
                }
            }
            break;
        case TYPE_COMPLEXMATRIX:
            // body: rows, columns, phloat[x2]]
            cm = (vartype_complexmatrix *) v->value;
            msz = cm->rows * cm->columns;
            if (cm->array->refcount > 1) {
                n = array_list_search(cm->array);
                if (n == -1) {
                    // A negative row count signals a new shared matrix
                    cm->rows = -cm->rows;
                    if (!array_list_grow()) {
                        return false;
                    }
                    array_list[array_count++] = cm->array;
                }
                else {
                    // A zero row count means this matrix shares its data
                    // with a previously written matrix
                    cm->rows = 0;
                    cm->columns = n;
                    msz = 0;
                }
            }
            j = ebmlWriteInt(EBMLFree42VarRows, cm->rows, &buf[sz]);
            if (j == 0) {
                return false;
            }
            sz += j;
            j = ebmlWriteInt(EBMLFree42VarColumns, cm->columns, &buf[sz]);
            if (j == 0) {
                return false;
            }
            sz += j;
            szEl = sz + msz * (EbmlPhloatSZ * 2);
            break;
        default:
            /* Should not happen */
            return false;
    }
    if (!ebmlFlushSizedEl(elId, szEl, sz, buf)) {
        return false;
    }
    sz = 0;
    if (type == TYPE_REALMATRIX) {
        for (i = 0; i < msz; i++) {
            if (rm->array->is_string[i]) {
                j = ebmlWriteString(EBMLFree42VarString, phloat_length(rm->array->data[i]), phloat_text(rm->array->data[i]), &buf[sz]);
            }
            else {
                j = ebmlWritePhloat(EBMLFree42VarPhloat, &rm->array->data[i], &buf[sz]);
            }
            if (j == 0) {
                return false;
            }
            sz += j;
            if ((sz >= (sizeof(buf) - EbmlPhloatSZ)) || (i == msz - 1)) {
                if (!shell_write_saved_state(buf, sz)) {
                    return false;
                }
                sz = 0;
            }
        }
    }
    else if (type == TYPE_COMPLEXMATRIX) {
        for (i = 0; i < msz * 2; i++) {
            j = ebmlWritePhloat(EBMLFree42VarPhloat, &cm->array->data[i], &buf[sz]);
            if (j == 0) {
                return false;
            }
            sz += j;
            if ((sz >= (sizeof(buf) - EbmlPhloatSZ)) || (i == 2 * msz - 1)) {
                if (!shell_write_saved_state(buf, sz)) {
                    return false;
                }
                sz = 0;
            }
        }
    }
    return true;
}

/*
 * stores program as ebml subdocument
 */
bool ebmlWriteProgram(int prgm_index, prgm_struct *p) {
unsigned char buf[64];
int i, j, sz, szPgm;
int length;
char c;
int done;
int4 pc;
    // core_program_size ignore (implicit ?) END...
    szPgm = core_program_size(prgm_index) + 3;
    length = 0;
    for (i = 0; i < labels_count; i++) {
        if (labels[i].prgm == prgm_index) {
            length = labels[i].length;
            break;
        }
    }
    sz = 0;
    j = ebmlWriteString(EBMLFree42ProgName, length, length != 0 ? labels[i].name : &c, &buf[sz]);
    if (j == 0) {
        return false;
    }
    sz += j;
    j = ebml2VInt(EBMLFree42Prog_size, &buf[sz]);
    if (j == 0) {
        return false;
    }
    sz += j;
    p += prgm_index;
    j = ebml2VInt(p->size, &buf[sz]);
    if (j == 0) {
        return false;
    }
    sz += j;
    j = ebml2VInt(EBMLFree42Prog_text, &buf[sz]);
    if (j == 0) {
        return false;
    }
    sz += j;
    j = ebml2VInt(szPgm, &buf[sz]);
    if (j == 0) {
        return false;
    }
    sz += j;
    if (!ebmlFlushSizedEl(EBMLFree42Prog, sz + szPgm, sz, buf)) {
        return false;
    }

    current_prgm = prgm_index;
    pc = 0;
    sz = 0;
    do {
        sz = 0;
        done = core_Free42To42(&pc, buf, &sz);
        if (!shell_write_saved_state(buf, sz)) {
            return false;
        }
    } while (!done);
    return true;
}

/*
 * write element as Vinteger
 */
bool ebmlWriteElVInt(unsigned int elId, unsigned int val) {
unsigned char buf[16];
int j, sz;
    sz = 0;
    j = ebml2VInt((elId & ~0x0f) + EBMLFree42VIntElement, &buf[0]);
    if (j == 0) {
        return false;
    }
    sz += j;
    j = ebml2VInt(val, &buf[sz]);
    if (j == 0) {
        return false;
    }
    sz += j;
    if (!shell_write_saved_state(buf, sz)) {
        return false;
    }
    return true; 
}

/*
 * write element as integer
 */
bool ebmlWriteElInt(unsigned int elId, int val) {
unsigned char buf[16];
int j;
    for (j = 0; j < sizeof(val); j++) {
        buf[j] = (unsigned char)((val >> ((sizeof(val) - j - 1) * 8)) & 0xff);
    }
    return ebmlFlushSizedEl((elId & ~0x0f) + EBMLFree42IntElement, j, j, buf);
}

/*
 * write element as string
 */
bool ebmlWriteElString(unsigned int elId, int l, char *val) {
    return ebmlFlushSizedEl((elId & ~0x0f) + EBMLFree42StringElement, l, l, (unsigned char *)val);
}

/*
 * write element as phloat
 */
bool ebmlWriteElPhloat(unsigned int elId, phloat* p) {
unsigned char buf[sizeof(phloat)];
#ifdef BCD_MATH
    memcpy(buf, &p->val, sizeof(phloat));
#else
    binary64_to_bid128((BID_UINT128*)buf, p);
#endif
    return ebmlFlushSizedEl((elId & ~0x0f) + EBMLFree42PhloatElement, sizeof(phloat), sizeof(phloat), buf);
}

/*
 * write element as boolean
 */
bool ebmlWriteElBool(unsigned int elId, bool val) {
unsigned char buf;
    buf = (val == true) ? 1 : 0;
    return ebmlFlushSizedEl((elId & ~0x0f) + EBMLFree42BooleanElement, 1, 1, &buf);
}

/*
 * write element as binary object
 */
bool ebmlWriteElBinary(unsigned int elId, unsigned int l, void * b) {
    return ebmlFlushSizedEl((elId & ~0x0f) + EBMLFree42BinaryElement, l, l, (unsigned char *)b);
}

/*
 * write element as arg
 */
bool ebmlWriteElArg(unsigned int elId, arg_struct *arg) {
unsigned char buf[64];
int j, sz;
    if (arg == 0) {
        // arg may not have been initalized yet
        return true;
    }
    sz = 0;
    j = ebmlWriteVInt(EBMLFree42ArgType, arg->type, &buf[0]);
    if (j == 0) {
        return false;
    }
    sz += j;
    switch (arg->type) {
        case ARGTYPE_NONE:
        case ARGTYPE_COMMAND:
            // nothing more needed
            break;
        case ARGTYPE_NUM:
        case ARGTYPE_NEG_NUM:
        case ARGTYPE_IND_NUM:
        case ARGTYPE_LBLINDEX:
            j = ebmlWriteInt(EBMLFree42ArgTarget, arg->target, &buf[sz]);
            if (j == 0) {
                return false;
            }
            sz += j;
            j = ebmlWriteInt(EBMLFree42ArgVal, arg->val.num, &buf[sz]);
            if (j == 0) {
                return false;
            }
            sz += j;
            break;
        case ARGTYPE_STK:
        case ARGTYPE_IND_STK:
            j = ebmlWriteString(EBMLFree42ArgVal, 1, &arg->val.stk, &buf[sz]);
            if (j == 0) {
                return false;
            }
            sz += j;
            break;
        case ARGTYPE_STR:
        case ARGTYPE_IND_STR:
            j = ebmlWriteString(EBMLFree42ArgVal, arg->length, arg->val.text, &buf[sz]);
            if (j == 0) {
                return false;
            }
            sz += j;
            break;
        case ARGTYPE_LCLBL:
            j = ebmlWriteString(EBMLFree42ArgVal, 1, &arg->val.lclbl, &buf[sz]);
            if (j == 0) {
                return false;
            }
            sz += j;
            break;
        case ARGTYPE_DOUBLE:
            j = ebmlWritePhloat(EBMLFree42ArgVal, &arg->val_d, &buf[sz]);
            if (j == 0) {
                return false;
            }
            sz += j;
            break;
            break;
        default:
            return false;
    }
    return ebmlFlushSizedEl(elId, sz, sz, buf);
}

/*
 *
 */
bool ebmlWriteMasterHeader() {
unsigned char buf[64];
int j, sz;
    sz = 0;
    j = ebmlWriteString(EBMLFree42Desc, sizeof(_EBMLFree42Desc)-1, _EBMLFree42Desc, &buf[0]);
    if (j == 0) {
        return false;
    }
    sz += j;
    j = ebmlWriteVInt(EBMLFree42Version, _EBMLFree42Version, &buf[sz]);
    if (j == 0) {
        return false;
    }
    sz += j;
    j = ebmlWriteVInt(EBMLFree42ReadVersion, _EBMLFree42ReadVersion, &buf[sz]);
    if (j == 0) {
        return false;
    }
    sz += j;
    return ebmlFlushSizedEl(EBMLFree42, -1, sz, buf);
}

/*
 *
 */
bool ebmlWriteShellDocument(unsigned int version, unsigned int readVersion, unsigned int len, char * OsVersion) {
unsigned char buf[64];
int j, sz;
    sz = 0;
    j = ebmlWriteVInt(EBMLFree42ShellVersion, version, &buf[0]);
    if (j == 0) {
        return false;
    }
    sz += j;
    j = ebmlWriteVInt(EBMLFree42ShellReadVersion, readVersion, &buf[sz]);
    if (j == 0) {
        return false;
    }
    sz += j;
    j = ebmlWriteString(EBMLFree42ShellOS, len, OsVersion, &buf[sz]);
    if (j == 0) {
        return false;
    }
    sz += j;
    return ebmlFlushSizedEl(EBMLFree42Shell, -1, sz, buf);
}

/*
 *
 */
bool ebmlWriteCoreDocument() {
unsigned char buf[64];
int j, sz;
    sz = 0;
    j = ebmlWriteVInt(EBMLFree42CoreVersion, _EBMLFree42CoreVersion, &buf[0]);
    if (j == 0) {
        return false;
    }
    sz += j;
    j = ebmlWriteVInt(EBMLFree42CoreReadVersion, _EBMLFree42CoreReadVersion, &buf[sz]);
    if (j == 0) {
        return false;
    }
    sz += j;
    return ebmlFlushSizedEl(EBMLFree42Core, -1, sz, buf);
}

/*
 *
 */
bool ebmlWriteDisplayDocument() {
unsigned char buf[64];
int j, sz;
    sz = 0;
    j = ebmlWriteVInt(EBMLFree42DisplayVersion, _EBMLFree42DisplayVersion, &buf[0]);
    if (j == 0) {
        return false;
    }
    sz += j;
    j = ebmlWriteVInt(EBMLFree42DisplayReadVersion, _EBMLFree42DisplayReadVersion, &buf[sz]);
    if (j == 0) {
        return false;
    }
    sz += j;
    return ebmlFlushSizedEl(EBMLFree42Display, -1, sz, buf);
}

/*
 *
 */
bool ebmlWriteVarsDocument(unsigned int count) {
unsigned char buf[64];
int j, sz;
    sz = 0;
    j = ebmlWriteVInt(EBMLFree42VarsVersion, _EBMLFree42VarsVersion, &buf[0]);
    if (j == 0) {
        return false;
    }
    sz += j;
    j = ebmlWriteVInt(EBMLFree42VarsReadVersion, _EBMLFree42VarsReadVersion, &buf[sz]);
    if (j == 0) {
        return false;
    }
    sz += j;
    j = ebmlWriteVInt(EBMLFree42VarsCount, count, &buf[sz]);
    if (j == 0) {
        return false;
    }
    sz += j;
    return ebmlFlushSizedEl(EBMLFree42Vars, -1, sz, buf);
}

/*
 *
 */
bool ebmlWriteProgsDocument(unsigned int count){
unsigned char buf[64];
int j, sz;
    sz = 0;
    j = ebmlWriteVInt(EBMLFree42ProgsVersion, _EBMLFree42ProgsVersion, &buf[0]);
    if (j == 0) {
        return false;
    }
    sz += j;
    j = ebmlWriteVInt(EBMLFree42ProgsReadVersion, _EBMLFree42ProgsReadVersion, &buf[sz]);
    if (j == 0) {
        return false;
    }
    sz += j;
    j = ebmlWriteVInt(EBMLFree42ProgsCount, count, &buf[sz]);
    if (j == 0) {
        return false;
    }
    sz += j;
    return ebmlFlushSizedEl(EBMLFree42Progs, -1, sz, buf);
}

/*
 * Write end of documents for unsized documents
 */
bool ebmlWriteEndOfDocument() {
unsigned char c;
    return ebmlFlushSizedEl(EBMLFree42EOD, 0, 0, &c);
}

/*
 * ebmlFetchVint
 */
int ebmlFetchVint(int *value) {
int nbytes, i;
unsigned char buf[5];
    nbytes = 0;
    // read first byte
    if (shell_read_saved_state(buf, 1) == 1) {
        if (buf[0] & 0x80) {
            if (buf[0] == 0xff) {
                // invalid value, unsized element ?
                *value = -1;
                return 0;
            }
            nbytes = 1;
        }
        else if (buf[0] & 0x40) {
            nbytes = 2;
        }
        else if (buf[0] & 0x20) {
            nbytes = 3;
        }
        else if (buf[0] & 0x10) {
            nbytes = 4;
        }
        else if (buf[0] & 0x08) {
            nbytes = 5;
        }
        // read next bytes
        if (nbytes > 1) {
            if (shell_read_saved_state(&buf[1], nbytes - 1) != nbytes - 1) {
                nbytes = 0;
            }
        }
        // get result
        if (nbytes != 0) {
            *value = (int)(buf[0] & (0xff >> nbytes));
            for (i = 1; i < nbytes; i++) {
                *value = (*value << 8) + buf[i];
            }
            return 0;
        }
    }
    return -1;
}

/*
 * ebmlFetchInt
 */
int ebmlFetchInt(int nbytes, int * value) {
int i;
unsigned char buf[8];
    if (nbytes > sizeof(buf)) {
        return -1;
    }
    // read bytes
    if (shell_read_saved_state(buf, nbytes) != 0) {
        return -1;
    }
    // get result
    *value = 0;
    for (i = 0; i < nbytes; i++) {
        *value = (*value << 8) + buf[i];
    }
    return 0;
}

/*
 * ebml seek for next
 * input -> pointer to ebmlElement_Struct
 * output -> result in ebmlElement_Struct
 */
int ebmlGetNext(ebmlElement_Struct * el) {
    elCurrent.elPos = shell_ftell();
    if (elCurrent.elPos == -1) {
        return -1;
    }
    if (ebmlFetchVint(&elCurrent.elId) != 0) {
        return -1;
    }
    if (ebmlFetchVint(&elCurrent.elLen) != 0) {
        return -1;
    }
    el->elId = elCurrent.elId;
    el->elLen = elCurrent.elLen;
    el->elPos = elCurrent.elPos;
    el->pos = elCurrent.pos = shell_ftell();
    return 0;
}

/*
 * ebml getsize, returns size of element
 * if size is undefined, need to parse the file
 */
int ebmlGetSize(ebmlElement_Struct *el) {
int i, len, id, pos;

    if (ebmlFetchVint(&len) != 0) {
        return -1;
    }
    el->pos = shell_ftell();
    if (len != -1) {
        // known size
        return len;
    }
    i = 0;
    // unknown size, parse the file
    do {
        pos = shell_ftell();
        if (pos == -1) {
            return -1;
        }
        if (ebmlFetchVint(&id) != 0) {
            return -1;
        }
        if (ebmlFetchVint(&len) != 0) {
            return -1;
        }
        if (id == EBMLFree42EOD) {
            if (i == 0) {
                // found corresponding End Of Document
                pos = shell_ftell();
                if (pos == -1) {
                    return -1;
                }
                // seek back to el pos
                shell_fseek(el->pos, SEEK_SET);
                return pos - el->pos;
            }
            else {
                i--;
            }
        }
        if (len != -1) {
            if ((id & 0x07) != EBMLFree42VIntElement) { 
                // not an VInt !!!
                if (shell_fseek(len, SEEK_CUR) == -1) {
                    return -1;
                }
            }
        }
        else {
            i++;
        }
    } while (pos < (el->docFirstEl + el->docLen));
    return -1;
}

/*
 * ebml seek for specified element in document
 * input -> pointer to ebmlElement_Struct
 *  - docId as required (current ?) document
 *  - elId as required element, if zero, seek only for doc
 *  - elPos as search start position, if outside of do
 * output -> result in ebmlElement_Struct
 */
int ebmlGetEl(ebmlElement_Struct * el) {
int l;
    //shell_logprintf("getEl called - looking for Id %.4x in doc %.4x\n", el->elId, el->docId);
    if (el->docId == 0) {
        // search from start of file
        el->pos = 0;
        elCurrent.docFirstEl = 0;
        elCurrent.docLen = shell_fsize(); 
    }
    else {
        elCurrent.pos = el->pos;
        elCurrent.docFirstEl = el->docFirstEl;
        elCurrent.docLen = el->docLen; 
    }
    if (shell_fseek(el->pos, SEEK_SET) != 0) {
        return -1;
    }
    do {
        elCurrent.elPos = shell_ftell();
        if (elCurrent.elPos == -1) {
            return -1;          
        }
        if (ebmlFetchVint(&elCurrent.elId) != 0) {
            return -1;
        }
        l = ebmlGetSize(&elCurrent);
        if (l == -1) {
            return -1;
        }
        if (elCurrent.elId == el->elId
            // special case for variables, wild card for type
            || (((el->elId & 0xff0f) == EBMLFree42VarNull) && ((el->elId & 0xff0f) == (elCurrent.elId & 0xff0f)))) {
            el->elId = elCurrent.elId;
            el->elLen = l;
            el->elPos = elCurrent.elPos;
            el->elData = elCurrent.pos;
            el->pos = elCurrent.pos;
            return 1;
        }
        // not found yet, get next
        if ((elCurrent.elId & 0x07) != EBMLFree42VIntElement) { 
            if (shell_fseek(l, SEEK_CUR) != 0) {
                return -1;
            }
        }
    } while (elCurrent.elPos < (el->docFirstEl + el->docLen));
    // not found
    return 0;
}

/*
 * ebml read elements 
 */

bool ebmlGetString(ebmlElement_Struct *el, char *value, int len) {
    if ((el->elId & 0x07) != EBMLFree42StringElement) {
        return false;
    }
    if ((len != 0) && (shell_read_saved_state(value, len) != len)) {
        return false;
    }
    el->pos = shell_ftell();
    return true;
}

bool ebmlGetBinary(ebmlElement_Struct *el, void *value, int len) {
    if ((el->elId & 0x07) != EBMLFree42BinaryElement) {
        return false;
    }
    if ((len != 0) && (shell_read_saved_state(value, len) != len)) {
        return false;
    }
    el->pos = shell_ftell();
    return true;
}

bool ebmlGetPhloat(ebmlElement_Struct *el, phloat *p) {
    unsigned char b[sizeof(phloat)];
    if ((el->elId & 0x07) != EBMLFree42PhloatElement) {
        return false;
    }
    if (el->elLen != sizeof(phloat)) {
        return false;
    }
    if (shell_read_saved_state(b, el->elLen) != el->elLen) {
        return false;
    }
#ifdef BCD_MATH
    memcpy(&p->val, b, sizeof(phloat));
#else
    bid128_to_binary64(p, (BID_UINT128*)b);
#endif
    return true;
}

bool ebmlGetProg(ebmlElement_Struct *el, char *value, int *sz) {
    if (shell_fseek(el->pos, SEEK_SET) != 0) {
        return false;
    }
    if ((el->pos - el->elData) > el->elLen) {
        // take care not to read after
        return false;
    }
    if ((el->pos + *sz) > (el->elData + el->elLen)) {
        // adjust buffer size to match last chunk
        *sz = el->elData + el->elLen - el->pos;
    }
    if (shell_read_saved_state(value, *sz) != *sz) {
        return false;
    }
    el->pos = shell_ftell();
    return true;
}

/*
 * ebml find and read element
 */

bool ebmlReadElBool(ebmlElement_Struct *el, bool *value) {
    char c;
    if (ebmlGetEl(el) != 1 || el->elLen != 1) {
        return false;
    }
    if (shell_read_saved_state(&c, el->elLen) != el->elLen) {
        return false;
    }
    el->pos = shell_ftell();
    *value = (c == 0) ? false : true;
    return true;
}

bool ebmlReadElInt(ebmlElement_Struct *el, int *value) {
    int i;
    unsigned char b[4];
    if (ebmlGetEl(el) != 1 || el->elLen != 4) {
        return false;
    }
    if (shell_read_saved_state(b, el->elLen) != el->elLen) {
        return false;
    }
    // get result
    *value = 0;
    for (i = 0; i < el->elLen; i++) {
        *value = (*value << 8) + b[i];
    }
    el->pos = shell_ftell();
    return true;
}

bool ebmlReadElString(ebmlElement_Struct *el, char *value, int *sz) {
    if (ebmlGetEl(el) != 1 || el->elLen > *sz) {
        return false;
    }
    if ((el->elLen != 0) && (shell_read_saved_state(value, el->elLen) != el->elLen)) {
        return false;
    }
    *sz = el->elLen;
    el->pos = shell_ftell();
    return true;
}

bool ebmlReadElPhloat(ebmlElement_Struct *el, phloat *p) {
    unsigned char b[sizeof(phloat)];
    if (ebmlGetEl(el) != 1 || el->elLen != sizeof(phloat)) {
        return false;
    }
    if (shell_read_saved_state(b, el->elLen) != el->elLen) {
        return false;
    }
#ifdef BCD_MATH
    memcpy(&p->val, b, sizeof(phloat));
#else
    binary64_to_bid128((BID_UINT128*)b, p);
#endif
    el->pos = shell_ftell();
    return true;
}

bool ebmlReadElArg(ebmlElement_Struct *el, arg_struct *arg) {
    ebmlElement_Struct argEl;
    int len;
    if (ebmlGetEl(el) != 1) {
        return false;
    }
    // arg as master document
    argEl.docId = el->elId;
    argEl.docFirstEl = el->elPos;
    argEl.docLen = el->elLen;
    // get arg type
    argEl.elId = EBMLFree42ArgType;
    argEl.pos = shell_ftell();
    if (ebmlGetEl(&argEl) != 1) {
        return false;
    }
    arg->type = argEl.elLen;
    switch (arg->type) {
        case ARGTYPE_NONE:
        case ARGTYPE_COMMAND:
            // nothing more needed
            break;
        case ARGTYPE_NUM:
        case ARGTYPE_NEG_NUM:
        case ARGTYPE_IND_NUM:
        case ARGTYPE_LBLINDEX:
            argEl.elId = EBMLFree42ArgTarget;
            if (!ebmlReadElInt(&argEl, &arg->target)) {
                return false;
            }
            argEl.elId = EBMLFree42ArgVal + EBMLFree42IntElement;
            if (!ebmlReadElInt(&argEl, &arg->val.num)) {
                return false;
            }
            break;
        case ARGTYPE_STK:
        case ARGTYPE_IND_STK:
            argEl.elId = EBMLFree42ArgVal + EBMLFree42StringElement;
            len = sizeof(arg->val.stk);
            if (!ebmlReadElString(&argEl, &arg->val.stk, &len)) {
                return false;
            }
            break;
        case ARGTYPE_STR:
        case ARGTYPE_IND_STR:
            argEl.elId = EBMLFree42ArgVal + EBMLFree42StringElement;
            len = sizeof(arg->val.text);
            if (!ebmlReadElString(&argEl, arg->val.text, &len)) {
                return false;
            }
            break;
        case ARGTYPE_LCLBL:
            argEl.elId = EBMLFree42ArgVal + EBMLFree42StringElement;
            len = sizeof(arg->val.lclbl);
            if (!ebmlReadElString(&argEl, &arg->val.lclbl, &len)) {
                return false;
            }
            break;
        case ARGTYPE_DOUBLE:
            argEl.elId = EBMLFree42ArgVal + EBMLFree42PhloatElement;
            if (!ebmlReadElPhloat(&argEl, &arg->val_d)) {
                return false;
            }
            break;
        default:
            return false;
    }
    el->pos = shell_ftell();
    return true;
}

bool ebmlReadAlphaReg(ebmlElement_Struct *el, char *value, int *length) {
    ebmlElement_Struct argEl;
    var_struct v;
    int type, len;
    el->elId = EBMLFree42VarStr;
    if (ebmlGetEl(el) != 1) {
        return false;
    }
    // var as master document
    argEl.docId = el->elId;
    argEl.docFirstEl = el->elPos;
    argEl.docLen = el->elLen;
    argEl.pos = shell_ftell();
    // get var type
    type = (el->elId & 0x00f0) >> 4;
    // get var name
    argEl.elId = EBMLFree42VarName;
    len = sizeof(v.name);
    if (!ebmlReadElString(&argEl, v.name, &len)) {
        return false;
    }
    v.length = len;
    if (v.length != 1 && v.name[0] != 'A') {
        return false;
    }
    if (type == TYPE_STRING) {
        argEl.elId = EBMLFree42VarString;
        if (!ebmlReadElString(&argEl, value, length)) {
            return false;
        }
    }
    else {
        return false;
    }
    return true;
}

bool ebmlReadVar(ebmlElement_Struct *el, var_struct *var) {
    ebmlElement_Struct argEl;
    var_struct v;
    int i, type, len, rows, columns;
    bool shared;
    allvartypes_ptr t;
    el->elId = EBMLFree42VarNull;
    if (ebmlGetEl(el) != 1) {
        return false;
    }
    // var as master document
    argEl.docId = el->elId;
    argEl.docFirstEl = el->elPos;
    argEl.docLen = el->elLen;
    argEl.pos = shell_ftell();
    // get var type
    type = (el->elId & 0x00f0) >> 4;
    // get var name
    argEl.elId = EBMLFree42VarName;
    len = sizeof(v.name);
    if (!ebmlReadElString(&argEl, v.name, &len)) {
        return false;
    }
    v.length = len;
    if (var->length != 0) {
        // seeking for named variable
        if (var->length != v.length || strncmp(var->name, v.name, v.length)) {
            return false;
        }
    }
    else {
        var->length = v.length;
        for (i = 0; i < v.length; i++) {
            var->name[i] = v.name[i];
        }
    }
    switch (type) {
        case TYPE_NULL:
            var->value = NULL;
            break;
        case TYPE_REAL:
            t.r = (vartype_real *) new_real(0);
            if (t.r == NULL) {
                return false;
            }
            argEl.elId = EBMLFree42VarPhloat;
            if (!ebmlReadElPhloat(&argEl, &t.r->x)) {
                free_vartype((vartype *) t.r);
                return false;
            }
            var->value = (vartype *)t.r;
            break;
        case TYPE_COMPLEX:
            t.c = (vartype_complex *) new_complex(0, 0);
            if (t.c == NULL) {
                return false;
            }
            argEl.elId = EBMLFree42VarPhloat;
            if (!ebmlReadElPhloat(&argEl, &t.c->re)) {
                free_vartype((vartype *) t.c);
                return false;
            }
            argEl.elId = EBMLFree42VarPhloat;
            if (!ebmlReadElPhloat(&argEl, &t.c->im)) {
                free_vartype((vartype *) t.c);
                return false;
            }
            var->value = (vartype *)t.c;
            break;
        case TYPE_STRING:
            t.s = (vartype_string *) new_string("", 0);
            if (t.s == NULL) {
                return false;
            }
            t.s->length = sizeof(t.s->text);
            argEl.elId = EBMLFree42VarString;
            if (!ebmlReadElString(&argEl, t.s->text, &t.s->length)) {
                free_vartype((vartype *) t.s);
                return false;
            }
            var->value = (vartype *)t.s;
            break;
        case TYPE_REALMATRIX:
            argEl.elId = EBMLFree42VarRows;
            if (!ebmlReadElInt(&argEl, &rows)) {
                return false;
            }
            argEl.elId = EBMLFree42VarColumns;
            if (!ebmlReadElInt(&argEl, &columns)) {
                return false;
            }
            if (rows == 0) {
                // known shared matrix
                t.g = new_matrix_alias((vartype *) array_list[columns]);
                if (t.g == NULL) {
                    return false;
                }
            }
            else {
                shared = rows < 0;
                if (shared) {
                    // new shared matrix
                    rows = -rows;
                }
                t.rm = (vartype_realmatrix *) new_realmatrix(rows, columns);
                if (t.rm == NULL) {
                    return false;
                }
                for (i = 0; i < rows * columns; i++) {
                    if (ebmlGetNext(&argEl)) {
                        free_vartype((vartype *)t.rm);
                        return false;
                    }
                    switch (argEl.elId) {
                        case EBMLFree42VarString:
                            if (argEl.elLen > sizeof(phloat_text(t.rm->array->data[i])) || !ebmlGetString(&argEl, phloat_text(t.rm->array->data[i]), argEl.elLen)) {
                                free_vartype((vartype *)t.rm);
                                return false;
                            }
                            phloat_length(t.rm->array->data[i]) = argEl.elLen;
                            t.rm->array->is_string[i] = 1;
                            break;
                        case EBMLFree42VarPhloat:
                            if (!ebmlGetPhloat(&argEl, &t.rm->array->data[i])) {
                                free_vartype((vartype *)t.rm);
                                return false;
                            }
                            t.rm->array->is_string[i] = 0;
                            break;
                        default:
                            free_vartype((vartype *)t.rm);
                            return false;
                    }
                }
                if (shared) {
                    if (!array_list_grow()) {
                        free_vartype((vartype *)t.rm);
                        return false;
                    }
                    array_list[array_count++] = t.rm;
                }
            }
            var->value = (vartype *)t.rm;
            break;
        case TYPE_COMPLEXMATRIX:
            argEl.elId = EBMLFree42VarRows;
            if (!ebmlReadElInt(&argEl, &rows)) {
                return false;
            }
            argEl.elId = EBMLFree42VarColumns;
            if (!ebmlReadElInt(&argEl, &columns)) {
                return false;
            }
            if (rows == 0) {
                // known shared matrix
                t.g = new_matrix_alias((vartype *) array_list[columns]);
                if (t.g == NULL) {
                    return false;
                }
            }
            else {
                shared = rows < 0;
                if (shared) {
                    // new shared matrix
                    rows = -rows;
                }
                t.cm = (vartype_complexmatrix *) new_complexmatrix(rows, columns);
                if (t.cm == NULL) {
                    return false;
                }
                for (i = 0; i < 2 * rows * columns; i++) {
                    argEl.elId = EBMLFree42VarPhloat;
                    if (!ebmlReadElPhloat(&argEl, &t.cm->array->data[i])) {
                        free_vartype((vartype *) t.cm);
                        return false;
                    }
                }
                if (shared) {
                    if (!array_list_grow()) {
                        free_vartype((vartype *)t.cm);
                        return false;
                    }
                    array_list[array_count++] = t.cm;
                }
            }
            var->value = (vartype *)t.cm;
            break;
        default:
            return false;
    }
    return true;
}

bool ebmlReadProgram(ebmlElement_Struct *el, prgm_struct *prgm) {
    ebmlElement_Struct argEl;
    int i, j, len, doneSoFar;
    char buf[100];
    int cmd;
    arg_struct arg;
    el->elId = EBMLFree42Prog;
    if (ebmlGetEl(el) != 1) {
        return false;
    }
    // prog as master document
    argEl.docId = el->elId;
    argEl.docFirstEl = el->elPos;
    argEl.docLen = el->elLen;
    argEl.pos = shell_ftell();
    // get prog size
    argEl.elId = EBMLFree42Prog_size;
    if (ebmlGetEl(&argEl) != 1) {
        return false;
    }
    prgm->capacity = argEl.elLen;   
    // get prog text
    argEl.elId = EBMLFree42Prog_text;
    if (ebmlGetEl(&argEl) != 1) {
        return false;
    }
    prgm->text = (unsigned char *) malloc(prgm->capacity);
    if (prgm->text == NULL) {
        return false;
    }
    prgm->size = 0;
    doneSoFar = 0;
    j = 0;
    do {
        len = sizeof(buf) - j;
        if (ebmlGetProg(&argEl, &buf[j], &len)) {
            // todo, free program ?
        }
        len += j;
        i = 0;
        while ((i < len) && (core_42ToFree42((unsigned char*)buf, &i, len - i) == 0));
        if (len - i > 50) {
            // foolproof against huge number of digits
            break;
        }
        doneSoFar += i;
        for (j = 0; i < len; ) {
            buf[j++] = buf[i++];
        }
    } while (doneSoFar < argEl.elLen);
    // make sure last instruction was CMD_END
    if (pc != -1) {
        cmd = CMD_END;
        arg.type = ARGTYPE_NONE;
        store_command_simple(&pc, cmd, &arg);
    }
    return true;
}
