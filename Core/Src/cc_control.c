/* 10/26
 * Capture and clear the target speed; speed regulation is not implemented yet.
 */

#include "cc_control.h"

// Initialize private variables
static cc_state_t previous_state;
static float      target_speed_mps;
static float current_speed_mps;
static float past_speed_mps;
static bool       target_valid;
static uint32_t prev_time;
static float integral;

void cc_control_init(void) {
    target_speed_mps = 0.0f;
    current_speed_mps = 0.0f;
    integral = 0.0f;
    target_valid     = false;           // No target speed has been captured
    previous_state   = CC_STATE_OFF;
}

void cc_control_update(cc_state_t state, float measured_speed_mps,
                       bool speed_valid) {
    
    // Check if measured speed is valid and recent enough to use for CC
    if (!speed_valid) {
        target_speed_mps = 0.0f;
        target_valid     = false;
        previous_state   = state;
        return;
    }

    switch (state) {
        case CC_STATE_OFF:
            target_speed_mps = 0.0f;
            integral = 0.0f;
            target_valid     = false;
            break;

        case CC_STATE_MANUAL:
            if (previous_state  != CC_STATE_MANUAL) {     // Capture speed ONLY when entering MANUAL
                target_speed_mps = measured_speed_mps;
                current_speed_mps = measured_speed_mps;
                target_valid     = true;
                prev_time = HAL_GetTick();
                break;
            }
            past_speed_mps = current_speed_mps;
            current_speed_mps = measured_speed_mps;
            uint32_t cur_time = HAL_GetTick();
            float delta_t = (cur_time - prev_time)/1000;
            prev_time = cur_time;

            float error = target_speed_mps - current_speed_mps;
            float past_error = target_speed_mps - past_speed_mps;

            integral += 0.5*(error + past_error) * delta_t;

            if (integral > 3.3f){
                integral = 3.3f;
            }
            if (integral < -3.3f){
                integral = -3.3f;
            }

            output =  1 * error + 1 * integral; //we will have to update this with a real output variable


            break;

        case CC_STATE_POWER_OPT:
            // TO BE IMPLEMENTED, CURRENTLY PLACEHOLDER
            target_speed_mps = 0.0f;
            target_valid     = false;
            break;

        default:
            target_speed_mps = 0.0f;
            target_valid     = false;
            break;
    }

    previous_state = state;

}
