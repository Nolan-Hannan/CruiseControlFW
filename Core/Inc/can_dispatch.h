/* Main-loop CAN routing. Device handlers decode payloads into vehicle_data. */

#ifndef CAN_DISPATCH_H
#define CAN_DISPATCH_H

#include "sys_can.h"
#include "vehicle_data.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CAN_DISPATCH_MAX_FRAMES 5U

typedef void (*canHandler_t)(const sys_canFrame_t *f, vehicle_data_t *d, uint8_t arg);

// Dispatcher struct
typedef struct {
    sys_canBus_t bus;
    uint32_t id;
    uint8_t ext;             /* FALSE: standard ID, TRUE: extended ID */
    canHandler_t handler;
    uint8_t arg;             /* Handler-specific, for example which MPPT. */
} canRoute_t;

/*
 * Call once before processing. The route array must remain alive and unchanged;
 * normally use a static const canRoute_t array. NULL/0 selects no routes.
 * Handlers must finish quickly, validate DLC and device status, and timestamp
 * accepted measurements with f->rxTimeMs rather than the processing time.
 */
void can_dispatch_init(const canRoute_t *routes, size_t route_count);

/*
 * Main loop only. Consume at most five frames TOTAL across LV and HV, alternating
 * buses even across calls. If one bus is empty, service the other.
 * The first matching route wins; a NULL handler discards that frame.
 * Unknown frames are discarded and count toward the budget too.
 * Returns frames consumed (not handlers called). NULL data consumes nothing.
 */
uint8_t can_dispatch_process(vehicle_data_t *d);

#ifdef __cplusplus
}
#endif

#endif /* CAN_DISPATCH_H */
