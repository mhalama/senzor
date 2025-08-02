#include "common.S"

.global rf_send

;       __                      _ 
;      / _|                    | |
; _ __| |_   ___  ___ _ __   __| |
;| '__|  _| / __|/ _ \ '_ \ / _` |
;| |  | |   \__ \  __/ | | | (_| |
;|_|  |_|   |___/\___|_| |_|\__,_|

#define BUF       X
#define BUFL      r26
#define BUFH      r27
#define NIBBLE_L  r30
#define NIBBLE_H  r31
#define BYTES_CNT r22
#define TIMER_CNT r19

; pro volani z C - dodrzuje C ABI
; void ir_send(uint8_t *buf, uint8_t len); 
; buf ptr: r24, r25
; len: r22 - predpokladame > 0

#include <avr/io.h>

; vypocitane pomoci calctimer.c
#define TIMER_PRESCALER 2 ; 1 tick - uS
#define TIMER_WRITE_HALF_NTICKS 250 ; x2 = 500 uS drzet vysilany bit, ktery je vzorkovan kazdych 62.5 uS

#define RH_ASK_TX_DDR DDRB
#define RH_ASK_TX_PORT PORTB
#define RH_ASK_TX_BIT PB0

.macro TX_OUTPUT
    sbi _SFR_IO_ADDR(RH_ASK_TX_DDR), RH_ASK_TX_BIT
.endm

.macro TX_SET_HIGH
    sbi _SFR_IO_ADDR(RH_ASK_TX_PORT), RH_ASK_TX_BIT
.endm

.macro TX_SET_LOW
    ; ldi r18, 0
    cbi _SFR_IO_ADDR(RH_ASK_TX_PORT), RH_ASK_TX_BIT
.endm

rf_send:
    DEBUG_LED_ON
    cli

    ; parametr *buf
    mov BUFL, r24
    mov BUFH, r25
    
    ; zachovame nastaveni timeru
    in TMP, _SFR_IO_ADDR(TCNT0)
    push TMP
    in TMP, _SFR_IO_ADDR(TCCR0A)
    push TMP
    in TMP, _SFR_IO_ADDR(TCCR0B)
    push TMP

; inicializace timeru
    ldi TMP, TIMER_PRESCALER
    out _SFR_IO_ADDR(TCCR0A), ZERO      ; standardni timer - normal mode - pouze pocitani 0-255
    out _SFR_IO_ADDR(TCCR0B), TMP       ; prescaler - delicka

    RESET_TIMER

    TX_OUTPUT
    TX_SET_LOW

    ;DEBUG_LED_ON

send_preambule: ; odesle nimbbles: 0x2a, 0x2a, 0x2a, 0x2a, 0x2a, 0x2a, 0x38, 0x2c
    ldi CNT, 6
preambule_2a:
    ldi BYTE, 0x2a
    rcall send_nibble ; odesle co je v TMP
    dec CNT
    brne preambule_2a

    ldi BYTE, 0x38
    rcall send_nibble
    ldi BYTE, 0x2c
    rcall send_nibble

    ; preambule je odeslana

    clr CRC
    mov BYTE, BYTES_CNT
    inc BYTE                    ; delka zpravy je soucasti zpravy
    inc BYTE                    ; pro crc na konci
    rcall send_byte             ; odesleme delku zpravy
ir_send_loop:
    ld BYTE, BUF+               ; nacteme dalsi byte k odeslani
    rcall send_byte
    dec BYTES_CNT
    brne ir_send_loop           ; pokracujeme dalsim bytem

    ; odesleme jeste CRC
    mov BYTE, CRC
    rcall send_byte
    
    ; musime pockat na dokoceni vysilani posledniho bitu !!!!
    rcall wait_for_next_timer_tick
    TX_SET_LOW

    ; mame hotovo - obnovime registry timeru
    pop TMP
    out _SFR_IO_ADDR(TCCR0B), TMP
    pop TMP
    out _SFR_IO_ADDR(TCCR0A), TMP
    pop TMP
    out _SFR_IO_ADDR(TCNT0), TMP

    sei
    ret                         ; ir_send

; --------------- SEND NIBBLE CODED ---------------

send_nibble_coded:                   ; BYTE - prevede se na nibble, odesle a znici
    andi BYTE, 0x0F
    ldi NIBBLE_L, lo8(nibbles)
    ldi NIBBLE_H, hi8(nibbles)
    add NIBBLE_L, BYTE
    adc NIBBLE_H, ZERO               ; pricte pripadny prenos Carry
    LPM BYTE, Z                      ; nacteme nibble z tabulky podle vstupniho TMP

; --------------- SEND NIBBLE ---------------

send_nibble:                         ; odesila nibble v registru BYTE
    push    CNT                      ; CNT se pouziva, musime ho ulozit a pak obnovit
    ldi     CNT, 6                   ; zbyvajici pocet bitu
send_nibble_loop:
    ldi     TIMER_CNT, 2             ; potrebujeme 2 cykly 250 ticku = 8*62.5 uS
    rcall   wait_for_next_timer_tick ; drive byl zde inlinovan

    ; nastavime vystupni linku podle nejnizsiho bitu z BYTE
    sbrs    BYTE, 0
    TX_SET_LOW
    sbrc    BYTE, 0
    TX_SET_HIGH
    lsr     BYTE

    dec     CNT                     ; snizime pocet zbyvajicich bitu
    brne    send_nibble_loop
    pop     CNT
    ret

send_byte:
    push BYTE                   ; ulozime si ho
    rcall calc_crc
    swap BYTE                   ; prohodime horni a dolni nibble
    rcall send_nibble_coded     ; posleme dolni 4 bity (horni 4 bity jsou zahozeny v send_nible)
    pop BYTE                    ; nacteme si ho zpet
    rcall send_nibble_coded     ; posleme dolni 4 bity (horni 4 bity jsou zahozeny v send_nible)
    ret

wait_for_next_timer_tick:
    in      TMP, _SFR_IO_ADDR(TCNT0)
    cpi     TMP, TIMER_WRITE_HALF_NTICKS
    brcs    wait_for_next_timer_tick
    RESET_TIMER
    dec     TIMER_CNT
    brne    wait_for_next_timer_tick
    ret