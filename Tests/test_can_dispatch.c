/*
 * Host check with simulated LV/HV receive queues. From the repo root:
 *   cc -std=c11 -Wall -Wextra -Werror -pedantic -ICore/Inc -ICore/Inc/SysLib \
 *      Tests/test_can_dispatch.c Core/Src/can_dispatch.c Core/Src/vehicle_data.c \
 *      -o /tmp/test_can_dispatch && /tmp/test_can_dispatch
 */

#include "can_dispatch.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define QUEUE_SIZE 16U

static sys_canFrame_t queues[SYS_CAN_BUS_COUNT][QUEUE_SIZE];
static unsigned heads[SYS_CAN_BUS_COUNT];
static unsigned tails[SYS_CAN_BUS_COUNT];
static unsigned handled;
static uint8_t args[2U * QUEUE_SIZE];

uint8_t sys_canRead(sys_canBus_t bus, sys_canFrame_t *frame) {
	assert(bus < SYS_CAN_BUS_COUNT);
	if (tails[bus] == heads[bus]) {
		return FALSE;
	}
	*frame = queues[bus][tails[bus]++];
	return TRUE;
}

static void enqueue(sys_canBus_t bus, uint32_t id, uint8_t ext, uint32_t time_ms) {
	assert(heads[bus] < QUEUE_SIZE);
	queues[bus][heads[bus]++] = (sys_canFrame_t) {
		.id = id, .ext = ext, .dlc = 1U, .data = {42U}, .rxTimeMs = time_ms
	};
}

static void handle(const sys_canFrame_t *f, vehicle_data_t *d, uint8_t arg) {
	assert(handled < sizeof(args));
	args[handled++] = arg;
	vehicle_data_update(&d->current_speed, (float)f->data[0], f->rxTimeMs);
}

/* Test IDs only; these are not device protocol definitions. */
static const canRoute_t routes[] = {
	{SYS_CAN_LV, 0x123U, FALSE, handle, 1U},
	{SYS_CAN_HV, 0x123U, FALSE, handle, 2U},
	{SYS_CAN_LV, 0x123U, TRUE,  handle, 3U},
	{SYS_CAN_HV, 0x18ABCDEFU, TRUE, handle, 4U},
	{SYS_CAN_LV, 0x124U, FALSE, NULL, 0U},
	{SYS_CAN_LV, 0x124U, FALSE, handle, 5U},
	{SYS_CAN_LV, 0x123U, FALSE, handle, 6U}
};

static void reset(vehicle_data_t *d) {
	memset(heads, 0, sizeof(heads));
	memset(tails, 0, sizeof(tails));
	handled = 0U;
	vehicle_data_init(d);
	can_dispatch_init(routes, sizeof(routes) / sizeof(routes[0]));
}

int main(void) {
	vehicle_data_t d;

	/* Five TOTAL per pass; the odd budget must not always favor LV. */
	reset(&d);
	for (unsigned i = 0U; i < 6U; i++) {
		enqueue(SYS_CAN_LV, 0x123U, FALSE, i);
		enqueue(SYS_CAN_HV, 0x123U, FALSE, i);
	}
	assert(can_dispatch_process(&d) == 5U);
	assert(tails[SYS_CAN_LV] == 3U && tails[SYS_CAN_HV] == 2U);
	assert(can_dispatch_process(&d) == 5U);
	assert(tails[SYS_CAN_LV] == 5U && tails[SYS_CAN_HV] == 5U);
	for (unsigned i = 0U; i < handled; i++) {
		assert(args[i] == (i % 2U == 0U ? 1U : 2U));
	}
	assert(can_dispatch_process(&d) == 2U);
	assert(can_dispatch_process(&d) == 0U);

	/* Match bus, ID and ext independently; preserve payload and receive time. */
	reset(&d);
	enqueue(SYS_CAN_HV, 0x123U, TRUE, 10U); /* wrong ext for this bus */
	enqueue(SYS_CAN_HV, 0x125U, FALSE, 20U); /* unknown ID */
	enqueue(SYS_CAN_HV, 0x18ABCDEFU, TRUE, 30U);
	enqueue(SYS_CAN_LV, 0x18ABCDEFU, TRUE, 40U); /* wrong bus */
	enqueue(SYS_CAN_LV, 0x123U, TRUE, 50U);
	assert(can_dispatch_process(&d) == 5U);
	assert(handled == 2U && args[0] == 3U && args[1] == 4U);
	assert(d.current_speed.value == 42.0f && d.current_speed.last_updated_ms == 30U);
	assert(data_point_valid(&d.current_speed, 30U));
	assert(!data_point_valid(&d.current_speed, 30U + d.current_speed.max_age_ms + 1U));

	/* One busy bus can use the whole budget, including for unknown traffic. */
	for (unsigned bus = 0U; bus < SYS_CAN_BUS_COUNT; bus++) {
		reset(&d);
		for (unsigned i = 0U; i < 6U; i++) {
			enqueue((sys_canBus_t)bus, 0x777U, FALSE, 100U);
		}
		assert(can_dispatch_process(&d) == 5U && handled == 0U);
		assert(tails[bus] == 5U);
		assert(!d.current_speed.source_healthy);
		assert(can_dispatch_process(&d) == 1U);
	}

	/* NULL handlers discard; duplicate routes never invoke a second handler. */
	reset(&d);
	enqueue(SYS_CAN_LV, 0x124U, FALSE, 100U);
	enqueue(SYS_CAN_LV, 0x123U, FALSE, 101U);
	assert(can_dispatch_process(&d) == 2U && handled == 1U && args[0] == 1U);

	/* An empty table drains frames without inventing measurements. */
	reset(&d);
	can_dispatch_init(NULL, 0U);
	enqueue(SYS_CAN_LV, 0x123U, FALSE, 100U);
	assert(can_dispatch_process(NULL) == 0U && tails[SYS_CAN_LV] == 0U);
	assert(can_dispatch_process(&d) == 1U && handled == 0U);
	assert(!d.current_speed.source_healthy);
	can_dispatch_init(routes, 0U);
	enqueue(SYS_CAN_HV, 0x123U, FALSE, 100U);
	assert(can_dispatch_process(&d) == 1U && handled == 0U);

	puts("can_dispatch: all checks passed");
	return 0;
}
