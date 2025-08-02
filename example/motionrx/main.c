#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <util/delay.h>
#include "config.h"
#include "rx.h"

#define FW_VERSION 0xBB

#define TICKS_MESSAGE_WAIT 50

#define FLAG_BOOT (1 << 0)
#define FLAG_PIR (1 << 1)
#define FLAG_RCWL (1 << 2)
#define FLAG_WDT (1 << 3)

static volatile uint8_t sensor_id;

static volatile uint32_t wdt_tick = 0;

static volatile uint8_t flags = 0;

typedef struct __attribute__((packed)) message
{
    uint8_t sensor_id;
    uint16_t msg_id;
    uint32_t tick;
    uint16_t vcc;
    uint8_t flags;
    uint8_t version;
    uint32_t humitemp;
    uint8_t dummy;
} message_t;

void jmp_to_bootloader(void);
void enable_watchdog(void);
void rf_send(uint8_t *buf, uint8_t len);
void speck_encrypt(uint8_t *buf);

void debug()
{
    for (uint8_t x = 0; x < 50; x++)
    {
        DEBUG_LED_ON;
        _delay_ms(20);
        DEBUG_LED_OFF;
        _delay_ms(20);
    }
}

ISR(WDT_vect)
{
    enable_watchdog();
    
    wdt_tick++;
    
    if (wdt_tick % TICKS_MESSAGE_WAIT == 0)
    {
        flags |= FLAG_WDT;
    }
}

ISR(PCINT0_vect)
{
    if (RCWL_ACTIVATED)
        flags |= FLAG_RCWL;

    if (PIR_ACTIVATED)
        flags |= FLAG_PIR;
}

void delay_ms_200()
{
    // pouziva se i v assembleru pri prevodu VCC
    _delay_ms(200);
}

uint16_t readVcc()
{
    uint8_t admux_old = ADMUX;

    ADCSRA |= (1 << ADEN);

    ADMUX = (1 << MUX5) | (1 << MUX0); // Pro ATTINY84: 0b100001
    _delay_ms(2);                      // počkej na stabilizaci
    ADCSRA |= (1 << ADSC);             // Start ADC conversion
    while (ADCSRA & (1 << ADSC))
        ; // Čekej na dokončení

    uint16_t result = ADC;
    long vcc = (1100L * 1024L) / result; // Výpočet napětí v mV

    ADMUX = admux_old;

    return vcc;
}

void setup_gpio()
{
    switch (sensor_id)
    {
    case 14:
        PCMSK0 = _BV(PCINT1);
        break;
    case 12:
    default:
        PCMSK0 = _BV(PCINT1) | _BV(PCINT2);
        break;
    }

    PCMSK1 = _BV(PCINT10); // PB2
    GIMSK = _BV(PCIE0) | _BV(PCIE1);

    DDRB &= ~_BV(PB2);
    PORTB |= _BV(PB2); // pullup pro PB2
}

#ifdef AM2302_BIT
uint32_t am2302_read(void);
#endif

void main()
{
    if (MCUSR & _BV(EXTRF))
        jmp_to_bootloader();

    MCUSR = 0;
    DDRA = 0;
    DDRB = 0;

    setup_gpio();

    sensor_id = eeprom_read_byte((uint8_t *)4);

    debug();

    // musime pockat na dojezd predchozi komunikace, aby nas neposlala znovu do bootloaderu
    _delay_ms(2000);

    enable_watchdog();

    ir_init();
    sei();

    uint16_t msg_id = 0;
    while (1)
    {
        if (flags || rx_buf_len/* && !send_delay*/)
        {
            delay_ms_200(); // pockame na senzory - pripadnou zmenu flags

            message_t msg = {
                .sensor_id = sensor_id,
                .msg_id = msg_id,
                .vcc = readVcc(),
                .tick = wdt_tick,
                .flags = flags,
                .version = FW_VERSION,
                .dummy = rx_buf_len,
#ifdef AM2302_BIT
                .humitemp = am2302_read(),
#else
                .humitemp = 0,
#endif
            };

            speck_encrypt((uint8_t *)&msg);
            speck_encrypt(((uint8_t *)&msg) + 8);

            rf_send((uint8_t *)&msg, 16);
            delay_ms_200();
            delay_ms_200();
            rf_send((uint8_t *)&msg, 16);
            delay_ms_200();

            msg_id++;
            // send_delay = MESSAGE_SEND_DELAY;
            // DEBUG_LED_OFF;
            ir_init();
        }

        flags = 0;

        if (!ir_comm_active)
        {
            DEBUG_LED_OFF;
            ADCSRA &= ~(1 << ADEN); // vypiname ADC
            set_sleep_mode(SLEEP_MODE_PWR_DOWN);
            sleep_mode();
            DEBUG_LED_ON;
        }
    }
}