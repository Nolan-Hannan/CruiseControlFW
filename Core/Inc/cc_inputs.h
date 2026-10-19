/* 10/16
 * Read cruise-control inputs and debounce the mode button.
 * The application supplies GPIO polarities, forward policy, and button settings.
 */

#ifndef CC_INPUTS_H
#define CC_INPUTS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CC_INPUT_LEVEL_UNKNOWN = 0,
    CC_INPUT_ACTIVE_LOW,
    CC_INPUT_ACTIVE_HIGH
} cc_input_active_level_t;

typedef enum {
    CC_FORWARD_RULE_UNKNOWN = 0,
    CC_FORWARD_REQUIRED,
    CC_FORWARD_NOT_REQUIRED
} cc_forward_rule_t;

/* Set levels and policy explicitly; debounce timing is ignored if disabled. */
typedef struct {
    cc_input_active_level_t mode_active_level;
    cc_input_active_level_t brake_active_level;
    cc_input_active_level_t forward_active_level;
    cc_input_active_level_t reverse_active_level;
    cc_forward_rule_t forward_rule;
    bool mode_button_enabled;
    uint32_t mode_debounce_ms; /* Must be nonzero when the button is enabled. */
} cc_inputs_config_t;

/* Electrical pin levels, available even before configuration is supplied. */
typedef struct {
    bool mode_high;
    bool brake_high;
    bool forward_high;
    bool reverse_high;
} cc_inputs_raw_t;

typedef struct {
    cc_inputs_raw_t raw;
    bool configured;
    bool brake_active;
    bool forward_active;
    bool reverse_active;
    bool mode_pressed;   /* One update per debounced press. */
    bool cruise_allowed; /* GPIO permission only; excludes sensor validity. */
} cc_inputs_t;

/*
 * Call after GPIO initialization. Copies the configuration.
 * NULL, unknown levels/policy, or an enabled button with zero debounce time
 * return false. A disabled button generates no press events; GPIO safety still
 * works. An enabled, held button must be released before it can engage.
 */
bool cc_inputs_init(const cc_inputs_config_t *config);

/*
 * Poll from the main loop without blocking. Call even when cruise is OFF.
 * Raw levels require initialized GPIOs; interpreted flags require configuration.
 * Safety inputs are checked each call. Unsafe conditions disarm the button;
 * a stable release while permitted is required before another press is accepted.
 */
cc_inputs_t cc_inputs_update(void);

#ifdef __cplusplus
}
#endif

#endif /* CC_INPUTS_H */
