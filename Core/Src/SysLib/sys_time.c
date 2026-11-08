/* TIM2 microsecond timebase and timers. */

#include "sys_time.h"
#include "main.h"
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
