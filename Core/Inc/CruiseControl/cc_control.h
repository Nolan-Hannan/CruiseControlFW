/* 10/26
 * Capture and clear the normal-cruise target speed.
 * Speed regulation and hardware output are not implemented yet.
 */

#ifndef CC_CONTROL_H
#define CC_CONTROL_H

#include <stdbool.h>
#include "cc_state.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Clear the target and initialize mode history to OFF. */
void cc_control_init(void);

/*
 * state: selected mode from cc_state_get().
 * measured_speed_mps: measured vehicle speed in meters per second.
 * speed_valid: whether that measurement is valid and recent.
 * Capture on entry to MANUAL; clear for invalid feedback or other modes.
 */
void cc_control_update(cc_state_t state, float measured_speed_mps,
                       bool speed_valid);

#ifdef __cplusplus
}
#endif

#endif /* CC_CONTROL_H */
