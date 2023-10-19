#include "avr/generic.h"

extern int __bss_start;
extern int __bss_end;
extern char __data_start;
extern char __data_end;
extern char __data_load_start;
extern void start();

// This only need to be considered external while it tries to link the og
EXTERNAL void BOOTFUNC __do_clear_bss(void) {
    int *bss_ptr;
    for (bss_ptr = &__bss_start; bss_ptr < &__bss_end; bss_ptr++) {
        *bss_ptr = 0;
    }
}

EXTERNAL void BOOTFUNC __do_copy_data(void) {
    char *data_dest_ptr = &__data_start;
    const char *data_src_ptr = &__data_load_start;

    // Loop through the entire .data section
    while (data_dest_ptr < &__data_end) {
        // Use pgm_read_byte to read a byte from program space (Flash)
        LPM(data_src_ptr++,*data_dest_ptr++);
    }

}

EXTERNAL void BOOTFUNC __attribute((OS_main)) __attribute__((used))  boot()
{
	__do_copy_data();
	__do_clear_bss();
	start();
}