/* ADC1: raw throttle and CC throttle output. */

#include "sys_adc.h"
#include "main.h"
#include <stddef.h>

/* Set to 0 when the ADC input resistors are not populated. */
#define ADC_FITTED 1

#define ADC_AVERAGES   8U
#define ADC_VREF_V     3.3f
#define ADC_MAX_CODE   4095.0f
#define ADC_TIMEOUT_MS 10U

extern ADC_HandleTypeDef hadc1;

static uint8_t scan(uint32_t *sums) {
	uint8_t i;

	if (HAL_ADC_Start(&hadc1) != HAL_OK) {
		return FALSE;
	}
	for (i = 0U; i < (uint8_t)SYS_ADC_CHANNEL_COUNT; i++) {
		if (HAL_ADC_PollForConversion(&hadc1, ADC_TIMEOUT_MS) != HAL_OK) {
			(void)HAL_ADC_Stop(&hadc1);
			return FALSE;
		}
		sums[i] += HAL_ADC_GetValue(&hadc1);
	}
	return HAL_ADC_Stop(&hadc1) == HAL_OK;
}

uint8_t sys_adcReadVolts(sys_adcChannel_t channel, float *volts) {
	uint32_t sums[SYS_ADC_CHANNEL_COUNT] = { 0U };
	uint8_t i;

	if (!ADC_FITTED || channel >= SYS_ADC_CHANNEL_COUNT || volts == NULL) {
		return FALSE;
	}
	for (i = 0U; i < ADC_AVERAGES; i++) {
		if (!scan(sums)) {
			return FALSE;
		}
	}
	*volts = (float)sums[channel] / (float)ADC_AVERAGES * ADC_VREF_V / ADC_MAX_CODE;
	return TRUE;
}
