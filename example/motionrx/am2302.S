#include <avr/io.h>
#include "config.h"

#ifdef AM2302_BIT
.global am2302_read

#define BITS_COUNT r18
#define TMP1 r19
#define TMP2 r20

am2302_read:
    set                                             ; nastavime T bit (cekame na high)
    rcall wait_for_bus

    ; start komunikace - stahneme pin na low na 18 ms
    cbi _SFR_IO_ADDR(AM2302_PORT), AM2302_BIT       ; vystup nastavime na low
    sbi _SFR_IO_ADDR(AM2302_DDR), AM2302_BIT        ; nastavime komunikacni pin jako vystup

    rcall delay_18ms

    ; uvolnime linku do high
    cbi _SFR_IO_ADDR(AM2302_DDR), AM2302_BIT        ; linku nastavime na vstup
    sbi _SFR_IO_ADDR(AM2302_PORT), AM2302_BIT       ; pull-up aktivace

    ldi r22, 0xFF                                   ; error pro timeout - no sensor
    clt
    rcall wait_for_bus                              ; pockej na low

    set
    rcall wait_for_bus                              ; pockej na high

    ldi BITS_COUNT, 40                 ; budeme cist 5 bytu
read_next_bit:
    ; pockame na stazeni do low (uz tam i muzeme byt)
    clt
    rcall wait_for_bus

    ; pripravime si misto pro dalsi bit - posuneme 32+8 bitu vlevo
    lsl r26
    rol r25
    rol r24
    rol r23
    rol r22    

    ; pockame na uvolneni do high
    set
    rcall wait_for_bus

    ; je prave vysilan dalsi bit, delka high: [bit0: 26-28 us], [bit1: 70 us]

    ; pockame 50 us
    ldi  TMP1, 133
delay_50us:
    dec  TMP1
    brne delay_50us
    
    sbic _SFR_IO_ADDR(AM2302_PIN), AM2302_BIT
    ori r26, 1                           ; byl to bit 1
    
    dec BITS_COUNT
    brne read_next_bit

    ; kontrola checksum - pri souctu ignorujeme prenos - potrebujeme jen 8 bitu vysledku
    clr  TMP1
    add  TMP1, r22
    add  TMP1, r23
    add  TMP1, r24
    add  TMP1, r25

    cp   TMP1, r26                       ; r26 obsahuje checksum
    breq read_finished

checksum_error:
    ldi r22, 0xFE
    rjmp read_failed

error_timeout:
    rjmp read_failed

read_failed:
    clr r23
    clr r24
    clr r25

read_finished:
    ret

wait_for_bus:
    ldi TMP1, 200 ; timeout 200 us

wait_for_bus_loop:
    brts wait_high

wait_low:
    sbis _SFR_IO_ADDR(AM2302_PIN), AM2302_BIT
    ret
    rjmp wait_for_bus_loop_cont

wait_high:
    sbic _SFR_IO_ADDR(AM2302_PIN), AM2302_BIT
    ret

wait_for_bus_loop_cont:
    rcall delay_1us
    dec TMP1
    brne wait_for_bus_loop
    ldi r22, 0xFD   ; nastavime chybovy kod
    pop TMP1        ; odstranime puvodni navratovou adresu 
    pop TMP1        ; ze zasobniku (uz se nebudeme vracet)
    
    rjmp error_timeout

delay_1us:
    nop
    ret

delay_18ms:
    ldi  TMP1, 11
    ldi  TMP2, 96
L1: dec  TMP2
    brne L1
    dec  TMP1
    brne L1
    ret
#endif
