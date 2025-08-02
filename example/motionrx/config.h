#pragma once

#define PIR_ACTIVATED (PINA & PA1)
#define RCWL_ACTIVATED (PINA & PA2)

#define AM2302_PORT PORTA
#define AM2302_DDR DDRA
#define AM2302_PIN PINA
#define AM2302_BIT PA5

#define DEBUG_LED_ON (DDRB |= _BV(PB1), PORTB |= _BV(PB1))
#define DEBUG_LED_OFF (DDRB &= ~_BV(PB1), PORTB &= ~_BV(PB1))
