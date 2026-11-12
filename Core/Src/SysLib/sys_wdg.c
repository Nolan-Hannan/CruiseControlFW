/* Independent watchdog (IWDG). */

#include "sys_wdg.h"
#include "main.h"

extern IWDG_HandleTypeDef hiwdg;

void sys_feedWdg(void) {
	(void)HAL_IWDG_Refresh(&hiwdg);
}
