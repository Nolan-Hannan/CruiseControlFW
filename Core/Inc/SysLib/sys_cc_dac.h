/* MCP48CVD21, SPI. Throttle voltage output. */

#ifndef SYS_CC_DAC_H
#define SYS_CC_DAC_H

#include "defines.h"

#ifdef __cplusplus
extern "C" {
#endif

uint8_t sys_ccDacInit(void);
uint8_t sys_ccDacSetCode(uint16_t code);
uint8_t sys_ccDacSetVolts(float volts);

#ifdef __cplusplus
}
#endif

#endif /* SYS_CC_DAC_H */
