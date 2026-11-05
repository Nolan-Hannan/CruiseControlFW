/* 
10/16
* Poll board GPIOs, normalize their levels, and produce mode-press events.

10/18
* GPIO input high
 */

#include "cc_inputs.h"
#include "main.h"

#include <stddef.h>

static cc_inputs_config_t input_config;
static bool configured = false;
static bool mode_candidate_pressed = false;
static bool mode_press_armed = false;
static uint32_t mode_candidate_since_ms = 0U;

static bool active_level_valid(cc_input_active_level_t level) {
    return level == CC_INPUT_ACTIVE_LOW || level == CC_INPUT_ACTIVE_HIGH;
}

static bool input_active(bool pin_high, cc_input_active_level_t level) {
    return level == CC_INPUT_ACTIVE_HIGH ? pin_high : !pin_high;
}

static cc_inputs_raw_t read_raw_inputs(void) {
    cc_inputs_raw_t raw;

    raw.mode_high = HAL_GPIO_ReadPin(MODE_SWITCH_GPIO_Port,
                                     MODE_SWITCH_Pin) == GPIO_PIN_SET;
    raw.brake_high = HAL_GPIO_ReadPin(BRAKE_GPIO_Port,
                                      BRAKE_Pin) == GPIO_PIN_SET;
    raw.forward_high = HAL_GPIO_ReadPin(FORWARD_GPIO_Port,
                                        FORWARD_Pin) == GPIO_PIN_SET;
    raw.reverse_high = HAL_GPIO_ReadPin(REVERSE_GPIO_Port,
                                        REVERSE_Pin) == GPIO_PIN_SET;
    return raw;
}

bool cc_inputs_init(const cc_inputs_config_t *config) {
    configured = false;
    mode_press_armed = false;
    mode_candidate_pressed = false;
    mode_candidate_since_ms = 0U;

    if (config == NULL ||
        !active_level_valid(config->mode_active_level) ||
        !active_level_valid(config->brake_active_level) ||
        !active_level_valid(config->forward_active_level) ||
        !active_level_valid(config->reverse_active_level) ||
        (config->forward_rule != CC_FORWARD_REQUIRED &&
         config->forward_rule != CC_FORWARD_NOT_REQUIRED) ||
        (config->mode_button_enabled && config->mode_debounce_ms == 0U)) {
        return false;
    }

    input_config = *config;
    if (input_config.mode_button_enabled) {
        mode_candidate_pressed = input_active(
            HAL_GPIO_ReadPin(MODE_SWITCH_GPIO_Port, MODE_SWITCH_Pin) == GPIO_PIN_SET,
            input_config.mode_active_level);
        mode_candidate_since_ms = HAL_GetTick();
    }
    configured = true;
    return true;
}

cc_inputs_t cc_inputs_update(void) {
    cc_inputs_t inputs = {0};
    inputs.raw = read_raw_inputs();

    /* Raw readings remain useful during bring-up with unknown polarities. */
    if (!configured) {
        return inputs;
    }

    inputs.configured = true;
    inputs.brake_active = input_active(inputs.raw.brake_high,
                                       input_config.brake_active_level);
    inputs.forward_active = input_active(inputs.raw.forward_high,
                                         input_config.forward_active_level);
    inputs.reverse_active = input_active(inputs.raw.reverse_high,
                                         input_config.reverse_active_level);
    inputs.cruise_allowed = !inputs.brake_active && !inputs.reverse_active &&
        (input_config.forward_rule == CC_FORWARD_NOT_REQUIRED ||
         inputs.forward_active);

    /* Button processing stays disabled until a button and timing are selected. */
    if (!input_config.mode_button_enabled) {
        return inputs;
    }

    const bool mode_active = input_active(inputs.raw.mode_high,
                                          input_config.mode_active_level);
    const uint32_t now_ms = HAL_GetTick();

    if (mode_active != mode_candidate_pressed) {
        mode_candidate_pressed = mode_active;
        mode_candidate_since_ms = now_ms;
    }

    /* A button held through an unsafe condition cannot engage on recovery. */
    if (!inputs.cruise_allowed) {
        mode_press_armed = false;
    }

    /* Unsigned subtraction handles the millisecond counter wrapping. */
    if ((uint32_t)(now_ms - mode_candidate_since_ms) >=
        input_config.mode_debounce_ms) {
        if (!mode_candidate_pressed) {
            mode_press_armed = inputs.cruise_allowed;
        } else if (mode_press_armed) {
            inputs.mode_pressed = true;
            mode_press_armed = false;
        }
    }

    return inputs;
}
