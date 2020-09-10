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

#include "fatfs.h"
#include <PPMImage.h>


__attribute__((section(".ram_d2"))) char imagePPM[6400 * 3];
static char imageTitle[] = "0:/pfm3/img_####.ppm";

extern uint16_t tftMemory[240 * 320];


PPMImage::PPMImage() {
    cptImage = 0;
}

PPMImage::~PPMImage() {
}

void PPMImage::init() {
    int cpt = 0;
    FILINFO fileInfo;
    FRESULT fresult = FR_OK;



    do {
        cpt++;
        updateImageName(cpt);
        fresult = f_stat(imageTitle, &fileInfo );
    } while (fresult != FR_NO_FILE);

    cptImage = cpt;
}

void PPMImage::updateImageName(int cpt) {
    imageTitle[12] = '0' + ((cpt % 10000) / 1000);
    imageTitle[13] = '0' + ((cpt % 1000) / 100);
    imageTitle[14] = '0' + ((cpt % 100) / 10);
    imageTitle[15] = '0' + (cpt % 10);
}

void PPMImage::saveImage() {

    if (!isInitialized) {
        isInitialized = true;
        init();
    }

    FIL imageFile;
    UINT byteWriten;

    updateImageName(cptImage);
    cptImage = (cptImage + 1) % 10000;


    f_open(&imageFile, imageTitle, FA_OPEN_ALWAYS | FA_WRITE);
    f_write(&imageFile, "P6\n240 320\n255\n", 15, &byteWriten);

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
                f_write(&imageFile, imagePPM, 6400 * 3, &byteWriten);
            }
        }
    }
    (void) f_close(&imageFile);

}
