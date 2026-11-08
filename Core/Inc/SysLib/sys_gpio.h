/* GPIO: BRAKE, FORWARD, REVERSE, MODE_SWITCH, MUX_SEL, RGB LEDs. */

#ifndef SYS_GPIO_H
#define SYS_GPIO_H

#include "defines.h"
#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

void sys_gpioWrite(GPIO_TypeDef *port, uint16_t pin, uint8_t level);
uint8_t sys_gpioRead(GPIO_TypeDef *port, uint16_t pin);

/* name is a CubeMX pin label from main.h, e.g. sys_gpioSet(MUX_SEL, HI). */
#define sys_gpioSet(name, level) sys_gpioWrite(name##_GPIO_Port, name##_Pin, (level))
#define sys_gpioGet(name) sys_gpioRead(name##_GPIO_Port, name##_Pin)

#ifdef __cplusplus
}
#endif

#endif /* SYS_GPIO_H */
