/****h* camelforth-a/src/cf/rp2040_pico/rp2040_pico.inc
 *
 * from:
 ****h* camelforth/tivaio.c
 *
 * NAME
 *  rp2040_pico.inc
 * DESCRIPTION
 *  Terminal I/O, and main entry point for
 *  CamelForth in C, for the RP2040
 *  Raspberry Pi Pico target board.
 * SYNOPSIS
 *  OUT OF DATE:
 *  Provides the functions
 *      void putch(char c)      write one character to terminal
 *      char getch(void)        await/read one character from keyboard
 *      int getquery(void)      return true if keyboard char available
 *      void initTermios(void)  NOT IMPLEMENTED - configure terminal for Forth
 *      void resetTermios(void) NOT IMPLEMENTED - reset terminal configuration, if req'd
 *      void camelforth(void)   probable main entry point for RP2040.   UPSTREAM: int main(void)
 *
 * AUTHORS
 *  Brad Rodriguez
 *  wa1tnr September, 2018; February 2021
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

#include "rp2040_pico.h"
#include "rp2040_pico_getkey_usb.inc"
#include "cut.inc"              // string cutter utility, for long USB printf's

extern unsigned int getKey(void);       // hardware-independent wrapper

/*
#include <stdint.h>
#include <stdbool.h>
*/

// #define NULL 0  // already defined for Atmel Start and gcc:

/*
 * Terminal I/O functions
 */

int length_print_string = 0;    // would like to shed this global

/* USB based */
#ifdef NOT_DEFINED_RP2040
#define printf(format,integer) \
    length_print_string = 0; \
    print_string[0] = (uint32_t) "\0"; \
    sprintf(print_string, format, integer); \
    length_print_string = strlen(print_string); \
        chopped_acm_write(print_string)
#endif // not defined rp2040

#define getch getKey // keep rp2040

/*
void putch_nomore(char c) {
    char *p;
    p = &c;
    c = *p + 0;
    print_string[0] = c;
    print_string[1] = '\0';
    chopped_acm_write(print_string);
}
*/

void putch(char c) {
    // putchar('I');
    putchar(c);
    // putchar('J');
}

int getquery(void) {
    return -1;                  /* TODO wa1tnr */
}

/****f* main/main
 * NAME
 *  main
 * SYNOPSIS
 *  int main(void)
 * DESCRIPTION
 *  Main program, entered after an MCU reset.
 ******
 */

void interpreter(void);         /* forward reference */

void camelforth(void) {
    for (volatile int i = 9080706; i > 0; i--) {
    }
    strcpy(print_string, "\n\ncamelforth()\n\n");
    putchar(' ');
    putchar('w'); // uart_puts(UART_ID, print_string);
    putchar('h');
    putchar('o');
    putchar('h');
    putchar(' ');
    // chopped_acm_write(print_string);
    while (1) {                 /* loop in case interpreter() ever terminates */
        interpreter();
    }
}
