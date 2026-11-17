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
static const uint32_t clock_sec = 1800000000U; /* what the wall clock reads */
static uint8_t clock_ok = TRUE;
static uint8_t plan_ok = TRUE;
static uint32_t plan_first_sec = 0U;          /* the plan covers first..last */
static uint32_t plan_last_sec = 0xFFFFFFFFU;
static float plan_soc = 50.0f;                /* planned SOC at clock_sec, percent */
static float plan_drop_pct = 0.0f;            /* planned SOC drop per interval, percent */
static float dac_volts = -1.0f;

#define PERIOD_S ((float)PO_PERIOD_MS / 1000.0f)
/* 1 % of the pack is PO_PACK_ENERGY_WH * 36 J; spent over one interval that is this many watts. */
#define W_PER_PCT (PO_PACK_ENERGY_WH * 36.0f / PERIOD_S)

uint8_t sys_wallClockGet(const wall_clock_t *c, uint32_t now_ms, world_time_t *out) {
	(void)c; (void)now_ms;
	out->total_sec = clock_sec;
	return clock_ok;
}

void sys_worldTimeFromEpoch(uint32_t total_sec, world_time_t *out) {
	out->total_sec = total_sec;
}

/* A straight line through plan_soc at clock_sec. */
uint8_t sys_socPlanned(const world_time_t *t, float *soc_percent) {
	*soc_percent = plan_soc - plan_drop_pct * (float)(int32_t)(t->total_sec - clock_sec) / PERIOD_S;
	return plan_ok && t->total_sec >= plan_first_sec && t->total_sec <= plan_last_sec;
}

uint8_t sys_infoDacSetVolts(float volts) {
	dac_volts = volts;
	return TRUE;
}

/* 900 W solar, 50 W aux, flat, no wind. */
static void fill(vehicle_data_t *d, float speed, float soc, uint32_t now_ms) {
	vehicle_data_update(&d->current_speed, speed, now_ms);
	vehicle_data_update(&d->battery_soc, soc, now_ms);
	vehicle_data_update(&d->mppt1_power, 300.0f, now_ms);
	vehicle_data_update(&d->mppt2_power, 300.0f, now_ms);
	vehicle_data_update(&d->mppt3_power, 300.0f, now_ms);
	vehicle_data_update(&d->aux_power, 50.0f, now_ms);
	vehicle_data_update(&d->gradient, 0.0f, now_ms);
	vehicle_data_update(&d->wind_speed, 0.0f, now_ms);
}

/* One optimizer run on fresh state: car at speed, actual SOC soc, against the 50 % plan. */
static float run(float speed, float soc) {
	vehicle_data_t d;
	float v = 0.0f;

	vehicle_data_init(&d);
	po_optimizer_init();
	fill(&d, speed, soc, PO_PERIOD_MS);
	po_optimizer_update(&d, PO_PERIOD_MS);
	assert(po_optimizer_speed(&v));
	return v;
}

static uint8_t near(float a, float b) {
	return fabsf(a - b) < 0.02f;
}

int main(void) {
	const float up = PO_MAX_ACCEL_MPS2 * PERIOD_S;
	const float down = PO_MAX_DECEL_MPS2 * PERIOD_S;
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

	/* On a level plan the budget is solar minus aux, 850 W. Starting at that speed the clamp is idle. */
	assert(po_solve_speed(850.0f, 0.0f, 0.0f, 20.0f, &flat));
	assert(near(run(flat, 50.0f), flat));
	/* Above the plan drives faster than on it, below drives slower. */
	assert(run(flat, 51.0f) > flat + 0.1f && run(flat, 49.0f) < flat - 0.1f);

	/* Planned battery spend: a plan that drops 128.06 W worth per interval lifts 850 W to the 20 m/s point. */
	plan_drop_pct = 128.06f / W_PER_PCT;
	assert(near(run(20.0f, 50.0f), 20.0f));
	/* At the first point of a day there is no previous point; the next one gives the same slope. */
	plan_first_sec = clock_sec;
	assert(near(run(20.0f, 50.0f), 20.0f));
	/* With no neighbour at all the planned spend is 0 W. */
	plan_last_sec = clock_sec;
	assert(near(run(flat, 50.0f), flat));
	plan_first_sec = 0U;
	plan_last_sec = 0xFFFFFFFFU;
	/* A plan that charges takes power away. */
	plan_drop_pct = -128.06f / W_PER_PCT;
	assert(po_solve_speed(850.0f - 128.06f, 0.0f, 0.0f, 20.0f, &other) && near(run(other, 50.0f), other));
	plan_drop_pct = 0.0f;

	/* Acceleration clamp: the target stays within one interval of accel / decel of the current speed. */
	assert(flat > 15.0f + up && near(run(15.0f, 50.0f), 15.0f + up));
	/* The info DAC shows the clamped target, not the solved speed. */
	assert(fabsf(dac_volts - (15.0f + up) * 0.25f) < 0.001f);
	assert(flat < 25.0f - down && near(run(25.0f, 50.0f), 25.0f - down));
	/* Far below the plan the solve is the minimum speed; the car is only slowed by one interval of decel. */
	assert(near(run(15.0f, 30.0f), 15.0f - down));
	/* A slow car is led up from where it is, even below the minimum speed. */
	assert(near(run(2.0f, 50.0f), 2.0f + up));
	/* The top speed limit beats the decel limit. */
	assert(run(40.0f, 90.0f) == PO_SPEED_MAX_MPS);

	/* The main loop owns timing; every explicit call computes, even early in uptime. */
	vehicle_data_init(&d);
	po_optimizer_init();
	assert(!po_optimizer_speed(&v));
	fill(&d, flat, 50.0f, 1U);
	po_optimizer_update(&d, 1U);
	assert(po_optimizer_speed(&v) && near(v, flat));
	fill(&d, flat, 51.0f, 2U);
	po_optimizer_update(&d, 2U);
	assert(po_optimizer_speed(&v) && v > flat + 0.1f);

	/* A stale input, a bad clock or a missing plan each drop the target and zero the display. */
	(void)run(flat, 50.0f);
	vehicle_data_init(&d);
	fill(&d, flat, 50.0f, PO_PERIOD_MS);
	po_optimizer_update(&d, 3U * PO_PERIOD_MS);
	assert(!po_optimizer_speed(&v) && dac_volts == 0.0f);
	clock_ok = FALSE;
	fill(&d, flat, 50.0f, 4U * PO_PERIOD_MS);
	po_optimizer_update(&d, 4U * PO_PERIOD_MS);
	assert(!po_optimizer_speed(&v));
	clock_ok = TRUE;
	plan_ok = FALSE;
	fill(&d, flat, 50.0f, 5U * PO_PERIOD_MS);
	po_optimizer_update(&d, 5U * PO_PERIOD_MS);
	assert(!po_optimizer_speed(&v));
	plan_ok = TRUE;
	fill(&d, flat, 50.0f, 6U * PO_PERIOD_MS);
	po_optimizer_update(&d, 6U * PO_PERIOD_MS);
	assert(po_optimizer_speed(&v) && dac_volts > 0.0f);

	printf("po_optimizer: all checks passed\n");
	return 0;
}
