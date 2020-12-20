/*
 * Copyright 2019 Xavier Hosxe
 *
 * Author: Xavier Hosxe (xavier . hosxe (at) gmail . com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "version.h"
#include "stm32h7xx_hal.h"
#include "fatfs.h"
#include "TftDisplay.h"
#include "ili9341.h"
#include "Encoders.h"
#include "FirmwareFile.h"
#include "usb_device.h"

Encoders encoders;
TftDisplay tft;
TftAlgo dummyAlgo;
FirmwareFile firmwares;

#define BLOCK_SIZE_UINT32 4096
#define BLOCK_SIZE_UINT8 (BLOCK_SIZE_UINT32 * 4)
__attribute__((section(".ram_d2"))) uint32_t d2Buffer[BLOCK_SIZE_UINT32];


char sdAccessAnimation[4] = {'-', '\\', '|', '/' };
uint32_t writeAnimationIndex = 0;
uint32_t readAnimationIndex = 0;

#ifdef __cplusplus
extern "C" {
#endif

#include "flash_if.h"
#include "bootloader.h"
#include "preenfm3lib.h"

typedef  void (*pFunction)(void);

enum {
    STATE_INIT,
    STATE_MAIN_LOOP,
    STATE_FLASH,
    STATE_REBOOT
};

bool readyForTFT = false;
uint32_t pfm3Millis = 1;
uint32_t cptMainLoop = 1;
uint32_t encoderMillis = 1;
uint16_t ledMillis = 0;
uint32_t state = STATE_INIT;
uint32_t fileSelect = 0;
int32_t button = -1;
uint32_t sdError = 0;

extern int32_t readDisplayCounter;
int32_t readDisplayCounterAnim = 0;
extern int32_t writeDisplayCounter;
int32_t writeDisplayCounterAnim = 0;


class BootloaderEncoderListener : public EncodersListener {
    void buttonPressed(int button);
    void encoderTurned(int encoder, int ticks) {};
    void encoderTurnedWhileButtonPressed(int encoder, int ticks, int button) {} ;
    void buttonLongPressed(int button) {};
    void twoButtonsPressed(int button1, int button2) {};
};
void BootloaderEncoderListener::buttonPressed(int b) {
    button = b;
}
BootloaderEncoderListener bootloaderEncoderListener;



void flashFirmware(const FirmwarePFM3File *pfm2File);
void formatFlash(int firmwareSize);
void jumpToBootloader();
void reboot();
void SDCardAccess();


void resetButtonPressed() {
    button = -1;
}

void waitForButtonRelease()
{
    while (button) {
        resetButtonPressed();
        encoders.checkSimpleStatus();
    }
}

uint32_t getButtonPressed() {
    resetButtonPressed();
    encoders.checkSimpleStatus();
    return button;
}

void bootloaderInit() {
    sdError = preenfm3LibInitSD();

    MX_USB_DEVICE_Init();

    tft.init(&dummyAlgo); // Dummy value is OK, we won't use algo
    ILI9341_Init();

    tft.clear();

    tft.fillArea(0, 0, 240, 23, COLOR_DARK_BLUE);
    tft.setCharBackgroundColor(COLOR_DARK_BLUE);
    tft.setCharColor(COLOR_YELLOW);
    tft.setCursorInPixel(37, 3);
    tft.print("Bootloader ");
    tft.print(PFM3_BOOTLOADER_VERSION);
    tft.setCharBackgroundColor(COLOR_BLACK);


    // Let's start main loop
    readyForTFT = true;

    HAL_Delay(400);

    preenfm3ForceTftBacklight();
}


void displayFile(int fileNumber) {

    if (sdError != 0) {
        tft.fillArea(0, 98, 240, 22, COLOR_RED);
        tft.setCharBackgroundColor(COLOR_RED);
        tft.setCharColor(COLOR_BLACK);
        tft.setCursor(5, 5);
        tft.print("SD CARD ERROR");
        tft.setCharBackgroundColor(COLOR_BLACK);
    } else if (firmwares.getNumberOfFiles() == 0) {
        tft.fillArea(0, 98, 240, 22, COLOR_BLACK);
        tft.setCharBackgroundColor(COLOR_BLACK);
        tft.setCharColor(COLOR_RED);
        tft.setCursor(2, 5);
        tft.print("No firmware in '/'");
        tft.setCharBackgroundColor(COLOR_BLACK);
    } else {
        tft.fillArea(0, 100, 240, 20, COLOR_BLACK);
        tft.setCharBackgroundColor(COLOR_BLACK);
        tft.setCharColor(COLOR_CYAN);
        tft.setCursor(5, 5);
        tft.print(firmwares.getFile(fileNumber)->name);
    }
}

void bootloaderLoop() {
    switch (state) {
    case STATE_INIT:
        tft.fillArea(0, 24, 240, 320-24, COLOR_BLACK);

        tft.setCharColor(COLOR_YELLOW);
        tft.setCursorSmallChar(11, 5);
        tft.printSmallChars("- / + : ");
        tft.setCharColor(COLOR_WHITE);
        tft.printSmallChars("SELECT");
        HAL_Delay(25);

        tft.setCharColor(COLOR_YELLOW);
        tft.setCursorSmallChar(8, 6);
        tft.printSmallChars("Button 1 : ");
        tft.setCharColor(COLOR_WHITE);
        tft.printSmallChars("FLASH");
        HAL_Delay(25);

        tft.setCharColor(COLOR_LIGHT_GRAY);
        tft.setCursorSmallChar(8, 8);
        tft.printSmallChars("Firmware to flash");
        HAL_Delay(25);




        tft.setCursorSmallChar(8, 14);
        tft.setCharColor(COLOR_YELLOW);
        tft.printSmallChars("Button 2 : ");
        tft.setCharColor(COLOR_WHITE);
        tft.printSmallChars("DFU Mode");
        HAL_Delay(25);

        tft.setCursorSmallChar(8, 16);
        tft.setCharColor(COLOR_YELLOW);
        tft.printSmallChars("Button 3 : ");
        tft.setCharColor(COLOR_WHITE);
        tft.printSmallChars("SD card access");
        HAL_Delay(25);

        tft.setCursorSmallChar(8, 18);
        tft.setCharColor(COLOR_YELLOW);
        tft.printSmallChars("Button 6 : ");
        tft.setCharColor(COLOR_WHITE);
        tft.printSmallChars("REBOOT");
        HAL_Delay(25);


        displayFile(fileSelect);
        HAL_Delay(25);
        state = STATE_MAIN_LOOP;
        break;
    case STATE_MAIN_LOOP:
        if ((pfm3Millis - encoderMillis) > 2) {
            encoders.checkSimpleStatus();
            // Button is set in encoder by BootloaderEncoderListener::buttonPressed

            switch (button) {
            case 8:
                if (sdError == 0 && firmwares.getNumberOfFiles() >= 2) {
                    if (fileSelect < (firmwares.getNumberOfFiles() - 1)) {
                        fileSelect ++;
                    } else {
                        fileSelect = 0;
                    }
                    displayFile(fileSelect);
                }
                break;
            case 7:
                if (sdError == 0 && firmwares.getNumberOfFiles() >= 2) {
                    if (fileSelect > 0) {
                        fileSelect --;
                    } else {
                        fileSelect = firmwares.getNumberOfFiles() - 1;
                    }
                    displayFile(fileSelect);
                }
                break;
            case 0:
                if (sdError == 0 && firmwares.getNumberOfFiles() >= 1) {
                    // Button 1
                    // Flash
                    formatFlash(firmwares.getFile(fileSelect)->size);
                    flashFirmware(firmwares.getFile(fileSelect));
                }
                break;
            case 1:
                // Button 2
                jumpToBootloader();
                break;
            case 2:
                // Button 3
                SDCardAccess();
                break;
            case 5:
                // Button 4
                reboot();
                break;
            }
            resetButtonPressed();
            encoderMillis = pfm3Millis;
        }
        break;
    case STATE_FLASH:
        break;
    case STATE_REBOOT:
        break;
    }
}



void bootloaderTftTic() {
    cptMainLoop++;

    if (!readyForTFT) {
        return;
    }

    ledMillis++;
    if (ledMillis == 30) {
        HAL_GPIO_WritePin(LED_CONTROL_GPIO_Port, LED_CONTROL_Pin, GPIO_PIN_RESET);
    } else if (ledMillis == 2000) {
        HAL_GPIO_WritePin(LED_CONTROL_GPIO_Port, LED_CONTROL_Pin, GPIO_PIN_SET);
        ledMillis = 0;
    }

    pfm3Millis++;
    tft.tic(false);
}


void dependencyInjection() {
    encoders.insertListener(&bootloaderEncoderListener);
}

void bootJumpToApplication() {
    uint32_t jumpAddress = *(__IO uint32_t*) (APPLICATION_ADDRESS + 4);
    pFunction jumpToApplication = (pFunction) jumpAddress;

    /* Initialize user application's Stack Pointer */
    __set_MSP(*(__IO uint32_t*) APPLICATION_ADDRESS);
    jumpToApplication();
}


void flashFirmware(const FirmwarePFM3File *pfm2File) {
    FIL firmwareFile;
    FRESULT fatFSResult = f_open(&firmwareFile, firmwares.getFullName(pfm2File->name), FA_READ);
    tft.setCharBackgroundColor(COLOR_BLACK);
    tft.setCharColor(COLOR_YELLOW);
    uint32_t size = pfm2File->size;
    uint32_t offset = 0;
    UINT byteRead;
    uint32_t flashAdress = 0x8020000;
    uint32_t sizeToFlash;
    uint32_t retFlash;

    if (fatFSResult == FR_OK) {
        tft.setCursor(6, 10);
        tft.print("Flashing");
        HAL_Delay(20);
        tft.setCharColor(COLOR_LIGHT_GRAY);

        while (offset < size) {
            float percent = offset * 100 / size;
            tft.setCursor(9, 11);
            tft.print((int)percent);
            tft.print("%");
            HAL_Delay(10);

            f_read(&firmwareFile, (void*)d2Buffer, BLOCK_SIZE_UINT8, &byteRead);
            sizeToFlash = ((byteRead + 3) / 4);
            retFlash = FLASH_If_Write(flashAdress, d2Buffer, sizeToFlash);
            if (retFlash != FLASHIF_OK) {
                tft.setCharColor(COLOR_RED);
                tft.setCursor(6, 12);
                tft.print("#Error#");
                tft.print((int)retFlash);
                HAL_Delay(50);
            }
            offset += byteRead;
            flashAdress += BLOCK_SIZE_UINT8;
        }

        tft.setCursor(9, 11);
        tft.print("100%");
        HAL_Delay(10);

        tft.setCharColor(COLOR_GREEN);
        tft.setCursor(7, 12);
        tft.print("Success");
        tft.setCharColor(COLOR_WHITE);
        reboot();
    } else {
        tft.setCharColor(COLOR_RED);
        tft.setCursor(6, 12);
        tft.print("#Error#");
        while (1);
    }



}

void formatFlash(int firmwareSize) {
    FLASH_EraseInitTypeDef pEraseInit;
    uint32_t SectorError;


    tft.setCharBackgroundColor(COLOR_BLACK);
    tft.setCharColor(COLOR_YELLOW);
    tft.setCursor(6, 10);
    tft.print("Formatting");
    HAL_Delay(20);


    // Use firmware size, no need to format more
    int number = firmwareSize / 131072 + 1;
    for (int k=0; k < number; k++) {
        tft.setCharColor(COLOR_LIGHT_GRAY);
        tft.setCursor(9, 11);
        tft.print((k + 1));
        tft.print("/");
        tft.print(number);
        HAL_Delay(30);

        HAL_FLASH_Unlock();
        FLASH_If_Init();

        pEraseInit.TypeErase = FLASH_TYPEERASE_SECTORS;
        pEraseInit.Sector = k + 1;
        pEraseInit.NbSectors = 1;
        pEraseInit.VoltageRange = FLASH_VOLTAGE_RANGE_3;
        pEraseInit.Banks = FLASH_BANK_1;

        if (HAL_FLASHEx_Erase(&pEraseInit, &SectorError) != HAL_OK)
        {
            tft.setCharColor(COLOR_RED);
            tft.setCursor(6, 12);
            tft.print("#Error#");
            tft.print((int)SectorError);
            while (1);
        }

        HAL_FLASH_Lock();
    }

    tft.fillArea(0, 200, 240, 60, COLOR_BLACK);
    HAL_Delay(5);
}

// https://community.st.com/s/article/STM32H7-bootloader-jump-from-application
void jumpToBootloader(void)
{
    tft.setCharColor(COLOR_YELLOW);
    tft.setCursor(4, 12);
    tft.print("- DFU MODE -");
    HAL_Delay(1000);

    uint32_t i=0;
    void (*SysMemBootJump)(void);

    /* Set the address of the entry point to bootloader */
    volatile uint32_t BootAddr = 0x1FF09800;

    /* Disable all interrupts */
    __disable_irq();

    /* Disable Systick timer */
    SysTick->CTRL = 0;

    /* Set the clock to the default state */
    HAL_RCC_DeInit();

    /* Clear Interrupt Enable Register & Interrupt Pending Register */
    for (i=0;i<5;i++)
    {
        NVIC->ICER[i]=0xFFFFFFFF;
        NVIC->ICPR[i]=0xFFFFFFFF;
    }

    /* Re-enable all interrupts */
    __enable_irq();

    /* Set up the jump to booloader address + 4 */
    SysMemBootJump = (void (*)(void)) (*((uint32_t *) ((BootAddr + 4))));

    /* Set the main stack pointer to the bootloader stack */
    __set_MSP(*(uint32_t *)BootAddr);

    /* Call the function to jump to bootloader location */
    SysMemBootJump();

    /* Jump is done successfully */
    while (1);
}

void reboot() {
    tft.setCharColor(COLOR_YELLOW);
    tft.setCursor(5, 12);
    tft.print("- REBOOT -");
    HAL_Delay(2500);
    __set_MSP(*(__IO uint32_t*) 0x08000000);
    NVIC_SystemReset();
    while (1);
}

void SDCardAccess() {
    tft.fillArea(0, 24, 240, 320-24, COLOR_BLACK);

    tft.setCharColor(COLOR_WHITE);
    tft.setCursor(4, 3);
    tft.print("SD CARD ACCESS");

    tft.setCharColor(COLOR_WHITE);
    tft.setCursorSmallChar(4, 14);
    tft.printSmallChars("Unmount the drive on your");
    tft.setCursorSmallChar(4, 15);
    tft.printSmallChars("computer before exiting.");

    tft.setCharColor(COLOR_YELLOW);
    tft.setCursor(2, 14);
    tft.print("Press MENU to exit");

    MX_USB_DEVICE_Start();

    tft.setCharColor(COLOR_GRAY);

    // Exit when MENU button is pressed
    while (getButtonPressed() != 6) {


        HAL_Delay(2);

        // Animation while displaying R and W
        if (readDisplayCounter > 0) {
            if ((readDisplayCounterAnim % 50) == 0) {
                tft.setCharColor(COLOR_GREEN);
                tft.setCursor(9, 5);
                tft.print("R");
                readAnimationIndex  = (readAnimationIndex + 1) % 4;
                tft.print((char)sdAccessAnimation[readAnimationIndex]);
            }
            readDisplayCounterAnim++;
            readDisplayCounter--;
        } else if (readDisplayCounter == 0) {
            tft.setCursor(9, 5);
            tft.print("  ");
            // Last one not to enter any tft.print
            readDisplayCounter--;
            readDisplayCounterAnim = 0;
        }

        if (writeDisplayCounter > 0) {
            if ((writeDisplayCounterAnim % 50) == 0) {
                tft.setCharColor(COLOR_RED);
                tft.setCursor(11, 5);
                tft.print("W");
                writeAnimationIndex  = (writeAnimationIndex + 1) % 4;
                tft.print((char)sdAccessAnimation[writeAnimationIndex]);
            }
            writeDisplayCounterAnim++;
            writeDisplayCounter--;
        } else if (writeDisplayCounter == 0) {
            tft.setCursor(11, 5);
            tft.print("  ");
            writeDisplayCounter--;
            writeDisplayCounterAnim = 0;
        }
    }

    MX_USB_DEVICE_Stop();

    fileSelect = 0;
    firmwares.reset();

    state = STATE_INIT;
}


/**
  * @brief Tx Transfer completed callback.
  * @param  hspi: pointer to a SPI_HandleTypeDef structure that contains
  *               the configuration information for SPI module.
  * @retval None
  */
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi) {
    // Unselect TFT as soon as the transfer is finished to avoid noise on data line
    if (hspi == &hspi1) {
        tft.pushToTftFinished();
    }
}


#ifdef __cplusplus
}
#endif
