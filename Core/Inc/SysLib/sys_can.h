/* CAN1 (LV) and CAN2 (HV) bus. */

#ifndef SYS_CAN_H
#define SYS_CAN_H

#include "defines.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SYS_CAN_LV,
    SYS_CAN_HV,
    SYS_CAN_BUS_COUNT
} sys_canBus_t;

typedef struct {
    uint32_t id;
    uint8_t ext;
    uint8_t dlc;
    uint8_t data[8];
    uint32_t rxTimeMs;
} sys_canFrame_t;

uint8_t sys_canInit(void);
uint8_t sys_canRead(sys_canBus_t bus, sys_canFrame_t *frame);
uint32_t sys_canDropped(sys_canBus_t bus);

#ifdef __cplusplus
}
#endif

#endif /* SYS_CAN_H */
