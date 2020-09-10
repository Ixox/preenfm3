/*
 * TftAlgo.h
 *
*  Created on: July 1, 2019
  *      Author: xavier
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
	void drawIM(uint8_t imNum, uint8_t opSource, uint8_t opDest);
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
