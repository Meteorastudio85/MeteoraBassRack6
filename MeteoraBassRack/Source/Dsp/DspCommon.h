#pragma once
// =============================================================================
//  Meteora Bass Rack - mattoni DSP condivisi (biquad, filtri a un polo, glide)
// =============================================================================
#include <cmath>
#include <algorithm>

namespace meteora
{
constexpr double kPi = 3.14159265358979323846;

inline double clampFreq (double f, double fs) { return std::clamp (f, 5.0, 0.48 * fs); }
inline double dbToGain (double db)            { return std::pow (10.0, db / 20.0); }

// Biquad in forma trasposta II, doppia precisione, stato stereo
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1[2] { 0, 0 }, z2[2] { 0, 0 };

    void reset() noexcept { z1[0] = z1[1] = z2[0] = z2[1] = 0.0; }

    inline double process (double x, int ch) noexcept
    {
        const double y = b0 * x + z1[ch];
        z1[ch] = b1 * x - a1 * y + z2[ch];
        z2[ch] = b2 * x - a2 * y;
        return y;
    }

    void setNormalised (double B0, double B1, double B2, double A0, double A1, double A2) noexcept
    {
        b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0;
    }

    void peaking (double fs, double f, double q, double gainDb) noexcept
    {
        f = clampFreq (f, fs);
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2.0 * kPi * f / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        setNormalised (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }

    void lowShelf (double fs, double f, double gainDb, double q = 0.707) noexcept
    {
        f = clampFreq (f, fs);
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2.0 * kPi * f / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        const double k = 2.0 * std::sqrt (A) * al;
        setNormalised (A * ((A + 1) - (A - 1) * c + k), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - k),
                       (A + 1) + (A - 1) * c + k, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - k);
    }

    void highShelf (double fs, double f, double gainDb, double q = 0.707) noexcept
    {
        f = clampFreq (f, fs);
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2.0 * kPi * f / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        const double k = 2.0 * std::sqrt (A) * al;
        setNormalised (A * ((A + 1) + (A - 1) * c + k), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - k),
                       (A + 1) - (A - 1) * c + k, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - k);
    }

    void highPass (double fs, double f, double q) noexcept
    {
        f = clampFreq (f, fs);
        const double w = 2.0 * kPi * f / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        setNormalised ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    }
};

// Filtro a un polo (passa-basso / passa-alto), stato stereo
struct OnePole
{
    double a = 1.0, z[2] { 0, 0 };
    void reset() noexcept { z[0] = z[1] = 0.0; }
    void setCutoff (double fs, double f) noexcept { a = 1.0 - std::exp (-2.0 * kPi * clampFreq (f, fs) / fs); }
    inline double lowPass  (double x, int ch) noexcept { z[ch] += a * (x - z[ch]); return z[ch]; }
    inline double highPass (double x, int ch) noexcept { return x - lowPass (x, ch); }
};

// Avvicinamento morbido di un parametro, aggiornato una volta ogni 32 campioni:
// niente "zipper noise" e i coefficienti si ricalcolano solo quando serve.
struct Glide
{
    double current = 0.0, target = 0.0;
    bool initialised = false;

    void setTarget (double t) noexcept
    {
        target = t;
        if (! initialised) { current = t; initialised = true; }
    }
    void snap() noexcept { current = target; }

    bool advance (double coeff) noexcept   // true se il valore è cambiato
    {
        if (current == target) return false;
        current += (target - current) * coeff;
        if (std::abs (target - current) <= 1.0e-5 * (1.0 + std::abs (target))) current = target;
        return true;
    }
};

constexpr int kChunk = 32;
inline double glideCoeff (double fs, double seconds) { return 1.0 - std::exp (-(double) kChunk / (seconds * fs)); }

} // namespace meteora
