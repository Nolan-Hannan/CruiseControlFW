/* Power optimizer: picks a target power from planned versus actual SOC, then the speed that uses it. */

#include "po_optimizer.h"
#include "sys_info_dac.h"
#include "sys_soc.h"
#include "sys_time.h"
#include <math.h>
#include <stddef.h>

#define PO_PERIOD_MS 1000U

/* Vehicle model. Placeholders until the car is measured. */
#define PO_MASS_KG     300.0f
#define PO_CDA_M2      0.12f
#define PO_CRR         0.006f
#define PO_DRIVE_EFF   0.95f /* battery to road */
#define PO_AIR_DENSITY 1.2f  /* kg/m^3 */
#define PO_GRAVITY     9.81f

/* PI on SOC error in percent (actual minus planned). Placeholders until tuned on the car. */
#define PO_KP_W_PER_PCT   100.0f
#define PO_KI_W_PER_PCT_S 1.0f
#define PO_INTEGRAL_MAX_W 1000.0f
#define PO_POWER_MAX_W    5000.0f

/* Info DAC: volts per m/s of target speed. Placeholder until the display scale is known. */
#define PO_INFO_V_PER_MPS 0.25f

#define PO_SOLVE_ITERATIONS 30U
#define PO_SOLVE_TOL_MPS    0.001f

static uint32_t last_run_ms;
static float integral_w;
static float target_speed_mps;
static uint8_t target_valid;

static float clampf(float x, float lo, float hi) {
	return (x < lo) ? lo : ((x > hi) ? hi : x);
}

/* Battery power to hold speed v, and its slope with v. */
static float drive_power(float v, float grade_force, float wind, float *slope) {
	const float k = 0.5f * PO_AIR_DENSITY * PO_CDA_M2;
	const float rel = v + wind;

	*slope = (k * fabsf(rel) * (3.0f * v + wind) + grade_force) / PO_DRIVE_EFF;
	return v * (k * rel * fabsf(rel) + grade_force) / PO_DRIVE_EFF;
}

uint8_t po_solve_speed(float power_w, float gradient_rad, float wind_mps, float guess_mps,
                       float *speed_mps) {
	float lo = PO_SPEED_MIN_MPS;
	float hi = PO_SPEED_MAX_MPS;
	float grade_force;
	float slope;
	float v;
	uint8_t i;

	if (speed_mps == NULL || !isfinite(power_w) || !isfinite(gradient_rad) ||
	    !isfinite(wind_mps) || !isfinite(guess_mps)) {
		return FALSE;
	}
	grade_force = PO_MASS_KG * PO_GRAVITY * (PO_CRR * cosf(gradient_rad) + sinf(gradient_rad));

	/* No root inside the limits: the power covers more than the top speed, or less than the bottom. */
	if (drive_power(hi, grade_force, wind_mps, &slope) <= power_w) {
		*speed_mps = hi;
		return TRUE;
	}
	if (drive_power(lo, grade_force, wind_mps, &slope) >= power_w) {
		*speed_mps = lo;
		return TRUE;
	}

	v = clampf(guess_mps, lo, hi);
	for (i = 0U; i < PO_SOLVE_ITERATIONS; i++) {
		const float f = drive_power(v, grade_force, wind_mps, &slope) - power_w;
		float next;

		if (f > 0.0f) {
			hi = v;
		} else {
			lo = v;
		}
		next = v - f / slope;
		/* Also catches a zero or negative slope (downhill), where Newton would run away. */
		if (!(slope > 0.0f) || !(next > lo && next < hi)) {
			next = 0.5f * (lo + hi);
		}
		if (fabsf(next - v) < PO_SOLVE_TOL_MPS) {
			*speed_mps = next;
			return TRUE;
		}
		v = next;
	}
	return FALSE;
}

static void set_invalid(void) {
	target_valid = FALSE;
	(void)sys_infoDacSetVolts(0.0f);
}

void po_optimizer_init(void) {
	last_run_ms = 0U;
	integral_w = 0.0f;
	target_speed_mps = 0.0f;
	set_invalid();
}

void po_optimizer_update(const vehicle_data_t *d, uint32_t now_ms) {
	const uint32_t elapsed_ms = now_ms - last_run_ms;
	world_time_t now;
	float planned_soc;
	float error;
	float power;
	float speed;

	if (elapsed_ms < PO_PERIOD_MS) {
		return;
	}
	last_run_ms = now_ms;
	if (!validate_po_data(d, now_ms) || !sys_wallClockGet(&wall_clock, now_ms, &now) ||
	    !sys_socPlanned(&now, &planned_soc)) {
		set_invalid();
		return;
	}

	/* Above the plan there is energy to spend, below it the car must save. */
	error = d->battery_soc.value - planned_soc;
	/* The clamp is the anti-windup. A long gap counts as one period. */
	integral_w = clampf(integral_w + PO_KI_W_PER_PCT_S * error * ((float)PO_PERIOD_MS / 1000.0f),
	                    -PO_INTEGRAL_MAX_W, PO_INTEGRAL_MAX_W);
	power = d->mppt1_power.value + d->mppt2_power.value + d->mppt3_power.value - d->aux_power.value +
	        PO_KP_W_PER_PCT * error + integral_w;
	power = clampf(power, 0.0f, PO_POWER_MAX_W);

	if (!po_solve_speed(power, d->gradient.value, d->wind_speed.value, d->current_speed.value, &speed)) {
		set_invalid();
		return;
	}
	target_speed_mps = speed;
	target_valid = TRUE;
	(void)sys_infoDacSetVolts(speed * PO_INFO_V_PER_MPS);
}

uint8_t po_optimizer_speed(float *speed_mps) {
	if (speed_mps != NULL && target_valid) {
		*speed_mps = target_speed_mps;
		return TRUE;
	}
	return FALSE;
}
