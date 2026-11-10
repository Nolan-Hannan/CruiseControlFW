/* XMODEM-CRC receiver on the debug UART. */

#ifndef SYS_XMODEM_H
#define SYS_XMODEM_H

#include "defines.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	SYS_XMODEM_RUNNING,
	SYS_XMODEM_DONE,
	SYS_XMODEM_TIMEOUT,
	SYS_XMODEM_CANCELLED,
	SYS_XMODEM_FAILED
} sys_xmodemStatus_t;

/* Called for each new 128-byte block; return FALSE to reject it and end the transfer. */
typedef uint8_t (*sys_xmodemBlockFn)(const uint8_t *data, uint16_t len);

void sys_xmodemStart(sys_xmodemBlockFn onBlock);

/* Call every main loop pass. Returns RUNNING until the transfer ends. */
sys_xmodemStatus_t sys_xmodemStep(void);

void sys_xmodemAbort(void);

#ifdef __cplusplus
}
#endif

#endif /* SYS_XMODEM_H */
