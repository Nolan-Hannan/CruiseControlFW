/* AD5721, SPI. Speed display output. */

#ifndef SYS_INFO_DAC_H
#define SYS_INFO_DAC_H

#include "defines.h"

#ifdef __cplusplus
extern "C" {
#endif

uint8_t sys_infoDacInit(void);
uint8_t sys_infoDacSetCode(uint16_t code);
uint8_t sys_infoDacSetVolts(float volts);

#ifdef __cplusplus
}
#endif

#endif /* SYS_INFO_DAC_H */
