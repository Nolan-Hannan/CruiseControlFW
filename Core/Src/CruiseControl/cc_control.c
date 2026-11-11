/* 10/26
 * Capture and clear the target speed; drive MUX_SEL, CC_DAC and the RGB LED.
 * Speed regulation is not implemented yet.
 */

#include "cc_control.h"
#include "sys_adc.h"
#include "sys_cc_dac.h"
#include "sys_gpio.h"
#include "sys_time.h"

/* RGB LED pins are HIGH = off. */
#define LED_ON  LO
#define LED_OFF HI

/* Loop test: DAC voltage, allowed error, and settling time around each step. */
#define LOOP_TEST_MV   13U
#define LOOP_TOL_MV    10U
#define LOOP_SETTLE_US 500U

// Initialize private variables
static cc_state_t previous_state;
static float      target_speed_mps;
static float current_speed_mps;
static float past_speed_mps;
static uint8_t    target_valid;
static uint32_t prev_time;
static float integral;

/* Set by the brake, forward and reverse interrupt. */
static volatile uint8_t fault_pending = FALSE;

static uint8_t test_speed_valid = FALSE;
static float test_speed_mps = 0.0f;
static uint8_t led_forced = FALSE;
static cc_led_t led_forced_color = CC_LED_OFF;

/* Every MUX_SEL and CC_DAC write goes through these two. While the stub speed
 * is valid they refuse to select the DAC or output a voltage, so a bench test
 * of the states cannot move the car. The interrupt only ever lowers MUX_SEL. */
static uint8_t mux_set(uint8_t level) {
    if (level == HI && test_speed_valid) {
        return FALSE;
    }
    sys_gpioSet(MUX_SEL, level);
    return TRUE;
}

static uint8_t dac_set(float volts) {
    if (volts > 0.0f && test_speed_valid) {
        return FALSE;
    }
    return sys_ccDacSetVolts(volts);
}

static void led_set(cc_led_t color) {
    sys_gpioSet(RED, color == CC_LED_RED ? LED_ON : LED_OFF);
    sys_gpioSet(GREEN, color == CC_LED_GREEN ? LED_ON : LED_OFF);
    sys_gpioSet(BLUE, color == CC_LED_BLUE ? LED_ON : LED_OFF);
}

static cc_led_t state_color(cc_state_t state) {
    switch (state) {
        case CC_STATE_MANUAL:
            return CC_LED_GREEN;
        case CC_STATE_POWER_OPT:
            return CC_LED_BLUE;
        default:
            return CC_LED_RED;
    }
}

/* Pedal selected, DAC zero, state OFF, forced LED released. Returns FALSE if the DAC write failed. */
static uint8_t safe_state(void) {
    uint8_t dac_zeroed;

    (void)mux_set(LO);
    dac_zeroed = dac_set(0.0f);
    cc_state_force_off();
    led_forced = FALSE;
    return dac_zeroed;
}

/* Select the pedal first, then zero the DAC. */
static void disengage(void) {
    (void)mux_set(LO);
    (void)dac_set(0.0f);
}

/* Start the DAC at the pedal throttle, then select it. Returns FALSE if unsafe. */
static uint8_t engage(void) {
    float volts;

    if (test_speed_valid) {
        return TRUE;
    }
    if (!sys_adcReadVolts(SYS_ADC_RAW, &volts) || !dac_set(volts)) {
        (void)mux_set(LO);
        return FALSE;
    }
    (void)mux_set(HI);
    /* An edge between the checks above and the write is caught here. */
    if (fault_pending) {
        (void)mux_set(LO);
        return FALSE;
    }
    return TRUE;
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin == BRAKE_Pin || GPIO_Pin == FORWARD_Pin || GPIO_Pin == REVERSE_Pin) {
        sys_gpioSet(MUX_SEL, LO);
        fault_pending = TRUE;
    }
}

void cc_control_init(void) {
    target_speed_mps = 0.0f;
    current_speed_mps = 0.0f;
    integral = 0.0f;
    target_valid     = FALSE;           // No target speed has been captured
    previous_state   = CC_STATE_OFF;
    fault_pending = FALSE;
    test_speed_valid = FALSE;
    test_speed_mps = 0.0f;
    led_forced = FALSE;
    (void)mux_set(LO);
    led_set(CC_LED_RED);
}

void cc_control_service(void) {
    if (fault_pending) {
        fault_pending = FALSE;
        safe_state();
    }
}

uint8_t cc_control_speed(float *speed_mps) {
    /* TODO: replace with the real speed source; this stub is for bench tests. */
    *speed_mps = test_speed_mps;
    return test_speed_valid;
}

void cc_control_update(cc_state_t state, float measured_speed_mps,
                       uint8_t speed_valid) {
    const uint8_t was_cruising = previous_state != CC_STATE_OFF;
    const uint8_t now_cruising = state != CC_STATE_OFF;

    if (was_cruising && !now_cruising) {
        disengage();
    } else if (!was_cruising && now_cruising && !engage()) {
        cc_state_force_off();
        state = CC_STATE_OFF;
        speed_valid = FALSE;
    }
    led_set(led_forced ? led_forced_color : state_color(state));

    // Check if measured speed is valid and recent enough to use for CC
    if (!speed_valid) {
        target_speed_mps = 0.0f;
        target_valid     = FALSE;
        previous_state   = state;
        return;
    }

    switch (state) {
        case CC_STATE_OFF:
            target_speed_mps = 0.0f;
            integral = 0.0f;
            target_valid     = FALSE;
            break;

        case CC_STATE_MANUAL:
            if (previous_state  != CC_STATE_MANUAL) {     // Capture speed ONLY when entering MANUAL
                target_speed_mps = measured_speed_mps;
                current_speed_mps = measured_speed_mps;
                target_valid     = TRUE;
                prev_time = sys_getTimeMs();
                break;
            }
            past_speed_mps = current_speed_mps;
            current_speed_mps = measured_speed_mps;
            uint32_t cur_time = sys_getTimeMs();
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

            float output =  1 * error + 1 * integral; //we will have to update this with a real output variable


            break;

        case CC_STATE_POWER_OPT:
            // TO BE IMPLEMENTED: keeps MANUAL's target until the optimizer supplies one.
            break;

        default:
            target_speed_mps = 0.0f;
            target_valid     = FALSE;
            break;
    }

    previous_state = state;

}

int cc_control_testSpeed(uint8_t valid, uint16_t speed_cmps) {
    /* The stub turns on only once the DAC is confirmed zero. */
    if (!safe_state() && valid) {
        return CC_TEST_FAILED;
    }
    test_speed_valid = valid ? TRUE : FALSE;
    test_speed_mps = (float)speed_cmps / 100.0f;
    return CC_TEST_OK;
}

void cc_control_testLed(cc_led_t color) {
    led_forced = TRUE;
    led_forced_color = color;
    led_set(color);
}

int cc_control_testDac(uint16_t millivolts) {
    if (millivolts > 0U && test_speed_valid) {
        return CC_TEST_BLOCKED;
    }
    return dac_set((float)millivolts / 1000.0f) ? CC_TEST_OK : CC_TEST_FAILED;
}

int cc_control_testMux(uint8_t level) {
    if (level == HI && test_speed_valid) {
        return CC_TEST_BLOCKED;
    }
    return mux_set(level) ? CC_TEST_OK : CC_TEST_FAILED;
}

void cc_control_testStop(void) {
    safe_state();
    test_speed_valid = FALSE;
    test_speed_mps = 0.0f;
}

static uint16_t to_mv(float volts) {
    return (volts > 0.0f) ? (uint16_t)(volts * 1000.0f + 0.5f) : 0U;
}

static uint16_t mv_diff(uint16_t a, uint16_t b) {
    return (a > b) ? (uint16_t)(a - b) : (uint16_t)(b - a);
}

int cc_control_testLoop(cc_loop_result_t *result) {
    float raw = 0.0f;
    float out = 0.0f;
    float loop = 0.0f;
    uint8_t ok;

    if (result == NULL || test_speed_valid || cc_state_get() != CC_STATE_OFF || fault_pending) {
        return CC_TEST_BLOCKED;
    }
    (void)mux_set(LO);
    ok = dac_set(0.0f);
    sys_delayUs(LOOP_SETTLE_US);
    ok = ok && sys_adcReadVolts(SYS_ADC_RAW, &raw) && sys_adcReadVolts(SYS_ADC_CC_OUT, &out) &&
         dac_set((float)LOOP_TEST_MV / 1000.0f);
    if (ok) {
        sys_delayUs(LOOP_SETTLE_US);
        /* An edge during the waits ends the test; the check after the write covers an edge just before it. */
        if (!fault_pending) {
            (void)mux_set(HI);
            if (!fault_pending) {
                sys_delayUs(LOOP_SETTLE_US);
                ok = sys_adcReadVolts(SYS_ADC_CC_OUT, &loop);
            }
        }
    }
    disengage();
    if (fault_pending) {
        return CC_TEST_BLOCKED;
    }
    if (!ok) {
        return CC_TEST_FAILED;
    }
    result->raw_mv = to_mv(raw);
    result->out_mv = to_mv(out);
    result->loop_mv = to_mv(loop);
    if (mv_diff(result->out_mv, LOOP_TEST_MV) <= LOOP_TOL_MV) {
        return CC_TEST_AMBIGUOUS;
    }
    return (mv_diff(result->loop_mv, LOOP_TEST_MV) <= LOOP_TOL_MV) ? CC_TEST_OK : CC_TEST_MISMATCH;
}
