/*****************************************************************************
 * Free42 -- an HP-42S calculator simulator
 * Copyright (C) 2004-2018  Thomas Okken
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

#include "core_globals.h"
#include "core_ebml.h"
#include "core_helpers.h"
#include "core_main.h"
#include "core_tables.h"
#include "shell.h"
#include <stdlib.h>

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
	j = ebml2VInt(unsigned int(l), &b[sz]);
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
	memcpy(&b[sz], &p->val, sizeof(phloat));
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
	if (v == 0) {
		// variable may not have been initalized yet
		return true;
	}
	else {
		namedReg.length = 1;
		namedReg.name[0] = reg;
		namedReg.value = v;
		return ebmlWriteVar(&namedReg);
	}
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
	j = ebml2VInt(EBMLFree42ProgData, &buf[sz]);
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
	memcpy(buf, &p->val, sizeof(phloat));
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
 * Write end of documents for unsized documents
 */
bool ebmlWriteEndOfDocument() {
unsigned char c;
	return ebmlFlushSizedEl(EBMLFree42EOD, 0, 0, &c);
}