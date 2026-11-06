/* USART1 debug. */

#ifndef SYS_UART_H
#define SYS_UART_H

#include "defines.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint8_t sys_uartInit(void);
uint16_t sys_uartRead(uint8_t *dst, uint16_t max);
uint8_t sys_uartWrite(const uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* SYS_UART_H */
