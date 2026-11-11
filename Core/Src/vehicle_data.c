/* Measured inputs shared by cruise control and the power optimizer, each with health and age. */

#include "vehicle_data.h"
#include <stddef.h>

static void initPoint(data_point_t *p, uint16_t maxAgeMs) {
	p->value = 0.0f;
	p->source_healthy = FALSE;
	p->max_age_ms = maxAgeMs;
	p->last_updated_ms = 0U;
}

void vehicle_data_init(vehicle_data_t *d) {
	if (d == NULL) {
		return;
	}
	initPoint(&d->current_speed, VEHICLE_DATA_AGE_SPEED_MS);
	initPoint(&d->battery_soc, VEHICLE_DATA_AGE_SOC_MS);
	initPoint(&d->mppt_power, VEHICLE_DATA_AGE_MPPT_MS);
	initPoint(&d->aux_power, VEHICLE_DATA_AGE_AUX_MS);
	initPoint(&d->gradient, VEHICLE_DATA_AGE_GRADIENT_MS);
	initPoint(&d->wind_speed, VEHICLE_DATA_AGE_WIND_MS);
}

void vehicle_data_update(data_point_t *p, float value, uint32_t time_ms) {
	if (p == NULL) {
		return;
	}
	p->value = value;
	p->last_updated_ms = time_ms;
	p->source_healthy = TRUE;
}

void vehicle_data_set_error(data_point_t *p) {
	if (p != NULL) {
		p->source_healthy = FALSE;
	}
}

uint8_t data_point_valid(const data_point_t *p, uint32_t now_ms) {
	if (p == NULL || !p->source_healthy) {
		return FALSE;
	}
	/* Unsigned subtraction handles the millisecond counter wrapping. */
	return ((uint32_t)(now_ms - p->last_updated_ms) <= p->max_age_ms) ? TRUE : FALSE;
}

uint8_t validate_cc_data(const vehicle_data_t *d, uint32_t now_ms) {
	return (d != NULL && data_point_valid(&d->current_speed, now_ms)) ? TRUE : FALSE;
}

uint8_t validate_po_data(const vehicle_data_t *d, uint32_t now_ms) {
	if (d == NULL) {
		return FALSE;
	}
	return (data_point_valid(&d->current_speed, now_ms) &&
	        data_point_valid(&d->battery_soc, now_ms) &&
	        data_point_valid(&d->mppt_power, now_ms) &&
	        data_point_valid(&d->aux_power, now_ms) &&
	        data_point_valid(&d->gradient, now_ms) &&
	        data_point_valid(&d->wind_speed, now_ms)) ? TRUE : FALSE;
}
