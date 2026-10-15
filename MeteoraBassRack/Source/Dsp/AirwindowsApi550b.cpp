#include "AirwindowsApi550b.h"

using namespace meteora;


void AirwindowsApi550b::prepare (double sampleRate)
{
    fs = sampleRate;
    glide = glideCoeff (fs, 0.025);
    outputTransformer.prepare (fs, IronTransformer::api2503());

    for (int b = 0; b < 4; ++b)
    {
        if (! logFreq[b].initialised) logFreq[b].setTarget (std::log2 ((double) bandFreqs[b][3]));
        if (! gain[b].initialised)    gain[b].setTarget (0.0);
        logFreq[b].snap();
        gain[b].snap();
        updateBand (b);   // i coefficienti sono SEMPRE validi dopo prepare (prima davano silenzio)
    }
    reset();
}

void AirwindowsApi550b::reset()
{
    for (auto& f : filters) f.reset();
    outputTransformer.reset();
}

void AirwindowsApi550b::setBand (int band, int freqIndex, float gainDb, bool shelf)
{
    band = std::clamp (band, 0, 3);
    freqIndex = std::clamp (freqIndex, 0, 6);
    logFreq[band].setTarget (std::log2 ((double) bandFreqs[band][freqIndex]));
    gain[band].setTarget (std::clamp ((double) gainDb, -12.0, 12.0));
    shelfWanted[band] = shelf && (band == 0 || band == 3);
}

void AirwindowsApi550b::updateBand (int b) noexcept
{
    const double f = std::exp2 (logFreq[b].current);
    const double g = gain[b].current;

    // Proportional Q: campana larga con poco guadagno, più stretta quando si spinge
    const double q = 0.45 + 0.095 * std::abs (g);

    if (shelfWanted[b] && b == 0)      filters[b].lowShelf  (fs, f, g, 0.6);
    else if (shelfWanted[b] && b == 3) filters[b].highShelf (fs, f, g, 0.6);
    else                               filters[b].peaking   (fs, f, q, g);

    shelfActive[b] = shelfWanted[b];
}

// Op-amp discreto 2520: pulito fino a livelli alti, poi satura in modo morbido.
// Curva "Spiral" di Airwindows (sin(x*|x|)/|x|) con 6 dB di headroom interno.
double AirwindowsApi550b::opAmp2520 (double x) noexcept
{
    constexpr double headroom = 0.5;
    double v = std::clamp (x * headroom, -1.2533141, 1.2533141);
    const double a = std::abs (v);
    if (a < 1.0e-12) return x;
    return std::sin (v * a) / a / headroom;
}

void AirwindowsApi550b::process (float* const* data, int numChannels, int numSamples) noexcept
{
    numChannels = std::min (numChannels, 2);

    for (int start = 0; start < numSamples; start += kChunk)
    {
        const int len = std::min (kChunk, numSamples - start);

        for (int b = 0; b < 4; ++b)
        {
            const bool changed = logFreq[b].advance (glide) | gain[b].advance (glide);
            if (changed || shelfWanted[b] != shelfActive[b])
                updateBand (b);
        }

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* d = data[ch] + start;
            for (int i = 0; i < len; ++i)
            {
                double x = d[i];
                for (auto& f : filters) x = f.process (x, ch);
                x = opAmp2520 (x);
                x = outputTransformer.process (x, ch);
                d[i] = (float) x;
            }
        }
    }
}
