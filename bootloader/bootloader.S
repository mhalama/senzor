.global main

; Vysilani a prijem dat technikou z RH_ASK - RadioHead library
; v assembleru pro ATTINY84. Cilem je mit kompaktni bootloader.
; Implementace je tedy v assembleru a neni vyuzito preruseni z timeru,
; aby nebyl problem s tabulkou preruseni.
; Timer se presto vyuziva pro casovani, ale ceka se aktivne smyckou.
;
; Programovani (tento kod) by mohl byt spusten z preruseni, pri prechodu hrany na low na
; vstupu IR receiveru. Tady zjistime, jestli jde skutecne o nejake vysilani pro nas
; nosna 01010101 a specificky START word. Pokud do timeout START word nedorazi,
; ukoncime preruseni a pokracuje se puvodnim programem.
; Pokud zacneme prepisovat, predpokladame po dokonceni RESET. V pripade selhani potom nekonecny 
; loop v receiver casti - bez navratu z preruseni.
;
; Format zpravy: START symbol, ZH, ZL (stranka), po 1 klesajici index k 0, data, CRC
#include <avr/io.h>

; vyuziti registru
; r0 - r15 rezervovano pro kodovani packetu pomoci sifry Speck

; Start symbol - FU
#define START_SYMBOL 0x4655

#define SPM_PAGE_LEN 64

#define SPM_WORD_LOW r0
#define SPM_WORD_HIGH r1
#define ZERO r2
#define RX_INTEGRATOR r18
#define RX_PLL_RAMP r19
#define RX_BITS_LOW r20
#define RX_BITS_HIGH r21
#define RX_LAST_PIN_STATE r15
#define RX_BITS_COUNT_REMAINS r16
#define RX_PAGES_REMAIN r22
#define RX_BUF_LEN r17
#define CRC r23
#define TMP1 r24
#define TMP2 r25
; z-pointer se pouziva k programovani (SPM)
#define ZL r30
#define ZH r31

; r24 - parametr do volani funkci
; r25 - pomocny - scratch

; vypocitane pomoci calctimer.c
#define TIMER_PRESCALER 2 ; 1 tick - uS
#define TIMER_READ_NTICKS 62   ; kazdych 1000 000 / 2000 / 8 = 62.5 uS potrebujeme cist bit
#define TIMER_WRITE_HALF_NTICKS 250 ; x2 = 500 uS drzet vysilany bit, ktery je vzorkovan kazdych 62.5 uS

#define RH_ASK_RX_DDR DDRB
#define RH_ASK_RX_PIN PINB
#define RH_ASK_RX_BIT PB2

#define RH_MIN_BUFFER_LEN 2
#define RH_ASK_MAX_PAYLOAD_LEN 150

.macro RESET_TIMER
    out _SFR_IO_ADDR(TCNT0), ZERO
.endm

.macro DEBUG_LED_ON
    sbi _SFR_IO_ADDR(PORTB), PB1 ; DEBUG LED
.endm

.macro DEBUG_LED_OFF
    cbi _SFR_IO_ADDR(PORTB), PB1 ; DEBUG LED
.endm

.macro DISABLE_WATCHDOG
    wdr
    in TMP1, _SFR_IO_ADDR(WDTCSR)
    ori TMP1, (1<<WDCE)|(1<<WDE)
    out _SFR_IO_ADDR(WDTCSR), TMP1
    out _SFR_IO_ADDR(WDTCSR), ZERO
.endm

main:
    rjmp start

; --------------- MAIN ---------------

.org 0x1d00

start:
    wdr

    clr ZERO
    
    out _SFR_IO_ADDR(MCUSR), ZERO

    DISABLE_WATCHDOG

    ldi r16, hi8(RAMEND)
    out _SFR_IO_ADDR(SPH), r16
    ldi r16, lo8(RAMEND)
    out _SFR_IO_ADDR(SPL), r16

    cli

    sbi _SFR_IO_ADDR(DDRB), 1           ; debug LED dioda

    ldi TMP1, 5
start_blinker:
    DEBUG_LED_ON
    rcall failed_delay              ; pouziva a neobnovuje r18, r19, r20
    rcall failed_delay              ; pouziva a neobnovuje r18, r19, r20
    rcall failed_delay              ; pouziva a neobnovuje r18, r19, r20
    rcall failed_delay              ; pouziva a neobnovuje r18, r19, r20
    rcall failed_delay              ; pouziva a neobnovuje r18, r19, r20
    DEBUG_LED_OFF
    rcall failed_delay              ; pouziva a neobnovuje r18, r19, r20
    rcall failed_delay              ; pouziva a neobnovuje r18, r19, r20
    rcall failed_delay              ; pouziva a neobnovuje r18, r19, r20
    rcall failed_delay              ; pouziva a neobnovuje r18, r19, r20
    rcall failed_delay              ; pouziva a neobnovuje r18, r19, r20
    rcall failed_delay              ; pouziva a neobnovuje r18, r19, r20
    DEBUG_LED_ON
    rcall failed_delay              ; pouziva a neobnovuje r18, r19, r20
    DEBUG_LED_OFF
    rcall failed_delay              ; pouziva a neobnovuje r18, r19, r20
    DEBUG_LED_ON
    rcall failed_delay              ; pouziva a neobnovuje r18, r19, r20
    DEBUG_LED_OFF
    rcall failed_delay              ; pouziva a neobnovuje r18, r19, r20
    dec TMP1
    brne start_blinker

    DEBUG_LED_ON                        ; debug LED dioda

; inicializace timeru
    ldi r24, TIMER_PRESCALER
    out _SFR_IO_ADDR(TCCR0A), ZERO      ; standardni timer - normal mode - pouze pocitani 0-255
    out _SFR_IO_ADDR(TIMSK0), ZERO      ; standardni timer - normal mode - pouze pocitani 0-255
    out _SFR_IO_ADDR(OCR0A),  ZERO      ; standardni timer - normal mode - pouze pocitani 0-255
    out _SFR_IO_ADDR(TCCR0B), r24       ; prescaler - delicka
    RESET_TIMER

    clr RX_PAGES_REMAIN                 ; stranek k zapsani - zmeni se prvni strankou
main_loop:
    rcall receive_data                  ; nacteme dalsi data stranky
    rjmp main_loop

; ----------------------------- CTENI bez preruseni -----------------------------
; cteme stav bitu kazdych cca 62.5 uS - aktivne cekame az prijde cas, protoze
; kod bude v bootloaderu a pouziti preruseni je tedy problematicke (bude ho chtit uzivatel).
; posledni stav linky uchovavame v T flagu
; v r24 po skonceni vracime pocet nactenych bytu v [rx_buffer]

#define RH_ASK_RAMP_TRANSITION 80
#define RH_ASK_RX_RAMP_LEN 160
#define RH_ASK_RAMP_INC 20
#define RH_ASK_RAMP_INC_RETARD 11
#define RH_ASK_RAMP_INC_ADVANCE 9

; r30:r31 Z: pouziva se pro tabulku nibbles
; r26:r27 X: pouziva se pro rx_buffer

; #define CURRENT_BYTE_READ r25

; ------------------------------------------------
; RECEIVE DATA
; r24 pouzivame jako temp pro ulozeni aktualniho stavu pinu
; r25 pomocny pouzivame pro rekonstrukci bytu ze dvou 4 bit nibblu
; do X: r26:r27 ukladame nactena data a vratime jejich pocet v r24
receive_data:
    clr RX_INTEGRATOR
    clt ; pouzivame T bit jako signalizaci, ze jsme ve fazi aktivniho cteni

wait_for_next_read_timer_tick:
    in r24, _SFR_IO_ADDR(TCNT0)
    cpi r24, TIMER_READ_NTICKS
    brcs wait_for_next_read_timer_tick

    RESET_TIMER

    in r24, _SFR_IO_ADDR(RH_ASK_RX_PIN) ; aktualni stav PINu
    sbrc r24, RH_ASK_RX_BIT
    inc RX_INTEGRATOR

if_edge_change:
        eor RX_LAST_PIN_STATE, r24
        sbrs RX_LAST_PIN_STATE, RH_ASK_RX_BIT
        rjmp always_inc_pll
    edge_change_detected: ; (rxSample != _rxLastSample)
        cpi RX_PLL_RAMP, RH_ASK_RAMP_TRANSITION
        brlo pll_retard
    pll_advance:
        subi RX_PLL_RAMP, -RH_ASK_RAMP_INC_ADVANCE ; pozor --11 -> +11
        rjmp always_inc_pll
    pll_retard:
        subi RX_PLL_RAMP, RH_ASK_RAMP_INC_RETARD ; -9

    always_inc_pll: ; nastane vzdy a pridame +20
        subi RX_PLL_RAMP, -RH_ASK_RAMP_INC ; pozor --x -> +x
edge_endif:

    mov RX_LAST_PIN_STATE, r24 ; save current sample state

if_ramp:
    cpi RX_PLL_RAMP, RH_ASK_RX_RAMP_LEN
if_ramp_not_finished:    
    brlo wait_for_next_read_timer_tick
if_ramp_finished: ; (_rxPllRamp >= RH_ASK_RX_RAMP_LEN)
    ; rxBits >>= 1 - v puvodni RH_ASK
    ; my mame obracene nez v RH_ASK rxBits <<= 1
    lsl RX_BITS_LOW
    rol RX_BITS_HIGH

    cpi RX_INTEGRATOR, 5 ; if (_rxRX_INTEGRATOR >= 5)
    brlo not_adding_one
    ori RX_BITS_LOW, 0x01 ; nastavime 0. bit z 12ti - MSB - opacne nez v RH_ASK
not_adding_one:
    subi RX_PLL_RAMP, RH_ASK_RX_RAMP_LEN ; _rxPllRamp -= RH_ASK_RX_RAMP_LEN
    clr RX_INTEGRATOR ; pripravim RX_INTEGRATOR na dalsi bitove hlasovani

if_rx_active:
    brts rx_is_active ; tady nemuzeme skocit az na rx_is_not_active, protoze je moc daleko
    rjmp rx_is_not_active
    rx_is_active:
        dec RX_BITS_COUNT_REMAINS
        brne wait_for_next_read_timer_tick
    all_12_bits_read: ; _rxBitCount == 12
    ; uint8_t this_byte = (symbol_6to4(_rxBits & 0x3f)) << 4 | symbol_6to4(_rxBits >> 6);

    andi RX_BITS_HIGH, 0xF ; vycistime srot z minula a ponechame jen 4 dolni bity

    mov r24, RX_BITS_LOW
    andi r24, 0x3F ; nechame jen spodnich 6 bitu

    rcall code_to_nibble
    mov r25, r24

    ; posuneme horni dva bity RX_BITS_LOW do dolnich 2 bitu RX_BITS_HIGH
    lsl RX_BITS_LOW
    rol RX_BITS_HIGH
    lsl RX_BITS_LOW
    rol RX_BITS_HIGH

    mov r24, RX_BITS_HIGH
    rcall code_to_nibble
    swap r24
    or r25, r24 ; v r25 mame nyni kompletni rekonstruovany byte
    
    mov r24, r25
    rcall calc_crc ; r24 je zase k dispozici

    cpi RX_BUF_LEN, 0xFF ; - zatim jsme nic neprecetli - 1. byte hlavicky
    brne after_header_1
    mov ZH, r25
    rjmp continue_next_byte ; dekrementuje i RX_BUF_LEN

after_header_1:
    cpi RX_BUF_LEN, 0xFE ; je to druhy byte hlavicky?
    brne after_header_Z
    mov ZL, r25
    rjmp continue_next_byte ; dekrementuje i RX_BUF_LEN

after_header_Z:
    cpi RX_BUF_LEN, 0xFD ; je to treti byte hlavicky - poradi zpravy odzadu
    brne after_header_done

    cp RX_PAGES_REMAIN, r25 
    breq not_yet_finished_programming ; mame ocekavany index zpravy, pokracujeme

    ; bud jsme jeden cely paket vynechali nebo je prvni
    tst RX_PAGES_REMAIN ; je to prvni?
    breq first_packet_set_message_id
    ; FAILED - vynechali jsme celou zpravu 
    ; TODO: smazat prvni stranku a znovu do bootloader
    ; zatim jen stuck
    rjmp failed

first_packet_set_message_id:
    mov RX_PAGES_REMAIN, r25
    rjmp not_yet_finished_programming

not_yet_finished_programming:    
    ldi RX_BUF_LEN, 66 ; 64 bytu stranka + 1 CRC + 1 protoze se za chvili odecte
    rjmp continue_next_byte

after_header_done:                            ; mame nactenou kompletni hlavicku - cteme uz data stranky
    sbrc RX_BUF_LEN, 0                        ; je to prvni byte wordu? (zaciname 65)
    rjmp not_word_aligned_have_first_byte
    mov r1, r25                               ; have already complete word - write it
    ldi r25, 1                                ; cmd save
    out _SFR_IO_ADDR(SPMCSR), r25
    spm
    rjmp prepare_read_next_data_byte
not_word_aligned_have_first_byte:
    mov r0, r25
prepare_read_next_data_byte:
    adiw ZL, 1                                ; posuneme Z na dalsi byte ve strance
    ; zatim neni vse nacteno, pokracujeme dalsimi 12 bity
continue_next_byte:
    dec RX_BUF_LEN
    breq buffer_is_completely_read
    ldi RX_BITS_COUNT_REMAINS, 12
    rjmp wait_for_next_read_timer_tick
buffer_is_completely_read:
    tst CRC ; po prijeti posledniho bytu musi byt CRC = 0
    breq message_is_correct

failed:    
    ; Neco se behem prenostu zvrtlo. Program je spatne. Cely program smazazeme a nechame jen bootloader, at to zkusi znovu.
    ; Na stranku 0
    clr zl  
    clr zh
    clr r1
    cli                             ; uz by melo byt - zbytecne

clean_failed_program:
    DEBUG_LED_ON
    
    rcall failed_delay

    ldi r24, 3                      ; smaz stranku zl:zh
    OUT _SFR_IO_ADDR(SPMCSR), r24
    SPM
    rcall wait_for_flash            ; pouziva a neobnovuje r24
    
    DEBUG_LED_OFF

    rcall wait_for_flash

    ldi r24, SPM_PAGE_LEN
    add zl, r24
    adc zh, r1                      ; zl:zh - dalsi stranka
    cpi zh, 0x1D                    ; tady uz sedi bootloader, tam nesmime zapisovat
    brlo clean_failed_program

    ; mame smazano, nasadime watchdog a pockame si na reset
    ldi r24, (1 << WDCE) | (1 << WDE)
    ldi r25, (1 << WDE) | (1 << WDP2) | (1 << WDP1) | (1 << WDP0) ; a za 2s reset
    out _SFR_IO_ADDR(WDTCSR), r24
    out _SFR_IO_ADDR(WDTCSR), r25
    wdr

    rjmp .          ; nekonecna smycka

failed_delay:
    ldi  r18, 3
    ldi  r19, 8
    ldi  r20, 120
failed_delay_L1:
    dec  r20
    brne failed_delay_L1
    dec  r19
    brne failed_delay_L1
    dec  r18
    brne failed_delay_L1
    ret

message_is_correct: ; zapiseme pripravenou stranku do flash
    clt ; _rxActive = false
    ; musime na zacatek stranky - TODO - udelat lepe - ted natvrdo 64 bytu
    sbiw ZL, 63
    sbiw ZL, 1

    ldi r24, 3 ; smaz stranku
    OUT _SFR_IO_ADDR(SPMCSR), r24
    SPM
    rcall wait_for_flash

    ; uloz stranku v bufferu
    ldi r24, 5
    OUT _SFR_IO_ADDR(SPMCSR), r24
    SPM
    rcall wait_for_flash

    dec RX_PAGES_REMAIN
    breq bootloader_finished_properly
    ret ; jeste receive_data - pokracujeme dalsi zpravou

wait_for_flash:
    in r24, _SFR_IO_ADDR(SPMCSR)
    sbrc r24, 0 ; je SPMEN porad 1?
    rjmp wait_for_flash
    ret

bootloader_finished_properly:
    ; A konec damy a panove, loucime se a odchazime do nacteneho programu
    ldi  r30, 0
    ldi  r31, 0
    ijmp ; sbohem, zase nekdy

rx_is_not_active:
    ; dokud jsme jen tady, muzeme se jeste vratit do puvodniho kodu
    ; mame start symbol? - kazdy senzor by mel mit jiny start symbol
    cpi RX_BITS_LOW, lo8(START_SYMBOL) ; 'U'lash
    brne endif_ramp ; zatim nemame - wait_for_next_read_timer_tick je daleko
    
    cpi RX_BITS_HIGH, hi8(START_SYMBOL) ; 'F'pdate
    brne endif_ramp ; zatim nemame - wait_for_next_read_timer_tick je daleko

    ; hura aktivujeme cteni, ale uz neni cesty zpet   
    set_active_state:
        set ; nastavime T bit -> _rxActive = true
        clr CRC ; pripravime si CRC pro zpravu
        ldi RX_BITS_COUNT_REMAINS, 12
        ldi RX_BUF_LEN, 0xFF ; nastavime delku zpravy na maximum - cteme hlavicku

endif_ramp:
    rjmp wait_for_next_read_timer_tick

code_to_nibble:
; v r24 je pripraven 6 bitovy kod a prevadime ho do r24 na 4 bitovy
; v r23 a r25 se pouzivaji jako temp, ale ulozime je
; pokud hledani selze, bude v r24 hodnota 0x10
    push r23 ; tohle by slo nejak optimalizovat vhodnou volbou registru
    push r25 ; tohle by slo nejak optimalizovat vhodnou volbou registru
    push ZL
    push ZH
    ldi ZL, lo8(nibbles)
    ldi ZH, hi8(nibbles)
    clr r23

code_to_nibble_search:
    lpm r25, z+
    cp r25, r24
    breq nibble_found
    inc r23
    brlo code_to_nibble_search ; hledame dal?
    ldi r23, 0x10
nibble_found:
    mov r24, r23
    pop ZH
    pop ZL
    pop r25 ; tohle by slo nejak optimalizovat vhodnou volbou registru
    pop r23 ; tohle by slo nejak optimalizovat vhodnou volbou registru
    ret

; Pocita postupne CRC z aktualniho bytu v r24
; CRC se uchovava v r23
calc_crc: ; (CRC, r24: data) -> CRC
    push    r18
    push    r19
    push    r25
    eor     CRC, r24
    ldi     r25, 0x08 ; counter
    ldi     r18, 0x8C 
calc_crc_loop:
    mov     r19, CRC
    andi    r19, 0x01       
    lsr     CRC
    sbrc    r19, 0 ; POZOR cpse r19, r1
    eor     CRC, r18
    subi    r25, 0x01  
    brne    calc_crc_loop
    pop     r25
    pop     r19
    pop     r18
    ret

nibbles:
    .byte 0xd, 0xe, 0x13, 0x15, 0x16, 0x19, 0x1a, 0x1c
    .byte 0x23, 0x25, 0x26, 0x29, 0x2a, 0x2c, 0x32, 0x34, 0xFF ; 0xFF ukonceni
