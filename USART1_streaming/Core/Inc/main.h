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
#include "stm32c0xx_hal.h"

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
#define CS1_Pin GPIO_PIN_9
#define CS1_GPIO_Port GPIOB
#define CS2_Pin GPIO_PIN_14
#define CS2_GPIO_Port GPIOC
#define CS3_Pin GPIO_PIN_15
#define CS3_GPIO_Port GPIOC
#define APP_TX4_Pin GPIO_PIN_0
#define APP_TX4_GPIO_Port GPIOA
#define APP_RX4_Pin GPIO_PIN_1
#define APP_RX4_GPIO_Port GPIOA
#define ADC1_Pin GPIO_PIN_2
#define ADC1_GPIO_Port GPIOA
#define ADC2_Pin GPIO_PIN_3
#define ADC2_GPIO_Port GPIOA
#define ADC3_Pin GPIO_PIN_4
#define ADC3_GPIO_Port GPIOA
#define ADC4_Pin GPIO_PIN_5
#define ADC4_GPIO_Port GPIOA
#define ADC5_Pin GPIO_PIN_6
#define ADC5_GPIO_Port GPIOA
#define ADC6_Pin GPIO_PIN_7
#define ADC6_GPIO_Port GPIOA
#define APP_RX3_Pin GPIO_PIN_0
#define APP_RX3_GPIO_Port GPIOB
#define CS4_Pin GPIO_PIN_1
#define CS4_GPIO_Port GPIOB
#define APP_TX3_Pin GPIO_PIN_2
#define APP_TX3_GPIO_Port GPIOB
#define APP_TX2_Pin GPIO_PIN_8
#define APP_TX2_GPIO_Port GPIOA
#define APP_TX1_Pin GPIO_PIN_9
#define APP_TX1_GPIO_Port GPIOA
#define MGR__RST__Pin GPIO_PIN_6
#define MGR__RST__GPIO_Port GPIOC
#define APP_RX1_Pin GPIO_PIN_10
#define APP_RX1_GPIO_Port GPIOA
#define CS5_6_Pin GPIO_PIN_11
#define CS5_6_GPIO_Port GPIOA
#define MGR_BOOT0_Pin GPIO_PIN_12
#define MGR_BOOT0_GPIO_Port GPIOA
#define APP_RX2_Pin GPIO_PIN_13
#define APP_RX2_GPIO_Port GPIOA
#define RPSPI0_0_Pin GPIO_PIN_15
#define RPSPI0_0_GPIO_Port GPIOA
#define RPSPI0_SCLK_Pin GPIO_PIN_3
#define RPSPI0_SCLK_GPIO_Port GPIOB
#define RPSPI0_MISO_Pin GPIO_PIN_4
#define RPSPI0_MISO_GPIO_Port GPIOB
#define RPSPI0_MOSI_Pin GPIO_PIN_5
#define RPSPI0_MOSI_GPIO_Port GPIOB
#define MGR2HOST485_Pin GPIO_PIN_6
#define MGR2HOST485_GPIO_Port GPIOB
#define SDA1_Pin GPIO_PIN_7
#define SDA1_GPIO_Port GPIOB
#define SCL1_Pin GPIO_PIN_8
#define SCL1_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
