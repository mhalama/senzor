.global main

#include <avr/io.h>

main:
    rjmp start

start:
    cli

    clr r24
    out _SFR_IO_ADDR(MCUSR), r24

    sbi _SFR_IO_ADDR(DDRB), 1
    ldi r24, 30

loop:
    sbi _SFR_IO_ADDR(PORTB), 1
    rcall delay
    cbi _SFR_IO_ADDR(PORTB), 1
    rcall delay
    dec r24
    brne loop

    ldi  r30, 0x00
    ldi  r31, 0x1D
    ijmp                ; skocime do bootloaderu

delay:
    ldi  r18, 6
    ldi  r19, 43
    ldi  r20, 0
L1: dec  r20
    brne L1
    dec  r19
    brne L1
    dec  r18
    brne L1
    lpm
    ret
