/* Power optimizer: picks a target power from planned versus actual SOC, then the speed that uses it. */

#ifndef PO_OPTIMIZER_H
#define PO_OPTIMIZER_H

#include "vehicle_data.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Speed limits the optimizer may ask for, m/s. Placeholders until the team sets them. */
#define PO_SPEED_MIN_MPS 8.0f
#define PO_SPEED_MAX_MPS 29.0f

/* No target, PI integral cleared, info DAC at 0 V. */
void po_optimizer_init(void);

/*
 * Call once per main loop pass; it does its work once a second. Needs every power optimizer
 * input valid, a valid wall clock and a planned SOC for the current time. Otherwise the
 * target becomes invalid and the info DAC goes to 0 V.
 */
void po_optimizer_update(const vehicle_data_t *d, uint32_t now_ms);

/* Target speed for cruise control. Returns TRUE if the last update produced one. */
uint8_t po_optimizer_speed(float *speed_mps);

/*
 * Speed at which the car draws power_w, by Newton-Raphson with a bisection step when
 * Newton leaves the bracket. The result is held to PO_SPEED_MIN_MPS..PO_SPEED_MAX_MPS.
 * gradient_rad is uphill positive, wind_mps is headwind positive.
 * Returns FALSE for a non-finite input or no convergence.
 */
uint8_t po_solve_speed(float power_w, float gradient_rad, float wind_mps, float guess_mps,
                       float *speed_mps);

#ifdef __cplusplus
}
#endif

#endif /* PO_OPTIMIZER_H */
