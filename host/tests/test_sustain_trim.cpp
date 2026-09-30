// Sustain trim tests (M8, ADR 0035, core/params/DriveVoicing.h "Sustain trim").
// Dependency-free: prints PASS/FAIL lines, returns nonzero on any failure.
//
// The owner's pad (1 Oct 2026, release b3e5ac3 on the Versio): a low-mid
// synth pad at mild settings turned the output LEDs red (limiter gain
// reduction >= 0.5 dB, firmware/LedMeter.h). The held sounds here are the
// ones tools/make_sustain_stimulus.py writes, synthesised in place (so the
// suite needs no WAVs): a C minor pad of detuned saws low-passed at 700 Hz
// (3 s swell, 6 s hold), a C2 drone (sine + 2nd harmonic), an organ chord.
//
//   1. Held sounds at -6 dBFS peak, the owner's settings (CLEAN, DRIVE 0,
//      SPLASH 0, DECAY noon) across SPRINGS x TONE x TENSION: the limiter
//      never pulls more than 0.5 dB (no red LED). MIX doesn't enter: the
//      limiter is on the wet, before MIX.
//   2. Hits and chord stabs are never trimmed (the trim stays exactly 1).
//   3. It lets go: a hit 0.4 s after a pad stops meets no trim.
//   4. No pumping: on a steady drone the trim, once settled, stays within
//      2 dB (its steady band is +-1 dB, DriveVoicing.h kSusSteadyDb).
//   5. The Howl: a Kick into KICKED DECAY 1 is never trimmed; a pad into the
//      Howl leaves it as loud as the Kick's alone, within 1 dB.

#include "dsp/Tank.h"
#include "params/ParamSpec.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <future>
#include <random>
#include <vector>

namespace {

int  failures = 0;
char msg[400];

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++failures;
}

using Buf = std::vector<float>;
constexpr float  kFs = 48000.0f;
constexpr double kPi = 3.14159265358979323846;
constexpr double C2 = 65.406, C3 = 130.813, EB3 = 155.563, G3 = 195.998;

size_t sec(double s) { return size_t(s * double(kFs)); }

void normalise(Buf& x, float peakDb)
{
    float p = 0.0f;
    for (float v : x) p = std::max(p, std::fabs(v));
    const float g = std::pow(10.0f, peakDb / 20.0f) / std::max(p, 1.0e-9f);
    for (float& v : x) v *= g;
}

// Raised-cosine attack / hold / release, framed by lead-in and tail silence.
Buf withEnvelope(const Buf& body, double attack, double hold, double release, double lead = 1.0, double tail = 6.0)
{
    const size_t na = sec(attack), nh = sec(hold), nr = sec(release);
    Buf x(sec(lead) + na + nh + nr + sec(tail), 0.0f);
    for (size_t i = 0; i < na + nh + nr; ++i) {
        double e = 1.0;
        if (i < na) e = 0.5 - 0.5 * std::cos(kPi * double(i) / double(na));
        else if (i >= na + nh) e = 0.5 + 0.5 * std::cos(kPi * double(i - na - nh) / double(nr));
        x[sec(lead) + i] = float(e) * body[i];
    }
    normalise(x, -6.0f);
    return x;
}

// RBJ low-pass, Q 0.707.
void lowpass(Buf& x, double fc)
{
    const double w = 2 * kPi * fc / kFs, alpha = std::sin(w) / (2 * 0.7071), cw = std::cos(w), a0 = 1 + alpha;
    const double b0 = (1 - cw) / 2 / a0, b1 = (1 - cw) / a0, a1 = -2 * cw / a0, a2 = (1 - alpha) / a0;
    double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    for (float& v : x) {
        const double y = b0 * v + b1 * x1 + b0 * x2 - a1 * y1 - a2 * y2;
        x2 = x1, x1 = v, y2 = y1, y1 = y;
        v = float(y);
    }
}

Buf pad()
{
    const size_t n = sec(12.0);
    Buf b(n, 0.0f);
    std::mt19937 rng(10);
    std::uniform_real_distribution<double> ph(0.0, 1.0);
    for (auto [f, amp] : {std::pair{C2, 1.0}, {C3, 0.8}, {EB3, 0.7}, {G3, 0.7}})
        for (double c : {-7.0, 0.0, 7.0}) {
            const double inc = f * std::pow(2.0, c / 1200.0) / kFs;
            double p = ph(rng);
            for (size_t i = 0; i < n; ++i) {
                b[i] += float(amp / 3.0 * (2 * p - 1));
                if ((p += inc) >= 1.0) p -= 1.0;
            }
        }
    for (int k = 0; k < 2; ++k) lowpass(b, 700.0);
    return withEnvelope(b, 3.0, 6.0, 3.0);
}

Buf drone()
{
    const size_t n = sec(12.0);
    Buf b(n);
    for (size_t i = 0; i < n; ++i) {
        const double t = double(i) / kFs;
        b[i] = float(std::sin(2 * kPi * C2 * t) + 0.5 * std::sin(2 * kPi * 2 * C2 * t + 0.3));
    }
    return withEnvelope(b, 1.0, 10.0, 1.0);
}

Buf organ()
{
    const size_t n = sec(8.07);
    Buf b(n, 0.0f);
    int k = 0;
    for (auto [f, a] : {std::pair{C2, 0.8}, {C3, 1.0}, {EB3, 1.0}, {G3, 1.0}})
        for (auto [h, d] : {std::pair{1, 1.0}, {2, 0.8}, {3, 0.6}, {4, 0.5}, {6, 0.3}, {8, 0.25}}) {
            const double w = 2 * kPi * f * h / kFs, p0 = 0.37 * k++;
            for (size_t i = 0; i < n; ++i) b[i] += float(a * d * std::sin(w * double(i) + p0));
        }
    return withEnvelope(b, 0.02, 8.0, 0.05);
}

// Snare-like hits (noise + 185 Hz body) and chord stabs (tools/make_stimulus.py
// hits() / skank(), shortened), -6 dBFS peak.
Buf hits()
{
    Buf x(sec(14.0), 0.0f);
    std::mt19937 rng(1);
    std::uniform_real_distribution<float> u(-1.0f, 1.0f);
    for (int h = 0; h < 4; ++h) {
        const size_t at = sec(1.0 + 3.0 * h);
        for (size_t i = 0; i < sec(0.25); ++i) {
            const double t = double(i) / kFs;
            x[at + i] = float(0.6 * std::sin(2 * kPi * 185 * t) * std::exp(-t / 0.03) + 1.2 * u(rng) * std::exp(-t / 0.06));
        }
    }
    normalise(x, -6.0f);
    return x;
}

Buf stabs()
{
    Buf x(sec(12.0), 0.0f);
    const double beat = 60.0 / 75.0;
    for (int k = 0; k < 12; ++k) {
        const size_t at = sec(1.0 + k * beat + beat / 2);
        static constexpr double kAm[3] = {220.0, 261.63, 329.63}, kD[3] = {293.66, 369.99, 440.0};
        const double* ch = (k / 4) % 2 ? kD : kAm;
        for (size_t i = 0; i < sec(0.12); ++i) {
            const double t = double(i) / kFs;
            double s = 0;
            for (int j = 0; j < 3; ++j) s += 2 * std::fmod(ch[j] * t, 1.0) - 1;
            x[at + i] += float(s / 3 * std::exp(-t / 0.035));
        }
    }
    lowpass(x, 2500.0);
    normalise(x, -6.0f);
    return x;
}

struct Set {
    int   att = 0, springs = 1;
    float decay = 0.5f, tone = 0.3f, tension = 0.8f, drive = 0.0f, splash = 0.0f;
};

struct Run {
    float minLimGain = 1.0f;          // limiter gain, lowest per 16-sample block
    std::vector<float> trimDb;        // Sustain trim per block (dB)
    Buf wet;                          // (L + R) / 2 at MIX 1
};

constexpr int kBlock = 16;

Run render(rv::Tank& t, const Set& s, const Buf& in, const std::vector<double>& kicks = {})
{
    using rv::ParamId;
    t.reset();
    t.setParam(ParamId::Attitude, rv::switchToNormalised(s.att));
    t.setParam(ParamId::Springs, rv::switchToNormalised(s.springs));
    t.setParam(ParamId::Decay, s.decay);
    t.setParam(ParamId::Tone, s.tone);
    t.setParam(ParamId::Tension, s.tension);
    t.setParam(ParamId::Drive, s.drive);
    t.setParam(ParamId::Splash, s.splash);
    t.setParam(ParamId::Mix, 1.0f);
    Run r;
    r.wet.resize(in.size());
    Buf l(kBlock), rr(kBlock);
    size_t ki = 0;
    for (size_t pos = 0; pos < in.size(); pos += kBlock) {
        const int n = int(std::min<size_t>(kBlock, in.size() - pos));
        while (ki < kicks.size() && sec(kicks[ki]) < pos + size_t(n)) t.kick(int(sec(kicks[ki++]) - pos));
        t.process(in.data() + pos, in.data() + pos, l.data(), rr.data(), n);
        for (int i = 0; i < n; ++i) r.wet[pos + size_t(i)] = 0.5f * (l[size_t(i)] + rr[size_t(i)]);
        r.minLimGain = std::min(r.minLimGain, t.limiterGain());
        r.trimDb.push_back(20.0f * std::log10(t.sustainTrim()));
    }
    return r;
}

double rmsDb(const Buf& x, double from, double to)
{
    double s = 0;
    const size_t a = sec(from), b = std::min(sec(to), x.size());
    for (size_t i = a; i < b; ++i) s += double(x[i]) * x[i];
    return 10 * std::log10(std::max(s / double(std::max<size_t>(1, b - a)), 1e-30));
}

float trimAt(const Run& r, double s) { return r.trimDb[std::min(r.trimDb.size() - 1, sec(s) / kBlock)]; }

// One stimulus over the 27 cells, on its own Tank (the three run in parallel).
struct Worst {
    float gr = 0.0f;
    char  at[120] = "none limited";
};
Worst worstCell(const Buf& x)
{
    rv::Tank t;
    t.prepare(kFs, kBlock);
    Worst w;
    for (int sp = 0; sp < 3; ++sp)
        for (float tone : {0.0f, 0.5f, 0.9f})
            for (float ten : {0.5f, 0.8f, 1.0f}) {
                Set s;
                s.springs = sp, s.tone = tone, s.tension = ten;
                const float gr = std::max(0.0f, -20.0f * std::log10(render(t, s, x).minLimGain));
                if (gr > w.gr) {
                    w.gr = gr;
                    std::snprintf(w.at, sizeof w.at, "SPRINGS %d TONE %.1f TENSION %.1f", sp + 1, double(tone), double(ten));
                }
            }
    return w;
}

void heldSoundsStayUnderTheLimiter()
{
    const char* const names[] = {"pad", "drone", "organ"};
    const Buf         stims[] = {pad(), drone(), organ()};
    std::vector<std::future<Worst>> jobs;
    for (const Buf& x : stims) jobs.push_back(std::async(std::launch::async, worstCell, std::cref(x)));
    for (size_t k = 0; k < jobs.size(); ++k) {
        const Worst w = jobs[k].get();
        std::snprintf(msg, sizeof msg,
                      "Held %s, -6 dBFS peak, CLEAN DRIVE 0 SPLASH 0 DECAY noon, 27 SPRINGS x TONE x TENSION cells: "
                      "limiter pulls at most %.2f dB (%s; limit < 0.5, the red LED)",
                      names[k], double(w.gr), w.at);
        check(w.gr < 0.5f, msg);
    }
    // Teeth: the drone's worst cell on main (b3e5ac3) pulled 5.9 dB.
    rv::Tank t;
    t.prepare(kFs, kBlock);
    t.setSustainTrimEnabled(false);
    Set s;
    s.springs = 2, s.tone = 0.0f, s.tension = 1.0f;
    const float off = -20.0f * std::log10(render(t, s, stims[1]).minLimGain);
    std::snprintf(msg, sizeof msg, "... and it is the Sustain trim: the drone at SPRINGS 3 TONE 0 TENSION 1 without it pulls %.2f dB (>= 0.5)",
                  double(off));
    check(off >= 0.5f, msg);
}

void hitsAreNeverTrimmed(rv::Tank& t)
{
    const Buf h = hits(), k = stabs();
    float lowest = 0.0f;
    for (int att : {0, 1, 2})
        for (int sp : {0, 1, 2})
            for (const Buf* x : {&h, &k}) {
                Set s;
                s.att = att, s.springs = sp, s.splash = 0.3f;
                const Run r = render(t, s, *x);
                for (float d : r.trimDb) lowest = std::min(lowest, d);
            }
    std::snprintf(msg, sizeof msg, "Snare hits and chord stabs, every ATTITUDE and SPRINGS: Sustain trim %.3f dB (limit exactly 0)",
                  double(lowest));
    check(lowest == 0.0f, msg);
}

void letsGoForTheNextHit(rv::Tank& t)
{
    // The pad, then a hit 0.4 s after its release ends (13.4 s).
    Buf x = pad();
    const Buf h = hits();
    x.resize(sec(16.0), 0.0f);
    for (size_t i = 0; i < sec(0.3); ++i) x[sec(13.4) + i] = h[sec(1.0) + i];
    Set s;
    s.springs = 2, s.tension = 1.0f;
    const Run r = render(t, s, x);
    const float held = trimAt(r, 8.0), atHit = trimAt(r, 13.4);
    std::snprintf(msg, sizeof msg, "Lets go: pad held %.1f dB, a hit 0.4 s after the pad ends meets %.2f dB (limit > -0.1)",
                  double(held), double(atHit));
    check(held < -1.0f && atHit > -0.1f, msg);
}

void noPumpingOnADrone(rv::Tank& t)
{
    const Buf x = drone();
    float worst = 0.0f;
    for (int sp = 0; sp < 3; ++sp)
        for (float ten : {0.5f, 0.8f, 1.0f}) {
            Set s;
            s.springs = sp, s.tension = ten;
            const Run r = render(t, s, x);
            float lo = 0.0f, hi = -100.0f;
            for (double tt = 4.0; tt < 12.0; tt += 0.05) { // settled: 2 s into the hold
                lo = std::min(lo, trimAt(r, tt));
                hi = std::max(hi, trimAt(r, tt));
            }
            worst = std::max(worst, hi - lo);
        }
    std::snprintf(msg, sizeof msg, "No pumping: on a held drone the settled trim moves %.2f dB at most (limit 2)", double(worst));
    check(worst <= 2.0f, msg);
}

void howlStaysLoud(rv::Tank& t)
{
    Set s;
    s.att = 2, s.decay = 1.0f, s.springs = 1;
    Buf silence(sec(24.0), 0.0f);
    const Run kick = render(t, s, silence, {1.0});
    float lowest = 0.0f;
    for (float d : kick.trimDb) lowest = std::min(lowest, d);
    Buf p = pad();
    p.resize(sec(24.0), 0.0f);
    const Run fed = render(t, s, p);
    const double a = rmsDb(kick.wet, 18.0, 24.0), b = rmsDb(fed.wet, 18.0, 24.0);
    std::snprintf(msg, sizeof msg,
                  "Howl (KICKED DECAY 1, 2 Springs): a Kick's Howl is never trimmed (%.3f dB); fed by the pad it settles at "
                  "%.1f dB vs %.1f from the Kick (limit +-1)",
                  double(lowest), b, a);
    check(lowest == 0.0f && std::fabs(a - b) <= 1.0, msg);
}

} // namespace

int main()
{
    rv::Tank t;
    t.prepare(kFs, kBlock);
    heldSoundsStayUnderTheLimiter();
    hitsAreNeverTrimmed(t);
    letsGoForTheNextHit(t);
    noPumpingOnADrone(t);
    howlStaysLoud(t);
    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
