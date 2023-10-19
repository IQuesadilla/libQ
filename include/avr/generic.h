#ifndef GENERIC_H
#define GENERIC_H

#ifndef uint
#define uint unsigned int
#endif

#define addr *(volatile uint*)

#define BOOTFUNC  __attribute__((__section__(".bootsection")))
#define INT __attribute__((signal))

#ifdef __cplusplus
#define EXTERNAL extern "C"
#elif
#define EXTERNAL
#endif

#define nop() ({__asm__ volatile("nop");})
#define setBits(base,mask) ((base) |= mask)
#define resetBits(base,mask) ((base) &= ~mask)
#define EnableInterrupts() ({__asm__ volatile("sei");})
#define DisableInterrupts() ({__asm__ volatile("cli");})
#define LPM(ptr, result) (asm("lpm %0, Z" : "=r" (result) : "z" (ptr)))

#ifdef __AVR_ATmega328P__
#include "m328p_defines.h"
#endif

#endif