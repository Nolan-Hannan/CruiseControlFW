/* BMI323, SPI. Gradient and acceleration. */

#ifndef SYS_IMU_H
#define SYS_IMU_H

#include "defines.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int16_t accX, accY, accZ;
    int16_t gyrX, gyrY, gyrZ;
    int16_t temp;
} sys_imuRaw_t;

/* Sensor axes: acceleration in g, rotation in deg/s, temperature in degrees C. */
typedef struct {
    float accX, accY, accZ;
    float gyrX, gyrY, gyrZ;
    float tempC;
} sys_imuData_t;

uint8_t sys_imuInit(void);

/* Both reads return FALSE until the sensors have a valid sample. */
uint8_t sys_imuReadRaw(sys_imuRaw_t *raw);
uint8_t sys_imuRead(sys_imuData_t *data);

#ifdef __cplusplus
}
#endif

#endif /* SYS_IMU_H */
