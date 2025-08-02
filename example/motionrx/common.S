#include <avr/io.h>

; pouziva se v sender.S a receiver.S

#define SCRATCH   r0
#define ZERO      r1
#define BYTE      r25
#define CNT       r18
#define CRC       r23
#define TMP       r24

.macro RESET_TIMER
    out _SFR_IO_ADDR(TCNT0), ZERO
.endm

.macro DEBUG_LED_ON
    sbi _SFR_IO_ADDR(PORTB), 0 ; DEBUG
.endm

calc_crc: ; (CRC, BYTE: data) -> CRC; Zrusi: TMP, SCRATCH, CNT
    push    TMP
    push    CNT
    eor     CRC, BYTE
    ldi     CNT, 8
    ldi     TMP, 0x8C
calc_crc_loop:
    mov     SCRATCH, CRC
    lsr     CRC
    sbrc    SCRATCH, 0
    eor     CRC, TMP
    dec     CNT
    brne    calc_crc_loop
    pop     CNT
    pop     TMP
    ret

nibbles:
    .byte 0x0d, 0x0e, 0x13, 0x15, 0x16, 0x19, 0x1a, 0x1c
    .byte 0x23, 0x25, 0x26, 0x29, 0x2a, 0x2c, 0x32, 0x34
    .byte 0xFF, 0x00
