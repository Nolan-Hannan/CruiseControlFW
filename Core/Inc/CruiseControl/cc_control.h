/* 10/26
 * Cruise-control actuation: target speed capture, MUX_SEL, CC_DAC and the RGB LED.
 * Speed regulation (PI output to the DAC) is not implemented yet.
 */

#ifndef CC_CONTROL_H
#define CC_CONTROL_H

#include "defines.h"
#include "cc_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CC_LED_OFF = 0,
    CC_LED_RED,
    CC_LED_GREEN,
    CC_LED_BLUE
} cc_led_t;

/* Results of the test calls. */
#define CC_TEST_OK        0
#define CC_TEST_BLOCKED   3 /* refused: stub speed on, cruise not OFF, or a safety edge */
#define CC_TEST_FAILED    4 /* hardware read or write failed */
#define CC_TEST_MISMATCH  5 /* loop test: the mux output did not follow the DAC */
#define CC_TEST_AMBIGUOUS 6 /* loop test: the pedal already reads near the test voltage */

/* Loop test readings in millivolts. */
typedef struct {
    uint16_t raw_mv;  /* ADC_RAW, pedal */
    uint16_t out_mv;  /* ADC_CC_OUT with the pedal selected */
    uint16_t loop_mv; /* ADC_CC_OUT with the DAC selected */
} cc_loop_result_t;

/* Clear the target, initialize mode history to OFF, pedal selected, LED red. */
void cc_control_init(void);

/*
 * Call first each main loop pass. After a brake, forward or reverse edge the
 * interrupt has already selected the pedal; this zeroes the DAC and forces OFF.
 */
void cc_control_service(void);

/*
 * Speed for the state machine. Returns speed_valid.
 * Stub for bench tests (default invalid) until a real speed source exists.
 */
uint8_t cc_control_speed(float *speed_mps);

/*
 * state: selected mode from cc_state_get().
 * measured_speed_mps: measured vehicle speed in meters per second.
 * speed_valid: whether that measurement is valid and recent.
 * Capture on entry to MANUAL; clear for invalid feedback or OFF; POWER_OPT keeps
 * the target. Engaging reads the pedal throttle into the DAC, then selects the DAC;
 * leaving cruise selects the pedal, then zeroes the DAC. Also drives the LED.
 * While the stub speed is valid nothing is driven: MUX_SEL stays LO and the DAC 0.
 */
void cc_control_update(cc_state_t state, float measured_speed_mps,
                       uint8_t speed_valid);

/* CLI test hooks. */
int cc_control_testSpeed(uint8_t valid, uint16_t speed_cmps);
void cc_control_testLed(cc_led_t color);
int cc_control_testDac(uint16_t millivolts);
int cc_control_testMux(uint8_t level);
void cc_control_testStop(void);

/*
 * Selects the DAC at a small fixed voltage for well under a millisecond, reads
 * ADC_CC_OUT, then selects the pedal and zeroes the DAC. Only runs with the
 * stub speed off and cruise OFF. The result is filled for OK, MISMATCH and AMBIGUOUS.
 */
int cc_control_testLoop(cc_loop_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* CC_CONTROL_H */
