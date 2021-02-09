#include "rp2040_pico.h"

unsigned int getKey(void) {     // hardware-independent wrapper
    uint8_t ch_read = (uint32_t) 'c';
    ch_read = getchar();
    // uncomment for local echo, maybe:
    // putchar(ch_read);
    return ch_read;
}
void queryKey(void) {
    // TODO wa1tnr
}
