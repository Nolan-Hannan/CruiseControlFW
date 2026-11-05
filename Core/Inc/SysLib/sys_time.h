/* TIM2 microsecond timebase and timers. */

#ifndef SYS_TIME_H
#define SYS_TIME_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Times in microseconds. */
typedef struct {
    uint32_t timerLengthMs;
    uint32_t startTick;
} sys_timer_t;

/* Returns true once the timer has expired. */
bool sys_checkOutTimerMs(const sys_timer_t *timer);

#ifdef __cplusplus
}
#endif

#endif /* SYS_TIME_H */
