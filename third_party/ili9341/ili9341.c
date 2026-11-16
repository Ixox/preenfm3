#include "stm32h7xx_hal.h"
#include "ili9341.h"

extern DMA_HandleTypeDef hdma_spi1_tx;
extern uint8_t dmaReady;

uint16_t windowLastX = 9999;

uint8_t dummyDataOut[] = { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 };
uint8_t dummyDataIn[16];

static void ILI9341_Reset() {
    HAL_GPIO_WritePin(ILI9341_RES_GPIO_Port, ILI9341_RES_Pin, GPIO_PIN_RESET);
    ILI9341_Unselect();
    HAL_Delay(5);
    ILI9341_Select();
    HAL_GPIO_WritePin(ILI9341_RES_GPIO_Port, ILI9341_RES_Pin, GPIO_PIN_SET);
    windowLastX = 9999;
}

static HAL_StatusTypeDef ILI9341_WriteCommand(uint8_t cmd) {
    PFM_CLEAR_PIN(ILI9341_DC_GPIO_Port, ILI9341_DC_Pin);
    return HAL_SPI_TransmitReceive(&ILI9341_SPI_PORT, &cmd, dummyDataIn, 1, HAL_MAX_DELAY);
}

static HAL_StatusTypeDef ILI9341_WriteData(uint8_t *buff, size_t buff_size) {
    PFM_SET_PIN(ILI9341_DC_GPIO_Port, ILI9341_DC_Pin);
    return HAL_SPI_TransmitReceive(&ILI9341_SPI_PORT, buff, dummyDataIn, buff_size, HAL_MAX_DELAY);
}

HAL_StatusTypeDef ILI9341_ReadPowerMode(uint8_t buff[2]) {
    static uint8_t readPowerModeCommand[2] = { 0x0A, 0xff};

    ILI9341_Select();
    // We send a command
    PFM_CLEAR_PIN(ILI9341_DC_GPIO_Port, ILI9341_DC_Pin);
    HAL_StatusTypeDef ret = HAL_SPI_TransmitReceive(&ILI9341_SPI_PORT, readPowerModeCommand, buff, 2, HAL_MAX_DELAY);
    ILI9341_Unselect();
    return ret;
}

HAL_StatusTypeDef ILI9341_ReadDisplayStatus(uint8_t buff[2]) {
    static uint8_t readPowerModeCommand[5] = { 0x09, 0xFF, 0xFF, 0xFF, 0xFF  };

    ILI9341_Select();
    // We send a command
    PFM_CLEAR_PIN(ILI9341_DC_GPIO_Port, ILI9341_DC_Pin);
    HAL_StatusTypeDef ret = HAL_SPI_TransmitReceive(&ILI9341_SPI_PORT, readPowerModeCommand, buff, 5, HAL_MAX_DELAY);
    ILI9341_Unselect();
    return ret;
}

HAL_StatusTypeDef ILI9341_ReadPixelFormat(uint8_t buff[3]) {
    static uint8_t readPixelFormatCommand[3] = { 0x0C, 0xFF, 0xFF };

    ILI9341_Select();
    // We send a command
    PFM_CLEAR_PIN(ILI9341_DC_GPIO_Port, ILI9341_DC_Pin);
    HAL_StatusTypeDef ret = HAL_SPI_TransmitReceive(&ILI9341_SPI_PORT, readPixelFormatCommand, buff, 3, HAL_MAX_DELAY);
    ILI9341_Unselect();
    return ret;
}



HAL_StatusTypeDef ILI9341_SetAddressWindow(uint16_t y0, uint16_t y1) {
    static uint8_t dataX[] = { 0, 0, 0, 239 };
    // column address set
    if (windowLastX != 0) {
        windowLastX = 0;
        if (ILI9341_WriteCommand(0x2A) != HAL_OK) {
            return HAL_ERROR;
        }
        if (ILI9341_WriteData(dataX, 4) != HAL_OK) {
            return HAL_ERROR;
        }
    }
    // row address set
    // Optimization removed to fix a button refresh problem
    if (ILI9341_WriteCommand(0x2B) != HAL_OK) {
        return HAL_ERROR;
    }
    uint8_t dataY[] = { (y0 >> 8) & 0xFF, y0 & 0xFF, (y1 >> 8) & 0xFF, y1 & 0xFF };
    if (ILI9341_WriteData(dataY, 4) != HAL_OK) {
        return HAL_ERROR;
    }
    // write to RAM
    return ILI9341_WriteCommand(0x2C); // RAMWR
}

void ILI9341_Init() {

    // INIT ILI9341
    ILI9341_Select();
    ILI9341_Reset();

    // command list is based on https://github.com/martnak/STM32-ILI9341

    // SOFTWARE RESET
    ILI9341_WriteCommand(0x01);
    ILI9341_Unselect();
    HAL_Delay(10);
    ILI9341_Select();

    // POWER CONTROL A
    ILI9341_WriteCommand(0xCB);
    {
        uint8_t data[] = { 0x39, 0x2C, 0x00, 0x34, 0x02 };
        ILI9341_WriteData(data, sizeof(data));
    }

    // POWER CONTROL B
    ILI9341_WriteCommand(0xCF);
    {
        uint8_t data[] = { 0x00, 0xC1, 0x30 };
        ILI9341_WriteData(data, sizeof(data));
    }

    // DRIVER TIMING CONTROL A
    ILI9341_WriteCommand(0xE8);
    {
        uint8_t data[] = { 0x85, 0x00, 0x78 };
        ILI9341_WriteData(data, sizeof(data));
    }

    // DRIVER TIMING CONTROL B
    ILI9341_WriteCommand(0xEA);
    {
        uint8_t data[] = { 0x00, 0x00 };
        ILI9341_WriteData(data, sizeof(data));
    }

    // POWER ON SEQUENCE CONTROL
    ILI9341_WriteCommand(0xED);
    {
        uint8_t data[] = { 0x64, 0x03, 0x12, 0x81 };
        ILI9341_WriteData(data, sizeof(data));
    }

    // PUMP RATIO CONTROL
    ILI9341_WriteCommand(0xF7);
    {
        uint8_t data[] = { 0x20 };
        ILI9341_WriteData(data, sizeof(data));
    }

    // POWER CONTROL,VRH[5:0]
    ILI9341_WriteCommand(0xC0);
    {
        uint8_t data[] = { 0x23 };
        ILI9341_WriteData(data, sizeof(data));
    }

    // POWER CONTROL,SAP[2:0];BT[3:0]
    ILI9341_WriteCommand(0xC1);
    {
        uint8_t data[] = { 0x10 };
        ILI9341_WriteData(data, sizeof(data));
    }

    // VCM CONTROL
    ILI9341_WriteCommand(0xC5);
    {
        uint8_t data[] = { 0x3E, 0x28 };
        ILI9341_WriteData(data, sizeof(data));
    }

    // VCM CONTROL 2
    ILI9341_WriteCommand(0xC7);
    {
        uint8_t data[] = { 0x86 };
        ILI9341_WriteData(data, sizeof(data));
    }

    // PIXEL FORMAT
    ILI9341_WriteCommand(0x3A);
    {
        uint8_t data[] = { 0x55 };
        ILI9341_WriteData(data, sizeof(data));
    }

    // FRAME RATIO CONTROL, STANDARD RGB COLOR
    ILI9341_WriteCommand(0xB1);
    {
        uint8_t data[] = { 0x00, 0x18 };
        ILI9341_WriteData(data, sizeof(data));
    }

    // DISPLAY FUNCTION CONTROL
    ILI9341_WriteCommand(0xB6);
    {
        uint8_t data[] = { 0x08, 0x82, 0x27 };
        ILI9341_WriteData(data, sizeof(data));
    }

    // 3GAMMA FUNCTION DISABLE
    ILI9341_WriteCommand(0xF2);
    {
        uint8_t data[] = { 0x00 };
        ILI9341_WriteData(data, sizeof(data));
    }

    // GAMMA CURVE SELECTED
    ILI9341_WriteCommand(0x26);
    {
        uint8_t data[] = { 0x01 };
        ILI9341_WriteData(data, sizeof(data));
    }

    // POSITIVE GAMMA CORRECTION
    ILI9341_WriteCommand(0xE0);
    {
        uint8_t data[] = { 0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00 };
        ILI9341_WriteData(data, sizeof(data));
    }

    // NEGATIVE GAMMA CORRECTION
    ILI9341_WriteCommand(0xE1);
    {
        uint8_t data[] = { 0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F };
        ILI9341_WriteData(data, sizeof(data));
    }

    // EXIT SLEEP
    ILI9341_WriteCommand(0x11);
    ILI9341_Unselect();
    HAL_Delay(120);
    ILI9341_Select();

    // TURN ON DISPLAY
    ILI9341_WriteCommand(0x29);

    // MADCTL
    ILI9341_WriteCommand(0x36);
    {
        uint8_t data[] = { ILI9341_MADCTL_MX | ILI9341_MADCTL_BGR };
        ILI9341_WriteData(data, sizeof(data));
    }

    ILI9341_Unselect();
}
