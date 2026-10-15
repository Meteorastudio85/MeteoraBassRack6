#include "AirwindowsLa2a.h"

using namespace meteora;

void AirwindowsLa2a::prepare (double sampleRate)
{
    fs = sampleRate;
    glide = glideCoeff (fs, 0.03);

    inputTransformer.prepare (fs, IronTransformer::utcHA100X());
    outputTransformer.prepare (fs, IronTransformer::la2aOutput());
    sidechainEmphasis.setCutoff (fs, 1500.0);   // il sidechain dell'LA-2A sente di più gli alti

    auto coeff = [this] (double seconds) { return 1.0 - std::exp (-1.0 / (seconds * fs)); };
    aAttack     = coeff (0.010);   // attacco T4B ~10 ms
    aRelease    = coeff (0.060);   // primo 50% del rilascio ~60 ms
    aCell       = coeff (0.008);   // inerzia della fotoresistenza (riduce il ripple sulle basse)
    aMemCharge  = coeff (0.900);   // la cella "si carica" se comprime a lungo...
    aMemRelease = coeff (2.500);   // ...e poi rilascia lentamente (coda da 1 a 5 s)

    if (! sidechainGain.initialised) sidechainGain.setTarget (0.0);
    if (! makeup.initialised)        makeup.setTarget (0.0);
    if (! outputTrim.initialised)    outputTrim.setTarget (0.0);
    if (! inputGain.initialised)     inputGain.setTarget (0.0);
    inputGain.snap();
    sidechainGain.snap();
    makeup.snap();
    outputTrim.snap();
    reset();
}

void AirwindowsLa2a::reset()
{
    inputTransformer.reset();
    outputTransformer.reset();
    sidechainEmphasis.reset();
    elPanel = cell = memory = 0.0;
    lastOut[0] = lastOut[1] = 0.0;
}

void AirwindowsLa2a::setPeakReduction (float amount)
{
    const double p = std::clamp ((double) amount, 0.0, 100.0) / 100.0;
    sidechainGain.setTarget (40.0 * std::pow (p, 2.2));   // 0 = nessuna compressione
}

void AirwindowsLa2a::setGainDb (float makeupDb)   { makeup.setTarget (std::clamp ((double) makeupDb, -20.0, 20.0)); }
void AirwindowsLa2a::setLimit (bool shouldLimit)  { limitMode = shouldLimit; }
void AirwindowsLa2a::setInputDb (float db)        { inputGain.setTarget (std::clamp ((double) db, -20.0, 20.0)); }
void AirwindowsLa2a::setOutputDb (float db)       { outputTrim.setTarget (std::clamp ((double) db, -20.0, 20.0)); }

// 12AX7 + 12BH7: leggera asimmetria (armoniche pari) e tanto headroom.
// La componente continua generata dall'asimmetria la toglie il trasformatore d'uscita.
double AirwindowsLa2a::tubeStage (double v) noexcept
{
    constexpr double headroom = 0.22, bias = 0.12;
    static const double t0 = std::tanh (bias);
    static const double norm = 1.0 / (1.0 - t0 * t0);
    return (std::tanh (v * headroom + bias) - t0) * norm / headroom;
}

void AirwindowsLa2a::process (float* const* data, int numChannels, int numSamples) noexcept
{
    numChannels = std::min (numChannels, 2);
    double minGain = 1.0;

    for (int start = 0; start < numSamples; start += kChunk)
    {
        const int len = std::min (kChunk, numSamples - start);
        sidechainGain.advance (glide);
        makeup.advance (glide);
        outputTrim.advance (glide);
        inputGain.advance (glide);
        const double inGain = dbToGain (inputGain.current);
        const double scGain = sidechainGain.current;
        const double outGain = dbToGain (makeup.current);
        const double trim = dbToGain (outputTrim.current);

        for (int i = start; i < start + len; ++i)
        {
            // 1) rilevatore a feedback: legge l'uscita della cella (campione precedente)
            double det = 0.0;
            for (int ch = 0; ch < numChannels; ++ch)
            {
                const double s = lastOut[ch] + 0.6 * sidechainEmphasis.highPass (lastOut[ch], ch);
                det = std::max (det, std::abs (s));
            }
            det *= scGain;

            // 2) pannello elettroluminescente + fotoresistenza
            elPanel += (det - elPanel) * (det > elPanel ? aAttack : aRelease);
            cell    += (elPanel - cell) * aCell;
            memory  += (cell - memory) * (cell > memory ? aMemCharge : aMemRelease);
            const double light = std::max (cell, memory * 0.55);

            double gain = limitMode ? 1.0 / (1.0 + std::pow (1.6 * light, 3.0))   // LIMIT: rapporto più alto
                                    : 1.0 / (1.0 + light * light);               // COMP: ~3:1 morbido
            gain = std::max (gain, 0.02);
            minGain = std::min (minGain, gain);

            for (int ch = 0; ch < numChannels; ++ch)
            {
                double x = inputTransformer.process (data[ch][i] * inGain, ch);
                x *= gain;
                lastOut[ch] = x;
                x = tubeStage (x * outGain);
                x = outputTransformer.process (x, ch);
                data[ch][i] = (float) (x * trim);
            }
        }
    }

    const float grDb = (float) (-20.0 * std::log10 (minGain));
    float prev = maxGrDb.load();
    while (grDb > prev && ! maxGrDb.compare_exchange_weak (prev, grDb)) {}
}
