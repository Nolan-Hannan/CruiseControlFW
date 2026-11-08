/* GPIO: BRAKE, FORWARD, REVERSE, MODE_SWITCH, MUX_SEL, RGB LEDs. */

#include "sys_gpio.h"

void sys_gpioWrite(GPIO_TypeDef *port, uint16_t pin, uint8_t level) {
	HAL_GPIO_WritePin(port, pin, level == HI ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

uint8_t sys_gpioRead(GPIO_TypeDef *port, uint16_t pin) {
	return HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_SET ? HI : LO;
}
