/* Main-loop CAN routing. Keep payload decoding in the device handlers. */

#include "can_dispatch.h"

// Store dispatcher state. Read LV first, and then HV
static const canRoute_t *routes;
static size_t route_count;
static sys_canBus_t next_bus = SYS_CAN_LV;

static sys_canBus_t otherBus(sys_canBus_t bus) {
	return bus == SYS_CAN_LV ? SYS_CAN_HV : SYS_CAN_LV;
}

void can_dispatch_init(const canRoute_t *table, size_t count) {
	routes = table;
	route_count = table != NULL ? count : 0U;
	next_bus = SYS_CAN_LV;
}

uint8_t can_dispatch_process(vehicle_data_t *d) {
	uint8_t consumed = 0U;
	sys_canFrame_t frame;

	if (d == NULL) {
		return 0U;        // if no data, return function w/o consuming frames
	}
	while (consumed < CAN_DISPATCH_MAX_FRAMES) {     // start w/ 5 frames, modify if needed
		sys_canBus_t bus = next_bus;
		if (!sys_canRead(bus, &frame)) {
			bus = otherBus(bus);
			if (!sys_canRead(bus, &frame)) {
				break;
			}
		}
		next_bus = otherBus(bus);
		consumed++;

		// Search through the frame's route and find a match
		for (size_t i = 0U; i < route_count; i++) {
			const canRoute_t *route = &routes[i];
			if (route->bus == bus && route->id == frame.id && route->ext == frame.ext) {
				if (route->handler != NULL) {
					route->handler(&frame, d, route->arg);
				}
				break;
			}
		}
	}
	return consumed;
}
