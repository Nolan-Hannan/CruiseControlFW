/* Road gradient from the IMU, smoothed and written into vehicle_data. */

#include "cc_gradient.h"
#include "main.h"
#include "sys_imu.h"
#include "sys_time.h"
#include "sys_flash.h"
#include "sys_crc.h"
#include "eep_addr.h"
#include <math.h>
#include <stddef.h>
#include <string.h>

extern IWDG_HandleTypeDef hiwdg;

/*
 * Complementary filter time constant. The gyro is trusted for changes faster than this and the
 * accelerometer for slower ones, so forward acceleration shorter than this barely moves the
 * estimate. This is also the exponential smoothing on the accelerometer angle.
 */
#define GRADIENT_TAU_S      2.0f
#define GRADIENT_MAX_DT_S   0.1f  /* a longer gap between good samples restarts the filter */

#define CAL_SAMPLES         50U
#define CAL_SPACING_MS      10U   /* the IMU makes a new sample every 10 ms */
#define CAL_MAX_ATTEMPTS    (CAL_SAMPLES * 2U)

#define CAL_ID              "IMUC"
#define CAL_VERSION         0x01U

#define DEG_TO_RAD          (3.14159265f / 180.0f)

/*
 * Placeholder axes until the IMU mounting is confirmed: accX forward, accY left, accZ up.
 * Nose up (uphill) is positive pitch, which is a negative rotation about Y.
 */
#define ACC_FORWARD(imu)    ((imu)->accX)
#define ACC_UP(imu)         ((imu)->accZ)
#define GYRO_PITCH(imu)     (-(imu)->gyrY)

static float pitch_offset;    /* radians, saved in flash */
static float gyro_bias;       /* deg/s, saved in flash */

static float filtered_gradient;
static uint8_t filter_seeded;
static uint32_t last_sample_us;

static float accelAngle(const sys_imuData_t *imu) {
	return atan2f(ACC_FORWARD(imu), ACC_UP(imu)) - pitch_offset;
}

static void resetFilter(void) {
	filtered_gradient = 0.0f;
	filter_seeded = FALSE;
}

static uint32_t le32(const uint8_t *p) {
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void putLe32(uint8_t *p, uint32_t v) {
	p[0] = (uint8_t)v;
	p[1] = (uint8_t)(v >> 8);
	p[2] = (uint8_t)(v >> 16);
	p[3] = (uint8_t)(v >> 24);
}

static uint8_t loadCalibration(void) {
	uint8_t rec[EEP_IMU_CAL_REC_SIZE];
	float pitch;
	float bias;

	if (!sys_flashRead(EEP_IMU_CAL_ADDR, rec, sizeof(rec)) ||
	    memcmp(&rec[EEP_IMU_ID_OFF], CAL_ID, 4U) != 0 ||
	    rec[EEP_IMU_VERSION_OFF] != CAL_VERSION ||
	    le32(&rec[EEP_IMU_CRC_OFF]) != sys_crc32(0U, rec, EEP_IMU_CRC_OFF)) {
		return FALSE;
	}
	memcpy(&pitch, &rec[EEP_IMU_PITCH_OFF], sizeof(pitch));
	memcpy(&bias, &rec[EEP_IMU_GYRO_BIAS_OFF], sizeof(bias));
	if (!isfinite(pitch) || !isfinite(bias)) {
		return FALSE;
	}
	pitch_offset = pitch;
	gyro_bias = bias;
	return TRUE;
}

static uint8_t saveCalibration(uint8_t samples) {
	uint8_t rec[EEP_IMU_CAL_REC_SIZE];
	uint8_t check[EEP_IMU_CAL_REC_SIZE];

	memset(rec, 0xFF, sizeof(rec)); /* unused bytes are 0xFF */
	memcpy(&rec[EEP_IMU_ID_OFF], CAL_ID, 4U);
	rec[EEP_IMU_VERSION_OFF] = CAL_VERSION;
	rec[EEP_IMU_SAMPLES_OFF] = samples;
	memcpy(&rec[EEP_IMU_PITCH_OFF], &pitch_offset, sizeof(pitch_offset));
	memcpy(&rec[EEP_IMU_GYRO_BIAS_OFF], &gyro_bias, sizeof(gyro_bias));
	putLe32(&rec[EEP_IMU_CRC_OFF], sys_crc32(0U, rec, EEP_IMU_CRC_OFF));

	/* Write erases and rewrites the page, so this needs no separate sector erase. */
	return sys_flashWriteBlock(EEP_IMU_CAL_ADDR, rec, sizeof(rec)) &&
	       sys_flashRead(EEP_IMU_CAL_ADDR, check, sizeof(check)) &&
	       memcmp(rec, check, sizeof(rec)) == 0;
}

void cc_gradient_init(void) {
	pitch_offset = 0.0f;
	gyro_bias = 0.0f;
	(void)loadCalibration(); /* stays at zero if nothing valid is stored */
	resetFilter();
}

void cc_gradient_update(vehicle_data_t *d) {
	sys_imuData_t imu;

	if (d == NULL) {
		return;
	}
	if (!sys_imuRead(&imu)) {
		/* Drop the old average so stale data never blends into the next good reading. */
		resetFilter();
		vehicle_data_set_error(&d->gradient);
		return;
	}
	const uint32_t now_us = sys_getTimeUs();
	const uint32_t time_ms = sys_getTimeMs();
	const float accel_angle = accelAngle(&imu);
	const float dt = (float)(uint32_t)(now_us - last_sample_us) * 1.0e-6f;
	last_sample_us = now_us;

	if (!filter_seeded || dt <= 0.0f || dt > GRADIENT_MAX_DT_S) {
		filtered_gradient = accel_angle; /* start from the accelerometer, not from 0 */
		filter_seeded = TRUE;
	} else {
		/* average over a few cycles. exponential smoothing, with the gyro carrying fast changes */
		const float gyro_rate = (GYRO_PITCH(&imu) - gyro_bias) * DEG_TO_RAD; /* rad/s */
		const float k = GRADIENT_TAU_S / (GRADIENT_TAU_S + dt);
		filtered_gradient = k * (filtered_gradient + gyro_rate * dt) + (1.0f - k) * accel_angle;
	}

	vehicle_data_update(&d->gradient, filtered_gradient, time_ms);
}

int cc_gradient_calibrate(void) {
	sys_imuData_t imu;
	float sum_forward = 0.0f;
	float sum_up = 0.0f;
	float sum_rate = 0.0f;
	uint8_t good = 0U;
	uint32_t attempts = 0U;

	while (good < CAL_SAMPLES && attempts < CAL_MAX_ATTEMPTS) {
		attempts++;
		HAL_IWDG_Refresh(&hiwdg);
		HAL_Delay(CAL_SPACING_MS);
		if (sys_imuRead(&imu)) {
			sum_forward += ACC_FORWARD(&imu);
			sum_up += ACC_UP(&imu);
			sum_rate += GYRO_PITCH(&imu);
			good++;
		}
	}
	HAL_IWDG_Refresh(&hiwdg);
	if (good < CAL_SAMPLES) {
		return CC_GRADIENT_CAL_ERR_IMU;
	}

	/* Average the gravity vector, then take one angle, rather than averaging angles. */
	pitch_offset = atan2f(sum_forward, sum_up);
	gyro_bias = sum_rate / (float)good;
	resetFilter();

	return saveCalibration(good) ? CC_GRADIENT_CAL_OK : CC_GRADIENT_CAL_ERR_SAVE;
}
