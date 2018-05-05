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
 *
 */
int ebmlWriteVInt(unsigned int i, unsigned char* b) {
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

int ebmlWriteCount(unsigned int i, unsigned char* b) {
int sz, bufPtr;
	sz = ebmlWriteVInt(EL_ID_COUNT, &b[0]);
	if (sz) {
		bufPtr = sz;
		sz = ebmlWriteVInt(i, &b[sz]);
		bufPtr += sz;
	}
	return (sz == 0) ? sz : bufPtr;
}

int ebmlWriteName(int l, char* s, unsigned char *b) {
int size, bufPtr, i;
	size = ebmlWriteVInt(EL_ID_NAME, &b[0]);
	if (size) {
		bufPtr = size;
		size = ebmlWriteVInt(unsigned int(l), &b[bufPtr]);
		if (size) {
			bufPtr += size;
			for (i = 0; i < l; i++) {
				b[bufPtr++] = (unsigned char) s[i];
			}
		}
	}
	return (size == 0) ? size : bufPtr;
}

int ebmlWritePhloat(phloat* p, unsigned char *b) {
int sz;
	sz = 0;
	/* need an Id for this ?
	int j;
	j = ebmlWriteVInt(EL_ID_PHLOAT, &b[0]);
	if (j == 0) {
		return j;
	}
	sz += j;
	*/
	memcpy(&b[sz], &p->val, sizeof(phloat));
	sz += sizeof(phloat);
	return sz;
}

int ebmlWriteAlpha(int l, char* a, unsigned char *b) {
int sz, i ,j;
	sz = 0;
	/* need an Id for this ?
	j = ebmlWriteVInt(EL_ID_VARALPHA, &b[0]);
	if (j == 0) {
		return j;
	}
	sz += j;
	*/
	j = ebmlWriteVInt(unsigned int(l), &b[sz]);
	if (j == 0) {
		return j;
	}
	sz += j;
	for (i = 0; i < l; i++) {
		b[sz++] = (unsigned char) a[i];
	}
	return sz;
}

bool ebmlWriteEl(int elId, int elSz, int bufSz, unsigned char *buf) {
unsigned char elBuf[16];
int sz, j;
	j = ebmlWriteVInt(elId, &elBuf[0]);
	if (j == 0) {
		return false;
	}
	sz = j;
	j = ebmlWriteVInt(elSz, &elBuf[sz]);
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

bool ebmlWriteReg(vartype *v, char reg) {
var_struct namedReg; 
	namedReg.length = 1;
	namedReg.name[0] = reg;
	namedReg.value = v;
	return ebmlWriteVar(&namedReg);
}

bool ebmlWriteAlphaReg() {
int elId, sz, j;
unsigned char buf[64];
char reg;
	elId = EL_ID_VARSTRING;
	sz = 0;
	reg = 'A';
	j = ebmlWriteName(1, &reg, &buf[0]);
	if (j == 0) {
		return false;
	}
	sz += j;
	j = ebmlWriteAlpha(reg_alpha_length, reg_alpha, &buf[sz]);
	if (j == 0) {
		return false;
	}
	sz += j;
	return ebmlWriteEl(elId, sz, sz, buf);
}

bool ebmlWriteElBool(int elId, bool val) {
unsigned char buf;
	buf = (val == true) ? 1 : 0;
	return ebmlWriteEl(elId, 1, 1, &buf);
}

bool ebmlWriteElInt(int elId, int val) {
unsigned char buf[8];
int j;
	j = ebmlWriteVInt(val, &buf[0]);
	if (j == 0) {
		return false;
	}
	return ebmlWriteEl(elId, j, j, buf);
}

bool ebmlWriteElString(int elId, int len, char *val) {
unsigned char buf[16];
int j;
	j = emblWriteName(len, val, &buf[0]);
	if (j == 0) {
		return false;
	}
	return ebmlWriteEl(elId, j, j, buf);
}

bool ebmlWriteElPhloat(int elId, phloat* p) {
unsigned char buf[16];
int j;
	j = emblWritePhloat(p, &buf[0]);
	if (j == 0) {
		return false;
	}
	return ebmlWriteEl(elId, j, j, buf);
}

bool ebmlWriteElArg(int elId, arg_struct *arg) {
#define ARGTYPE_NONE      0
#define ARGTYPE_NUM       1
#define ARGTYPE_NEG_NUM   2 /* Used internally */
#define ARGTYPE_STK       3
#define ARGTYPE_STR       4
#define ARGTYPE_IND_NUM   5
#define ARGTYPE_IND_STK   6
#define ARGTYPE_IND_STR   7
#define ARGTYPE_COMMAND   8 /* For backward compatibility only! */
#define ARGTYPE_LCLBL     9
#define ARGTYPE_DOUBLE   10
#define ARGTYPE_LBLINDEX 11
}

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
	elId = EL_ID_VARBASE + type;
	// Variable object - common header: ElId, Size, [ ElName[
	j = ebmlWriteName((unsigned int)v->length, v->name, &buf[0]);
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
			j = ebmlWritePhloat(&r->x, &buf[sz]);
			if (j == 0) {
				return false;
			}
			sz += j;
			szEl = sz;
			break;
        case TYPE_COMPLEX:
			// body: phloat, phloat]
            c = (vartype_complex *) v;
			j = ebmlWritePhloat(&c->re, &buf[sz]);
			if (j == 0) {
				return false;
			}
			sz += j;
			j = ebmlWritePhloat(&c->im, &buf[sz]);
			if (j == 0) {
				return false;
			}
			sz += j;
			szEl = sz;
			break;
        case TYPE_STRING:
			// body: string length, string]
            s = (vartype_string *) v;
			j = ebmlWriteAlpha(s->length, s->text, &buf[sz]);
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
			j = ebmlWriteVInt(rm->rows, &buf[sz]);
			if (j == 0) {
				return false;
			}
			sz += j;
			j = ebmlWriteVInt(rm->columns, &buf[sz]);
			if (j == 0) {
				return false;
			}
			sz += j;
			szEl = sz + msz * (EbmlPhloatSZ	+ EbmlCharSZ);
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
			j = ebmlWriteVInt(cm->rows, &buf[sz]);
			if (j == 0) {
				return false;
			}
			sz += j;
			j = ebmlWriteVInt(cm->columns, &buf[sz]);
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
	if (ebmlWriteEl(elId, szEl, sz, buf)) {
		return false;
	}

	if (type == TYPE_REALMATRIX) {
		sz = 0;
		for (i = 0; i < msz; i++) {
			j = ebmlWritePhloat(&rm->array->data[i], &buf[sz]);
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
		for (i = 0; i < msz; i++) {
			buf[sz++] = ((unsigned char*) (rm->array->is_string))[(i * sizeof(char)) + j];
			j = ebmlWriteAlpha(1, &rm->array->is_string[i], &buf[sz]);
			if (j == 0) {
				return false;
			}
			sz += j;
			if ((sz >= (sizeof(buf) + EbmlCharSZ)) || (i == msz)) {
				if (shell_write_saved_state(buf, sz)) {
					return false;
				}
				sz = 0;
			}
		}
	}
	else if (type == TYPE_COMPLEXMATRIX) {
		for (i = 0; i < msz * 2; i++) {
			j = ebmlWritePhloat(&cm->array->data[i], &buf[sz]);
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
