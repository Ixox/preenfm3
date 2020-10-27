/** 
  ******************************************************************************
  * @file    stm32h7xx_nucleo_bus.h
  * @author  MCD Application Team
  * @brief   This file is the header of stm32h7xx_nucleo_bus.c
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
  
/*
 * Modified for preenfm3
 * SD is on SPI2
 */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef STM32H7XX_NUCLEO_144_BUS_H
#define STM32H7XX_NUCLEO_144_BUS_H

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32h7xx_nucleo_conf.h"
#include "stm32h7xx_nucleo_errno.h"
   
/** @addtogroup BSP
  * @{
  */

/** @defgroup STM32H7XX_NUCLEO_144 STM32H7XX_NUCLEO_144
  * @{
  */

/** @addtogroup STM32H7XX_NUCLEO_144_BUS STM32H7XX_NUCLEO_144 BUS
  * @{
  */ 


/* Maximum Timeout values for flags waiting loops. These timeouts are not based
   on accurate values, they just guarantee that the application will not remain
   stuck if the SPI communication is corrupted.
   You may modify these timeout values depending on CPU frequency and application
   conditions (interrupts routines ...). */   
#define BUS_SPI_TIMEOUT_MAX                        1000U
#define BUS_SPI_BAUDRATE                           12500000 /* baud rate of SPI1 = 12.5 Mbps*/
   
/** @defgroup STM32H7XX_NUCLEO_144_BUS_Exported_Types BUS Exported Types
  * @{
  */

#if (USE_HAL_SPI_REGISTER_CALLBACKS == 1)
typedef struct
{
  pSPI_CallbackTypeDef  pMspSpiInitCb;
  pSPI_CallbackTypeDef  pMspSpiDeInitCb;
}BSP_SPI_Cb_t;
#endif /* (USE_HAL_SPI_REGISTER_CALLBACKS == 1) */
/**
  * @}
  */ 


/** @defgroup STM32H7XX_NUCLEO_144_BUS_Exported_Functions BUS Exported Functions
  * @{
  */
int32_t BSP_SPI_Init(void);
int32_t BSP_SPI_DeInit(void);
#if (USE_HAL_SPI_REGISTER_CALLBACKS == 1)
int32_t BSP_SPI_RegisterMspCallbacks(uint32_t Instance, BSP_SPI_Cb_t *CallBacks);
int32_t BSP_SPI_RegisterDefaultMspCallbacks(uint32_t Instance);
#endif /* (USE_HAL_SPI_REGISTER_CALLBACKS == 1) */
int32_t BSP_SPI_Send(uint8_t *pTxData, uint32_t Legnth);
int32_t BSP_SPI_Recv(uint8_t *pRxData, uint32_t Legnth);
int32_t BSP_SPI_SendRecv(uint8_t *pTxData, uint8_t *pRxData, uint32_t Legnth);
void BSP_SPI_SendRecv_IT(uint8_t *pTxData, uint8_t *pRxData, uint32_t Legnth);
HAL_StatusTypeDef MX_SPI_Init(SPI_HandleTypeDef *phspi, uint32_t baudrate_presc);
int32_t BSP_GetTick(void);
/**
  * @}
  */ 

/**
  * @}
  */ 

/**
  * @}
  */

/**
  * @}
  */

#ifdef __cplusplus
}
#endif

#endif /* STM32H7XX_NUCLEO_144_BUS_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
