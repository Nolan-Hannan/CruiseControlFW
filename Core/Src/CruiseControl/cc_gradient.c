/* Road gradient from the IMU's gravity vector, smoothed and written into vehicle_data. */

#include "cc_gradient.h"
#include "sys_imu.h"
#include "sys_time.h"
#include <math.h>
#include <stddef.h>

/* Exponential smoothing weight for each new sample. Larger reacts faster, smaller is smoother. */
#define GRADIENT_ALPHA 0.1f

static float filtered_gradient;  /* radians */
static float calibration_offset; /* radians */
static uint8_t filter_seeded;

/*
 * Tilt from gravity. Placeholder axes until the IMU mounting is confirmed:
 * accX forward, accZ up, so nose up (uphill) is positive.
 */
static float rawAngle(const sys_imuData_t *imu) {
	return atan2f(imu->accX, imu->accZ);
}

void cc_gradient_init(void) {
	filtered_gradient = 0.0f;
	calibration_offset = 0.0f;
	filter_seeded = FALSE;
}

void cc_gradient_update(vehicle_data_t *d) {
	if (d == NULL) {
		return;
	}

	sys_imuData_t imu;
	if (!sys_imuRead(&imu)) {
		vehicle_data_set_error(&d->gradient);
		return;
	}
	const uint32_t time_ms = sys_getTimeMs();

	float corrected = rawAngle(&imu) - calibration_offset;

	/* average over a few cycles. exponential smoothing */
	if (filter_seeded) {
		filtered_gradient = (1.0f - GRADIENT_ALPHA) * filtered_gradient + GRADIENT_ALPHA * corrected;
	} else {
		filtered_gradient = corrected; /* start from the first sample, not from 0 */
		filter_seeded = TRUE;
	}

	vehicle_data_update(&d->gradient, filtered_gradient, time_ms);
}

uint8_t cc_gradient_calibrate(void) {
	sys_imuData_t imu;
	if (!sys_imuRead(&imu)) {
		return FALSE;
	}
	calibration_offset = rawAngle(&imu);
	filter_seeded = FALSE; /* re-seed from the next corrected sample */
	return TRUE;
}
