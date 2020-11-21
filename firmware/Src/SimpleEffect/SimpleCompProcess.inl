/*
 *	Simple Compressor (runtime function)
 *
 *  File		: SimpleCompProcess.inl
 *	Library		: SimpleSource
 *  Version		: 1.12
 *  Implements	: void SimpleComp::process( float &in1, float &in2 )
 *				  void SimpleComp::process( float &in1, float &in2, float keyLinked )
 *				  void SimpleCompRms::process( float &in1, float &in2 )
 *
 *	� 2006, ChunkWare Music Software, OPEN-SOURCE
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


/*
 * Adapted by Xavier Hosxe for the preenfm3
 */

#ifndef __SIMPLE_COMP_PROCESS_INL__
#define __SIMPLE_COMP_PROCESS_INL__

#define INV32 0.03125f
#define unlikely(x)     __builtin_expect((x),0)

namespace chunkware_simple
{

	//-------------------------------------------------------------
	INLINE float SimpleComp::processPfm3( float *inStereo )
	{
	    float *startBuffer = inStereo;
	    float maxAbsSample = 0.0f;
	    for (int s = 0; s < 32; s++) {
            float rect1 = abs( *inStereo++ );	// rectify input
            float rect2 = abs( *inStereo++ );
            maxAbsSample = std::max( maxAbsSample, std::max( rect1, rect2 )); // find the max
	    }
	    float gain = getGain(maxAbsSample);
	    // Let's slowly (linearly) reach  the new gain
	    float incGain = (gain - previousGain_) * INV32;
        if (gain < (1.0f - DC_OFFSET)) {
            inStereo = startBuffer;
            for (int s = 0; s < 32; s++) {
                previousGain_ += incGain;
                *(inStereo++) *= previousGain_;
                *(inStereo++) *= previousGain_;
            }
        }
        previousGain_ = gain;
        return gain;
	}

    //-------------------------------------------------------------
    INLINE float SimpleComp::getGain(float sample )
    {
        // convert key to dB
        float keydB = lin2dB( sample );  // convert linear -> dB

        // Let's keep the max during 127 calls.
        keydBMaxCpt_++;

        keydBMaxTmp_ = std::max(keydBMaxTmp_, keydB);

        // Update max immediately to avoid latency on mixer metter
        if (unlikely(keydBMaxTmp_ > keydBMax_)) {
            keydBMax_ = keydBMaxTmp_;
        }

        if (unlikely(keydBMaxCpt_ == 0)) {
            // Update the value we return;
            keydBMax_ = keydBMaxTmp_;
            keydBMaxTmp_ = -100.0f;
        }

        // No compressor ?
        if (threshdB_ > 100.0f) {
            gr_ = 0.0f;
            return 1.0f;
        }

        // threshold
        float overdB = keydB - threshdB_;   // delta over threshold
        if ( overdB < 0.0 )
            overdB = 0.0;

        // attack/release
        AttRelEnvelope::run( overdB, envdB_ );  // run attack/release envelope

        overdB = envdB_;

        // transfer function
        gr_ = overdB * ( ratio_ - 1.0 );   // gain reduction (dB)
        return dB2lin( gr_ );              // convert dB -> linear

    }

}	// end namespace chunkware_simple

#endif	// end __SIMPLE_COMP_PROCESS_INL__
