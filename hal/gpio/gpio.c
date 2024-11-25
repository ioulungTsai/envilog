#include "gpio.h"
#include "esp32s3.h"
#include <string.h>

/* Register access macros */
#define REG_READ(addr)       (*((volatile uint32_t *)(addr)))
#define REG_WRITE(addr,val)  (*((volatile uint32_t *)(addr))) = (uint32_t)(val)
#define REG_SET_BIT(addr, mask) REG_WRITE((addr + GPIO_OUT_W1TS_REG), (mask))
#define REG_CLR_BIT(addr, mask) REG_WRITE((addr + GPIO_OUT_W1TC_REG), (mask))

/* Internal state tracking */
typedef struct {
    bool initialized;
    gpio_config_t config;
    gpio_interrupt_handler_t int_handler;
} gpio_internal_state_t;

/* State tracking for all GPIO pins */
static gpio_internal_state_t gpio_states[GPIO_PIN_COUNT] = {0};

/* Validation functions */
static bool is_valid_pin(uint32_t pin) {
    return pin < GPIO_PIN_COUNT;
}

// static bool is_valid_mode(gpio_mode_t mode) {
//     return mode <= GPIO_MODE_ALTERNATE;
// }

/* Internal configuration functions */
// static gpio_error_t configure_pin_mode(uint32_t pin, const gpio_config_t* config) {
//     if (!is_valid_pin(pin) || !config) {
//         return GPIO_ERROR_INVALID_PIN;
//     }

//     // Configure IO MUX
//     uint32_t mux_reg = IO_MUX_BASE_REG + (pin * 4);
//     uint32_t mux_val = MCU_SEL_GPIO_FUNC;
    
//     if (config->mode == GPIO_MODE_ALTERNATE) {
//         mux_val |= (config->alternate_func << 4);
//     }
    
//     REG_WRITE(mux_reg, mux_val);

//     // Configure direction
//     if (config->mode == GPIO_MODE_OUTPUT || config->mode == GPIO_MODE_INPUT_OUTPUT) {
//         REG_WRITE(GPIO_BASE_REG + GPIO_ENABLE_W1TS_REG, (1ULL << pin));
//     } else {
//         REG_WRITE(GPIO_BASE_REG + GPIO_ENABLE_W1TC_REG, (1ULL << pin));
//     }

//     return GPIO_OK;
// }

// static gpio_error_t configure_pin_pull(uint32_t pin, const gpio_config_t* config) {
//     (void)pin;    // Suppress unused parameter warning
//     (void)config; // Suppress unused parameter warning
//     // Implementation will come later
//     return GPIO_OK;
// }

// static gpio_error_t configure_pin_interrupt(uint32_t pin, const gpio_config_t* config) {
//     (void)pin;    // Suppress unused parameter warning
//     (void)config; // Suppress unused parameter warning
//     // Implementation will come later
//     return GPIO_OK;
// }

/* Public API Implementation */
gpio_error_t gpio_init(uint32_t pin, gpio_direction_t direction) {
    if (!is_valid_pin(pin)) {
        return GPIO_ERROR_INVALID_PIN;
    }

    /* Configure pin mode */
    uint32_t mux_reg = IO_MUX_BASE_REG + (pin * 4);
    uint32_t mux_val = MCU_SEL_GPIO_FUNC;
    REG_WRITE(mux_reg, mux_val);
    
    /* Set direction */
    if (direction == GPIO_OUTPUT) {
        REG_WRITE(GPIO_BASE_REG + GPIO_ENABLE_W1TS_REG, (1ULL << pin));
    } else {
        REG_WRITE(GPIO_BASE_REG + GPIO_ENABLE_W1TC_REG, (1ULL << pin));
    }

    return GPIO_OK;
}

gpio_error_t gpio_set_state(uint32_t pin, bool state) {
    if (!is_valid_pin(pin)) {
        return GPIO_ERROR_INVALID_PIN;
    }

    if (!gpio_states[pin].initialized) {
        return GPIO_ERROR_INVALID_PIN;
    }

    if (state) {
        REG_WRITE(GPIO_BASE_REG + GPIO_OUT_W1TS_REG, (1ULL << pin));
    } else {
        REG_WRITE(GPIO_BASE_REG + GPIO_OUT_W1TC_REG, (1ULL << pin));
    }

    return GPIO_OK;
}

gpio_error_t gpio_toggle(uint32_t pin) {
    if (!is_valid_pin(pin)) {
        return GPIO_ERROR_INVALID_PIN;
    }

    uint32_t pin_mask = (1ULL << pin);
    uint32_t current_state = REG_READ(GPIO_BASE_REG + GPIO_OUT_REG) & pin_mask;
    
    if (current_state) {
        REG_WRITE(GPIO_BASE_REG + GPIO_OUT_W1TC_REG, pin_mask);
    } else {
        REG_WRITE(GPIO_BASE_REG + GPIO_OUT_W1TS_REG, pin_mask);
    }

    return GPIO_OK;
}

// Implement other functions similarly...
