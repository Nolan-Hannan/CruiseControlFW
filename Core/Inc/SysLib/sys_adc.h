/* ADC1: raw throttle and CC throttle output. */

#ifndef SYS_ADC_H
#define SYS_ADC_H

#include "defines.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Order matches the CubeMX ADC1 rank order. */
typedef enum {
    SYS_ADC_RAW,        /* PC0, pedal before the mux */
    SYS_ADC_CC_OUT,     /* PC1, mux output */
    SYS_ADC_CHANNEL_COUNT
} sys_adcChannel_t;

/* Returns FALSE if the read fails or the ADC is not fitted. */
uint8_t sys_adcReadVolts(sys_adcChannel_t channel, float *volts);

#ifdef __cplusplus
}
#endif

#endif /* SYS_ADC_H */
