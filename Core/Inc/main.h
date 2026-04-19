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
#include "stm32h7xx_hal.h"

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
#define SDRAM_REFRESH 2066
#define PT100_CS_Pin GPIO_PIN_4
#define PT100_CS_GPIO_Port GPIOE
#define SEN_ENABLE_Pin GPIO_PIN_10
#define SEN_ENABLE_GPIO_Port GPIOF
#define ENC_BT_Pin GPIO_PIN_0
#define ENC_BT_GPIO_Port GPIOA
#define ENC_BT_EXTI_IRQn EXTI0_IRQn
#define LTE_PWK_Pin GPIO_PIN_14
#define LTE_PWK_GPIO_Port GPIOB
#define LTE_RST_Pin GPIO_PIN_15
#define LTE_RST_GPIO_Port GPIOB
#define LCD_SPI_CS_Pin GPIO_PIN_11
#define LCD_SPI_CS_GPIO_Port GPIOD
#define LCD_SPI_CLK_Pin GPIO_PIN_2
#define LCD_SPI_CLK_GPIO_Port GPIOG
#define LCD_SPI_MOSI_Pin GPIO_PIN_3
#define LCD_SPI_MOSI_GPIO_Port GPIOG
#define LCD_RESET_Pin GPIO_PIN_6
#define LCD_RESET_GPIO_Port GPIOG
#define LCD_BL_Pin GPIO_PIN_10
#define LCD_BL_GPIO_Port GPIOA
#define ESP_RST_Pin GPIO_PIN_4
#define ESP_RST_GPIO_Port GPIOD
#define ENC_BT_B_Pin GPIO_PIN_7
#define ENC_BT_B_GPIO_Port GPIOD
#define ENC_BT_B_EXTI_IRQn EXTI9_5_IRQn
#define ENC_BT_A_Pin GPIO_PIN_13
#define ENC_BT_A_GPIO_Port GPIOG
#define ENC_BT_A_EXTI_IRQn EXTI15_10_IRQn
#define RL_2_Pin GPIO_PIN_4
#define RL_2_GPIO_Port GPIOB
#define RL_1_Pin GPIO_PIN_8
#define RL_1_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
