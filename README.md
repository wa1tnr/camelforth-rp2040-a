## Tue Feb  9 04:09:26 UTC 2021

CamelForth in C, by Dr. Brad Rodriguez

Reasonably well debugged - functional Forth interpreter 04:10 UTC Tue 09 Feb 2021

Port: rp2040, Raspberry Pi Pico target board, February, 2021.

# PARENT was rpi-pico-basics   0.1.0-pre-alpha   Mon Feb  8 03:48:42 UTC 2021

# About

First attempt to make a meaningful interpeter on the
RP2040 and Raspberry Pi Pico, locally.

A primitive USB interpreter already exists.

# Interesting Files

This is a very rough interpreter that broadcasts
also on the UART, and only accepts keystrokes on
the USB.

The /dev/ttyACM0 is the primary interface (USB).

Using CP2104 USB-to-USART bridge, the secondary
interface (in Linux host PC) is /dev/ttyUSB0, but
that is arbitrary (you can bridge using other
methods and chips).

This is the interpreter file and its location:

rpi-pico-basics/07-interpreter.d/pico-examples/interpreter-a/interpreter-a.c

Found on this github in another repository, at

 https://github.com/wa1tnr/rpi-pico-basics.git

# Topics

# camelforth
# rp2040
# raspberry-pi-pico

# rpi-pico-pio

# programmable-i-o

# forth assembly-language-programming
# basics
# c cpp


## old branches:

BUILD ENVIRONMENT - bare bones text interpreter

no Forth functionality at all - not even base code.

Just keyboard echo loop.

END.
