
/*
 * preenfm3 bootloader
 * redirect all access to SD card driver
 * Comes from usbd_storage_if.c
  */



#include "usbd_storage_if.h"
#include "adafruit_802_sd.h"

#define PFM3_SD_LUN_NBR                  1
#define SD_DATATIMEOUT           ((uint32_t)100000000)

int32_t readDisplayCounter = 0;
int32_t writeDisplayCounter = 0;

extern bool dmaForSDAccess;

const int8_t PFM3_SD_Inquirydata_FS[] = {/* 36 */
  
  /* LUN 0 */
  0x00,
  0x80,
  0x02,
  0x02,
  (STANDARD_INQUIRY_DATA_LEN - 5),
  0x00,
  0x00,	
  0x00,
  'I', 'X', 'O', 'X', ' ', ' ', ' ', ' ', /* Manufacturer : 8 bytes */
  'P', 'f', 'm', '3', 'S', 'D', ' ', ' ', /* Product      : 16 Bytes */
  ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
  '1', '.', '0' ,'5'                      /* Version      : 4 Bytes */
}; 

extern USBD_HandleTypeDef hUsbDeviceFS;


static int8_t PFM3_SD_Init_FS(uint8_t lun);
static int8_t PFM3_SD_GetCapacity_FS(uint8_t lun, uint32_t *block_num, uint16_t *block_size);
static int8_t PFM3_SD_IsReady_FS(uint8_t lun);
static int8_t PFM3_SD_IsWriteProtected_FS(uint8_t lun);
static int8_t PFM3_SD_Read_FS(uint8_t lun, uint8_t *buf, uint32_t blk_addr, uint16_t blk_len);
static int8_t PFM3_SD_Write_FS(uint8_t lun, uint8_t *buf, uint32_t blk_addr, uint16_t blk_len);
static int8_t PFM3_SD_GetMaxLun_FS(void);


USBD_StorageTypeDef USBD_Storage_Interface_fops_FS =
{
  PFM3_SD_Init_FS,
  PFM3_SD_GetCapacity_FS,
  PFM3_SD_IsReady_FS,
  PFM3_SD_IsWriteProtected_FS,
  PFM3_SD_Read_FS,
  PFM3_SD_Write_FS,
  PFM3_SD_GetMaxLun_FS,
  (int8_t *)PFM3_SD_Inquirydata_FS
};


/**
  * @brief  Initializes over USB FS IP
  * @param  lun:
  * @retval USBD_OK if all operations are OK else USBD_FAIL
  */
int8_t PFM3_SD_Init_FS(uint8_t lun)
{
  /* USER CODE BEGIN 2 */
  return (USBD_OK);
  /* USER CODE END 2 */
}

/**
  * @brief  .
  * @param  lun: .
  * @param  block_num: .
  * @param  block_size: .
  * @retval USBD_OK if all operations are OK else USBD_FAIL
  */
int8_t PFM3_SD_GetCapacity_FS(uint8_t lun, uint32_t *block_num, uint16_t *block_size)
{
  ADAFRUIT_802_SD_CardInfo_t CardInfo;
  ADAFRUIT_802_SD_GetCardInfo(0,&CardInfo);

  *block_num  = CardInfo.LogBlockNbr;
  *block_size = 512;
  return (USBD_OK);
}

/**
  * @brief  .
  * @param  lun: .
  * @retval USBD_OK if all operations are OK else USBD_FAIL
  */
int8_t PFM3_SD_IsReady_FS(uint8_t lun)
{
  /* USER CODE BEGIN 4 */
  return (USBD_OK);
  /* USER CODE END 4 */
}

/**
  * @brief  .
  * @param  lun: .
  * @retval USBD_OK if all operations are OK else USBD_FAIL
  */
int8_t PFM3_SD_IsWriteProtected_FS(uint8_t lun)
{
  /* USER CODE BEGIN 5 */
  return (USBD_OK);
  /* USER CODE END 5 */
}

/**
  * @brief  .
  * @param  lun: .
  * @retval USBD_OK if all operations are OK else USBD_FAIL
  */
int8_t PFM3_SD_Read_FS(uint8_t lun, uint8_t *buf, uint32_t blk_addr, uint16_t blk_len)
{
    int8_t res = USBD_FAIL;
    uint32_t timeout = 100000;

    if (PFM3_SD_ReadBlocks((uint32_t*) buf, (uint32_t) (blk_addr), blk_len, SD_DATATIMEOUT, dmaForSDAccess) == BSP_ERROR_NONE) {
        /* wait until the Write operation is finished */
        while (ADAFRUIT_802_SD_GetCardState(0) != BSP_ERROR_NONE) {
            if (timeout-- == 0) {
                return USBD_FAIL;
            }
        }
        readDisplayCounter = 500;
        res = USBD_OK;
    }

    return res;
}

/**
  * @brief  .
  * @param  lun: .
  * @retval USBD_OK if all operations are OK else USBD_FAIL
  */
int8_t PFM3_SD_Write_FS(uint8_t lun, uint8_t *buf, uint32_t blk_addr, uint16_t blk_len)
{
    int8_t res = USBD_FAIL;
    uint32_t timeout = 100000;

    if (PFM3_SD_WriteBlocks((uint32_t*) buf, (uint32_t) (blk_addr), blk_len, SD_DATATIMEOUT, dmaForSDAccess) == BSP_ERROR_NONE) {
        /* wait until the Write operation is finished */
        while (ADAFRUIT_802_SD_GetCardState(0) != BSP_ERROR_NONE) {
            if (timeout-- == 0) {
                return USBD_FAIL;
            }
        }
        writeDisplayCounter = 500;
        res = USBD_OK;
    }

    return res;
}

/**
  * @brief  .
  * @param  None
  * @retval .
  */
int8_t PFM3_SD_GetMaxLun_FS(void)
{
  /* USER CODE BEGIN 8 */
  return (PFM3_SD_LUN_NBR - 1);
  /* USER CODE END 8 */
}

