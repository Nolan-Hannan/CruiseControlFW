/*
 * Host check for the power optimizer. Not part of the firmware build. From the repo root:
 *   cc -Wall -Wextra -ICore/Inc -ICore/Inc/SysLib -ICore/Inc/PowerOptimization \
 *      Tests/test_po_optimizer.c Core/Src/PowerOptimization/po_optimizer.c Core/Src/vehicle_data.c \
 *      -lm -o /tmp/test_po && /tmp/test_po
 */

#include "po_optimizer.h"
#include "sys_info_dac.h"
#include "sys_soc.h"
#include "sys_time.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

/* Stubs for the hardware the optimizer touches. */
wall_clock_t wall_clock;
static uint8_t clock_ok = TRUE;
static uint8_t plan_ok = TRUE;
static float plan_soc = 50.0f;
static float dac_volts = -1.0f;

uint8_t sys_wallClockGet(const wall_clock_t *c, uint32_t now_ms, world_time_t *out) {
	(void)c; (void)now_ms; (void)out;
	return clock_ok;
}

uint8_t sys_socPlanned(const world_time_t *t, float *soc_percent) {
	(void)t;
	*soc_percent = plan_soc;
	return plan_ok;
}

uint8_t sys_infoDacSetVolts(float volts) {
	dac_volts = volts;
	return TRUE;
}

static void fill(vehicle_data_t *d, float soc, uint32_t now_ms) {
	vehicle_data_update(&d->current_speed, 15.0f, now_ms);
	vehicle_data_update(&d->battery_soc, soc, now_ms);
	vehicle_data_update(&d->mppt1_power, 300.0f, now_ms);
	vehicle_data_update(&d->mppt2_power, 300.0f, now_ms);
	vehicle_data_update(&d->mppt3_power, 300.0f, now_ms);
	vehicle_data_update(&d->aux_power, 50.0f, now_ms);
	vehicle_data_update(&d->gradient, 0.0f, now_ms);
	vehicle_data_update(&d->wind_speed, 0.0f, now_ms);
}

/* One optimizer run on fresh state with the given actual SOC against a 50 % plan. */
static float run(float soc) {
	vehicle_data_t d;
	float v = 0.0f;

	vehicle_data_init(&d);
	po_optimizer_init();
	fill(&d, soc, 1000U);
	po_optimizer_update(&d, 1000U);
	assert(po_optimizer_speed(&v));
	return v;
}

int main(void) {
	vehicle_data_t d;
	float v;
	float flat;
	float other;

	/* Flat, no wind, 20 m/s with the placeholder model: 20 * (28.8 + 17.658) / 0.95 W. */
	assert(po_solve_speed(978.06f, 0.0f, 0.0f, 10.0f, &v) && fabsf(v - 20.0f) < 0.01f);
	flat = v;
	/* A poor guess reaches the same root. */
	assert(po_solve_speed(978.06f, 0.0f, 0.0f, 500.0f, &v) && fabsf(v - 20.0f) < 0.01f);
	/* Uphill and headwind are slower, downhill is faster, for the same power. */
	assert(po_solve_speed(978.06f, 0.02f, 0.0f, 20.0f, &other) && other < flat);
	assert(po_solve_speed(978.06f, 0.0f, 5.0f, 20.0f, &other) && other < flat);
	assert(po_solve_speed(978.06f, -0.004f, 0.0f, 20.0f, &other) && other > flat);
	/* Steep downhill and no power still land inside the limits. */
	assert(po_solve_speed(100000.0f, 0.0f, 0.0f, 20.0f, &v) && v == PO_SPEED_MAX_MPS);
	assert(po_solve_speed(0.0f, 0.05f, 0.0f, 20.0f, &v) && v == PO_SPEED_MIN_MPS);
	assert(po_solve_speed(500.0f, -0.1f, 0.0f, 20.0f, &v) && v == PO_SPEED_MAX_MPS);
	assert(po_solve_speed(300.0f, -0.01f, -10.0f, 20.0f, &v) && v >= PO_SPEED_MIN_MPS && v <= PO_SPEED_MAX_MPS);
	assert(!po_solve_speed(NAN, 0.0f, 0.0f, 20.0f, &v));
	assert(!po_solve_speed(500.0f, 0.0f, INFINITY, 20.0f, &v));

	/* Above the plan drives faster than on it, below drives slower. */
	flat = run(50.0f);
	assert(run(52.0f) > flat && run(48.0f) < flat);
	assert(fabsf(dac_volts - run(48.0f) * 0.25f) < 0.001f);

	/* Nothing runs before a second has passed. */
	vehicle_data_init(&d);
	po_optimizer_init();
	fill(&d, 50.0f, 500U);
	po_optimizer_update(&d, 500U);
	assert(!po_optimizer_speed(&v));

	/* A stale input, a bad clock or a missing plan each drop the target and zero the display. */
	(void)run(50.0f);
	vehicle_data_init(&d);
	fill(&d, 50.0f, 1000U);
	po_optimizer_update(&d, 9000U);
	assert(!po_optimizer_speed(&v) && dac_volts == 0.0f);
	clock_ok = FALSE;
	fill(&d, 50.0f, 10000U);
	po_optimizer_update(&d, 10000U);
	assert(!po_optimizer_speed(&v));
	clock_ok = TRUE;
	plan_ok = FALSE;
	po_optimizer_update(&d, 11000U);
	assert(!po_optimizer_speed(&v));
	plan_ok = TRUE;
	fill(&d, 50.0f, 12000U);
	po_optimizer_update(&d, 12000U);
	assert(po_optimizer_speed(&v));

	printf("po_optimizer: all checks passed\n");
	return 0;
}
