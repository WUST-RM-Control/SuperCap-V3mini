/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
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
#include "stm32g4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "adc.h"

#include "dma.h"
#include "fdcan.h"
#include "hrtim.h"
#include "opamp.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"
#include "i2c.h"

#include "arm_math.h"
#include "stdio.h"

/* ---- App 层（替代原 Program/） ---- */
#include "params.h"
#include "app_main.h"
#include "sample.h"
#include "modulator.h"
#include "ctrl_core.h"
#include "ctrl_isr.h"
#include "protect.h"
#include "comm_can.h"
#include "comm_uart.h"
#include "ui_oled.h"
#include "calib.h"
#include "OLED.h"
#include "LED.h"
#include "DWT.h"

#define  CCMRAM __attribute__((section("ccmram")))
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
#define LED_R_Pin GPIO_PIN_0
#define LED_R_GPIO_Port GPIOC
#define LED_G_Pin GPIO_PIN_1
#define LED_G_GPIO_Port GPIOC
#define LED_B_Pin GPIO_PIN_2
#define LED_B_GPIO_Port GPIOC
#define A_Power_V_ADC_Pin GPIO_PIN_3
#define A_Power_V_ADC_GPIO_Port GPIOA
#define A_Power_I_ADC_Pin GPIO_PIN_7
#define A_Power_I_ADC_GPIO_Port GPIOA
#define Referee_I_ADC_Pin GPIO_PIN_0
#define Referee_I_ADC_GPIO_Port GPIOB
#define B_Power_I_ADC_Pin GPIO_PIN_13
#define B_Power_I_ADC_GPIO_Port GPIOB
#define B_Power_V_ADC_Pin GPIO_PIN_14
#define B_Power_V_ADC_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
