#include "AirwindowsVog.h"

using namespace meteora;

void AirwindowsVog::prepare (double sampleRate)
{
    fs = sampleRate;
    glide = glideCoeff (fs, 0.03);

    if (! logFreq.initialised)  logFreq.setTarget (std::log2 (40.0));
    if (! ampDb.initialised)    ampDb.setTarget (6.0);
    if (! q.initialised)        q.setTarget (1.4);
    if (! outDb.initialised)    outDb.setTarget (0.0);
    if (! hpMix.initialised)    hpMix.setTarget (1.0);
    if (! slopeMix.initialised) slopeMix.setTarget (0.0);
    logFreq.snap(); ampDb.snap(); q.snap(); outDb.snap(); hpMix.snap(); slopeMix.snap();
    updateFilters();
    reset();
}

void AirwindowsVog::reset()
{
    resonance.reset(); hp12.reset(); hp24a.reset(); hp24b.reset();
}

void AirwindowsVog::setFrequency (float hz)    { logFreq.setTarget (std::log2 (std::clamp ((double) hz, 15.0, 500.0))); }
void AirwindowsVog::setAmplitudeDb (float db)  { ampDb.setTarget (std::clamp ((double) db, 0.0, 15.0)); }
void AirwindowsVog::setQ (float newQ)          { q.setTarget (std::clamp ((double) newQ, 0.5, 4.0)); }
void AirwindowsVog::setOutputDb (float db)     { outDb.setTarget (std::clamp ((double) db, -12.0, 12.0)); }
void AirwindowsVog::setHighPass (bool on)      { hpMix.setTarget (on ? 1.0 : 0.0); }
void AirwindowsVog::setSlope24 (bool on)       { slopeMix.setTarget (on ? 1.0 : 0.0); }

void AirwindowsVog::updateFilters() noexcept
{
    const double f = std::exp2 (logFreq.current);
    resonance.peaking (fs, f, q.current, ampDb.current);

    // il taglio sta sotto la risonanza, così la campana resta intera
    const double fHp = f * 0.62;
    hp12.highPass (fs, fHp, 0.7071);
    hp24a.highPass (fs, fHp, 0.5412);   // Butterworth del 4° ordine
    hp24b.highPass (fs, fHp, 1.3066);
}

void AirwindowsVog::process (float* const* data, int numChannels, int numSamples) noexcept
{
    numChannels = std::min (numChannels, 2);

    for (int start = 0; start < numSamples; start += kChunk)
    {
        const int len = std::min (kChunk, numSamples - start);
        const bool changed = logFreq.advance (glide) | ampDb.advance (glide) | q.advance (glide);
        if (changed) updateFilters();
        outDb.advance (glide); hpMix.advance (glide); slopeMix.advance (glide);

        const double out = dbToGain (outDb.current);
        const double hm = hpMix.current, sm = slopeMix.current;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* p = data[ch] + start;
            for (int i = 0; i < len; ++i)
            {
                const double r = resonance.process (p[i], ch);
                const double h12 = hp12.process (r, ch);
                const double h24 = hp24b.process (hp24a.process (r, ch), ch);
                const double h = h12 + (h24 - h12) * sm;
                p[i] = (float) ((r + (h - r) * hm) * out);
            }
        }
    }
}
