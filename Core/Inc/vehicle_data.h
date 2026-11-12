/* Measured inputs shared by cruise control and the power optimizer, each with health and age. */

#ifndef VEHICLE_DATA_H
#define VEHICLE_DATA_H

#include "defines.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Recency requirements in milliseconds. Placeholders until the CAN rates are confirmed. */
#define VEHICLE_DATA_AGE_SPEED_MS    200U
#define VEHICLE_DATA_AGE_SOC_MS      1000U
#define VEHICLE_DATA_AGE_MPPT_MS     2000U
#define VEHICLE_DATA_AGE_AUX_MS      2000U
#define VEHICLE_DATA_AGE_GRADIENT_MS 500U
#define VEHICLE_DATA_AGE_WIND_MS     1000U

typedef struct {
    float value;
    uint8_t source_healthy;     /* FALSE at boot and after an error code, TRUE after a good update */
    uint16_t max_age_ms;
    uint32_t last_updated_ms;
} data_point_t;

typedef struct {
    data_point_t current_speed; /* m/s */
    data_point_t battery_soc;   /* percent, 0 to 100 */
    data_point_t mppt1_power;   /* W, one point per MPPT */
    data_point_t mppt2_power;   /* W */
    data_point_t mppt3_power;   /* W */
    data_point_t aux_power;     /* W */
    data_point_t gradient;      /* radians */
    data_point_t wind_speed;    /* m/s */
} vehicle_data_t;

/* Clears every point to unhealthy and sets its recency requirement. */
void vehicle_data_init(vehicle_data_t *d);

/* time_ms is when the data arrived, for example sys_canFrame_t.rxTimeMs. */
void vehicle_data_update(data_point_t *p, float value, uint32_t time_ms);

/* Marks the source unhealthy and keeps the old value. The next good update recovers it. */
void vehicle_data_set_error(data_point_t *p);

/* TRUE if the source is healthy and the data is no older than its recency requirement. */
uint8_t data_point_valid(const data_point_t *p, uint32_t now_ms);

/*
 * Single writer, the main loop. Per pass: ingest new data first, then take now_ms
 * once (sys_getTimeMs), then validate and compute.
 */

/* Cruise control: current speed only. */
uint8_t validate_cc_data(const vehicle_data_t *d, uint32_t now_ms);

/* Power optimizer: every point. */
uint8_t validate_po_data(const vehicle_data_t *d, uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* VEHICLE_DATA_H */
