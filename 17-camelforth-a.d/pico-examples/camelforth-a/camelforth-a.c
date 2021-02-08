// Mon Feb  8 18:39:16 UTC 2021
// wa1tnr
// camelforth

/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include "pico/stdlib.h"

/// \tag::hello_uart[]

#define UART_ID uart0
#define BAUD_RATE 115200

#define UART_TX_PIN 0
#define UART_RX_PIN 1

char ch;
void _USB_read_tnr(void) {
    ch = getchar();

    // backspace primitives
    // also Ctrl J and Ctrl K are useful.
    if (ch == '\010') {
        uart_putc(UART_ID, '\010'); putchar('\010');
        return ;
    }

    uart_putc(UART_ID, ch); putchar(ch);

    // printf("%c", ch);
    // uart_putc(UART_ID, ch);
    // putchar() on /dev/ttyACM0 - USB
    // putchar('X');
}

void tryme() {
    ch = 'Q';
    _USB_read_tnr();
}

void looper() {

    tryme();
}

int main() {
    sleep_ms(9500);
    uart_init(UART_ID, BAUD_RATE);

    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    stdio_init_all();

    uart_putc_raw(UART_ID, 'A');

    uart_putc(UART_ID, 'B');
    uart_putc(UART_ID, 'B');
    uart_putc(UART_ID, 'B');
    uart_putc(UART_ID, 'B');
    uart_putc(UART_ID, 'B');
    uart_putc(UART_ID, 'B');
    uart_putc(UART_ID, 'B');
    uart_putc(UART_ID, 'B');
    uart_putc(UART_ID, 'B');
    uart_putc(UART_ID, 'B');
    uart_putc(UART_ID, 'B');
    sleep_ms(19500);
    uart_puts(UART_ID, " Hello, UART!\r\n");
    uart_puts(UART_ID, " project codenamed camelForth-rp2 v0.0.0-b\r\n\r\n\r\n");
    uart_puts(UART_ID, " 8 Feb BUILD env test nice keyboard mirroring UART and USB\r\n");

    while(1) {
        looper(); // called once and ran once ask asked ;)
    }
}

/// \end::hello_uart[]
