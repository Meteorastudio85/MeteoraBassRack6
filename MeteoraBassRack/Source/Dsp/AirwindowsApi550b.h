#pragma once
// =============================================================================
//  API 550B - EQ a 4 bande con "Proportional Q", stadio 2520 e trasformatore 2503
// =============================================================================
#include "DspCommon.h"
#include "IronTransformer.h"

class AirwindowsApi550b
{
public:
    // Frequenze reali a scatti dell'API 550B
    static constexpr float bandFreqs[4][7] = {
        { 30.0f,   40.0f,   50.0f,   100.0f,   200.0f,   300.0f,   400.0f   },
        { 75.0f,   150.0f,  180.0f,  240.0f,   500.0f,   700.0f,   1000.0f  },
        { 800.0f,  1500.0f, 3000.0f, 5000.0f,  8000.0f,  10000.0f, 12500.0f },
        { 2500.0f, 5000.0f, 7000.0f, 10000.0f, 12500.0f, 15000.0f, 20000.0f }
    };

    void prepare (double sampleRate);
    void reset();

    // band 0..3, freqIndex 0..6, gainDb -12..+12 (scatti da 2 dB come l'originale)
    // shelf: solo per le bande 1 e 4 (indice 0 e 3), come sul 550B
    void setBand (int band, int freqIndex, float gainDb, bool shelf);

    void process (float* const* data, int numChannels, int numSamples) noexcept;

private:
    void updateBand (int band) noexcept;
    static double opAmp2520 (double x) noexcept;

    double fs = 44100.0, glide = 0.1;
    meteora::Biquad filters[4];
    meteora::Glide logFreq[4], gain[4];
    bool shelfWanted[4] { false, false, false, false };
    bool shelfActive[4] { false, false, false, false };
    meteora::IronTransformer outputTransformer;
};
