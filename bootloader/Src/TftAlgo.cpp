/*
 * TftAlgos.cpp
 *
 *  dummy algo for bootloader
 *      Author: xavier
 */

#include "TftAlgo.h"

TftAlgo::TftAlgo() {
}

TftAlgo::~TftAlgo() {
}

void TftAlgo::setBufferAdress(uint16_t *bufferAdress) {
}

void TftAlgo::setFGBufferAdress(uint8_t *bufferAdress) {
}

void TftAlgo::setColor(uint16_t col) {
}

void TftAlgo::drawLine(uint8_t mode, int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
}

void TftAlgo::drawNumber(int x, int y, int opNum) {
}

void TftAlgo::highlightOperator(uint8_t opPosition) {
}

void TftAlgo::drawOperator(uint8_t opNum, uint8_t opPosition) {
}

void TftAlgo::drawIM(uint8_t imNum, uint8_t opSource, uint8_t opDest) {
}

// Draw the IM on
void TftAlgo::highlightIM(bool draw, uint8_t imNum, uint8_t opSource, uint8_t opDest) {
}

void TftAlgo::drawMix(uint8_t imNum) {
}

void TftAlgo::drawAlgoOperator(int algo, int op) {
}

void TftAlgo::drawAlgo(int algo) {
}


const uint8_t* TftAlgo::getDigitBits(uint8_t digit) {
    return 0;
}
