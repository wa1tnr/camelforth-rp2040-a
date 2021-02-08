/****h* camelforth/forth.c
 * NAME
 *  forth.c
 * DESCRIPTION
 *  Interactive Forth interpreter written in C.
 *  Allows C routines to be executed, and data to be examined, 
 *  from a command line interface.
 * NOTES
 *  Ref. Norman E. Smith, "Write Your Own Programming Language
 *  Using C++", although the implementation here is entirely 
 *  different.
 * AUTHOR
 *  Brad Rodriguez
 * TODO
 *  split ROM and RAM space
 * HISTORY
 *  12 dec 2017 bjr - released as v0.1
 *  14 feb 2016 bjr - first implementation
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

// Rpi Pico RP2040 SDK:
#include <stdio.h>
// #include "pico/stdlib.h"

// original camelforth:
#include <stdint.h>
#include <stdbool.h>
#include "forth.h"

/*
 * DATA STACKS
 * stacks grow downward to allow positive index from psp,rsp
 */

unsigned int pstack[PSTACKSIZE];    /* grows down from end */
unsigned int rstack[RSTACKSIZE];    /* grows down from end */
unsigned int *psp, *rsp;            /* stack pointers */
void *ip;                           /* interpreter pointer */
bool run;                           /* "run" flag */

unsigned int lstack[LSTACKSIZE];    /* grows down from end */
unsigned int uservars[USERSIZE];
unsigned char tibarea[TIBSIZE];
unsigned char padarea[PADSIZE];
unsigned char holdarea[HOLDSIZE];

/* STUBS TBD */
unsigned char RAMDICT[8192];
unsigned char ROMDICT[1024];

#ifdef LINUX
#include "linuxio.c"
#endif

#ifdef TIVA_C
#include "tivaio.inc"
#endif

#ifdef SAMDX1
#include "atsamdx1.inc"
#endif

#ifdef RP2040_PICO
#include "rp2040_pico.inc"
#endif

/* 
 * RUN-TIME FUNCTIONS FOR DEFINED WORDS
 */

void Fdocon (void * pfa) {
    *--psp = *(unsigned int *)pfa;
}

void Fdovar (void * pfa) {
    *--psp = *(unsigned int *)pfa;  /* pf holds variable address */
}

void Fdorom (void * pfa) {
    *--psp = (unsigned int)pfa;
}

void Fenter (void * pfa) {
    *--rsp = (unsigned int)ip;      /* push old IP on return stack */
    ip = pfa;                       /* IP points to thread */
}

void Fdouser (void * pfa) {
    unsigned int i;
    i = *(unsigned int *)pfa;               /* pf holds user var index */
    *--psp = (unsigned int)(&uservars[i]);  /* stack adrs of user var */
}

/* CREATE to support FIG-Forth style DOES>
 * CREATE must reserve a two-cell code field, so that DOES> can later
 * be used.  (See Fdobuilds, below.  See also >BODY)
 * Defined (child) word is originally [Fdocreate] [don'tcare] [...data...]
 * which returns the address of 'data' (CFA + 2 cells).
 * DOES> will change child word to    [Fdobuilds] [Tdoesword] [...data...]
 */

void Fdocreate (void * pfa) {
    *--psp = (unsigned int)pfa + CELL;
}


/* FIG-Forth style <BUILDS..DOES> 
 * Defined (child) word is  [Fdobuilds] [Tdoesword] [...data...]
 * i.e., the second cell of the code field is the xt of the 
 * Forth word to be executed, that implements the DOES> action */

void Fdobuilds (void * pfa) {
    void (*xt)(void *);     /* pointer to code function */
    void *w, *x;            /* generic pointers */

        w = *(void **)pfa;      /* fetch word address from param field */
        pfa += CELL;
        *--psp = (unsigned int)pfa; /* push address of following data */
            
        x = *(void **)w;        /* fetch function adrs from word def */
        xt = (void (*)())x;     /* too much casting! */
        w += CELL;
        (*xt)(w);               /* call function w/adrs of word def */
}

/* 
 * PRIMITIVE FUNCTIONS
 */

CODE(exit) {
    ip = (void *)(*rsp++);        /* pop IP from return stack */
}

CODE(execute) {
    void (*xt)(void *);     /* pointer to code function */
    void *w, *x;            /* generic pointers */
    
        w = *(void **)psp;      /* fetch word address from stack */
        psp++;
        x = *(void **)w;        /* fetch function adrs from word def */
        xt = (void (*)())x;     /* too much casting! */
        w += CELL;
        (*xt)(w);               /* call function w/adrs of word def */
}

CODE(lit) {
    *--psp = *(unsigned int*)ip;     /* fetch inline value */
    ip += CELL;
}    

/* STACK OPERATIONS */

CODE(dup) {
    --psp;
    psp[0] = psp[1];
}

CODE(qdup) {
    if (*psp != 0) {
        --psp;
        psp[0] = psp[1];
    }
}

CODE(drop) {
    psp++;
}

CODE(swap) {
    unsigned int x;
    x = psp[1];
    psp[1] = psp[0];
    psp[0] = x;
}

CODE(over) {
    --psp;
    psp[0] = psp[2];
}

CODE(rot) {
    unsigned int x;
    x = psp[2];
    psp[2] = psp[1];
    psp[1] = psp[0];
    psp[0] = x;
}

CODE(nip) {     // x1 x2 -- x2
    psp[1] = psp[0];
    psp++;
}

CODE(tuck) {    // x1 x2 -- x2 x1 x2
    unsigned int x;
    --psp;
    x = psp[2];   // x1
    psp[2] = psp[0] = psp[1];   // x2
    psp[1] = x;
}

CODE(tor) {
    *--rsp = *psp++;
}

CODE(rfrom) {
    *--psp = *rsp++;
}

CODE(rfetch) {
    *--psp = *rsp;
}

CODE(spfetch) {
    unsigned int temp;
    temp = (unsigned int)psp;    /* circumvent weird optimization */
    *--psp = temp;
}

CODE(spstore) {
    psp = (unsigned int *)(*psp);   /* do not use *psp++ */
}

CODE(rpfetch) {
    *--psp = (unsigned int)rsp;
}

CODE(rpstore) {
    rsp = (unsigned int *)(*psp++);
}

/* MEMORY OPERATIONS */

CODE(fetch) {
    unsigned int *ptr;
    ptr = (unsigned int*) psp[0];
    psp[0] = *ptr;
}

CODE(store) {
    unsigned int *ptr;
    ptr = (unsigned int*)(*psp++);
    *ptr = *psp++;
}

CODE(cfetch) {
    unsigned char *ptr;
    ptr = (unsigned char*) psp[0];
    psp[0] = (unsigned int)(*ptr);
}

CODE(cstore) {
    unsigned char *ptr;
    ptr = (unsigned char*)(*psp++);
    *ptr = (unsigned char)(*psp++);
}

/* ARITHMETIC AND LOGIC OPERATIONS */

CODE(plus) {
    psp[1] += psp[0];
    psp++;
}

CODE(plusstore) {
    unsigned int *ptr;
    ptr = (unsigned int*)(*psp++);
    *ptr += *psp++;
}    

CODE(mplus) {
    uint64_t d;
    unsigned int n;
    n = *psp++;
    /* enforce Forth word order, high on top */
    d = (((uint64_t)psp[0]) << CELLWIDTH) + psp[1];
    d += n;
    psp[0] = (unsigned int)(d >> CELLWIDTH);
    psp[1] = d & CELLMASK;
}

CODE(minus) {
    psp[1] -= psp[0];
    psp++;
}

CODE(mult) {
    signed int r;
    r = (signed int)psp[1] * (signed int)psp[0];
    psp++;
    psp[0] = (unsigned int)r;
}

CODE(div) {
    signed int r;
    r = (signed int)psp[1] / (signed int)psp[0];
    psp++;
    psp[0] = (unsigned int)r;
}

CODE(and) {
    psp[1] &= psp[0];
    psp++;
}
    
CODE(or) {
    psp[1] |= psp[0];
    psp++;
}
    
CODE(xor) {
    psp[1] ^= psp[0];
    psp++;
}

CODE(invert) {
    *psp ^= CELLMASK;
}

CODE(negate) {
    *psp = -(*psp);
}

CODE(oneplus) {
    *psp += 1;
}

CODE(oneminus) {
    *psp -= 1;
}

CODE(swapbytes) {
    unsigned int u;
    u = *psp;
    *psp = ((u & 0xff) << 8) | ((u & 0xff00) >> 8);
}

CODE(twostar) {
    *psp = (*psp) << 1;
}

CODE(twoslash) {
    signed int n;
    n = (signed int)(*psp);
    *psp = (unsigned int)(n >> 1);
}

CODE(lshift) {
    unsigned int u;
    u = *psp++;
    *psp = (*psp) << u;
}

CODE(rshift) {
    unsigned int u;
    u = *psp++;
    *psp = (*psp) >> u;
}

/* COMPARISONS */

CODE(zeroequal) {
    psp[0] = (psp[0] == 0) ? -1 : 0;
}

CODE(zeroless) {
    psp[0] = ((signed int)(psp[0]) < 0) ? -1 : 0;
}

CODE(equal) {
    psp[1] = (psp[1] == psp[0]) ? -1 : 0;
    psp++;
}

CODE(notequal) {
    psp[1] = (psp[1] != psp[0]) ? -1 : 0;
    psp++;
}

CODE(less) {
    psp[1] = ((signed int)(psp[1]) < (signed int)(psp[0])) ? -1 : 0;
    psp++;
}

CODE(greater) {
    psp[1] = ((signed int)(psp[1]) > (signed int)(psp[0])) ? -1 : 0;
    psp++;
}

CODE(uless) {
    psp[1] = (psp[1] < psp[0]) ? -1 : 0;
    psp++;
}

CODE(ugreater) {
    psp[1] = (psp[1] > psp[0]) ? -1 : 0;
    psp++;
}

/* BRANCH AND LOOP */

CODE(branch) {     /* Tbranch,-4  loops back to itself */
    int offset;                 /* Tbranch,+4  is a no-op */
    offset = *(unsigned int*)ip;     /* fetch inline offset */
    ip += offset;
}

CODE(qbranch) {    /* Tbranch,-4  loops back to itself */
    int offset;                 
    if (*psp++ == 0) {
        offset = *(unsigned int*)ip;     /* fetch inline offset */
        ip += offset;
    } else {
        ip += CELL;
    }
}

/* '83 and ANSI standard +LOOP behavior:  (per dpans-6)
 * "Add n to the loop index. If the loop index did not cross the boundary 
 * between the loop limit minus one and the loop limit, continue execution 
 * at the beginning of the loop. Otherwise, discard the current loop 
 * control parameters and continue execution immediately following the 
 * loop."
 * rsp[0] = index, rsp[1] = limit */

/* circular comparison of two unsigned ints, returns true if x>=y */
#define CIRCULARGE(x,y)  ((signed int)(x-y) >= 0)

CODE(xplusloop) {   /* n -- */
    int offset;
    bool f;
    f = CIRCULARGE(rsp[0],rsp[1]);      // circular compare index:limit
    rsp[0] += *psp++;                   // add n to index
    if (CIRCULARGE(rsp[0],rsp[1]) != f) { // have we crossed the boundary?
        rsp += 2;                           // yes: drop index, limit
        ip += CELL;                         // and exit loop
    } else {                            
        offset = *(unsigned int*)ip;        // no: branch 
        ip += offset;
    }
}        

CODE(xloop) {
    int offset;
    rsp[0] += 1;                        // add 1 to index
    if (rsp[0] == rsp[1]) {             // have we reached the limit?
        rsp += 2;                           // yes: drop index, limit
        ip += CELL;                         // and exit loop
    } else {                            
        offset = *(unsigned int*)ip;        // no: branch 
        ip += offset;
    }
}        

CODE(xdo) {     /* limit start -- */
    *--rsp = psp[1];        // push limit
    *--rsp = *psp++;        // push starting index
    psp++;
}
    
CODE(i) {
    *--psp = rsp[0];        // first loop index 
}

CODE(j) {
    *--psp = rsp[2];        // second loop index
}
    
CODE(unloop) {
    rsp += 2;
}

/* MULTIPLY AND DIVIDE */

CODE(umstar) {  /* u1 u2 -- ud */
    uint64_t ud;
    ud = (uint64_t)psp[0] * (uint64_t)psp[1];
    psp[1] = ud & (uint64_t)0xffffffff;
    psp[0] = ud >> 32;
}    
    
CODE(umslashmod) {  /* ud u1 -- rem quot */
    uint64_t ud, u1;
    u1 = *psp++;
    ud = ((uint64_t)psp[0] << 32) | (uint64_t)psp[1];
    psp[1] = (unsigned int)(ud % u1);
    psp[0] = (unsigned int)(ud / u1);
}
 
/* BLOCK AND STRING OPERATIONS */

CODE(fill) {    /* c-addr u char -- */
    unsigned char c, *dst;
    unsigned int u;
    c = (unsigned char)*psp++;
    u = *psp++;
    dst = (unsigned char *)*psp++;
    while (u-- > 0) *dst++ = c;
}

CODE(cmove) {   /* src dst u -- */
    unsigned char *dst, *src;
    unsigned int u;
    u = *psp++;
    dst = (unsigned char *)*psp++;
    src = (unsigned char *)*psp++;
    while (u-- > 0) *dst++ = *src++;
}

CODE(cmoveup) {   /* src dst u -- */
    unsigned char *dst, *src;
    unsigned int u;
    u = *psp++;
    dst = (unsigned char *)(u + *psp++);
    src = (unsigned char *)(u + *psp++);
    while (u-- > 0) *--dst = *--src;
}

CODE(skip) {    /* c-addr u c -- c-addr' u' */
    unsigned char c, *src;
    unsigned int u;
    c = (unsigned char)*psp++;
    u = *psp++;
    src = (unsigned char *)*psp++;
    while ((u > 0) && (c == *src)) {
        src++; u--;
    }
    *--psp = (unsigned int)src;
    *--psp = u;
}

CODE(scan) {    /* c-addr u c -- c-addr' u' */
    unsigned char c, *src;
    unsigned int u;
    c = (unsigned char)*psp++;
    u = *psp++;
    src = (unsigned char *)*psp++;
    while ((u > 0) && (c != *src)) {
        src++; u--;
    }
    *--psp = (unsigned int)src;
    *--psp = u;
}

CODE(sequal) {  /* c-addr1 c-addr2 u -- n */
    unsigned char *dst, *src;
    unsigned int u;
    int result = 0;
    u = *psp++;
    dst = (unsigned char *)*psp++;
    src = (unsigned char *)*psp++;
    while ((u-- > 0) & (result == 0)) {
        if (*dst != *src) {
            if (*dst > *src) result = 1;
            else if (*dst < *src) result = -1;
        }
        dst++; src++;
    }
    *--psp = (unsigned int)result;
}

/* TERMINAL I/O */

CODE(key) {
    *--psp = (unsigned int)getch();
}

CODE(emit) {
    putch((char)*psp++);
}

CODE(keyq) {
    *--psp = getquery(); 
}

CODE(dot) {        /* temporary definition for testing */
    printf(" %d", *psp++);
}

CODE(dothh) {        /* temporary definition for testing */
    printf(" %2x", *psp++);
}

CODE(dothhhh) {        /* temporary definition for testing */
    printf(" %8x", *psp++);
}

CODE(dots) {    /* print stack, for testing */
    unsigned int *p;
    p = &pstack[PSTACKSIZE-2];      /* deepest element on stack */
    putchar(' ');
    putchar('D');
    putchar('e');
    putchar('b');
    putchar('u');
    putchar('g');
    putchar(' ');
    printf("\n%8x:", (unsigned int)p);
 // while (p >= psp) printf(" %8x", *p--); // crashes the interpreter - wa1tnr 10 Sep 2018
    while (p >= psp) { printf(" %8x", *p--); }
}

CODE(dump) {   /* adr n -- */
    unsigned char *p;
    unsigned int n, i;
    n = *psp++;
    p = (unsigned char *)*psp++;
    for (i=0; i<n; i++) {
        if ((i&0xf)==0) printf("\n%8x:", (unsigned int)p);
        printf(" %02x", *p++);
    }
}       

CODE(bye) {
    run = 0;
}

/*
 * HIGH LEVEL WORD DEFINITIONS
 */

PRIMITIVE(exit);
PRIMITIVE(execute);
PRIMITIVE(lit);
PRIMITIVE(dup);  // const void * Tdup[]  = { Fdup };
PRIMITIVE(qdup);
PRIMITIVE(drop);
PRIMITIVE(swap);
PRIMITIVE(over);
PRIMITIVE(rot);
PRIMITIVE(nip);
PRIMITIVE(tuck);
PRIMITIVE(tor);
PRIMITIVE(rfrom);
PRIMITIVE(rfetch);
PRIMITIVE(spfetch);
PRIMITIVE(spstore);
PRIMITIVE(rpfetch);
PRIMITIVE(rpstore);
PRIMITIVE(fetch);
PRIMITIVE(store);
PRIMITIVE(cfetch);
PRIMITIVE(cstore);

/* synonyms for unified code and data space */
#define Tifetch  Tfetch
#define Ticfetch Tcfetch
#define Tistore  Tstore
#define Ticstore Tcstore
#define Thfetch  Tfetch
#define Thcfetch Tcfetch
#define Thstore  Tstore
#define Thcstore Tcstore

PRIMITIVE(plus);
PRIMITIVE(plusstore);
PRIMITIVE(mplus);
PRIMITIVE(minus);
PRIMITIVE(mult);
PRIMITIVE(div);
PRIMITIVE(and);
PRIMITIVE(or);
PRIMITIVE(xor);
PRIMITIVE(invert);
PRIMITIVE(negate);
PRIMITIVE(oneplus);
PRIMITIVE(oneminus);
PRIMITIVE(swapbytes);
PRIMITIVE(twostar);
PRIMITIVE(twoslash);
PRIMITIVE(lshift);
PRIMITIVE(rshift);

PRIMITIVE(zeroequal);
PRIMITIVE(zeroless);
PRIMITIVE(equal);
PRIMITIVE(notequal);
PRIMITIVE(less);
PRIMITIVE(greater);
PRIMITIVE(uless);
PRIMITIVE(ugreater);

PRIMITIVE(branch);
PRIMITIVE(qbranch);
PRIMITIVE(xplusloop);
PRIMITIVE(xloop);
PRIMITIVE(xdo);
PRIMITIVE(i);
PRIMITIVE(j);
PRIMITIVE(unloop);
PRIMITIVE(umstar);
PRIMITIVE(umslashmod);
PRIMITIVE(fill);
PRIMITIVE(cmove);
PRIMITIVE(cmoveup);
THREAD(itod) = { Fcmove };  /* synonym */

PRIMITIVE(skip);
PRIMITIVE(scan);
PRIMITIVE(sequal);
THREAD(nequal) = { Fsequal };  /* synonym */
    
PRIMITIVE(key);
PRIMITIVE(emit);
PRIMITIVE(keyq);
// PRIMITIVE(dot);
PRIMITIVE(dothh);
PRIMITIVE(dothhhh);
PRIMITIVE(dots);
PRIMITIVE(dump);
PRIMITIVE(bye);

/* USER VARIABLES */

THREAD(u0) = { Fdouser, LIT(0) };
THREAD(toin) = { Fdouser, LIT(1) };
THREAD(base) = { Fdouser, LIT(2) };
THREAD(state) = { Fdouser, LIT(3) };
THREAD(dp) = { Fdouser, LIT(4) };
THREAD(ticksource) = { Fdouser, LIT(5) };    /* two cells */
THREAD(latest) = { Fdouser, LIT(7) };
THREAD(hp) = { Fdouser, LIT(8) };
THREAD(lp) = { Fdouser, LIT(9) };
// THREAD(idp) = { Fdouser, LIT(10) };          /* not used in this model */
THREAD(newest) = { Fdouser, LIT(11) };

extern const struct Header Hcold;

THREAD(uinit) = { Fdorom, 
    LIT(0),  LIT(0),  LIT(10), LIT(0),  // u0 >in base state
    RAMDICT, LIT(0),  LIT(0),  Hcold.nfa,    // dp source latest
    LIT(0),  LIT(0),  ROMDICT, LIT(0) };  // hp lp idp newest
THREAD(ninit) = { Fdocon, LIT(16*CELL) };

/* CONSTANTS and some system variables */

THREAD(pad) = { Fdocon, padarea };
THREAD(l0) = { Fdocon, &lstack[LSTACKSIZE-1] };
THREAD(s0) = { Fdocon, &pstack[PSTACKSIZE-1] };
THREAD(r0) = { Fdocon, &rstack[RSTACKSIZE-1] };
THREAD(tib) = { Fdocon, tibarea };
THREAD(tibsize) = { Fdocon, LIT(TIBSIZE) };
THREAD(bl) = { Fdocon, LIT(0x20) };

THREAD(zero) = { Fdocon, LIT(0) };
THREAD(one) = { Fdocon, LIT(1) };
THREAD(two) = { Fdocon, LIT(2) };
THREAD(three) = { Fdocon, LIT(3) };
THREAD(minusone) = { Fdocon, LIT(-1) };

/* TO BE SUPPLIED */
#define TBD(name)  const void * T##name[] = { Fenter, Texit }

/* CPU DEPENDENCIES */

THREAD(cell) = { Fdocon, LIT(CELL) };
THREAD(chars) = { Fenter, Texit };   /* no operation */

/* DICTIONARY MANAGEMENT */

THREAD(here) = { Fenter, Tdp, Tfetch, Texit };
THREAD(allot) = { Fenter, Tdp, Tplusstore, Texit };
THREAD(comma) = { Fenter, There, Tstore, Tcell, Tallot, Texit };
THREAD(ccomma) = { Fenter, There, Tcstore, Tone, Tchars, Tallot, Texit };

/* synonyms for unified code and data space */
#define Tidp     Tdp
#define Tihere   There
#define Tiallot  Tallot
#define Ticomma  Tcomma
#define Ticcomma Tccomma
/* synonyms for unified header and data space */
#define Thhere   There
#define Thallot  Tallot
#define Thcomma  Tcomma
#define Thccomma Tccomma
#define Thword   Tword

/* CPU DEPENDENCIES */

    /* cell alignment */
THREAD(aligned) = { Fenter, Tcell, Tover, Tminus, Tcell, Toneminus, Tand,
                Tplus, Texit };
THREAD(align) = { Fenter, Tihere, Taligned, Tidp, Tstore, Texit };
THREAD(cellplus) = { Fenter, Tcell, Tplus, Texit };
THREAD(charplus) = { Fenter, Tone, Tplus, Texit };

/* >BODY is entered with CFA on stack.  For most words, body is
 * CFA + 1 cell.  For words built with CREATE or CREATE..DOES> ,
 * body is CFA + 2 cells. */
THREAD(tobody) = { Fenter, 
    Tdup, Tifetch,                      /* fetch code field */
    Tdup, Tlit, Fdocreate, Tequal,      /* if it's Fdocreate */
    Tswap, Tlit, Fdobuilds, Tequal, Tor,  /* or Fdobuilds */
    Tqbranch, OFFSET(3), Tcell, Tplus,  /* then add an extra cell */
    Tcell, Tplus, Texit };

THREAD(commaxt) = { Fenter, Ticomma, Texit };
THREAD(storecf) = { Fenter, Tistore, Texit };
THREAD(commacf) = { Fenter, Tihere, Tstorecf, Tcell, Tiallot, Texit };
THREAD(commaexit) = { Fenter, Tlit, Texit, Tcommaxt, Texit };

    /* the c model uses relative addressing from the location of the 
     * offset cell */
THREAD(commabranch) = { Fenter, Ticomma, Texit };
THREAD(commadest) = { Fenter, Tihere, Tminus, Ticomma, Texit };
THREAD(storedest) = { Fenter, Ttuck, Tminus, Tswap, Tistore, Texit };
THREAD(commanone) = { Fenter, Tcell, Tiallot, Texit };

/* DOUBLE OPERATORS */

THREAD(twofetch) = { Fenter, Tdup, Tcellplus, Tfetch, Tswap, Tfetch, Texit };
THREAD(twostore) = { Fenter, Tswap, Tover, Tstore, Tcellplus, Tstore, Texit };
THREAD(twodrop) = { Fenter, Tdrop, Tdrop, Texit };
THREAD(twodup) = { Fenter, Tover, Tover, Texit };
THREAD(twoswap) = { Fenter, Trot, Ttor, Trot, Trfrom, Texit };
THREAD(twoover) = { Fenter, Ttor, Ttor, Ttwodup, Trfrom, Trfrom,
                    Ttwoswap, Texit };

/* ARITHMETIC OPERATORS */

THREAD(stod) = { Fenter, Tdup, Tzeroless, Texit };
THREAD(qnegate) = { Fenter, Tzeroless, 
                    Tqbranch, OFFSET(2), Tnegate, Texit };
THREAD(abs) = { Fenter, Tdup, Tqnegate, Texit };
THREAD(dnegate) = { Fenter, Tswap, Tinvert, Tswap, Tinvert, 
                    Tone, Tmplus, Texit };
THREAD(qdnegate) = { Fenter, Tzeroless, 
                    Tqbranch, OFFSET(2), Tdnegate, Texit };
THREAD(dabs) = { Fenter, Tdup, Tqdnegate, Texit };

THREAD(mstar) = { Fenter, Ttwodup, Txor, Ttor,
                    Tswap, Tabs, Tswap, Tabs, Tumstar,
                    Trfrom, Tqdnegate, Texit }; 
THREAD(smslashrem) = { Fenter, Ttwodup, Txor, Ttor, Tover, Ttor,
                    Tabs, Ttor, Tdabs, Trfrom, Tumslashmod, Tswap,
                    Trfrom, Tqnegate, Tswap, Trfrom, Tqnegate, Texit };
THREAD(fmslashmod) = { Fenter, Tdup, Ttor, Ttwodup, Txor, Ttor, Ttor,
                    Tdabs, Trfetch, Tabs, Tumslashmod,
                    Tswap, Trfrom, Tqnegate, Tswap, Trfrom, Tzeroless, 
                    Tqbranch, OFFSET(10),
                    Tnegate, Tover, Tqbranch, OFFSET(6),
                    Trfetch, Trot, Tminus, Tswap, Toneminus,
                    /* branch dest */ Trfrom, Tdrop, Texit };
THREAD(star) = { Fenter, Tmstar, Tdrop, Texit };
THREAD(slashmod) = { Fenter, Ttor, Tstod, Trfrom, Tfmslashmod, Texit };
THREAD(slash) = { Fenter, Tslashmod, Tnip, Texit };
THREAD(mod) = { Fenter, Tslashmod, Tdrop, Texit };
THREAD(starslashmod) = { Fenter, Ttor, Tmstar, Trfrom, Tfmslashmod, Texit };
THREAD(starslash) = { Fenter, Tstarslashmod, Tnip, Texit };
THREAD(max) = { Fenter, Ttwodup, Tless, Tqbranch, OFFSET(2), Tswap,
                Tdrop, Texit };
THREAD(min) = { Fenter, Ttwodup, Tgreater, Tqbranch, OFFSET(2), Tswap,
                Tdrop, Texit };
THREAD(umax) = { Fenter, Ttwodup, Tuless, Tqbranch, OFFSET(2), Tswap,
                Tdrop, Texit };
THREAD(umin) = { Fenter, Ttwodup, Tugreater, Tqbranch, OFFSET(2), Tswap,
                Tdrop, Texit };

/* CPU DEPENDENCIES CONT'D. */

THREAD(cells) = { Fenter, Tcell, Tstar, Texit };
THREAD(storecolon) = { Fenter, Ttwo, Tcells, Tnegate, Tiallot, 
                      Tlit, Fenter, Tcommacf, Texit };

/* INPUT/OUTPUT */

THREAD(count) = { Fenter, Tdup, Tcharplus, Tswap, Tcfetch, Texit };
THREAD(cr) = { Fenter, Tlit, LIT(0x0d), Temit, Tlit, LIT(0x0a), Temit,
                Texit };
THREAD(space) = { Fenter, Tlit, LIT(0x20), Temit, Texit };
THREAD(spaces) = { Fenter, Tdup, Tqbranch, OFFSET(5), Tspace, Toneminus,
                Tbranch, OFFSET(-6), Tdrop, Texit };

#ifdef LINUX
#define NEWLINE 0x0a
#define BACKSPACE 0x7f      /* key returned for backspace */
#define BACKUP  8           /* what to emit for backspace */
#else
#define NEWLINE 0x0d
#define BACKSPACE 8         /* key returned for backspace */
#define BACKUP  8           /* what to emit for backspace */
#endif
                
THREAD(accept) = { Fenter, Tover, Tplus, Toneminus, Tover,
/* 1 */  Tkey, Tdup, Tlit, LIT(NEWLINE), Tnotequal, Tqbranch, OFFSET(27 /*5*/),
         Tdup, Tlit, LIT(BACKSPACE), Tequal, Tqbranch, OFFSET(12 /*3*/),
         Tdrop, Tlit, LIT(BACKUP), Temit, Toneminus, Ttor, Tover, Trfrom, 
         Tumax, Tbranch, OFFSET(8 /*4*/),
/* 3 */  Tdup, Temit, Tover, Tcstore, Toneplus, Tover, Tumin,
/* 4 */  Tbranch, OFFSET(-32 /*1*/),
/* 5 */  Tdrop, Tnip, Tswap, Tminus, Texit };

THREAD(type) = { Fenter, Tqdup, Tqbranch, OFFSET(12 /*4*/),
         Tover, Tplus, Tswap, Txdo,
/* 3 */  Ti, Tcfetch, Temit, Txloop, OFFSET(-4 /*3*/),
         Tbranch,  OFFSET(2 /*5*/),
/* 4 */  Tdrop,
/* 5 */  Texit };

#define Ticount Tcount
#define Titype Ttype

/* NUMERIC OUTPUT */

THREAD(udslashmod) = { Fenter, Ttor, Tzero, Trfetch, Tumslashmod, Trot, Trot,
         Trfrom, Tumslashmod, Trot, Texit };
THREAD(udstar) = { Fenter, Tdup, Ttor, Tumstar, Tdrop,
         Tswap, Trfrom, Tumstar, Trot, Tplus, Texit };
         
THREAD(hold) = { Fenter, Tminusone, Thp, Tplusstore,
                Thp, Tfetch, Tcstore, Texit };
THREAD(lessnum) = { Fenter, Tlit, &holdarea[HOLDSIZE-1], Thp, Tstore, Texit };
THREAD(todigit) = { Fenter, Tdup, Tlit, LIT(9), Tgreater, Tlit, LIT(7),
                Tand, Tplus, Tlit, LIT(0x30), Tplus, Texit };
THREAD(num) = { Fenter, Tbase, Tfetch, Tudslashmod, Trot, Ttodigit,
                Thold, Texit };
THREAD(nums) = { Fenter, Tnum, Ttwodup, Tor, Tzeroequal, Tqbranch, OFFSET(-5),
                Texit };
THREAD(numgreater) = { Fenter, Ttwodrop, Thp, Tfetch, 
                Tlit, &holdarea[HOLDSIZE-1], Tover, Tminus, Texit };
THREAD(sign) = { Fenter, Tzeroless, Tqbranch, OFFSET(4), Tlit, LIT(0x2d), 
                Thold, Texit };
THREAD(udot) = { Fenter, Tlessnum, Tzero, Tnums, Tnumgreater, Ttype,
                Tspace, Texit };
THREAD(dot) = { Fenter, Tlessnum, Tdup, Tabs, Tzero, Tnums, Trot, Tsign,
                Tnumgreater, Ttype, Tspace, Texit };
THREAD(decimal) = { Fenter, Tlit, LIT(10), Tbase, Tstore, Texit };
THREAD(hex) = { Fenter, Tlit, LIT(16), Tbase, Tstore, Texit };

/* INTERPRETER */

THREAD(source) = { Fenter, Tticksource, Ttwofetch,  Texit };
THREAD(slashstring) = { Fenter, Trot, Tover, Tplus, Trot, Trot, Tminus,
                        Texit };
THREAD(tocounted) = { Fenter, Ttwodup, Tcstore, Tcharplus, Tswap, Tcmove,
                        Texit };
THREAD(adrtoin) = { Fenter, Tsource, Trot, Trot, Tminus, Tmin, Tzero, Tmax,
                    Ttoin, Tstore, Texit };
THREAD(parse) = { Fenter, Tsource, Ttoin, Tfetch, Tslashstring,   
        Tover, Ttor, Trot, Tscan, Tover, Tswap, Tqbranch, OFFSET(2),
        Tcharplus, Tadrtoin, Trfrom, Ttuck, Tminus, Texit };
THREAD(word) = { Fenter, Tdup, Tsource, Ttoin, Tfetch, Tslashstring,
        Trot, Tskip, Tdrop, Tadrtoin, Tparse, There, Ttocounted, There,
        Tbl, Tover, Tcount, Tplus, Tcstore, Texit };

/* (S") S" ." for unified code/data space */
THREAD(xsquote) = { Fenter, Trfrom, Tcount, Ttwodup, Tplus, Taligned, Ttor,
         Texit };

THREAD(squote) = { Fenter, Tlit, Txsquote, Tcommaxt,
         Tlit, LIT(0x22), Tword, Tcfetch, Toneplus,
         Taligned, Tallot, Texit };

#define Txisquote Txsquote
#define Tisquote  Tsquote
         
THREAD(dotquote) = { Fenter, Tsquote, Tlit, Ttype, Tcommaxt, Texit };

    /* Dictionary header [sizes in bytes]:
     *   link[4], cfa[4], flags[1], name[n] 
     * Note that link is to the previous name field. */
    
// THREAD(lfatonfa) = { Fenter, Tlit, LIT(CELL*2 + 1), Tplus, Texit };
THREAD(nfatolfa) = { Fenter, Tlit, LIT(CELL*2 + 1), Tminus, Texit };
// THREAD(lfatocfa) = {  Fenter, Tcell, Tplus, Thfetch, Texit };
THREAD(nfatocfa) = {  Fenter, Tlit, LIT(CELL+1), Tminus, Thfetch, Texit };
THREAD(immedq) = { Fenter, Toneminus, Thcfetch, Tone, Tand, Texit };

THREAD(find) = { Fenter, Tlatest, Tfetch,
 /*1*/  Ttwodup, Tover, Tcfetch, Tcharplus,
        Tnequal, Tdup, Tqbranch, OFFSET(5 /*2*/),
        Tdrop, Tnfatolfa, Thfetch, Tdup,
 /*2*/  Tzeroequal, Tqbranch, OFFSET(-14 /*1*/),
        Tdup, Tqbranch, OFFSET(9 /*3*/),
        Tnip, Tdup, Tnfatocfa,
        Tswap, Timmedq, Tzeroequal, Tone, Tor,
 /*3*/  Texit };

THREAD(literal) = { Fenter,   
        Tstate, Tfetch, Tqbranch, OFFSET(5),
        Tlit, Tlit, Tcommaxt, Ticomma, 
        Texit };

THREAD(digitq) = { Fenter,   
        Tdup, Tlit, LIT(0x39), Tgreater, Tlit, LIT(0x100), Tand, Tplus,
        Tdup, Tlit, LIT(0x140), Tgreater, Tlit, LIT(0x107), Tand,
        Tminus, Tlit, LIT(0x30), Tminus,
        Tdup, Tbase, Tfetch, Tuless, Texit };

THREAD(qsign) = { Fenter,   
        Tover, Tcfetch, Tlit, LIT(0x2c), Tminus, Tdup, Tabs,
        Tone, Tequal, Tand, Tdup, Tqbranch, OFFSET(6),
            Toneplus, Ttor, Tone, Tslashstring, Trfrom,
        Texit };

THREAD(tonumber) = { Fenter,   
 /*1*/  Tdup, Tqbranch, OFFSET(21 /*3*/),
        Tover, Tcfetch, Tdigitq,
        Tzeroequal, Tqbranch, OFFSET (3 /*2*/),
        Tdrop, Texit,
 /*2*/  Ttor, Ttwoswap, Tbase, Tfetch, Tudstar,
        Trfrom, Tmplus, Ttwoswap,
        Tone, Tslashstring, Tbranch, OFFSET (-22 /*1*/),
 /*3*/  Texit };

THREAD(qnumber) = { Fenter, Tdup, Tzero, Tdup, Trot, Tcount,
        Tqsign, Ttor, Ttonumber, Tqbranch, OFFSET(7 /*1*/),
        Trfrom, Ttwodrop, Ttwodrop, Tzero,
        Tbranch, OFFSET(8 /*3*/),
 /*1*/  Ttwodrop, Tnip, Trfrom, Tqbranch, OFFSET(2 /*2*/),
        Tnegate,
 /*2*/  Tminusone,
 /*3*/  Texit };

extern const void * Tabort[];   /* forward reference */

THREAD(interpret) = { Fenter,   
        Tticksource, Ttwostore, Tzero, Ttoin, Tstore,
 /*1*/  Tbl, Tword, Tdup, Tcfetch, Tqbranch, OFFSET(33 /*9*/),
        Tfind, Tqdup, Tqbranch, OFFSET(14 /*4*/),
        Toneplus, Tstate, Tfetch, Tzeroequal, Tor, 
        Tqbranch, OFFSET(4 /*2*/),
        Texecute, Tbranch, OFFSET(2 /*3*/),
 /*2*/  Tcommaxt,
 /*3*/  Tbranch, OFFSET(14 /*8*/),
 /*4*/  Tqnumber, Tqbranch, OFFSET(4 /*5*/),
        Tliteral, Tbranch, OFFSET(8 /*6*/),
 /*5*/  Tcount, Ttype, Tlit, LIT(0x3f), Temit, Tcr, Tabort,
 /*6*/
 /*8*/  Tbranch, OFFSET(-37 /*1*/),
 /*9*/  Tdrop, Texit };

THREAD(evaluate) = { Fenter, Tticksource, Ttwofetch, Ttor, Ttor,
        Ttoin, Tfetch, Ttor, Tinterpret,
        Trfrom, Ttoin, Tstore, Trfrom, Trfrom,
        Tticksource, Ttwostore, Texit };

const char okprompt[] = "\003ok ";

THREAD(quit) = { Fenter, Tl0, Tlp, Tstore,
        Tr0, Trpstore, Tzero, Tstate, Tstore,
 /*1*/  Ttib, Tdup, Ttibsize, Taccept, Tspace, Tinterpret,
        Tcr, Tstate, Tfetch, Tzeroequal, Tqbranch, OFFSET(5 /*2*/),
        Tlit, okprompt, Ticount, Titype,
 /*2*/  Tbranch, OFFSET(-17 /*1*/) };     // never exits

THREAD(abort) = { Fenter, Ts0, Tspstore, Tquit };

THREAD(qabort) = { Fenter, Trot, Tqbranch, OFFSET(3), Titype, Tabort,
                   Ttwodrop, Texit };
                   
THREAD(abortquote) = { Fenter, Tisquote, Tlit, Tqabort, Tcommaxt, Texit };

const char huhprompt[] = "\001?";

THREAD(tick) = { Fenter, Tbl, Tword, Tfind, Tzeroequal, 
        Tlit, huhprompt, Ticount, Tqabort, Texit };

/* COMPILER */

THREAD(char) = { Fenter, Tbl, Tword, Toneplus, Tcfetch, Texit };

THREAD(bracchar) = { Fenter, Tchar, Tlit, Tlit, Tcommaxt, Ticomma, Texit };

THREAD(paren) = { Fenter, Tlit, LIT(0x29), Tparse, Ttwodrop, Texit };

/* header is  { link-to-nfa, cfa, flags, name }  where default cfa,
 * in unified memory space, is immediately following header */

THREAD(header) = { Fenter, Tlatest, Tfetch, Thcomma, /* link */
        Thhere, Tcell, Thallot,                 /* reserve cell for cfa */
        Tzero, Thccomma,                        /* flags byte */
        Thhere, Tlatest, Tstore,                /* new latest = nfa */
        Tbl, Thword, Thcfetch, Toneplus, Thallot,   /* name field */
        Talign, Tihere, Tswap, Thstore, Texit };    /* patch cfa cell */

/* defined word is { Fdobuilds, Tdoesword, ... }  
 * Fdobuilds is installed by DOES> so we can use CREATE or <BUILDS.
 * Both CREATE and <BUILDS should reserve the two cells */

THREAD(create) = { Fenter, Theader, Tlit, Fdocreate, Tcommacf, 
        Tihere, Tcellplus, Ticomma, Texit };

THREAD(builds) = { Fenter, Tcreate, Texit };    /* same as CREATE */

THREAD(variable) = { Fenter, Theader, Tlit, Fdovar, Tcommacf, 
        Tihere, Tcellplus, Ticomma, Tcell, Tiallot, Texit };    
        /* TODO: this is inline variable. fix for RAM/ROM */

THREAD(constant) = { Fenter, Theader, Tlit, Fdocon, Tcommacf, 
        Ticomma, Texit };

THREAD(user) = { Fenter, Theader, Tlit, Fdouser, Tcommacf, 
        Ticomma, Texit };

/* defining word thread is 
 *   { ...build code... Txdoes, Fenter, ...DOES> code.... }   
 *                              \--headless Forth word--/     */

THREAD(xdoes) = { Fenter, Trfrom,           /* xt of headless doesword */
        Tlatest, Tfetch, Tnfatocfa,         /* cfa of word being built */
        Tlit, Fdobuilds, Tover, Tstorecf,   /* first cell: Fdobuilds */
        Tcellplus, Tistore,                 /* second cell: xt of doesword */
        Texit };
        
THREAD(does) = { Fenter, Tlit, Txdoes, Tcommaxt, 
        Tlit, Fenter, Ticomma, Texit };
        
THREAD(recurse) = { Fenter, Tnewest, Tfetch, Tnfatocfa, Tcommaxt, Texit };

THREAD(leftbracket) = { Fenter, Tzero, Tstate, Tstore, Texit };

THREAD(rightbracket) = { Fenter, Tminusone, Tstate, Tstore, Texit };

THREAD(hide) = { Fenter, Tlatest, Tfetch, Tdup, Tnewest, Tstore,
        Tnfatolfa, Thfetch, Tlatest, Tstore, Texit };
        
THREAD(reveal) = { Fenter, Tnewest, Tfetch, Tlatest, Tstore, Texit };

THREAD(immediate) = { Fenter, Tone, Tlatest, Tfetch, 
        Tone, Tchars, Tminus, Thcstore, Texit };
        
THREAD(colon) = { Fenter, Tbuilds, Thide, Trightbracket, Tstorecolon,
        Texit };
        
THREAD(semicolon) = { Fenter, Treveal, Tcommaexit, Tleftbracket, Texit };

THREAD(brackettick) = { Fenter, Ttick, Tlit, Tlit, Tcommaxt, Ticomma, Texit };

THREAD(postpone) = { Fenter, Tbl, Tword, Tfind, Tdup, Tzeroequal, 
        Tlit, huhprompt, Ticount, Tqabort,
        Tzeroless, Tqbranch, OFFSET(10),
        Tlit, Tlit, Tcommaxt, Ticomma, 
        Tlit, Tcommaxt, Tcommaxt, Tbranch, OFFSET(2),
        Tcommaxt, Texit };

THREAD(compile) = { Fenter, Trfrom, Tdup, Tcellplus, Ttor, 
        Tifetch, Tcommaxt, Texit };

/* CONTROL STRUCTURES */

THREAD(if) = { Fenter, Tlit, Tqbranch, Tcommabranch, Tihere, Tcommanone,
        Texit };
THREAD(then) = { Fenter, Tihere, Tswap, Tstoredest, Texit };
THREAD(else) = { Fenter, Tlit, Tbranch, Tcommabranch, Tihere, Tcommanone,
        Tswap, Tthen, Texit };
THREAD(begin) = { Fenter, Tihere, Texit };
THREAD(until) = { Fenter, Tlit, Tqbranch, Tcommabranch, Tcommadest, Texit };
THREAD(again) = { Fenter, Tlit, Tbranch, Tcommabranch, Tcommadest, Texit };
THREAD(while) = { Fenter, Tif, Tswap, Texit };
THREAD(repeat) = { Fenter, Tagain, Tthen, Texit };

THREAD(tol) = { Fenter, Tcell, Tlp, Tplusstore, Tlp, Tfetch, Tstore, Texit };
THREAD(lfrom) = { Fenter, Tlp, Tfetch, Tfetch, Tcell, Tnegate, Tlp, 
        Tplusstore, Texit };
THREAD(do) = { Fenter, Tlit, Txdo, Tcommaxt, Tihere, Tzero, Ttol, Texit };
THREAD(endloop) = { Fenter, Tcommabranch, Tcommadest, 
        Tlfrom, Tqdup, Tqbranch, OFFSET(4), Tthen, Tbranch, OFFSET(-6),
        Texit };
THREAD(loop) = { Fenter, Tlit, Txloop, Tendloop, Texit };
THREAD(plusloop) = { Fenter, Tlit, Txplusloop, Tendloop, Texit };
THREAD(leave) = { Fenter, Tlit, Tunloop, Tcommaxt, 
        Tlit, Tbranch, Tcommabranch, Tihere, Tcommanone, Ttol, Texit };

/* OTHER OPERATIONS */

THREAD(within) = { Fenter, Tover, Tminus, Ttor, Tminus, Trfrom,
        Tuless, Texit };
THREAD(move) =  { Fenter, Ttor, Ttwodup, Tswap, Tdup, Trfetch, Tplus,
        Twithin, Tqbranch, OFFSET(5), Trfrom, Tcmoveup, Tbranch, OFFSET(3),
        Trfrom, Tcmove, Texit };
THREAD(depth) = { Fenter, Tspfetch, Ts0, Tswap, Tminus, Tcell, Tslash, 
        Texit };
THREAD(environmentq) = { Fenter, Ttwodrop, Tzero, Texit };


/*
THREAD() = { Fenter,   Texit };
THREAD() = { Fenter,   Texit };
THREAD() = { Fenter,   Texit };
*/

/* UTILITY WORDS */

THREAD(marker) = { Fenter, Tlatest, Tfetch, Tihere, There,
        Tbuilds, Ticomma, Ticomma, Ticomma, Txdoes,
        /* DOES> action as a headerless Forth word */
        Fenter, Tdup, Tifetch, Tswap, Tcellplus, Tdup, Tifetch,
        Tswap, Tcellplus, Tifetch,
        Tlatest, Tstore, Tidp, Tstore, Tdp, Tstore, Texit };

#define Thcount Tcount
#define Thtype Ttype

THREAD(words) = { Fenter, Tlatest, Tfetch,
 /*1*/  Tdup, Thcount, Tlit, LIT(0x7f), Tand, Thtype, Tspace,
        Tnfatolfa, Thfetch, Tdup, Tzeroequal, Tqbranch, OFFSET(-12),
        Tdrop, Texit, };


/* MAIN ENTRY POINT */

const char coldprompt[] = "\042CamelForth in C v0.1 - 14 Feb 2016 2021:aa";

THREAD(cold) = { Fenter, 
    Tuinit, Tu0, Tninit, Titod,     /* important initialization! */
    Tlit, coldprompt, Tcount, Ttype, Tcr,
    Tabort, };                      /* Tabort never exits */
    
/*
 * INNER INTERPRETER
 */

void interpreter(void)
{
    void (*xt)(void *);     /* pointer to code function */
    void *w, *x;            /* generic pointers */
    
    psp = &pstack[PSTACKSIZE-1];
    rsp = &rstack[RSTACKSIZE-1];
    ip = &Tcold;
    ip += CELL;
    run = 1;                /* set to zero to terminate interpreter */
    while (run) {
        w = *(void **)ip;       /* fetch word address from thread */
        ip += CELL;
        x = *(void **)w;        /* fetch function adrs from word def */
        xt = (void (*)())x;     /* too much casting! */
        w += CELL;
        (*xt)(w);               /* call function w/adrs of word def */
    }        
}

/*
 * DICTIONARY HEADERS
 */

/* first header must be explicitly spelled out for null link */
const struct Header Hexit = {  NULL, Texit, 0, "\004EXIT"  };

HEADER(execute, exit, 0, "\007EXECUTE");
HEADER(lit, execute, 0, "\003lit");
HEADER(dup, lit, 0, "\003DUP");
HEADER(qdup, dup, 0, "\004?DUP");
HEADER(drop, qdup, 0, "\004DROP");
HEADER(swap, drop, 0, "\004SWAP");
HEADER(over, swap, 0, "\004OVER");
HEADER(rot, over, 0, "\003ROT");
HEADER(nip, rot, 0, "\003NIP");
HEADER(tuck, nip, 0, "\004TUCK");
HEADER(tor, tuck, 0, "\002>R");
HEADER(rfrom, tor, 0, "\002R>");
HEADER(rfetch, rfrom, 0, "\002R@");
HEADER(spfetch, rfetch, 0, "\003SP@");
HEADER(spstore, spfetch, 0, "\003SP!");
HEADER(rpfetch, spstore, 0, "\003RP@");
HEADER(rpstore, rpfetch, 0, "\003RP!");
HEADER(fetch, rpstore, 0, "\001@");
HEADER(store, fetch, 0, "\001!");
HEADER(cfetch, store, 0, "\002C@");
HEADER(cstore, cfetch, 0, "\002C!");

/* synonyms for unified code/data/header space */
HEADER(ifetch, cstore, 0, "\002I@");
HEADER(istore, ifetch, 0, "\002I!");
HEADER(icfetch, istore, 0, "\003IC@");
HEADER(icstore, icfetch, 0, "\003IC!");
HEADER(hfetch, icstore, 0, "\002H@");
HEADER(hstore, hfetch, 0, "\002H!");
HEADER(hcfetch, hstore, 0, "\003HC@");
HEADER(hcstore, hcfetch, 0, "\003HC!");

HEADER(plus, hcstore, 0, "\001+");
HEADER(plusstore, plus, 0, "\002+!");
HEADER(mplus, plusstore, 0, "\002M+");
HEADER(minus, mplus, 0, "\001-");
HEADER(mult, minus, 0, "\001*");
HEADER(div, mult, 0, "\001/");
HEADER(and, div, 0, "\003AND");
HEADER(or, and, 0, "\002OR");
HEADER(xor, or, 0, "\003XOR");
HEADER(invert, xor, 0, "\006INVERT");
HEADER(negate, invert, 0, "\006NEGATE");
HEADER(oneplus, negate, 0, "\0021+");
HEADER(oneminus, oneplus, 0, "\0021-");
HEADER(swapbytes, oneminus, 0, "\002><");
HEADER(twostar, swapbytes, 0, "\0022*");
HEADER(twoslash, twostar, 0, "\0022/");
HEADER(lshift, twoslash, 0, "\006LSHIFT");
HEADER(rshift, lshift, 0, "\006RSHIFT");

HEADER(zeroequal, rshift, 0, "\0020=");
HEADER(zeroless, zeroequal, 0, "\0020<");
HEADER(equal, zeroless, 0, "\001=");
HEADER(notequal, equal, 0, "\002<>");
HEADER(less, notequal, 0, "\001<");
HEADER(greater, less, 0, "\001>");
HEADER(uless, greater, 0, "\002U<");
HEADER(ugreater, uless, 0, "\002U>");

HEADER(branch, ugreater, 0, "\006branch");
HEADER(qbranch, branch, 0, "\007?branch");
HEADER(xplusloop, qbranch, 0, "\007(+loop)");
HEADER(xloop, xplusloop, 0, "\006(loop)");
HEADER(xdo, xloop, 0, "\004(do)");
HEADER(i, xdo, 0, "\001I");
HEADER(j, i, 0, "\001J");
HEADER(unloop, j, 0, "\006UNLOOP");
HEADER(umstar, unloop, 0, "\003UM*");
HEADER(umslashmod, umstar, 0, "\006UM/MOD");
HEADER(fill, umslashmod, 0, "\004FILL");
HEADER(cmove, fill, 0, "\005CMOVE");
HEADER(cmoveup, cmove, 0, "\006CMOVE>");
HEADER(itod, cmoveup, 0, "\004I->D");

HEADER(skip, itod, 0, "\004SKIP");
HEADER(scan, skip, 0, "\004SCAN");
HEADER(sequal, scan, 0, "\002S=");
HEADER(nequal, sequal, 0, "\002N=");
    
HEADER(key, nequal, 0, "\003KEY");
HEADER(emit, key, 0, "\004EMIT");
HEADER(keyq, emit, 0, "\004KEY?");
HEADER(bye, keyq, 0, "\003BYE");

/* high level definitions */
HEADER(u0, bye, 0, "\002U0");
HEADER(toin, u0, 0, "\003>IN");
HEADER(base, toin, 0, "\004BASE");
HEADER(state, base, 0, "\005STATE");
HEADER(dp, state, 0, "\002DP");
HEADER(ticksource, dp, 0, "\007'SOURCE");
HEADER(latest, ticksource, 0, "\006LATEST");
HEADER(hp, latest, 0, "\002HP");
HEADER(lp, hp, 0, "\002LP");
// HEADER(idp, lp, 0, "\003IDP");
HEADER(newest, lp, 0, "\006NEWEST");
HEADER(uinit, newest, 0, "\005UINIT");
HEADER(ninit, uinit, 0, "\005#INIT");
HEADER(pad, ninit, 0, "\003PAD");
HEADER(l0, pad, 0, "\002L0");
HEADER(s0, l0, 0, "\002S0");
HEADER(r0, s0, 0, "\002R0");
HEADER(tib, r0, 0, "\003TIB");
HEADER(tibsize, tib, 0, "\007TIBSIZE");
HEADER(bl, tibsize, 0, "\002BL");
HEADER(zero, bl, 0, "\0010");
HEADER(one, zero, 0, "\0011");
HEADER(two, one, 0, "\0012");
HEADER(three, two, 0, "\0013");
HEADER(minusone, three, 0, "\002-1");
HEADER(cell, minusone, 0, "\004CELL");
HEADER(chars, cell, 0, "\005CHARS");
HEADER(here, chars, 0, "\004HERE");
HEADER(allot, here, 0, "\005ALLOT");
HEADER(comma, allot, 0, "\001,");
HEADER(ccomma, comma, 0, "\002C,");
HEADER(aligned, ccomma, 0, "\007ALIGNED");
HEADER(align, aligned, 0, "\005ALIGN");
HEADER(cellplus, align, 0, "\005CELL+");
HEADER(charplus, cellplus, 0, "\005CHAR+");
HEADER(tobody, charplus, 0, "\005>BODY");
HEADER(commaxt, tobody, 0, "\010COMPILE,");
HEADER(storecf, commaxt, 0, "\003!CF");
HEADER(commacf, storecf, 0, "\003,CF");
HEADER(storecolon, commacf, 0, "\006!COLON");
HEADER(commaexit, storecolon, 0, "\005,EXIT");
HEADER(commabranch, commaexit, 0, "\007,BRANCH");
HEADER(commadest, commabranch, 0, "\005,DEST");
HEADER(storedest, commadest, 0, "\005!DEST");
HEADER(commanone, storedest, 0, "\005,NONE");
HEADER(twofetch, commanone, 0, "\0022@");
HEADER(twostore, twofetch, 0, "\0022!");
HEADER(twodrop, twostore, 0, "\0052DROP");
HEADER(twodup, twodrop, 0, "\0042DUP");
HEADER(twoswap, twodup, 0, "\0052SWAP");
HEADER(twoover, twoswap, 0, "\0052OVER");
HEADER(stod, twoover, 0, "\003S>D");
HEADER(qnegate, stod, 0, "\007?NEGATE");
HEADER(abs, qnegate, 0, "\003ABS");
HEADER(dnegate, abs, 0, "\007DNEGATE");
HEADER(qdnegate, dnegate, 0, "\010?DNEGATE");
HEADER(dabs, qdnegate, 0, "\004DABS");
HEADER(mstar, dabs, 0, "\002M*");
HEADER(smslashrem, mstar, 0, "\006SM/REM");
HEADER(fmslashmod, smslashrem, 0, "\006FM/MOD");
HEADER(star, fmslashmod, 0, "\001*");
HEADER(slashmod, star, 0, "\004/MOD");
HEADER(slash, slashmod, 0, "\001/");
HEADER(mod, slash, 0, "\003MOD");
HEADER(starslashmod, mod, 0, "\005*/MOD");
HEADER(starslash, starslashmod, 0, "\002*/");
HEADER(max, starslash, 0, "\003MAX");
HEADER(min, max, 0, "\003MIN");
HEADER(umax, min, 0, "\004UMAX");
HEADER(umin, umax, 0, "\004UMIN");
HEADER(cells, umin, 0, "\005CELLS");
HEADER(count, cells, 0, "\005COUNT");
HEADER(cr, count, 0, "\002CR");
HEADER(space, cr, 0, "\005SPACE");
HEADER(spaces, space, 0, "\006SPACES");
HEADER(accept, spaces, 0, "\006ACCEPT");
HEADER(type, accept, 0, "\004TYPE");
HEADER(udslashmod, type, 0, "\006UD/MOD");
HEADER(udstar, udslashmod, 0, "\003UD*");
HEADER(hold, udstar, 0, "\004HOLD");
HEADER(lessnum, hold, 0, "\002<#");
HEADER(todigit, lessnum, 0, "\006>DIGIT");
HEADER(num, todigit, 0, "\001#");
HEADER(nums, num, 0, "\002#S");
HEADER(numgreater, nums, 0, "\002#>");
HEADER(sign, numgreater, 0, "\004SIGN");
HEADER(udot, sign, 0, "\002U.");
HEADER(dot, udot, 0, "\001.");
HEADER(decimal, dot, 0, "\007DECIMAL");
HEADER(hex, decimal, 0, "\003HEX");
HEADER(source, hex, 0, "\006SOURCE");
HEADER(slashstring, source, 0, "\007/STRING");
HEADER(tocounted, slashstring, 0, "\010>COUNTED");
HEADER(adrtoin, tocounted, 0, "\006ADR>IN");
HEADER(parse, adrtoin, 0, "\005PARSE");
HEADER(word, parse, 0, "\004WORD");
HEADER(xsquote, word, 0, "\004(S\")");
HEADER(squote, xsquote, IMMEDIATE, "\002S\"");
HEADER(dotquote, squote, IMMEDIATE, "\002.\"");
HEADER(nfatolfa, dotquote, 0, "\007NFA>LFA");
HEADER(nfatocfa, nfatolfa, 0, "\007NFA>CFA");
HEADER(immedq, nfatocfa, 0, "\006IMMED?");
HEADER(find, immedq, 0, "\004FIND");
HEADER(literal, find, IMMEDIATE, "\007LITERAL");
HEADER(digitq, literal, 0, "\006DIGIT?");
HEADER(qsign, digitq, 0, "\005?SIGN");
HEADER(tonumber, qsign, 0, "\007>NUMBER");
HEADER(qnumber, tonumber, 0, "\007?NUMBER");
HEADER(interpret, qnumber, 0, "\011INTERPRET");
HEADER(evaluate, interpret, 0, "\010EVALUATE");
HEADER(quit, evaluate, 0, "\004QUIT");
HEADER(abort, quit, 0, "\005ABORT");
HEADER(qabort, abort, 0, "\006?ABORT");
HEADER(abortquote, qabort, IMMEDIATE, "\006ABORT\"");
HEADER(tick, abortquote, 0, "\001'");
HEADER(char, tick, 0, "\004CHAR");
HEADER(bracchar, char, IMMEDIATE, "\006[CHAR]");
HEADER(paren, bracchar, IMMEDIATE, "\001(");
HEADER(header, paren, 0, "\006HEADER");
HEADER(builds, header, 0, "\007<BUILDS");
HEADER(variable, builds, 0, "\010VARIABLE");
HEADER(constant, variable, 0, "\010CONSTANT");
HEADER(user, constant, 0, "\004USER");
HEADER(create, user, 0, "\006CREATE");
HEADER(xdoes, create, 0, "\007(DOES>)");
HEADER(does, xdoes, IMMEDIATE, "\005DOES>");       
HEADER(recurse, does, IMMEDIATE, "\007RECURSE");
HEADER(leftbracket, recurse, IMMEDIATE, "\001[");
HEADER(rightbracket, leftbracket, 0, "\001]");
HEADER(hide, rightbracket, 0, "\004HIDE");
HEADER(reveal, hide, 0, "\006REVEAL");
HEADER(immediate, reveal, 0, "\011IMMEDIATE");
HEADER(colon, immediate, 0, "\001:");
HEADER(semicolon, colon, IMMEDIATE, "\001;");
HEADER(brackettick, semicolon, IMMEDIATE, "\003[']");
HEADER(postpone, brackettick, IMMEDIATE, "\010POSTPONE");
HEADER(compile, postpone, 0, "\007COMPILE");
HEADER(if, compile, IMMEDIATE, "\002IF");
HEADER(then, if, IMMEDIATE, "\004THEN");
HEADER(else, then, IMMEDIATE, "\004ELSE");
HEADER(begin, else, IMMEDIATE, "\005BEGIN");
HEADER(until, begin, IMMEDIATE, "\005UNTIL");
HEADER(again, until, IMMEDIATE, "\005AGAIN");
HEADER(while, again, IMMEDIATE, "\005WHILE");
HEADER(repeat, while, IMMEDIATE, "\006REPEAT");
HEADER(tol, repeat, 0, "\002>L");
HEADER(lfrom, tol, 0, "\002L>");
HEADER(do, lfrom, IMMEDIATE, "\002DO");
HEADER(endloop, do, 0, "\007ENDLOOP");
HEADER(loop, endloop, IMMEDIATE, "\004LOOP");
HEADER(plusloop, loop, IMMEDIATE, "\005+LOOP");
HEADER(leave, plusloop, IMMEDIATE, "\005LEAVE");
HEADER(within, leave, 0, "\006WITHIN");
HEADER(move, within, 0, "\004MOVE");
HEADER(depth, move, 0, "\005DEPTH");
HEADER(environmentq, depth, 0, "\014ENVIRONMENT?");
HEADER(marker, environmentq, 0, "\006MARKER");

/* for testing */
HEADER(dothh, marker, 0, "\003.HH");
HEADER(dothhhh, dothh, 0, "\005.HHHH");
HEADER(dots, dothhhh, 0, "\002.S");
HEADER(dump, dots, 0, "\004DUMP");
HEADER(words, dump, 0, "\005WORDS");
HEADER(cold, words, 0, "\004COLD");
