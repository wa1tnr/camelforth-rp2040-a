// dump.inc

#undef AINSU_DUMP_EXTERN
#define AINSU_DUMP_EXTERN

/* Charley Shatttuck's rdumps() */

/* Tiny interpreter,
   similar to myforth's Standalone Interpreter
   This example code is in the public domain */

/* dump 16 bytes of RAM in hex with ascii on the side */

void d_three_sp(void) {         // print three spaces
    for (int i = 3; i > 0; i--) {
        putch('\040');
    }
}

void d_two_sp(void) {
    putch('\040');
    putch('\040');
}

void dumpRAM() {
    char *ram;
    int p = psp[0];
    psp++;
    ram = (char *) p;
    printf("\n%4X: ", p);

    int count = -1;
    for (int i = 0; i < 16; i++) {
        count++;
        if (i == 8)
            putch('\040');
        if (count > 3) {
            count = (count & 0x03);
            putch('\040');
        }
        char c = *ram++;
        printf(" %02X", (c & 0xff));
    }

    ram = (char *) p;
    d_three_sp();

    for (int i = 0; i < 16; i++) {
        print_string[0] = (uint32_t) * ram++;
        if (print_string[0] > 0x7e || print_string[0] < ' ')
            print_string[0] = (uint32_t) '.';
        print_string[1] = '\0';
        chopped_acm_write(print_string);
    }
    psp--;
    psp[0] = p + 16;
}

/* dump 256 bytes of RAM */
CODE(dump) {                    /* adr n -- */
    int j = psp[0];
    psp++;
    for (int i = 0; i < j; i++) {
        dumpRAM();
    }
}
