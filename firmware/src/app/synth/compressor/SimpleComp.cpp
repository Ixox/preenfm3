#include "SimpleComp.h"

#if defined(__GNUC__) && !defined(DEBUG)
#pragma GCC push_options
#pragma GCC optimize("Ofast", "fast-math")
#endif
/*
 *	Simple Compressor (source)
 *
 *  File		: SimpleComp.cpp
 *	Library		: SimpleSource
 *  Version		: 1.12
 *  Implements	: SimpleComp, SimpleCompRms
 *
 *	 2006, ChunkWare Music Software, OPEN-SOURCE
 *
 *	Permission is hereby granted, free of charge, to any person obtaining a
 *	copy of this software and associated documentation files (the "Software"),
 *	to deal in the Software without restriction, including without limitation
 *	the rights to use, copy, modify, merge, publish, distribute, sublicense,
 *	and/or sell copies of the Software, and to permit persons to whom the
 *	Software is furnished to do so, subject to the following conditions:
 *
 *	The above copyright notice and this permission notice shall be included in
 *	all copies or substantial portions of the Software.
 *
 *	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 *	THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *	FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 *	DEALINGS IN THE SOFTWARE.
 */
namespace chunkware_simple {
//-------------------------------------------------------------
// simple compressor
//-------------------------------------------------------------
SimpleComp::SimpleComp()
    : AttRelEnvelope(10.0, 100.0), threshdB_(0.0), ratio_(1.0), envdB_(DC_OFFSET), previousGain_(1.0f), keydBMax_(-100.0f),
      keydBMaxTmp_(-100.0f), gr_(0.0f), keydBMaxCpt_(0), kneeQuarterdB_(0), makeupQuarterdB_(0) {
}

//-------------------------------------------------------------
void SimpleComp::setThresh(float dB) {
    threshdB_ = dB;
}

//-------------------------------------------------------------
void SimpleComp::setRatio(float ratio) {
    assert(ratio > 0.0);
    ratio_ = ratio;
}

//-------------------------------------------------------------
void SimpleComp::setKnee(float kneeWidthdB) {
    if (kneeWidthdB < 0.0f)
        kneeWidthdB = 0.0f;
    if (kneeWidthdB > 60.0f)
        kneeWidthdB = 60.0f;
    kneeQuarterdB_ = (uint8_t)(kneeWidthdB * 4.0f + 0.5f);
}

//-------------------------------------------------------------
void SimpleComp::setMakeup(float makeupdB) {
    if (makeupdB < 0.0f)
        makeupdB = 0.0f;
    if (makeupdB > 60.0f)
        makeupdB = 60.0f;
    makeupQuarterdB_ = (uint8_t)(makeupdB * 4.0f + 0.5f);
}

//-------------------------------------------------------------
void SimpleComp::initRuntime(void) {
    envdB_ = DC_OFFSET;
    previousGain_ = 1.0f;
}

//-------------------------------------------------------------
// simple compressor with RMS detection
//-------------------------------------------------------------
SimpleCompRms::SimpleCompRms() : ave_(5.0), aveOfSqrs_(DC_OFFSET) {
}

//-------------------------------------------------------------
void SimpleCompRms::setSampleRate(float sampleRate) {
    SimpleComp::setSampleRate(sampleRate);
    ave_.setSampleRate(sampleRate);
}

//-------------------------------------------------------------
void SimpleCompRms::setWindow(float ms) {
    ave_.setTc(ms);
}

//-------------------------------------------------------------
void SimpleCompRms::initRuntime(void) {
    SimpleComp::initRuntime();
    aveOfSqrs_ = DC_OFFSET;
}

} // end namespace chunkware_simple

#if defined(__GNUC__) && !defined(DEBUG)
#pragma GCC pop_options
#endif