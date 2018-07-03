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
	return size;
}

/*
 * append vint representation of element to buffer
 */
int ebmlWriteVInt(int elId, unsigned int i, unsigned char *b) {
int j, sz;
	sz = 0;
	/* write Id */
	j = ebml2VInt(elId & ~0x0f + EBMLFree42VIntElement, &b[0]);
	if (j == 0) {
		return j;
	}
	sz += j;
	/* write size */
	j = ebml2VInt(sizeof(phloat), &b[sz]);
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
unsigned char buf[16];
int j, sz;
	sz = 0;
	/* write Id */
	j = ebml2VInt(elId & ~0x0f + EBMLFree42IntElement, &b[0]);
	if (j == 0) {
		return j;
	}
	sz += j;
	j = ebml2VInt(sizeof(i), &buf[0]);
	if (j == 0) {
		return false;
	}
	sz += j;
	for (j = 0; j < sizeof(i); j++) {
		buf[sz++] = (unsigned char)((i >> (j * 8)) & 0xff);
	}
	return sz;
}

/*
 * append string representation of element to buffer
 */
int ebmlWriteString(int elId, int l, char* a, unsigned char *b) {
int i, j, sz;
	sz = 0;
	/* write Id and size */
	j = ebmlWriteVInt(elId & ~0x0f + EBMLFree42StringElement, unsigned int(l), &b[0]);
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
	j = ebmlWriteVInt(elId & ~0x0f + EBMLFree42PhloatElement, sizeof(phloat), &b[sz]);
	if (j == 0) {
		return j;
	}
	sz += j;
	memcpy(&b[sz], &p->val, sizeof(phloat));
	sz += sizeof(phloat);
	return sz;
}

/*
 * write sized master element header
 * - write master element
 * - write size Id (always master + 10) and value
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
	j = ebmlWriteVInt(elId + 0x10, elSz, &elBuf[sz]);
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
	namedReg.length = 1;
	namedReg.name[0] = reg;
	namedReg.value = v;
	return ebmlWriteVar(&namedReg);
}

/*
 * turns alpha reg to a named variable
 */
bool ebmlWriteAlphaReg() {
int sz, j;
unsigned char buf[64];
char reg;
	sz = 0;
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
int sz, szEl, elId;
unsigned char buf[64];
vartype_real *r;
vartype_complex *c;
vartype_string *s;
vartype_realmatrix *rm;
vartype_complexmatrix *cm;
int n, msz;  
int type, i, j;
	type = v->value->type;
	elId = EBMLFree42VarNull + type;
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
			r = (vartype_real *) v;
			j = ebmlWritePhloat(EBMLFree42VarPhloat, &r->x, &buf[sz]);
			if (j == 0) {
				return false;
			}
			sz += j;
			szEl = sz;
			break;
        case TYPE_COMPLEX:
			// body: phloat, phloat]
            c = (vartype_complex *) v;
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
            s = (vartype_string *) v;
			j = ebmlWriteString(EBMLFree42VarString, s->length, s->text, &buf[sz]);
			if (j == 0) {
				return false;
			}
			sz += j;
			break;
        case TYPE_REALMATRIX:
			// body: rows, columns, phloat[], char[]]
			rm = (vartype_realmatrix *) v;
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
			cm = (vartype_complexmatrix *) v;
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
	if (ebmlFlushSizedEl(elId, szEl, sz, buf)) {
		return false;
	}

	if (type == TYPE_REALMATRIX) {
		sz = 0;
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
			if ((sz >= (sizeof(buf) + EbmlPhloatSZ)) || (i == msz)) {
				if (shell_write_saved_state(buf, sz)) {
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
			if ((sz >= (sizeof(buf) + EbmlPhloatSZ)) || (i == msz * 2)) {
				if (shell_write_saved_state(buf, sz)) {
					return false;
				}
				sz = 0;
			}
		}
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
		buf[j] = (unsigned char)((val >> (j * 8)) & 0xff);
	}
	return ebmlFlushSizedEl(elId & ~0x0f + EBMLFree42IntElement, j, j, buf);
}

/*
 * write element as string
 */
bool ebmlWriteElString(unsigned int elId, int l, char *val) {
unsigned char buf[16];
int j;
	for (j = 0; j < l; j++) {
		buf[j++] = (unsigned char) val[j];
	}
	return ebmlFlushSizedEl(elId & ~0x0f + EBMLFree42StringElement, j, j, buf);
}

/*
 * write element as phloat
 */
bool ebmlWriteElPhloat(unsigned int elId, phloat* p) {
unsigned char buf[sizeof(phloat)];
	memcpy(buf, &p->val, sizeof(phloat));
	return ebmlFlushSizedEl(elId & ~0x0f + EBMLFree42PhloatElement, sizeof(phloat), sizeof(phloat), buf);
}

/*
 * write element as boolean
 */
bool ebmlWriteElBool(unsigned int elId, bool val) {
unsigned char buf;
	buf = (val == true) ? 1 : 0;
	return ebmlFlushSizedEl(elId & ~0x0f + EBMLFree42BooleanElement, 1, 1, &buf);
}

bool ebmlWriteElArg(unsigned int elId, arg_struct *arg) {
unsigned char buf[64];
int j, sz;
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
int ebmlWriteProgramHeader(int l, unsigned char *b) {
int size, bufPtr;
	size = ebmlWriteVInt(EL_ID_PROGRAM, &b[0]);
	if (size) {
		bufPtr = size;
		size = ebmlWriteVInt(unsigned int(l), &b[bufPtr]);
		bufPtr += size;
	}
	return (size == 0) ? size : bufPtr;
}
*/




/*
int ebmlReadVInt(unsigned int* i, unsigned char* b) {
int size = 0;
	if (b[0] & 0x80) {
		size = 1;
		*i = (unsigned int)(b[0]) & 0x07f;
	}
	else if (b[0] & 0x40) {
		size = 2;
		*i = (((unsigned int)(b[0]) & 0x3f) << 8) + (unsigned int)b[1];
	}
	else if (b[0] & 0x20) {
		size = 3;
		*i = (((unsigned int)(b[0] & 0x1f)) << 8) + (((unsigned int)b[1]) << 16) + (unsigned int)b[0];
	}
	else if (b[0] & 0x10) {
		size = 4;
		*i = (((unsigned int)(b[0] & 0x0f)) << 8) + (((unsigned int)b[1]) << 24) + (((unsigned int)b[2]) << 16) + (unsigned int)b[0];
	}
	return size;
}



/*
int ebmlWriteVarCount(unsigned int i, unsigned char* b) {
int sz, bufPtr;
	sz = ebmlWriteVInt(EBMLFree42VarsCount, &b[0]);
	if (sz) {
		bufPtr = sz;
		sz = ebmlWriteVInt(i, &b[sz]);
		bufPtr += sz;
	}
	return (sz == 0) ? sz : bufPtr;
}
/*
 * write unsized master element header
 * - write master element
 * - write buffer
 /*
bool ebmlFlushEl(int elId, int elSz, int bufSz, unsigned char *buf) {
unsigned char elBuf[16];
int sz, j;
	j = ebml2VInt(elId, &elBuf[0]);
	if (j == 0) {
		return false;
	}
	sz = j;
	if (!shell_write_saved_state(elBuf, sz)) { 						
		return false;
	}
	if (!shell_write_saved_state(buf, bufSz)) { 						
		return false;
	}
	return true; 
}

*/