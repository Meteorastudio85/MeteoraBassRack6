#pragma once
// =============================================================================
//  Tube Smile - emulazione del circuito Tube Screamer TS808
//
//  Nel TS808 lo stadio di clipping è un op-amp non invertente con i diodi nel
//  feedback e un filtro 4,7k/0,047uF (720 Hz) che dà guadagno SOLO sopra i 720 Hz.
//  Risultato: le basse passano pulite a guadagno unitario e distorcono i medi.
//  È esattamente il motivo per cui sul basso il TS mantiene il fondamentale.
//    uscita clip = ingresso + diodi( (G-1) * passaAlto720(ingresso) )
//    G-1 = (51k + Rdrive) / 4,7k     (Rdrive = 500k logaritmico)
//    51 pF nel feedback -> passa-basso che scende quando alzi il drive
//  Poi: filtro di tono (1k/0,22uF = 723 Hz + controllo attivo), LEVEL, BLEND.
// =============================================================================
#include "DspCommon.h"

class AirwindowsTubeSmile
{
public:
    void prepare (double sampleRate);
    void reset();

    void setInputDb (float db);           // INPUT: quanto segnale entra nel pedale (-12..+12 dB)
    void setDrive (float drive0to10);
    void setTone (float tone0to10);
    void setLevel (float level0to10);     // OUTPUT del pedale: volume di tutto il pedale, 5 = 0 dB
    void setBlend (float wetPercent);     // 0 = solo pulito, 100 = solo pedale

    void process (float* const* data, int numChannels, int numSamples) noexcept;

private:
    static double diodePair (double v) noexcept;

    double fs = 44100.0, glide = 0.1;
    meteora::OnePole gainHighPass, feedbackLowPass, toneLowPass, dcBlock;
    meteora::Glide inputDb, drive, tone, levelDb, blend;
};
