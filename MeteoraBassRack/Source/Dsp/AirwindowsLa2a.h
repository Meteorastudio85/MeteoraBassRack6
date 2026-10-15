#pragma once
// =============================================================================
//  LA-2A - compressore ottico T4B
//  Ingresso: trasformatore UTC HA-100X -> cella ottica T4B (topologia a feedback,
//  attacco ~10 ms, rilascio in due fasi: ~60 ms per il primo 50%, poi 1-5 s
//  in base a quanto ha lavorato) -> stadio valvolare 12AX7/12BH7 -> trasformatore d'uscita.
// =============================================================================
#include <atomic>
#include "DspCommon.h"
#include "IronTransformer.h"

class AirwindowsLa2a
{
public:
    void prepare (double sampleRate);
    void reset();

    void setPeakReduction (float amount0to100);   // quantità di segnale mandata alla cella
    void setInputDb (float db);                   // INPUT: livello che entra nel compressore
    void setGainDb (float makeupDb);              // guadagno di uscita (fisso a 0 dB nel rack)
    void setLimit (bool shouldLimit);             // LIMIT / COMP
    void setOutputDb (float db);                  // OUTPUT del pannello LA-2A (dopo valvola e trasformatore)

    void process (float* const* data, int numChannels, int numSamples) noexcept;

    // Riduzione di guadagno massima dall'ultima lettura (dB positivi), per il VU
    float popGainReductionDb() noexcept { return maxGrDb.exchange (0.0f); }

private:
    static double tubeStage (double v) noexcept;

    double fs = 44100.0, glide = 0.1;
    meteora::IronTransformer inputTransformer, outputTransformer;
    meteora::OnePole sidechainEmphasis;
    meteora::Glide sidechainGain, makeup, outputTrim, inputGain;
    bool limitMode = false;

    // stato della cella ottica (unica, stereo linkata come l'hardware)
    double elPanel = 0.0, cell = 0.0, memory = 0.0;
    double lastOut[2] { 0.0, 0.0 };
    double aAttack = 0, aRelease = 0, aCell = 0, aMemCharge = 0, aMemRelease = 0;

    std::atomic<float> maxGrDb { 0.0f };
};
