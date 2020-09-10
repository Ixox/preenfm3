/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
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
#ifdef DEFINIED_IN_PREENFM3_LIB_PREENFM3_PINS_H
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define HC165_CLK_Pin GPIO_PIN_7
#define HC165_CLK_GPIO_Port GPIOE
#define HC165_LOAD_Pin GPIO_PIN_8
#define HC165_LOAD_GPIO_Port GPIOE
#define HC165_DATA_Pin GPIO_PIN_9
#define HC165_DATA_GPIO_Port GPIOE
#define SD_CS_Pin GPIO_PIN_10
#define SD_CS_GPIO_Port GPIOE
#define TFT_CS_Pin GPIO_PIN_11
#define TFT_CS_GPIO_Port GPIOE
#define TFT_DC_Pin GPIO_PIN_12
#define TFT_DC_GPIO_Port GPIOE
#define TFT_RESET_Pin GPIO_PIN_13
#define TFT_RESET_GPIO_Port GPIOE
/* USER CODE BEGIN Private defines */
#endif
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
