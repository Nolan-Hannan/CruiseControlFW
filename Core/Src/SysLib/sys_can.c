/* CAN1 (LV) and CAN2 (HV) bus. */

#include "sys_can.h"
#include "sys_time.h"
#include "main.h"
#include <stddef.h>
#include <string.h>

#define RING_SIZE 32U
#define RING_MASK (RING_SIZE - 1U)
#define SLAVE_START_BANK 14U

extern CAN_HandleTypeDef hcan1;
extern CAN_HandleTypeDef hcan2;

typedef struct {
	sys_canFrame_t frames[RING_SIZE];
	volatile uint8_t head;
	volatile uint8_t tail;
	volatile uint32_t dropped;
} ring_t;

static ring_t rings[SYS_CAN_BUS_COUNT];

static sys_canBus_t busOf(CAN_HandleTypeDef *hcan) {
	return hcan == &hcan1 ? SYS_CAN_LV : SYS_CAN_HV;
}

static uint8_t setAcceptAll(CAN_HandleTypeDef *hcan, uint32_t bank) {
	CAN_FilterTypeDef filter = {0};

	filter.FilterBank = bank;
	filter.FilterMode = CAN_FILTERMODE_IDMASK;
	filter.FilterScale = CAN_FILTERSCALE_32BIT;
	filter.FilterFIFOAssignment = CAN_FILTER_FIFO0;
	filter.FilterActivation = ENABLE;
	filter.SlaveStartFilterBank = SLAVE_START_BANK;
	return HAL_CAN_ConfigFilter(hcan, &filter) == HAL_OK;
}

static uint8_t startBus(CAN_HandleTypeDef *hcan) {
	if (HAL_CAN_Start(hcan) != HAL_OK) {
		return FALSE;
	}
	return HAL_CAN_ActivateNotification(hcan, CAN_IT_RX_FIFO0_MSG_PENDING |
	                                          CAN_IT_RX_FIFO1_MSG_PENDING) == HAL_OK;
}

uint8_t sys_canInit(void) {
	if (!setAcceptAll(&hcan1, 0U) || !setAcceptAll(&hcan2, SLAVE_START_BANK)) {
		return FALSE;
	}
	return startBus(&hcan1) && startBus(&hcan2);
}

uint8_t sys_canRead(sys_canBus_t bus, sys_canFrame_t *frame) {
	if (bus >= SYS_CAN_BUS_COUNT || frame == NULL) {
		return FALSE;
	}
	ring_t *ring = &rings[bus];
	if (ring->tail == ring->head) {
		return FALSE;
	}
	*frame = ring->frames[ring->tail];
	__DMB();
	ring->tail = (uint8_t)((ring->tail + 1U) & RING_MASK);
	return TRUE;
}

uint8_t sys_canPending(sys_canBus_t bus) {
	if (bus >= SYS_CAN_BUS_COUNT) {
		return 0U;
	}
	return (uint8_t)((rings[bus].head - rings[bus].tail) & RING_MASK);
}

uint32_t sys_canDropped(sys_canBus_t bus) {
	return bus < SYS_CAN_BUS_COUNT ? rings[bus].dropped : 0U;
}

/* All CAN interrupts share one priority, so each ring has a single producer at a time. */
static void drain(CAN_HandleTypeDef *hcan, uint32_t fifo) {
	ring_t *ring = &rings[busOf(hcan)];
	CAN_RxHeaderTypeDef rxHeader;
	uint8_t rxData[8];

	while (HAL_CAN_GetRxFifoFillLevel(hcan, fifo) > 0U) {
		if (HAL_CAN_GetRxMessage(hcan, fifo, &rxHeader, rxData) != HAL_OK) {
			break;
		}
		uint8_t nextHead = (uint8_t)((ring->head + 1U) & RING_MASK);
		if (nextHead == ring->tail) {
			ring->dropped++;
			continue;
		}
		sys_canFrame_t *frame = &ring->frames[ring->head];
		frame->ext = (rxHeader.IDE == CAN_ID_EXT) ? TRUE : FALSE;
		frame->id = frame->ext ? rxHeader.ExtId : rxHeader.StdId;
		frame->dlc = (rxHeader.DLC > 8U) ? 8U : (uint8_t)rxHeader.DLC;
		memcpy(frame->data, rxData, sizeof(frame->data));
		frame->rxTimeMs = sys_getTimeMs();
		__DMB();
		ring->head = nextHead;
	}
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan) {
	drain(hcan, CAN_RX_FIFO0);
}

void HAL_CAN_RxFifo1MsgPendingCallback(CAN_HandleTypeDef *hcan) {
	drain(hcan, CAN_RX_FIFO1);
}
