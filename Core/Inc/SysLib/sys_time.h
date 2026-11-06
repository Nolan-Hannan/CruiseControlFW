/* TIM2 microsecond timebase and timers. */

#ifndef SYS_TIME_H
#define SYS_TIME_H

#include "defines.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Times in milliseconds. */
typedef struct {
    const uint32_t timerLengthMs;
    uint32_t startTick;
} sys_timer_t;

/* Returns true once the timer has expired. */
uint8_t sys_checkOutTimerMs(sys_timer_t *timer);
void sys_timerInit(sys_timer_t *timer);

uint32_t sys_getTimeUs(void);
uint32_t sys_getTimeMs(void);

#ifdef __cplusplus
}
#endif

#endif /* SYS_TIME_H */
