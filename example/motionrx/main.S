.global enable_watchdog
.global jmp_to_bootloader

#include <avr/io.h>

enable_watchdog: ; jako tmp pouzito r24 a r25
    ldi r24, (1 << WDCE) | (1 << WDE)
    ldi r25, (1 << WDIE) | (1 << WDP3) | (1 << WDP0) ; kazdych 8s se probudime
    out _SFR_IO_ADDR(WDTCSR), r24
    out _SFR_IO_ADDR(WDTCSR), r25
    wdr
    ret

jmp_to_bootloader:
    clr r30
    out _SFR_IO_ADDR(MCUSR), r30
    ldi r31, 0x1D
    ijmp
