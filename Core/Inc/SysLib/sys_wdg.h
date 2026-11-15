/* Independent watchdog (IWDG). */

#ifndef SYS_WDG_H
#define SYS_WDG_H

#include "defines.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Restarts the watchdog timeout. Once per main loop pass, and during long waits. */
void sys_feedWdg(void);

#ifdef __cplusplus
}
#endif

#endif /* SYS_WDG_H */
