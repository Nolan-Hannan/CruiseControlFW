/* TIM2 microsecond timebase, timers and the UTC wall clock. */

#include "sys_time.h"
#include "main.h"
#include <stddef.h>
extern TIM_HandleTypeDef htim2;

void sys_timerInit(sys_timer_t *timer) {
	timer->startTick = sys_getTimeMs();
}

uint8_t sys_checkOutTimerMs(sys_timer_t *timer) {
	uint32_t curTickMs = sys_getTimeMs();
	uint32_t deltaMs = (uint32_t) (curTickMs - timer->startTick);
	if(deltaMs >= timer->timerLengthMs) {
		timer->startTick += timer->timerLengthMs;
		return TRUE;
	} else {
		return FALSE;
	}
}

uint32_t sys_getTimeUs(void) {
	return __HAL_TIM_GET_COUNTER(&htim2);
}

uint32_t sys_getTimeMs(void) {
	return HAL_GetTick();
}

void sys_delayUs(uint32_t us) {
	uint32_t start = sys_getTimeUs();

	while ((uint32_t)(sys_getTimeUs() - start) < us) {
	}
}

/* ---- UTC wall clock ---- */

#define YEAR_MIN        2000U
#define YEAR_MAX        2099U
#define EPOCH_MIN       946684800UL  /* 2000-01-01 00:00:00 */
#define EPOCH_MAX       4102444799UL /* 2099-12-31 23:59:59 */
#define SECONDS_PER_DAY 86400U

static uint8_t isLeap(uint32_t year) {
	return ((year % 4U == 0U && year % 100U != 0U) || year % 400U == 0U) ? TRUE : FALSE;
}

static uint8_t daysInMonth(uint32_t year, uint32_t month) {
	static const uint8_t days[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

	return (month == 2U && isLeap(year)) ? (uint8_t)29U : days[month - 1U];
}

static uint8_t worldTimeValid(const world_time_t *t) {
	return (t->year >= YEAR_MIN && t->year <= YEAR_MAX &&
	        t->month >= 1U && t->month <= 12U &&
	        t->day >= 1U && t->day <= daysInMonth(t->year, t->month) &&
	        t->hours < 24U && t->min < 60U && t->sec < 60U) ? TRUE : FALSE;
}

/* Days since 1970-01-01 for a valid date, counting years from March. */
static uint32_t daysFromCivil(uint32_t year, uint32_t month, uint32_t day) {
	uint32_t y = (month <= 2U) ? year - 1U : year;
	uint32_t era = y / 400U;
	uint32_t yoe = y - era * 400U;
	uint32_t mp = (month > 2U) ? month - 3U : month + 9U;
	uint32_t doy = (153U * mp + 2U) / 5U + day - 1U;
	uint32_t doe = yoe * 365U + yoe / 4U - yoe / 100U + doy;

	return era * 146097U + doe - 719468U;
}

static void civilFromDays(uint32_t days, world_time_t *out) {
	uint32_t z = days + 719468U;
	uint32_t era = z / 146097U;
	uint32_t doe = z - era * 146097U;
	uint32_t yoe = (doe - doe / 1460U + doe / 36524U - doe / 146096U) / 365U;
	uint32_t doy = doe - (365U * yoe + yoe / 4U - yoe / 100U);
	uint32_t mp = (5U * doy + 2U) / 153U;
	uint32_t month = (mp < 10U) ? mp + 3U : mp - 9U;

	out->year = (uint16_t)(yoe + era * 400U + ((month <= 2U) ? 1U : 0U));
	out->month = (uint8_t)month;
	out->day = (uint8_t)(doy - (153U * mp + 2U) / 5U + 1U);
}

uint32_t sys_worldTimeToEpoch(const world_time_t *t) {
	if (t == NULL || t->year < 1970U) {
		return 0U;
	}
	return daysFromCivil(t->year, t->month, t->day) * SECONDS_PER_DAY +
	       (uint32_t)t->hours * 3600U + (uint32_t)t->min * 60U + t->sec;
}

void sys_worldTimeFromEpoch(uint32_t total_sec, world_time_t *out) {
	uint32_t secOfDay = total_sec % SECONDS_PER_DAY;

	if (out == NULL) {
		return;
	}
	civilFromDays(total_sec / SECONDS_PER_DAY, out);
	out->hours = (uint8_t)(secOfDay / 3600U);
	out->min = (uint8_t)((secOfDay % 3600U) / 60U);
	out->sec = (uint8_t)(secOfDay % 60U);
	out->total_sec = total_sec;
}

static void store(wall_clock_t *c, const world_time_t *anchor, uint32_t time_ms) {
	c->anchor = *anchor;
	c->last_updated_ms = time_ms;
	c->source_healthy = TRUE;
}

void sys_wallClockInit(wall_clock_t *c) {
	if (c == NULL) {
		return;
	}
	c->anchor.day = 0U;
	c->anchor.month = 0U;
	c->anchor.year = 0U;
	c->anchor.hours = 0U;
	c->anchor.min = 0U;
	c->anchor.sec = 0U;
	c->anchor.total_sec = 0U;
	c->source_healthy = FALSE;
	c->max_age_ms = SYS_WALLCLOCK_MAX_AGE_MS;
	c->last_updated_ms = 0U;
}

uint8_t sys_wallClockSet(wall_clock_t *c, const world_time_t *t, uint32_t time_ms) {
	world_time_t anchor;

	if (c == NULL || t == NULL || !worldTimeValid(t)) {
		return FALSE;
	}
	anchor = *t;
	anchor.total_sec = sys_worldTimeToEpoch(&anchor);
	store(c, &anchor, time_ms);
	return TRUE;
}

uint8_t sys_wallClockSetEpoch(wall_clock_t *c, uint32_t total_sec, uint32_t time_ms) {
	world_time_t anchor;

	if (c == NULL || total_sec < EPOCH_MIN || total_sec > EPOCH_MAX) {
		return FALSE;
	}
	sys_worldTimeFromEpoch(total_sec, &anchor);
	store(c, &anchor, time_ms);
	return TRUE;
}

void sys_wallClockSetError(wall_clock_t *c) {
	if (c != NULL) {
		c->source_healthy = FALSE;
	}
}

uint8_t sys_wallClockValid(const wall_clock_t *c, uint32_t now_ms) {
	if (c == NULL || !c->source_healthy) {
		return FALSE;
	}
	/* Unsigned subtraction handles the millisecond counter wrapping. */
	return ((uint32_t)(now_ms - c->last_updated_ms) <= c->max_age_ms) ? TRUE : FALSE;
}

uint8_t sys_wallClockGet(const wall_clock_t *c, uint32_t now_ms, world_time_t *out) {
	if (out == NULL || !sys_wallClockValid(c, now_ms)) {
		return FALSE;
	}
	sys_worldTimeFromEpoch(c->anchor.total_sec + (uint32_t)(now_ms - c->last_updated_ms) / 1000U, out);
	return TRUE;
}
