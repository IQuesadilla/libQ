#include "generic.h"

#define FOSC 8000000UL

#define UBRR(baud) ((FOSC / (16 * (long)baud)) - 1)

#define PINB addr 0x23
#define DDRB addr 0x24
#define PORTB addr 0x25

#define PIND addr 0x29
#define DDRD addr 0x2A
#define PORTD addr 0x2B

#define TIFR0 addr 0x35

#define EIFR addr 0x3C
#define EIMSK addr 0x3D

#define TCCR0A addr 0x44
#define TCCR0B addr 0x45
#define TCNT0 addr 0x46

#define EICRA addr 0x69
#define TIMSK0 addr 0x6E

#define UCSR0A addr 0xC0
#define UCSR0B addr 0xC1
#define UCSR0C addr 0xC2
#define UBRR0L addr 0xC4
#define UBRR0H addr 0xC5
#define UDR0 addr 0xC6