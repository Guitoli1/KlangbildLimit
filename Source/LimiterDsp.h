#pragma once

// Klangbild Limit – True-Peak-Limiter mit Vorausschau
//
// Pegelregelung je Kanal (Basis-Samplerate):
//   True-Peak-Schätzung (4-fach interpoliert) -> benötigte Verstärkung
//   -> gleitendes Minimum über die Vorausschau L -> Release (sofort runter, weich hoch)
//   -> gleitender Mittelwert über L (weicher Einsatz, erreicht die volle Reduktion genau zur Spitze)
// Das Signal wird um dieselbe Zeit verzögert; damit liegt jede Abtastung garantiert unter der Obergrenze.

#include <cmath>
#include <array>
#include <vector>
#include <algorithm>

namespace limiter
{

// Interpolator (Kaiser-gefensterter Sinc) zur Schätzung von Zwischenabtastwerten.
// 4-fach ist der übliche Messstandard, liest bei sehr hohen Frequenzen aber bis zu ~0,4 dB zu wenig;
// die Sicherheitsstufe nutzt deshalb 8-fach.
template <int Phases, int Taps>
class TruePeakT
{
public:
    static constexpr int taps = Taps, phases = Phases;
    static constexpr int delay = taps / 2;   // Verzögerung des Detektors in Abtastungen

    TruePeakT()
    {
        constexpr double pi = 3.14159265358979323846;
        const int n = taps * phases;
        for (int p = 1; p < phases; ++p)
            for (int k = 0; k < taps; ++k)
            {
                const int i = k * phases + p;                       // Index im langen Filter
                const double t = (double) i - (double) n / 2.0;     // Abstand zur Mitte (in 1/4 Abtastungen)
                const double x = t / phases;
                const double sinc = std::abs (x) < 1e-9 ? 1.0 : std::sin (pi * x) / (pi * x);
                const double r = 2.0 * t / n;                                       // -1 .. 1
                const double w = r * r < 1.0 ? bessel0 (6.0 * std::sqrt (1.0 - r * r)) / bessel0 (6.0) : 0.0;   // Kaiser, beta 6
                coef[(size_t) p][(size_t) k] = (float) (sinc * w);
            }
        // Normieren: jede Phase hat DC-Verstärkung 1
        for (int p = 1; p < phases; ++p)
        {
            float s = 0; for (float c : coef[(size_t) p]) s += c;
            for (float& c : coef[(size_t) p]) c /= s;
        }
        reset();
    }

    void reset() { hist.fill (0.0f); pos = 0; }

    static double bessel0 (double x)
    {
        double sum = 1.0, term = 1.0;
        for (int k = 1; k < 30; ++k) { term *= (x / (2.0 * k)) * (x / (2.0 * k)); sum += term; }
        return sum;
    }

    // liefert den Spitzenwert (Betrag) rund um die Abtastung von vor `delay` Abtastungen
    float process (float x)
    {
        hist[(size_t) pos] = x; hist[(size_t) (pos + taps)] = x;
        pos = (pos + 1) % taps;
        const float* h = &hist[(size_t) pos];                // älteste .. neueste
        float pk = std::abs (h[taps - 1 - delay]);
        for (int p = 1; p < phases; ++p)
        {
            float s = 0;
            const auto& c = coef[(size_t) p];
            for (int k = 0; k < taps; ++k) s += h[k] * c[(size_t) (taps - 1 - k)];
            pk = std::max (pk, std::abs (s));
        }
        return pk;
    }

private:
    std::array<std::array<float, (size_t) taps>, (size_t) phases> coef {};
    std::array<float, (size_t) taps * 2> hist {};
    int pos = 0;
};

using TruePeak = TruePeakT<4, 32>;
using TruePeak8 = TruePeakT<8, 32>;

// gleitendes Minimum über ein festes Fenster (monotone Warteschlange, O(1) im Mittel)
class SlidingMin
{
public:
    void prepare (int window)
    {
        len = std::max (1, window);
        val.assign ((size_t) len + 1, 1.0f); idx.assign ((size_t) len + 1, 0);
        reset();
    }
    void reset() { head = tail = 0; count = 0; t = 0; }

    float process (float x)
    {
        const int cap = len + 1;
        while (count > 0 && val[(size_t) ((tail + cap - 1) % cap)] >= x) { tail = (tail + cap - 1) % cap; --count; }
        val[(size_t) tail] = x; idx[(size_t) tail] = t; tail = (tail + 1) % cap; ++count;
        while (idx[(size_t) head] <= t - len) { head = (head + 1) % cap; --count; }
        ++t;
        return val[(size_t) head];
    }

private:
    std::vector<float> val; std::vector<long long> idx;
    int len = 1, head = 0, tail = 0, count = 0;
    long long t = 0;
};

class BoxAverage
{
public:
    void prepare (int window) { len = std::max (1, window); buf.assign ((size_t) len, 1.0f); reset(); }
    void reset() { std::fill (buf.begin(), buf.end(), 1.0f); sum = (double) len; pos = 0; n = 0; }
    float process (float x)
    {
        sum += (double) x - (double) buf[(size_t) pos];
        buf[(size_t) pos] = x;
        pos = (pos + 1) % len;
        if (++n >= 65536) { n = 0; sum = 0; for (float v : buf) sum += v; }   // Rundungsfehler auffrischen
        return (float) (sum / len);
    }
private:
    std::vector<float> buf; double sum = 1.0; int len = 1, pos = 0, n = 0;
};

struct Delay
{
    std::vector<float> buf; int pos = 0;
    void prepare (int samples) { buf.assign ((size_t) std::max (1, samples) + 1, 0.0f); pos = 0; }
    void reset() { std::fill (buf.begin(), buf.end(), 0.0f); pos = 0; }
    float process (float x)
    {
        buf[(size_t) pos] = x;
        pos = (pos + 1) % (int) buf.size();
        return buf[(size_t) pos];   // Wert von vor (size-1) Abtastungen
    }
};

// weicher Clipper: linear bis zum Knie, darüber sanft bis höchstens zur Obergrenze
inline float softClip (float x, float ceiling)
{
    const float knee = 0.5f * ceiling;
    const float a = std::abs (x);
    if (a <= knee) return x;
    const float y = knee + (ceiling - knee) * std::tanh ((a - knee) / (ceiling - knee));
    return x < 0 ? -y : y;
}

template <typename Detector>
class LimiterT
{
public:
    void prepare (double sampleRate, int numChannels)
    {
        sr = sampleRate;
        channels = std::clamp (numChannels, 1, 2);
        look = std::max (1, (int) std::lround (0.002 * sr));        // 2 ms Vorausschau
        for (auto& c : ch)
        {
            c.minF.prepare (look);
            c.avg.prepare (look);
            c.sig.prepare (look - 1 + Detector::delay);
        }
        reset();
    }

    void reset()
    {
        for (auto& c : ch)
        {
            c.tp.reset(); c.minF.reset(); c.avg.reset(); c.sig.reset();
            c.fast = c.slow = c.prevNeed = 1.0f;
        }
        lastGain = { 1.0f, 1.0f };
    }

    int latency() const { return look - 1 + Detector::delay; }

    void setParams (float ceilingDb, float releaseMs, bool autoRelease, float linkAmount)
    {
        ceiling = std::pow (10.0f, ceilingDb / 20.0f);
        link = linkAmount;
        isAuto = autoRelease;
        const double relS = std::max (0.005, (double) releaseMs * 0.001);
        auto coef = [this] (double s) { return (float) (1.0 - std::exp (-1.0 / (s * sr))); };
        if (autoRelease)
        {
            relFast = coef (relS * 0.3);          // kurze Spitzen: schnell zurück
            relSlow = coef (relS * 3.0);          // dauerhafte Reduktion: langsam zurück, weniger Pumpen
            attSlow = coef (0.08);
        }
        else
        {
            relFast = coef (relS);
            relSlow = relFast; attSlow = 1.0f;
        }
    }

    // verarbeitet einen Frame (L/R); gibt die aktuelle Verstärkung (min der Kanäle) zurück
    float process (float* x)
    {
        std::array<float, 2> need { 1.0f, 1.0f };
        for (int c = 0; c < channels; ++c)
        {
            const float pk = ch[(size_t) c].tp.process (x[c]);
            need[(size_t) c] = pk > ceiling ? ceiling / pk : 1.0f;
        }
        if (channels == 2)
        {
            const float m = std::min (need[0], need[1]);
            need[0] = m + (need[0] - m) * (1.0f - link);
            need[1] = m + (need[1] - m) * (1.0f - link);
        }

        float gMin = 1.0f;
        for (int c = 0; c < channels; ++c)
        {
            auto& s = ch[(size_t) c];
            // der Detektor bewertet die Strecke zwischen zwei Abtastungen -> beide Endpunkte müssen sie einhalten
            const float nd = std::min (need[(size_t) c], s.prevNeed);
            s.prevNeed = need[(size_t) c];
            const float m = s.minF.process (nd);
            // Release: sofort herunter, weich zurück
            s.fast = m < s.fast ? m : s.fast + (m - s.fast) * relFast;
            // Auto: ein zweiter, träger Regler greift nur bei länger anhaltender Reduktion
            if (isAuto)
                s.slow = m < s.slow ? s.slow + (m - s.slow) * attSlow : s.slow + (m - s.slow) * relSlow;
            else
                s.slow = 1.0f;
            const float r = std::min (s.fast, s.slow);   // beide liegen nie über dem benötigten Wert
            const float g = s.avg.process (r);
            const float d = s.sig.process (x[c]);
            float y = d * g;
            // Sicherheitsnetz für Rundungsfehler
            y = std::clamp (y, -ceiling, ceiling);
            x[c] = y;
            gMin = std::min (gMin, g);
        }
        return gMin;
    }

private:
    struct Channel
    {
        Detector tp; SlidingMin minF; BoxAverage avg; Delay sig;
        float fast = 1.0f, slow = 1.0f, prevNeed = 1.0f;
    };
    std::array<Channel, 2> ch;
    std::array<float, 2> lastGain { 1.0f, 1.0f };
    double sr = 48000.0;
    int channels = 2, look = 96;
    float ceiling = 0.89f, link = 1.0f, relFast = 0.001f, relSlow = 0.001f, attSlow = 1.0f;
    bool isAuto = true;
};

using Limiter = LimiterT<TruePeak>;
using SafetyLimiter = LimiterT<TruePeak8>;

} // namespace limiter
