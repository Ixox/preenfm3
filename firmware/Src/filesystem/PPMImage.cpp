/*
 * Copyright 2020 Xavier Hosxe
 *
 * Author: Xavier Hosxe (xavier <dot> hosxe (at) g m a i l <dot> com)
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


#include <PPMImage.h>

#ifdef PPMIMAGE_ENABLE
#include "TftDisplay.h"
#include "fatfs.h"

extern TftDisplay tft;


__attribute__((section(".ram_d2"))) char imagePPM[6400 * 3];
extern uint16_t tftMemory[240 * 320];


PPMImage::PPMImage() {
    cptImage = 0;

    char imageTitlePattern[] = "0:/PPM/img_####.ppm";
    for (int i = 0; i < 32; i++) {
    	imageTitle[i] = 0;
    }
    for (int i = 0; imageTitlePattern[i] != 0; i++) {
    	imageTitle[i] = imageTitlePattern[i];
    }
}

PPMImage::~PPMImage() {
}

void PPMImage::init() {
    int cpt = 0;
    FILINFO fileInfo;
    FRESULT fresult = FR_OK;

    // Calculcate sharpIndexInName once
    sharpIndexInName = 0;
    while (imageTitle[sharpIndexInName] != '#'
			&& imageTitle[sharpIndexInName] != 0
			&& sharpIndexInName < 50) {
    	sharpIndexInName++;
    }

    // We don't care about the result and we call this only once in the init() method
    f_mkdir("0:/PPM");

    do {
        cpt++;
        updateImageName(cpt);
        fresult = f_stat(imageTitle, &fileInfo );
    } while (fresult != FR_NO_FILE);

    cptImage = cpt;
}

void PPMImage::updateImageName(int cpt) {
	imageTitle[sharpIndexInName] = '0' + ((cpt % 10000) / 1000);
	imageTitle[sharpIndexInName+1] = '0' + ((cpt % 1000) / 100);
	imageTitle[sharpIndexInName+2] = '0' + ((cpt % 100) / 10);
	imageTitle[sharpIndexInName+3] = '0' + (cpt % 10);
}

void PPMImage::saveImage() {


	if (!isInitialized) {
        isInitialized = true;
        init();
    }

    FIL imageFile;
    UINT byteWriten;
    int errorNumber = 0;
    updateImageName(cptImage);

    if (f_open(&imageFile, imageTitle, FA_OPEN_ALWAYS | FA_WRITE) != FR_OK) {
    	errorNumber = 1;
    }
    if (f_write(&imageFile, "P6\n240 320\n255\n", 15, &byteWriten)) {
    	errorNumber = 2;

    }


    for (int j = 0; j < 320; ++j) {
        for (int i = 0; i < 240; ++i) {
            int iIndex = (j * 240 + i) % 6400;
            uint16_t p = tftMemory[j * 240 + i];
            uint16_t pixel = (p >> 8) + (p << 8);
            imagePPM[iIndex * 3 + 0] = (uint8_t) ((pixel & 0xF800) >> 11) << 3; /* red   */
            imagePPM[iIndex * 3 + 1] = (uint8_t) ((pixel & 0x07E0) >> 5) << 2; /* green */
            imagePPM[iIndex * 3 + 2] = (uint8_t) ((pixel & 0x001F) << 3); /* blue  */
            UINT byteWriten;
            if (iIndex == 6399) {
                FRESULT fres = f_write(&imageFile, imagePPM, 6400 * 3, &byteWriten);
                if (fres != FR_OK || byteWriten != 6400* 3) {
                	errorNumber = 3;
                }
            }
        }
    }


    if ( f_close(&imageFile) != FR_OK) {
    	errorNumber = 4;
    }

	tft.fillArea(22, 0, 240, 320-22, COLOR_BLACK);
	tft.setCharBackgroundColor(COLOR_BLACK);
    if (errorNumber == 0) {
		tft.setCharColor(COLOR_WHITE);
    } else {
		tft.setCharColor(COLOR_RED);
	    tft.setCursor(5, 4);
	    tft.print("##ERROR : ");
	    tft.print(errorNumber);
    }
    tft.setCursor(4, 6);
	tft.print(imageTitle + 12);

	HAL_Delay(1000);

    cptImage = (cptImage + 1) % 10000;
}
#endif
