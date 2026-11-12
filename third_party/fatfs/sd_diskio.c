/**
  ******************************************************************************
  * @file    sd_diskio.c
  * @author  MCD Application Team
  * @brief   SD Disk I/O driver
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2017 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */

/*
 * Xavier Hosxe for Preenfm3 : add a DMA variant
 */

/* Includes ------------------------------------------------------------------*/
#include "ff_gen_drv.h"
#include "sd_diskio.h"


/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* use the default SD timout as defined in the platform BSP driver*/
#if defined(SDMMC_DATATIMEOUT)
#define SD_TIMEOUT SDMMC_DATATIMEOUT
#elif defined(SD_DATATIMEOUT)
#define SD_TIMEOUT SD_DATATIMEOUT
#else
#define SD_TIMEOUT 30 * 1000
#endif

#define SD_DEFAULT_BLOCK_SIZE 512
#define SD_DATATIMEOUT           ((uint32_t)100000000)

/*
 * Depending on the usecase, the SD card initialization could be done at the
 * application level, if it is the case define the flag below to disable
 * the ADAFRUIT_802_SD_Init() call in the SD_Initialize().
 */

#define DISABLE_SD_INIT

/* Private variables ---------------------------------------------------------*/
/* Disk status */
static volatile DSTATUS Stat = STA_NOINIT;

/* Private function prototypes -----------------------------------------------*/
static DSTATUS SD_CheckStatus(BYTE lun);
DSTATUS SD_initialize (BYTE);
DSTATUS SD_status (BYTE);
DRESULT SD_read (BYTE, BYTE*, DWORD, UINT);
DRESULT SD_write (BYTE, const BYTE*, DWORD, UINT);
DRESULT SD_read_DMA (BYTE, BYTE*, DWORD, UINT);
DRESULT SD_write_DMA (BYTE, const BYTE*, DWORD, UINT);
DRESULT SD_ioctl (BYTE, BYTE, void*);

const Diskio_drvTypeDef  SD_Driver =
{
  SD_initialize,
  SD_status,
  SD_read,
  SD_write,
  SD_ioctl,
};

const Diskio_drvTypeDef  SD_Driver_DMA =
{
  SD_initialize,
  SD_status,
  SD_read_DMA,
  SD_write_DMA,
  SD_ioctl,
};


/* Private functions ---------------------------------------------------------*/
static DSTATUS SD_CheckStatus(BYTE lun)
{
  Stat = STA_NOINIT;

  if(ADAFRUIT_802_SD_GetCardState(0) == BSP_ERROR_NONE)
  {
    Stat &= ~STA_NOINIT;
  }

  return Stat;
}

/**
  * @brief  Initializes a Drive
  * @param  lun : not used
  * @retval DSTATUS: Operation status
  */
DSTATUS SD_initialize(BYTE lun)
{
  Stat = STA_NOINIT;
#if !defined(DISABLE_SD_INIT)

  if(ADAFRUIT_802_SD_Init(0) == BSP_ERROR_NONE)
  {
    Stat = SD_CheckStatus(lun);
  }

#else
  Stat = SD_CheckStatus(lun);
#endif
  return Stat;
}

/**
  * @brief  Gets Disk Status
  * @param  lun : not used
  * @retval DSTATUS: Operation status
  */
DSTATUS SD_status(BYTE lun)
{
  return SD_CheckStatus(lun);
}

/**
  * @brief  Reads Sector(s)
  * @param  lun : not used
  * @param  *buff: Data buffer to store read data
  * @param  sector: Sector address (LBA)
  * @param  count: Number of sectors to read (1..128)
  * @retval DRESULT: Operation result
  */
/**
 * @brief  Reads Sector(s)
 * @param  lun : not used
 * @param  *buff: Data buffer to store read data
 * @param  sector: Sector address (LBA)
 * @param  count: Number of sectors to read (1..128)
 * @retval DRESULT: Operation result
 */
DRESULT SD_read_DMA(BYTE lun, BYTE *buff, DWORD sector, UINT count) {
    DRESULT res = RES_ERROR;
    uint32_t timeout = 100000;

    if (PFM3_SD_ReadBlocks((uint32_t*) buff, (uint32_t) (sector), count, SD_DATATIMEOUT, true) == BSP_ERROR_NONE) {
        /* wait until the Write operation is finished */
        while (ADAFRUIT_802_SD_GetCardState(0) != BSP_ERROR_NONE) {
            if (timeout-- == 0) {
                return RES_ERROR;
            }
        }
        res = RES_OK;
    }

    return res;
}
DRESULT SD_read(BYTE lun, BYTE *buff, DWORD sector, UINT count) {
    DRESULT res = RES_ERROR;
    uint32_t timeout = 100000;

    __disable_irq();

    if (PFM3_SD_ReadBlocks((uint32_t*) buff, (uint32_t) (sector), count, SD_DATATIMEOUT, false) == BSP_ERROR_NONE) {
        /* wait until the Write operation is finished */
        int32_t error ;
        while ((error = ADAFRUIT_802_SD_GetCardState(0)) != BSP_ERROR_NONE) {
            if (timeout-- == 0) {
                __enable_irq();
                return RES_ERROR;
            }
        }
        res = RES_OK;
    }

    __enable_irq();

    return res;
}

/**
  * @brief  Writes Sector(s)
  * @param  lun : not used
  * @param  *buff: Data to be written
  * @param  sector: Sector address (LBA)
  * @param  count: Number of sectors to write (1..128)
  * @retval DRESULT: Operation result
  */
DRESULT SD_write_DMA(BYTE lun, const BYTE *buff, DWORD sector, UINT count) {
    DRESULT res = RES_ERROR;
    uint32_t timeout = 100000;

    if (PFM3_SD_WriteBlocks((uint32_t*) buff, (uint32_t) (sector), count, SD_DATATIMEOUT, true) == BSP_ERROR_NONE) {
        /* wait until the Write operation is finished */
        while (ADAFRUIT_802_SD_GetCardState(0) != BSP_ERROR_NONE) {
            if (timeout-- == 0) {
                return RES_ERROR;
            }
        }
        res = RES_OK;
    }

    return res;
}

DRESULT SD_write(BYTE lun, const BYTE *buff, DWORD sector, UINT count) {
    DRESULT res = RES_ERROR;
    uint32_t timeout = 100000;

    __disable_irq();

    if (PFM3_SD_WriteBlocks((uint32_t*) buff, (uint32_t) (sector), count, SD_DATATIMEOUT, false) == BSP_ERROR_NONE) {
        /* wait until the Write operation is finished */
        while (ADAFRUIT_802_SD_GetCardState(0) != BSP_ERROR_NONE) {
            if (timeout-- == 0) {
                __enable_irq();

                return RES_ERROR;
            }
        }
        res = RES_OK;
    }

    __enable_irq();

    return res;
}

/**
  * @brief  I/O control operation
  * @param  lun : not used
  * @param  cmd: Control code
  * @param  *buff: Buffer to send/receive control data
  * @retval DRESULT: Operation result
  */
DRESULT SD_ioctl(BYTE lun, BYTE cmd, void *buff)
{
  DRESULT res = RES_ERROR;
  ADAFRUIT_802_SD_CardInfo_t CardInfo;

  if (Stat & STA_NOINIT) return RES_NOTRDY;

  switch (cmd)
  {
  /* Make sure that no pending write process */
  case CTRL_SYNC :
    res = RES_OK;
    break;

  /* Get number of sectors on the disk (DWORD) */
  case GET_SECTOR_COUNT :
    ADAFRUIT_802_SD_GetCardInfo(0,&CardInfo);
    *(DWORD*)buff = CardInfo.LogBlockNbr;
    res = RES_OK;
    break;

  /* Get R/W sector size (WORD) */
  case GET_SECTOR_SIZE :
    ADAFRUIT_802_SD_GetCardInfo(0,&CardInfo);
    *(WORD*)buff = CardInfo.LogBlockSize;
    res = RES_OK;
    break;

  /* Get erase block size in unit of sector (DWORD) */
  case GET_BLOCK_SIZE :
    ADAFRUIT_802_SD_GetCardInfo(0,&CardInfo);
    *(DWORD*)buff = CardInfo.LogBlockSize / SD_DEFAULT_BLOCK_SIZE;
  res = RES_OK;
    break;

  default:
    res = RES_PARERR;
  }

  return res;
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
