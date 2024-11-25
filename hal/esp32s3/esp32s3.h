#ifndef HAL_ESP32S3_H
#define HAL_ESP32S3_H

#include <stdint.h>

/* Base addresses for GPIO configuration */
#define GPIO_BASE_REG             0x60004000
#define IO_MUX_BASE_REG           0x60009000

/* GPIO register offsets */
#define GPIO_OUT_REG              0x0004
#define GPIO_OUT_W1TS_REG         0x0008
#define GPIO_OUT_W1TC_REG         0x000C
#define GPIO_ENABLE_REG           0x0020
#define GPIO_ENABLE_W1TS_REG      0x0024
#define GPIO_ENABLE_W1TC_REG      0x0028
#define GPIO_FUNC_OUT_SEL_CFG_REG 0x0554

/* IO MUX register offsets */
#define IO_MUX_GPIO48_REG         (IO_MUX_BASE_REG + 0x12C) // Example for LED on GPIO48

/* GPIO Pin Definitions */
#define GPIO_PIN_MIN              0U
#define GPIO_PIN_MAX              47U // ESP32-S3 has pins 0-47
#define GPIO_PIN_COUNT            48U // Total number of pins

/* GPIO function selection */
#define MCU_SEL_GPIO_FUNC         0x00
#define GPIO_MODE_OUTPUT          1

#endif /* HAL_ESP32S3_H */
