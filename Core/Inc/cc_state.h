/* 10/14
 * Defines cruise-control states: OFF = 0, MANUAL = 1, POWER_OPT = 2.
 * typedef names the enum cc_state_t for use in variables and functions.
 * The enum identifies states; transition logic belongs in cc_state.c.
 * cc_state_init() initializes to OFF; cc_state_get() returns the state.
 * Include guards prevent repeated inclusion within a source file.
 * extern "C" allows these C functions to be called from C++.
 */

#ifndef CC_STATE_H
#define CC_STATE_H

#ifdef __cplusplus
extern "C" {
#endif

/** Cruise-control operating modes. */
typedef enum
{
    CC_STATE_OFF = 0,
    CC_STATE_MANUAL,
    CC_STATE_POWER_OPT
} cc_state_t;

/** Initialize the state machine to OFF. */
void cc_state_init(void);

/** Return the current cruise-control state. */
cc_state_t cc_state_get(void);

#ifdef __cplusplus
}
#endif

#endif /* CC_STATE_H */
