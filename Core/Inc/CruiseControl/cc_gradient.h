/* Road gradient from the IMU, smoothed and written into vehicle_data. */

#ifndef CC_GRADIENT_H
#define CC_GRADIENT_H

#include "vehicle_data.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CC_GRADIENT_CAL_OK       0
#define CC_GRADIENT_CAL_ERR_IMU  1 /* not enough good IMU samples */
#define CC_GRADIENT_CAL_ERR_SAVE 2 /* calibrated, but the flash write or verify failed */

/* Loads the saved calibration from flash (zeros if none). Call after sys_flashInit and sys_imuInit. */
void cc_gradient_init(void);

/*
 * Call once per main loop pass. Reads the IMU, fuses gyro and accelerometer, and writes the
 * gradient to d->gradient (radians, uphill positive, with the read time in ms). A failed read
 * marks the gradient unhealthy and resets the filter.
 */
void cc_gradient_update(vehicle_data_t *d);

/*
 * Averages samples with the car still and on level ground, takes that as 0, and saves it to
 * flash. Blocks about half a second and refreshes the watchdog while it waits.
 * Returns a CC_GRADIENT_CAL_ code.
 */
int cc_gradient_calibrate(void);

#ifdef __cplusplus
}
#endif

#endif /* CC_GRADIENT_H */
