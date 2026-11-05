/* 10/15
 * Implement functions declared in cc_state.h
*/

#include "cc_state.h"

static cc_state_t current_state = CC_STATE_OFF;

void cc_state_init(void) {
    current_state = CC_STATE_OFF;
}

cc_state_t cc_state_get(void) {
    return current_state;
}

// State update function
void cc_state_update(bool mode_pressed, bool cruise_allowed) {
    /* Safety overrides a simultaneous mode press. */
    if (!cruise_allowed) {
        current_state = CC_STATE_OFF;
        return;
    }

    if (!mode_pressed) {
        return;
    }

    switch (current_state) {
        case CC_STATE_OFF:
            current_state = CC_STATE_MANUAL;
            break;

        case CC_STATE_MANUAL:
            current_state = CC_STATE_POWER_OPT;
            break;

        case CC_STATE_POWER_OPT:
            current_state = CC_STATE_OFF;
            break;

        default:
            current_state = CC_STATE_OFF;
            break;
    }
}
