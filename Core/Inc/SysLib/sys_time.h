/* TIM2 microsecond timebase, timers and the UTC wall clock. */

#ifndef SYS_TIME_H
#define SYS_TIME_H

#include "defines.h"

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
void sys_delayUs(uint32_t us);

/* UTC. The calendar fields and total_sec are the same time; only the helpers write them. */
typedef struct {
    uint8_t day;
    uint8_t month;
    uint16_t year;
    uint8_t hours;
    uint8_t min;
    uint8_t sec;
    uint32_t total_sec; /* seconds since 1970-01-01 00:00:00 */
} world_time_t;

typedef struct {
    world_time_t anchor;      /* time at the last sync, not the current time */
    uint8_t source_healthy;   /* FALSE at boot and after an error code, TRUE after a good set */
    uint32_t max_age_ms;      /* how long after a sync the clock is trusted */
    uint32_t last_updated_ms; /* sys_getTimeMs() at the sync, e.g. sys_canFrame_t.rxTimeMs */
} wall_clock_t;

/* Placeholder until the GPS update rate is known. */
#define SYS_WALLCLOCK_MAX_AGE_MS 600000U

/* Defined in main.c. */
extern wall_clock_t wall_clock;

void sys_wallClockInit(wall_clock_t *c);

/* Return TRUE if accepted. A bad value returns FALSE and leaves the clock unchanged. */
uint8_t sys_wallClockSet(wall_clock_t *c, const world_time_t *t, uint32_t time_ms); /* t->total_sec is ignored */
uint8_t sys_wallClockSetEpoch(wall_clock_t *c, uint32_t total_sec, uint32_t time_ms);

/* Marks the clock unhealthy and keeps the anchor. The next good set restores it. */
void sys_wallClockSetError(wall_clock_t *c);

/* TRUE if healthy and synced no longer ago than max_age_ms. */
uint8_t sys_wallClockValid(const wall_clock_t *c, uint32_t now_ms);

/* Current time: the anchor plus the whole seconds since the sync. FALSE if not valid. */
uint8_t sys_wallClockGet(const wall_clock_t *c, uint32_t now_ms, world_time_t *out);

/* Calendar conversion for valid dates from 1970 on; the date fields must already be valid. */
uint32_t sys_worldTimeToEpoch(const world_time_t *t);
void sys_worldTimeFromEpoch(uint32_t total_sec, world_time_t *out);

#ifdef __cplusplus
}
#endif

#endif /* SYS_TIME_H */
