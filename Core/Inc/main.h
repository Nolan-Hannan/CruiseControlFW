/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define BRAKE_Pin GPIO_PIN_0
#define BRAKE_GPIO_Port GPIOA
#define BRAKE_EXTI_IRQn EXTI0_IRQn
#define FORWARD_Pin GPIO_PIN_1
#define FORWARD_GPIO_Port GPIOA
#define FORWARD_EXTI_IRQn EXTI1_IRQn
#define REVERSE_Pin GPIO_PIN_2
#define REVERSE_GPIO_Port GPIOA
#define REVERSE_EXTI_IRQn EXTI2_IRQn
#define MODE_SWITCH_Pin GPIO_PIN_3
#define MODE_SWITCH_GPIO_Port GPIOA
#define MUX_SEL_Pin GPIO_PIN_4
#define MUX_SEL_GPIO_Port GPIOA
#define CAN2_RX_HV_Pin GPIO_PIN_12
#define CAN2_RX_HV_GPIO_Port GPIOB
#define CAN2_TX_HV_Pin GPIO_PIN_13
#define CAN2_TX_HV_GPIO_Port GPIOB
#define BLUE_Pin GPIO_PIN_7
#define BLUE_GPIO_Port GPIOC
#define GREEN_Pin GPIO_PIN_8
#define GREEN_GPIO_Port GPIOC
#define RED_Pin GPIO_PIN_9
#define RED_GPIO_Port GPIOC
#define CAN1_RX_LV_Pin GPIO_PIN_11
#define CAN1_RX_LV_GPIO_Port GPIOA
#define CAN1_TX_LV_Pin GPIO_PIN_12
#define CAN1_TX_LV_GPIO_Port GPIOA

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
