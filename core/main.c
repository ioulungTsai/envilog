#include "system.h"
#include "gpio.h"

#define LED_PIN 48  // ESP32-S3-DevKitC-1 LED pin

void led_interrupt_handler(uint32_t pin) {
    (void)pin;  // Suppress unused parameter warning
    if (gpio_toggle(LED_PIN) != GPIO_OK) {
        // Handle error
    }
}

int main(void) {
    /* Initialize system */
    system_init();
    
    /* Initialize LED pin */
    gpio_init(LED_PIN, GPIO_OUTPUT);
    
    /* Main loop */
    while (1) {
        if (gpio_toggle(LED_PIN) != GPIO_OK) {
            // Handle error
        }
        system_delay(500);  // 500ms delay
    }
    
    return 0;
}
