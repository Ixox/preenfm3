/**
  ******************************************************************************
  * @file   fatfs.c
  * @brief  Code for fatfs applications
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */

#include "fatfs.h"

uint8_t retUSER;    /* Return value for USER */
char USERPath[4];   /* USER logical drive path */

__attribute__((section(".ram_d2b")))  FATFS USERFatFS;    /* File system object for USER logical drive */

/* USER CODE BEGIN Variables */

/* USER CODE END Variables */    

void MX_FATFS_Init(bool userDma)
{
  /*## FatFS: Link the USER driver ###########################*/
  if (userDma) {
      FATFS_LinkDriver(&SD_Driver_DMA, USERPath);
  } else {
      FATFS_LinkDriver(&SD_Driver, USERPath);
  }
}

/**
  * @brief  Gets Time from RTC 
  * @param  None
  * @retval Time in DWORD
  */
DWORD get_fattime(void)
{
  /* USER CODE BEGIN get_fattime */
    return  ((DWORD)(2020 - 1980) << 25)  // Year 2014
            | ((DWORD)1 << 21)            // Month 7
            | ((DWORD)1 << 16)           // Mday 10
            | ((DWORD)10 << 11)           // Hour 16
            | ((DWORD)30 << 5)             // Min 0
            | ((DWORD)0 >> 1);            // Sec 0
  /* USER CODE END get_fattime */  
}

/* USER CODE BEGIN Application */
     
/* USER CODE END Application */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
