# camelforth-rp2040-a   0.1.1-pre-alpha   Tue Feb  9 04:13:56 UTC 2021

CamelForth in C, by Dr. Brad Rodriguez

# SUPERSEDED by:

https://github.com/wa1tnr/camelforth-rp2040-b-MS-U

October, 2021:

The present project is archived for reference only. Please see the above archive for current work.

# OLD:

# - - - everything below this line is old news - October 2021, forward. - - -

Reasonably well debugged - functional Forth interpreter 04:10 UTC Tue 09 Feb 2021

Port: rp2040, Raspberry Pi Pico target board, February, 2021.

# PARENT was rpi-pico-basics   0.1.0-pre-alpha   Mon Feb  8 03:48:42 UTC 2021

# About

CamelForth

A Forth in C by Dr Brad Rodriguez

Forth interpreter for the
RP2040 and Raspberry Pi Pico.

Requires pico-sdk and is modeled on pico-examples.

The /dev/ttyACM0 is the primary interface (USB).

(Reference host PC is Linux Debian amd64)

Using CP2104 USB-to-USART bridge, the secondary
interface (in Linux host PC) is /dev/ttyUSB0, but
that is arbitrary (you can bridge using other
methods and chips).

Connected to UART0 on the pico.

# Topics

# camelforth
# rp2040
# raspberry-pi-pico

# rpi-pico-pio

# forth
# c


## old branches:

BUILD ENVIRONMENT - bare bones text interpreter

no Forth functionality at all - not even base code.

Just keyboard echo loop.

END.
