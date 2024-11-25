#ifndef HAL_GPIO_H
#define HAL_GPIO_H

#include <stdint.h>
#include <stdbool.h>

/* Error Handling */
typedef enum {
    GPIO_OK = 0,
    GPIO_ERROR_INVALID_PIN,
    GPIO_ERROR_INVALID_MODE,
    GPIO_ERROR_INVALID_PULL,
    GPIO_ERROR_INVALID_STATE,
    GPIO_ERROR_PIN_BUSY
} gpio_error_t;

/* GPIO direction */
typedef enum {
    GPIO_INPUT = 0,
    GPIO_OUTPUT = 1
} gpio_direction_t;

/* GPIO pin states */
typedef enum {
    GPIO_LOW = 0,
    GPIO_HIGH = 1
} gpio_state_t;

/* Pin Configurations */
typedef enum {
    GPIO_MODE_INPUT = 0,
    GPIO_MODE_OUTPUT,
    GPIO_MODE_INPUT_OUTPUT,    // Bidirectional
    GPIO_MODE_ANALOG,
    GPIO_MODE_ALTERNATE
} gpio_mode_t;

typedef enum {
    GPIO_PULL_NONE = 0,
    GPIO_PULL_UP,
    GPIO_PULL_DOWN,
    GPIO_PULL_UP_DOWN
} gpio_pull_t;

typedef enum {
    GPIO_SPEED_LOW = 0,
    GPIO_SPEED_MEDIUM,
    GPIO_SPEED_HIGH,
    GPIO_SPEED_VERY_HIGH
} gpio_speed_t;

/* Interrupt Configuration */
typedef enum {
    GPIO_INT_DISABLE = 0,
    GPIO_INT_RISING_EDGE,
    GPIO_INT_FALLING_EDGE,
    GPIO_INT_BOTH_EDGES,
    GPIO_INT_LOW_LEVEL,
    GPIO_INT_HIGH_LEVEL
} gpio_interrupt_t;

/* Pin Configuration Structure */
typedef struct {
    gpio_mode_t mode;
    gpio_pull_t pull;
    gpio_speed_t speed;
    gpio_interrupt_t interrupt;
    bool initial_state;
    uint8_t alternate_func;  // For alternate function mode
} gpio_config_t;

/* GPIO number */
typedef enum {
        GPIO_PIN_0  = 0U,
        GPIO_PIN_1  = 1U,
        GPIO_PIN_2  = 2U,
        GPIO_PIN_3  = 3U,
        GPIO_PIN_4  = 4U,
        GPIO_PIN_5  = 5U,
        GPIO_PIN_6  = 6U,
        GPIO_PIN_7  = 7U,
        GPIO_PIN_8  = 8U,
        GPIO_PIN_9  = 9U,
        GPIO_PIN_10 = 10U,
        GPIO_PIN_11 = 11U,
        GPIO_PIN_12 = 12U,
        GPIO_PIN_13 = 13U,
        GPIO_PIN_14 = 14U,
        GPIO_PIN_15 = 15U,
        GPIO_PIN_16 = 16U,
        GPIO_PIN_17 = 17U,
        GPIO_PIN_18 = 18U,
        GPIO_PIN_19 = 19U,
        GPIO_PIN_21 = 21U,
        GPIO_PIN_22 = 22U,
        GPIO_PIN_23 = 23U,
        GPIO_PIN_25 = 25U,
        GPIO_PIN_26 = 26U,
        GPIO_PIN_27 = 27U,
        GPIO_PIN_32 = 32U,
        GPIO_PIN_33 = 33U,
        GPIO_PIN_34 = 34U,
        GPIO_PIN_35 = 35U,
        GPIO_PIN_36 = 36U,
        GPIO_PIN_37 = 37U,
        GPIO_PIN_38 = 38U,
        GPIO_PIN_39 = 39U,
        GPIO_PIN_48 = 48U,
} gpio_pin_num_t;

/* Interrupt Callback */
typedef void (*gpio_interrupt_handler_t)(uint32_t pin);

/* Function Prototypes */

/**
 * @brief Initialize GPIO with configuration
 * @param pin: GPIO pin number
 * @param config: Pointer to configuration structure
 * @return gpio_error_t: Error code
 */
gpio_error_t gpio_init(uint32_t pin, gpio_direction_t direction);

/**
 * @brief Deinitialize GPIO pin
 * @param pin: GPIO pin number
 * @return gpio_error_t: Error code
 */
gpio_error_t gpio_deinit(uint32_t pin);

/**
 * @brief Set GPIO state
 * @param pin: GPIO pin number
 * @param state: true for high, false for low
 * @return gpio_error_t: Error code
 */
gpio_error_t gpio_set_state(uint32_t pin, bool state);

/**
 * @brief Get GPIO state
 * @param pin: GPIO pin number
 * @param state: Pointer to store state
 * @return gpio_error_t: Error code
 */
gpio_error_t gpio_get_state(uint32_t pin, bool* state);

/**
 * @brief Toggle GPIO state
 * @param pin: GPIO pin number
 * @return gpio_error_t: Error code
 */
gpio_error_t gpio_toggle(uint32_t pin);

/**
 * @brief Register interrupt handler for GPIO
 * @param pin: GPIO pin number
 * @param handler: Interrupt handler function
 * @return gpio_error_t: Error code
 */
gpio_error_t gpio_register_interrupt(uint32_t pin, gpio_interrupt_handler_t handler);

/**
 * @brief Enable GPIO interrupt
 * @param pin: GPIO pin number
 * @return gpio_error_t: Error code
 */
gpio_error_t gpio_enable_interrupt(uint32_t pin);

/**
 * @brief Disable GPIO interrupt
 * @param pin: GPIO pin number
 * @return gpio_error_t: Error code
 */
gpio_error_t gpio_disable_interrupt(uint32_t pin);

/* Power Management */
gpio_error_t gpio_enter_low_power(uint32_t pin);
gpio_error_t gpio_exit_low_power(uint32_t pin);

#endif /* HAL_GPIO_H */
