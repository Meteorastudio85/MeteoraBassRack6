#include "AirwindowsTubeSmile.h"

using namespace meteora;

void AirwindowsTubeSmile::prepare (double sampleRate)
{
    fs = sampleRate;
    glide = glideCoeff (fs, 0.02);

    gainHighPass.setCutoff (fs, 720.0);   // 4,7k + 0,047uF
    toneLowPass.setCutoff (fs, 723.0);    // 1k + 0,22uF
    dcBlock.setCutoff (fs, 8.0);

    if (! inputDb.initialised) inputDb.setTarget (0.0);
    if (! drive.initialised)   drive.setTarget (5.0);
    if (! tone.initialised)    tone.setTarget (5.0);
    if (! levelDb.initialised) levelDb.setTarget (0.0);
    if (! blend.initialised)   blend.setTarget (1.0);
    inputDb.snap(); drive.snap(); tone.snap(); levelDb.snap(); blend.snap();
    reset();
}

void AirwindowsTubeSmile::reset()
{
    gainHighPass.reset();
    feedbackLowPass.reset();
    toneLowPass.reset();
    dcBlock.reset();
}

void AirwindowsTubeSmile::setInputDb (float d) { inputDb.setTarget (std::clamp ((double) d, -12.0, 12.0)); }
void AirwindowsTubeSmile::setDrive (float d)  { drive.setTarget (std::clamp ((double) d, 0.0, 10.0)); }
void AirwindowsTubeSmile::setTone (float t)   { tone.setTarget (std::clamp ((double) t, 0.0, 10.0)); }
// LEVEL a 5 = volume circa uguale a pedale spento (il clipping aggiunge circa +6 dB)
void AirwindowsTubeSmile::setLevel (float l)  { levelDb.setTarget ((std::clamp ((double) l, 0.0, 10.0) - 5.0) * 4.0); }   // 5 = 0 dB
void AirwindowsTubeSmile::setBlend (float b)  { blend.setTarget (std::clamp ((double) b, 0.0, 100.0) / 100.0); }

// Coppia di diodi al silicio 1N914 in antiparallelo: ginocchio più netto di una tanh
double AirwindowsTubeSmile::diodePair (double v) noexcept
{
    constexpr double vd = 0.6;
    const double u = v / vd;
    return vd * u / std::pow (1.0 + std::pow (std::abs (u), 2.5), 0.4);
}

void AirwindowsTubeSmile::process (float* const* data, int numChannels, int numSamples) noexcept
{
    numChannels = std::min (numChannels, 2);

    for (int start = 0; start < numSamples; start += kChunk)
    {
        const int len = std::min (kChunk, numSamples - start);
        inputDb.advance (glide); drive.advance (glide); tone.advance (glide); levelDb.advance (glide); blend.advance (glide);
        const double inGain = dbToGain (inputDb.current);

        // potenziometro DRIVE 500k a curva logaritmica
        const double d = drive.current / 10.0;
        const double rDrive = 500.0e3 * (std::pow (10.0, 2.0 * d) - 1.0) / 99.0;
        const double gainMinusOne = (51.0e3 + rDrive) / 4.7e3;              // da ~11 a ~117
        feedbackLowPass.setCutoff (fs, 1.0 / (2.0 * kPi * 51.0e-12 * (51.0e3 + rDrive)));

        const double toneAmount = 0.08 + 1.5 * std::pow (tone.current / 10.0, 1.5);
        const double level = dbToGain (levelDb.current);
        const double wet = blend.current;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* p = data[ch] + start;
            for (int i = 0; i < len; ++i)
            {
                const double dry = p[i] * inGain;

                // stadio di clipping TS808
                double v = gainMinusOne * gainHighPass.highPass (dry, ch);
                v = feedbackLowPass.lowPass (v, ch);
                double y = dry + diodePair (v);

                // stadio di tono
                const double lp = toneLowPass.lowPass (y, ch);
                y = lp + (y - lp) * toneAmount;

                // il clipping aggiunge circa +6 dB: lo compenso solo sul ramo distorto,
                // poi BLEND mescola pulito e distorto e OUTPUT regola il volume di tutto il pedale
                y = dcBlock.highPass (y, ch) * 0.5012;
                p[i] = (float) ((dry + (y - dry) * wet) * level);
            }
        }
    }
}
