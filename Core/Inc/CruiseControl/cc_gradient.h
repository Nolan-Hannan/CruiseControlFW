/* Road gradient from the IMU's gravity vector, smoothed and written into vehicle_data. */

#ifndef CC_GRADIENT_H
#define CC_GRADIENT_H

#include "vehicle_data.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Clears the filter and the calibration offset. Call once at startup, after sys_imuInit. */
void cc_gradient_init(void);

/*
 * Call once per main loop pass. Reads the IMU, smooths the angle, and writes it to
 * d->gradient (radians, received time in ms). A failed read marks the gradient unhealthy.
 */
void cc_gradient_update(vehicle_data_t *d);

/* Takes the current angle as level (0). Run with the car on flat ground. TRUE on success. */
uint8_t cc_gradient_calibrate(void);

#ifdef __cplusplus
}
#endif

#endif /* CC_GRADIENT_H */
