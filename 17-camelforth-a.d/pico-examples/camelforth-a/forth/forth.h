/****h* camelforth/forth.h
 * NAME
 *  forth.h
 * DESCRIPTION
 *  Header file for forth.c
 ******
 * LICENSE TERMS
 *  CamelForth in C 
 *  copyright (c) 2016,2017 Bradford J. Rodriguez.
 *  
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *  
 *  Commercial inquiries should be directed to the author at 
 *  115 First St., #105, Collingwood, Ontario L9Y 4W3 Canada
 *  or via email to bj@camelforth.com
 */

/*
 * CONFIGURATION FLAGS
 */

// #define INTERPRETER_ONLY       /* to omit Forth compiler words */

/* define only one of the following */
// #define LINUX                  /* for development under Linux */
// #define TIVA_C                 /* for use with TI TM4C12x */
// #define SAMDX1                    /* for use with Adafruit Feather M0 Express */
#define RP2040_PICO               /* for use with Raspberry Pi Pico RP2040 based target */
#define USB_IFACE                 /* only some implementations */

/* 
 * CONFIGURATION PARAMETERS
 */

#define CELL 4              /* CPU dependency, # of bytes/cell */
#define CELLWIDTH 32        /* # of bits/cell */
#define CELLMASK 0xffffffff /* mask for CELLWIDTH bits */

#define PSTACKSIZE 64       /* 64 cells */
#define RSTACKSIZE 64       /* 64 cells */
#define LSTACKSIZE 32       /* 32 cells */
#define USERSIZE   32       /* 32 cells */
#define TIBSIZE    84       /* 84 characters */
#define PADSIZE    84       /* 84 characters */
#define HOLDSIZE   34       /* 34 characters */

/*
 * DATA STRUCTURES
 */

#ifdef TIVA_C       /* why won't TI compiler handle variable string? */
struct Header {
    void * link;            /* pointer to previous header */
    void * cfa;             /* pointer to high level def'n */
    unsigned char flags;    /* immed flag and others */
    char nfa[13];           /* inline name string */
};
#else
struct Header {
    void * link;            /* pointer to previous header */
    void * cfa;             /* pointer to high level def'n */
    unsigned char flags;    /* immed flag and others */
    char nfa[];             /* inline name string */
};
#endif
 
#define HEADER(name,prev,flags,namestring) const struct Header H##name =\
    { (char *)H##prev.nfa, T##name, flags, namestring }
#define IMMEDIATE 1         /* immediate bit in flags */

#define CODE(name)       void F##name (void * pfa)
#define PRIMITIVE(name)  const void * T##name[] = { F##name }
#define THREAD(name)     const void * T##name[]
#define OFFSET(n)   (void *)(n*CELL)       /* see CELL above, = 4 */
#define LIT(n)      (void *)(n)
