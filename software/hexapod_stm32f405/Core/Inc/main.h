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

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);
void MX_I2C1_SMBUS_Init(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define VM_EN_Pin GPIO_PIN_13
#define VM_EN_GPIO_Port GPIOC
#define AC_OK_Pin GPIO_PIN_14
#define AC_OK_GPIO_Port GPIOC
#define MPU_INT_Pin GPIO_PIN_15
#define MPU_INT_GPIO_Port GPIOC
#define MPU_INT_EXTI_IRQn EXTI15_10_IRQn
#define BC_IOUT_Pin GPIO_PIN_0
#define BC_IOUT_GPIO_Port GPIOC
#define TEMP1_ADC_Pin GPIO_PIN_1
#define TEMP1_ADC_GPIO_Port GPIOC
#define TEMP2_ADC_Pin GPIO_PIN_2
#define TEMP2_ADC_GPIO_Port GPIOC
#define SVM_ADC_Pin GPIO_PIN_3
#define SVM_ADC_GPIO_Port GPIOC
#define CS1_MPU_Pin GPIO_PIN_4
#define CS1_MPU_GPIO_Port GPIOA
#define FAN_EN_Pin GPIO_PIN_4
#define FAN_EN_GPIO_Port GPIOC
#define CV_ADC_Pin GPIO_PIN_5
#define CV_ADC_GPIO_Port GPIOC
#define BUZZ_Pin GPIO_PIN_10
#define BUZZ_GPIO_Port GPIOA
#define WS2812_Pin GPIO_PIN_7
#define WS2812_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
