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



#ifndef HARDWARE_TFTALGO_H_
#define HARDWARE_TFTALGO_H_

#define PREENFM_FREQUENCY 47916.0f

#include "stm32h7xx_hal.h"
// #include "Common.h"

enum {
	OPERATOR = 1,
	IM,
	MIX,
	END,
};


struct ModulationIndex {
	unsigned int source:4;
	unsigned int destination:4;
};



class TftAlgo {
public:
	TftAlgo();
	virtual ~TftAlgo();

	void setBufferAdress(uint16_t *bufferAdress);
	void setFGBufferAdress(uint8_t *bufferAdress);
	void drawAlgo(int algo);
	void drawAlgoOperator(int algo, int op);
	void highlightIM(bool draw, uint8_t imNum, uint8_t opSource, uint8_t opDest);
    const uint8_t* getDigitBits(uint8_t digit);

private:
	void drawLine(uint8_t mode, int16_t x1, int16_t y1, int16_t x2, int16_t y2);
	void setColor(uint16_t col);
	void drawNumber(int x, int y, int digit);
	void drawOperator(uint8_t opNum, uint8_t opPosition);
	void highlightOperator(uint8_t opNum);
	void drawIM(uint8_t mode, uint8_t imNum, uint8_t opSource, uint8_t opDest);
	void drawMix(uint8_t imNum);

	uint8_t operatorPosition[6];
	uint8_t imSource[6];
	uint8_t imDest[6];
	uint8_t operatorMix[6];
	uint16_t *buffer;
	uint8_t *fgBuffer;
	uint16_t color;

};

#endif /* HARDWARE_TFTALGO_H_ */
