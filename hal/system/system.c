#include "system.h"
#include <stdint.h>

/* Simple delay implementation - will be replaced with proper timer later */
void system_delay(uint32_t ms) {
    volatile uint32_t i;
    for (i = 0; i < (ms * 1000); i++) {
        __asm__("nop");
    }
}

void system_init(void) {
    /* Basic system initialization */
    /* Will add more initialization as needed */
}
