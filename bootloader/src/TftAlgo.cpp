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

void TftAlgo::setBufferAdress(uint16_t* bufferAdress) {
}

void TftAlgo::setFGBufferAdress(uint8_t* bufferAdress) {
}

void TftAlgo::setColor(uint16_t col) {
}

void TftAlgo::plot(uint8_t mode, int16_t x, int16_t y) {
}

void TftAlgo::hLine(int16_t x0, int16_t x1, int16_t y) {
}

void TftAlgo::vLine(int16_t y0, int16_t y1, int16_t x) {
}

void TftAlgo::fillRect(int16_t x, int16_t y, int16_t width, int16_t height) {
}

void TftAlgo::drawLine(uint8_t mode, int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
}

void TftAlgo::drawNumber(int x, int y, int opNum) {
}

void TftAlgo::drawOperator(uint8_t opNum) {
}

void TftAlgo::highlightOperator(bool draw, uint8_t opNum) {
}

void TftAlgo::drawIM(uint8_t mode, uint8_t imNum, uint8_t opSource, uint8_t opDest) {
}

// Draw the IM on
void TftAlgo::highlightIM(bool draw, uint8_t imNum, uint8_t opSource, uint8_t opDest) {
}

void TftAlgo::drawFeedback(uint8_t mode, uint8_t opSource, uint8_t opDest) {
}

void TftAlgo::highlightFeedback(bool draw, uint8_t opSource, uint8_t opDest) {
}

void TftAlgo::drawCarrierArrow(uint8_t opNum) {
}

void TftAlgo::computeLayout() {
}

void TftAlgo::drawAlgoFromTable(const uint8_t* algoInfo) {
}

void TftAlgo::drawSegmentGlyph(uint16_t* buffer, uint16_t width, uint16_t height, int16_t x, int16_t y, uint8_t glyph, uint16_t color) {
}