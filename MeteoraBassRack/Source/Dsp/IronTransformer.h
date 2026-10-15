#pragma once
// =============================================================================
//  IronTransformer - modello di trasformatore audio a nucleo di ferro
//
//  Il flusso magnetico nel nucleo è l'integrale della tensione: per questo un
//  trasformatore satura soprattutto sulle BASSE frequenze, mentre medi e alti
//  restano puliti. Il modello segue il circuito reale:
//    - induttanza di magnetizzazione + impedenza di sorgente -> taglio basso dolce
//    - saturazione del nucleo (cresce col flusso: frequenze basse e livelli alti)
//    - isteresi (operatore "play"): poche armoniche dispari anche a basso livello
//    - perdita di alte (induttanza dispersa + capacità degli avvolgimenti)
//  Con i valori dell'UTC HA-100X (ingresso LA-2A) una nota di basso a 40 Hz
//  a -6 dBFS dà circa 0,3% di terza armonica, a 0 dBFS circa 1%; a 1 kHz è pulito.
// =============================================================================
#include "DspCommon.h"

namespace meteora
{
class IronTransformer
{
public:
    struct Spec
    {
        double lowCutHz   = 10.0;    // taglio basso (-3 dB) dovuto all'induttanza primaria
        double refHz      = 30.0;    // un sinusoide a 0 dBFS a questa frequenza porta il flusso a 1
        double saturation = 0.10;    // quantità di saturazione del nucleo
        double hysteresis = 0.05;    // quantità di isteresi
        double hystWidth  = 0.010;   // larghezza del ciclo di isteresi (unità di flusso)
        double highCutHz  = 45000.0; // perdita di alte
    };

    void prepare (double sampleRate, const Spec& s)
    {
        spec = s;
        fs = sampleRate;
        g = 2.0 * kPi * spec.refHz / fs;
        kLin = spec.lowCutHz / spec.refHz;
        useHighCut = spec.highCutHz < 0.45 * fs;
        highCut.setCutoff (fs, spec.highCutHz);
        reset();
    }

    void reset() noexcept
    {
        phi[0] = phi[1] = 0.0;
        play[0] = play[1] = 0.0;
        highCut.reset();
    }

    inline double process (double x, int ch) noexcept
    {
        const double p = phi[ch];

        // corrente di magnetizzazione non lineare: il nucleo "cede" quando il flusso cresce
        const double nl = p * p * p / (1.0 + 0.3 * p * p);

        // isteresi con operatore play (continuo, nessuno scatto)
        double& m = play[ch];
        if (p > m + spec.hystWidth)      m = p - spec.hystWidth;
        else if (p < m - spec.hystWidth) m = p + spec.hystWidth;

        double y = x - kLin * p - spec.saturation * nl - spec.hysteresis * (p - m);

        phi[ch] = p + g * y;   // il flusso integra la tensione ai capi dell'avvolgimento

        if (useHighCut) y = highCut.lowPass (y, ch);
        return y;
    }

    // Trasformatori usati nel rack
    static Spec utcHA100X()  { return { 8.0,  30.0, 0.11, 0.06, 0.012, 45000.0 }; } // ingresso LA-2A
    static Spec la2aOutput() { return { 12.0, 30.0, 0.05, 0.03, 0.010, 35000.0 }; } // uscita LA-2A
    static Spec api2503()    { return { 6.0,  30.0, 0.04, 0.02, 0.008, 60000.0 }; } // uscita API 550B

private:
    Spec spec;
    double fs = 44100.0, g = 0.0, kLin = 0.0;
    double phi[2] { 0, 0 }, play[2] { 0, 0 };
    bool useHighCut = false;
    OnePole highCut;
};
} // namespace meteora
