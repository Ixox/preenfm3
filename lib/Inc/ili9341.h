/* vim: set ai et ts=4 sw=4: */
#ifndef __ILI9341_H__
#define __ILI9341_H__

#include "fonts.h"
#include <stdbool.h>
#include "preenfm3_pins.h"

#define DMA_BUFFER __attribute__((section(".dma_buffer")))

#define ILI9341_MADCTL_MY  0x80
#define ILI9341_MADCTL_MX  0x40
#define ILI9341_MADCTL_MV  0x20
#define ILI9341_MADCTL_ML  0x10
#define ILI9341_MADCTL_RGB 0x00
#define ILI9341_MADCTL_BGR 0x08
#define ILI9341_MADCTL_MH  0x04

/*** Redefine if necessary ***/
#define ILI9341_SPI_PORT hspi1
extern SPI_HandleTypeDef ILI9341_SPI_PORT;

// init from preenfm3_pins.j

#define ILI9341_CS_Pin        TFT_CS_Pin
#define ILI9341_CS_GPIO_Port  TFT_CS_GPIO_Port
#define ILI9341_RES_Pin       TFT_RESET_Pin
#define ILI9341_RES_GPIO_Port TFT_RESET_GPIO_Port
#define ILI9341_DC_Pin        TFT_DC_Pin
#define ILI9341_DC_GPIO_Port  TFT_DC_GPIO_Port

// default orientation
#define ILI9341_WIDTH  240
#define ILI9341_HEIGHT 320
#define ILI9341_ROTATION (ILI9341_MADCTL_MX | ILI9341_MADCTL_BGR)

// rotate right
/*
 #define ILI9341_WIDTH  320
 #define ILI9341_HEIGHT 240
 #define ILI9341_ROTATION (ILI9341_MADCTL_MX | ILI9341_MADCTL_MY | ILI9341_MADCTL_MV | ILI9341_MADCTL_BGR)
 */

// rotate left
/*
 #define ILI9341_WIDTH  320
 #define ILI9341_HEIGHT 240
 #define ILI9341_ROTATION (ILI9341_MADCTL_MV | ILI9341_MADCTL_BGR)
 */

// upside down
/*
 #define ILI9341_WIDTH  240
 #define ILI9341_HEIGHT 320
 #define ILI9341_ROTATION (ILI9341_MADCTL_MY | ILI9341_MADCTL_BGR)
 */

/****************************/

// Color definitions
#define	ILI9341_BLACK   0x0000
#define	ILI9341_BLUE    0x001F
#define	ILI9341_RED     0xF800
#define	ILI9341_GREEN   0x07E0
#define ILI9341_CYAN    0x07FF
#define ILI9341_MAGENTA 0xF81F
#define ILI9341_YELLOW  0xFFE0
#define ILI9341_WHITE   0xFFFF
#define ILI9341_COLOR565(r, g, b) (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3))


#ifdef __cplusplus
extern "C" {
#endif

// call before initializing any SPI devices

#define ILI9341_Select() PFM_CLEAR_PIN(ILI9341_CS_GPIO_Port, ILI9341_CS_Pin)
#define ILI9341_Unselect() PFM_SET_PIN(ILI9341_CS_GPIO_Port, ILI9341_CS_Pin)

void ILI9341_Init(void);
HAL_StatusTypeDef ILI9341_ReadPowerMode(uint8_t buff[2]);
HAL_StatusTypeDef ILI9341_SetAddressWindow(uint16_t y0, uint16_t y1);

#ifdef __cplusplus
}
#endif

#endif // __ILI9341_H__
