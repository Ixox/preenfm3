/**
  ******************************************************************************
  * @file    adafruit_802_conf.h
  * @author  MCD Application Team
  * @brief   This file includes the nucleo configuration and errno files
  *
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2019 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef ADAFRUIT_802_CONF_H
#define ADAFRUIT_802_CONF_H

#ifdef __cplusplus
 extern "C" {
#endif


#include "stm32h7xx_nucleo_conf.h"
#include "stm32h7xx_nucleo_errno.h"
#include "stm32h7xx_nucleo_bus.h"
#include "preenfm3_pins.h"


#define ADAFRUIT_802_ADCx_RANK                 ADC_REGULAR_RANK_1
#define ADAFRUIT_802_ADCx_SAMPLETIME           ADC_SAMPLETIME_2CYCLES_5
#define ADAFRUIT_802_ADCx_PRESCALER            ADC_CLOCKPRESCALER_PCLK_DIV4
#define ADAFRUIT_802_ADCx_POLL_TIMEOUT         10U

#define ADAFRUIT_802_SD_CS_LOW()       HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_RESET)
#define ADAFRUIT_802_SD_CS_HIGH()      HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_SET)


#ifdef __cplusplus
}
#endif

#endif /* ADAFRUIT_802_CONF_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
