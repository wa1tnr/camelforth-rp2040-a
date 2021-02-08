#include <stdio.h>
#include <string.h>

#define BUFFLEN 128
#define CHOP_LN 5

char instring[BUFFLEN];
char tempstring[BUFFLEN];

void init_cutter(void) {
    strcpy(instring, print_string);
}

/*
 *      Remove given section from string. Negative len means remove
 *      everything up to the end.
 */

int str_cut(char *str, int begin, int len) {
    int l = strlen(str);

    if (len < 0)
        len = l - begin;
    if (begin + len > l)
        len = l - begin;
    memmove(str + begin, str + begin + len, l - len + 1);

    return len;
}

void slower(void) {
    for (volatile int i = 1295; i > 0; i--) {
    }
}

// printf_cutter() needs rework or discard - is why it says 'cccc' for a dump or a .S 08 Feb 2021
void printf_cutter(void) {
    // cdcdf_acm_write((uint8_t *) tempstring, strlen(tempstring));
    putchar('c'); // cdcdf_acm_write((uint8_t *) tempstring, strlen(tempstring));
    slower();                   // no ringbuffer - kludge
}

void do_output(void) {
    printf_cutter();
}

void slicer(char *instring) {
    tempstring[0] = '\0';
    tempstring[1] = '\0';

    int origin = 0;
    int chopln = CHOP_LN;       // chop length
    int to_end = -1;

    int l = (strlen(instring) / chopln);

    for (int index = 1; index < (l + 1); index++) {
        int j = chopln * index;
        int k = j - chopln;
        strcpy(tempstring, instring);
        str_cut(tempstring, j, to_end);
        if (index > 1) {
            str_cut(tempstring, origin, k);
        }
        do_output();
    }

    if ((strlen(instring)) > ((strlen(instring) / chopln) * chopln)) {
        strcpy(tempstring, instring);
        int c = l * chopln;
        str_cut(tempstring, 0, c);
        do_output();
    }

}

void cut_main(void) {
    init_cutter();
    slicer(instring);
}

// primary api is chopped_acm_write(print_string) from caller

void chopped_acm_write(char *str) {
    cut_main();
}

#ifdef NEVER_DEFINED_EVER
/*

Stack Overflow

permalink:

 [ https://stackoverflow.com/a/20346241 ]

other link:

 [ https://stackoverflow.com/questions/20342559/how-to-cut-part-of-a-string-in-c ]

The following function cuts a given range out of a char buffer.
The range is identified by startng index and length. A negative
length may be specified to indicate the range from the starting
index to the end of the string.
*/

/*
 *      Remove given section from string. Negative len means
 *      remove everything up to the end.
 */

int str_cut(char *str, int begin, int len) {
    int l = strlen(str);

    if (len < 0)
        len = l - begin;
    if (begin + len > l)
        len = l - begin;
    memmove(str + begin, str + begin + len, l - len + 1);

    return len;
}

/*

The char range is cut out by moving everything after the
range including the terminating '\0' to the starting index
with memmove, thereby overwriting the range. The text in
the range is lost.

Note that you need to pass a char buffer whose contents
can be changed. Don't pass string literals that are stored
in read-only memory.

answered Dec 3 '13 at 8:33
M Oehm

site design / logo © 2018 Stack Exchange Inc;
user contributions
licensed under cc by-sa 3.0 with attribution required.
rev 2018.9.14.31567
*/
#endif // #ifdef NEVER_DEFINED_EVER
