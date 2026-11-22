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
	INLINE float SimpleComp::compressBlock( float *inStereo )
	{
	    float *startBuffer = inStereo;
	    float maxAbsSample = 0.0f;
	    float sumAbsSample = 0.0f;
	    for (int s = 0; s < 32; s++) {
            float rect1 = fabsf( *inStereo++ );	// rectify input (VABS.F32 hardware instruction)
            float rect2 = fabsf( *inStereo++ );
            float pairMax = std::max( rect1, rect2 );
            maxAbsSample = std::max( maxAbsSample, pairMax ); // find the max
            sumAbsSample += pairMax;
	    }
	    float meanAbsSample = sumAbsSample * INV32;
	    float detSample = (ratio_ > 0.35f)
	        ? (0.55f * maxAbsSample + 0.45f * meanAbsSample)
	        : (0.85f * maxAbsSample + 0.15f * meanAbsSample);
	    float gain = getGain(detSample);

        if (threshdB_ <= 100.0f) {
            // Linearly interpolate gain across the block in FPU register
            float incGain = (gain - previousGain_) * INV32;
            float g = previousGain_;
            float *p = startBuffer;
            // Unroll by 2 to pipeline FPU operations and prevent stalls
            for (int s = 0; s < 16; s++) {
                g += incGain;
                p[0] *= g;
                p[1] *= g;
                g += incGain;
                p[2] *= g;
                p[3] *= g;
                p += 4;
            }
            previousGain_ = gain;
        } else {
            previousGain_ = 1.0f;
        }
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

        // Soft-knee threshold calculation
        float delta = keydB - threshdB_;
        float overdB = 0.0f;

        if (kneeQuarterdB_ > 0) {
            float halfKnee = kneeQuarterdB_ * 0.125f; // (kneeQuarterdB_ * 0.25f) * 0.5f
            if (delta <= -halfKnee) {
                overdB = 0.0f;
            } else if (delta >= halfKnee) {
                overdB = delta;
            } else {
                float kneePos = delta + halfKnee;
                float inv2Knee = 2.0f / (float)kneeQuarterdB_;
                overdB = (kneePos * kneePos) * inv2Knee;
            }
        } else {
            if (delta > 0.0f) {
                overdB = delta;
            }
        }

        // Program-dependent / adaptive envelope detection (no per-block expf or release speedup)
        float prevEnv = envdB_;
        AttRelEnvelope::run( overdB, envdB_ );
        if (overdB <= prevEnv && prevEnv > 4.0f) {
            // Smoother release on deep/sustained compression without speeding up small modulations
            envdB_ += 0.25f * (prevEnv - envdB_);
        }

        // transfer function
        gr_ = envdB_ * ( ratio_ - 1.0f );   // gain reduction (dB, <= 0)
        float totalGaindB = gr_ + (makeupQuarterdB_ * 0.25f);
        return dB2lin( totalGaindB );       // convert combined gain reduction + make-up -> linear
    }

}	// end namespace chunkware_simple

#endif	// end __SIMPLE_COMP_PROCESS_INL__