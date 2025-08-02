// Implementuje IR protokol podobny NEC (pulse distance modulation) pro odeslani dat do senzoru. Pouziva jine kodovani nez bootloader
// Bity 0 a 1 jsou kodovany jako kratky pulz (aktivni vysilani 38 kHz) nasledovany ruzne dlouhou pauzou
// 0 - pauza je pocatecnimu aktivnimu impulzu
// 1 - pauza je 4x pocatecnimu aktivnimu impulzu
// ukonceni komunikace - pulz je trojnasobny
// Vyhoda - slave si muze casove synchronizovat na zaklade delky pocatecniho pulzu

#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>

void jmp_to_bootloader(void);

volatile uint8_t rx_buf_index = 0, current_pulse_length = 0, rx_byte = 0, rx_bits = 0;
volatile uint8_t rx_buf[10], rx_buf_len = 0, ir_comm_active = 0;

static void got_bit(uint8_t bit)
{
    rx_byte >>= 1;
    if (bit)
        rx_byte |= 0x80;

    if (++rx_bits == 8)
    {
        rx_buf[rx_buf_index++] = rx_byte;

        if (rx_buf_index >= sizeof(rx_buf))
        {
            // buffer je plny
            rx_buf_len = rx_buf_index;
        }

        rx_byte = 0;
        rx_bits = 0;
    }
}

ISR(PCINT1_vect)
{
    if (rx_buf_len)
        return; // dokud si nevyzvedne vysledek, ignorujeme to

    uint8_t sestupna_hrana = (PINB & _BV(PB2)) == 0;
    uint8_t t = TCNT0;
    TCNT0 = 0;

    if (sestupna_hrana) // zacina synchronizacni pulz
    {
        ir_comm_active = 1;
        if (current_pulse_length)
        {
            // uz predchazel synchronizacni pulz - mame ted konec dalsiho bitu
            got_bit(t > 2 * current_pulse_length);
        }
    }
    else
    { // vzestupna hrana
        if (current_pulse_length && (t > current_pulse_length * 2))
        {
            // aktualni pulz je prilis dlouhy - master oznamuje konec vysilani
            rx_buf_len = rx_buf_index; // oznacime, ze mame data pripravena
                                       
            // pripravime se na dalsi komunikaci
            current_pulse_length = 0;
            rx_buf_index = 0;
            rx_byte = 0;
            rx_bits = 0;

            if (rx_buf_len == 4 && rx_buf[0] == 'B' && rx_buf[1] == 'O' && rx_buf[2] == 'O' && rx_buf[3] == 'T')
                jmp_to_bootloader();
        }
        else
        {
            current_pulse_length = t; // zapamatujeme si, jak byl pulz dlouhy
            // rx_buf[5] = t;
        }
    }
}

ISR(TIM0_OVF_vect)
{
    rx_buf_index = 0;
    rx_byte = 0;
    rx_bits = 0;
    ir_comm_active = 0;
    current_pulse_length = 0;
}

void ir_init() // vola se opakovane
{
    PCMSK1 |= _BV(PCINT10);
    GIMSK |= _BV(PCIE1);

    // inicializace timeru
    TCCR0A = 0; // normal rezim
    TCCR0B = 3; // /64 : 1 tick - 8 uS
    TCNT0 = 0;
    TIMSK0 |= (1 << TOIE0); // preruseni pri preteceni casovace

    // pripravime prenos
    rx_buf_index = 0;
    rx_byte = 0;
    rx_bits = 0;
    current_pulse_length = 0;
    rx_buf_len = 0;
    ir_comm_active = 0;

    sei();
}