#include <stdint.h>

/* Reset handler - called on reset */
void __attribute__((weak)) reset_handler(void)
{
        /* Jump to main */
        extern int main(void);
        main();
}

/* Default handler for unhandled interrupts */
void __attribute__((weak)) default_handler(void)
{
        while (1)
                ;
}

/* Vector table */
__attribute__((section(".vectors"))) void (*const vectors[])(void) = {
        reset_handler, /* Reset handler */
        default_handler, /* NMI handler */
        default_handler /* Hard fault handler */
        /* Add more vectors as needed */
};
