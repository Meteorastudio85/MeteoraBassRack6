#pragma once
// =============================================================================
//  VOG - risonanza sulle basse ("passa-alto risonante")
//  Una campana di risonanza sulla frequenza scelta e, sotto, un taglio passa-alto
//  che pulisce il sub. Così il basso guadagna peso dove serve senza fango sotto.
//    amplitude: altezza della risonanza (0..+15 dB)
//    frequency: dove cade la risonanza (range 40: 20-80 Hz, range 200: 100-400 Hz)
//    q:         larghezza della risonanza
//    hp:        taglio sotto la risonanza; 24dB: pendenza 12 o 24 dB/ottava
// =============================================================================
#include "DspCommon.h"

class AirwindowsVog
{
public:
    void prepare (double sampleRate);
    void reset();

    void setFrequency (float hz);
    void setAmplitudeDb (float db);      // 0..15
    void setQ (float q);                 // 0,5..4
    void setOutputDb (float db);         // -12..+12
    void setHighPass (bool on);
    void setSlope24 (bool on);

    void process (float* const* data, int numChannels, int numSamples) noexcept;

private:
    void updateFilters() noexcept;

    double fs = 44100.0, glide = 0.1;
    meteora::Biquad resonance, hp12, hp24a, hp24b;
    meteora::Glide logFreq, ampDb, q, outDb, hpMix, slopeMix;
};
