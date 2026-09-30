// SPLASH model tests (SPEC §4.5, §7 M7; ADR 0032): HitDetector, HitEnvelope
// (the Clang and the Bite), Clatter (the Kick's crash), Jolt and the Splash
// facade, stand-alone (the Tank's side: test_m7_tank).
//
// Stimulus: the synthetic snare and rim of tools/make_stimulus.py / 02_hits
// (snare: 185 Hz body + 800 Hz–7 kHz noise; rim: 1.7 kHz + 480 Hz tone +
// > 2 kHz noise), peaking at −6 / −12 / −18 dBFS, one hit per render, and a
// chord stab as 04_skank's (three sawtooths, low-passed at 2.5 kHz). Fed
// straight in: stand-alone, the input is the Tank's input at INPUT gain 1
// (DRIVE 0, ADR 0033).
// "Clang energy" = sum of squares of the Clang amount for one hit (the Tank
// multiplies the hit's own highs by it); likewise the Bite.
// Jolt pitch: the Loop delay offset D[n] = jolt × L shifts one pass by
// 1200 log2(1 − ΔD) cents (as test_wobble), L = 55 ms (DECAY noon).

#include "Fft.h"
#include "Wav.h"
#include "dsp/Splash.h"
#include "params/ParamSpec.h"
#include "params/Mappings.h"
#include "params/SplashVoicing.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

int failures = 0;
char msg[400];

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++failures;
}

using Buf = std::vector<float>;
constexpr float kFs = 48000.0f;
const char* const kAttName[3] = {"CLEAN", "DRIVEN", "KICKED"};
std::array<float, 3> att(int a)
{
    std::array<float, 3> w{{0.0f, 0.0f, 0.0f}};
    w[size_t(a)] = 1.0f;
    return w;
}

Buf onePoleLp(Buf x, float hz, float fs)
{
    const float c = 1.0f - std::exp(-2.0f * rv::map::kPi * hz / fs);
    float y = 0;
    for (auto& v : x) v = (y += c * (v - y));
    return x;
}
Buf onePoleHp(const Buf& x, float hz, float fs)
{
    Buf lp = onePoleLp(x, hz, fs), y(x.size());
    for (size_t i = 0; i < x.size(); ++i) y[i] = x[i] - lp[i];
    return y;
}

// One snare (rim = false) or rim hit at `at` seconds, peak `peakDb` dBFS.
Buf hit(float peakDb, bool rim, float seconds, float fs, float at = 0.2f, uint32_t seed = 1)
{
    rv::dsp::Rng rng;
    rng.seed(seed);
    const size_t len = size_t((rim ? 0.06f : 0.25f) * fs);
    Buf nz(len), h(len);
    for (auto& v : nz) v = rng.bipolar();
    nz = rim ? onePoleHp(nz, 2000.0f, fs) : onePoleHp(onePoleLp(nz, 7000.0f, fs), 800.0f, fs);
    float peak = 0;
    for (size_t i = 0; i < len; ++i) {
        const float t = float(i) / fs;
        h[i] = rim ? (std::sin(2 * rv::map::kPi * 1700 * t) + 0.5f * std::sin(2 * rv::map::kPi * 480 * t) + 0.7f * nz[i]) * std::exp(-t / 0.008f)
                   : 0.6f * std::sin(2 * rv::map::kPi * 185 * t) * std::exp(-t / 0.03f) + 1.2f * nz[i] * std::exp(-t / 0.06f);
        peak = std::max(peak, std::fabs(h[i]));
    }
    Buf out(size_t(seconds * fs), 0.0f);
    const float g = std::pow(10.0f, peakDb / 20.0f) / peak;
    for (size_t i = 0; i < len; ++i) out[size_t(at * fs) + i] = g * h[i];
    return out;
}

// A chord stab as 04_skank's (A minor, sawtooths, 35 ms decay, 120 ms,
// two one-pole low-passes at 2.5 kHz), peak -6 dBFS, at `at` seconds.
Buf stab(float seconds, float fs, float at = 0.2f)
{
    const size_t len = size_t(0.12f * fs);
    Buf h(len);
    for (size_t i = 0; i < len; ++i) {
        const float t = float(i) / fs;
        float s = 0.0f;
        for (float f : {220.0f, 261.63f, 329.63f}) s += 2.0f * std::fmod(f * t, 1.0f) - 1.0f;
        h[i] = s / 3.0f * std::exp(-t / 0.035f);
    }
    h = onePoleLp(onePoleLp(h, 2500.0f, fs), 2500.0f, fs);
    float peak = 0;
    for (float v : h) peak = std::max(peak, std::fabs(v));
    Buf out(size_t(seconds * fs), 0.0f);
    for (size_t i = 0; i < len; ++i) out[size_t(at * fs) + i] = 0.5f / peak * h[i];
    return out;
}

struct Run {
    Buf clatter, jolt, clang, bite;
    Buf env; // Clatter burst envelope after each block (per sample at block 1)
    float maxHit = 0, maxJolt = 0, maxAllpass = 0;
    int impacts = 0, strokes = 0;
};

// strikeAt >= 0: a Kick strike at that absolute sample.
Run run(const Buf& in, int attitude, float splashV, int block = 48, float fs = kFs, long strikeAt = -1,
        uint32_t seed = 7u)
{
    rv::dsp::Splash sp;
    sp.prepare(fs, seed);
    sp.set(att(attitude), splashV);
    const size_t n0 = in.size();
    Run r{Buf(n0), Buf(n0), Buf(n0), Buf(n0), Buf(n0)};
    Buf cB(n0), cC(n0);
    for (size_t pos = 0; pos < n0; pos += size_t(block)) {
        const int n = int(std::min(size_t(block), n0 - pos));
        if (strikeAt >= long(pos) && strikeAt < long(pos) + n) sp.strike(1.0f, int(strikeAt - long(pos)));
        sp.process(in.data() + pos, r.clang.data() + pos, r.bite.data() + pos, r.clatter.data() + pos, cB.data() + pos,
                   cC.data() + pos, r.jolt.data() + pos, n);
        for (int i = 0; i < n; ++i) r.env[pos + size_t(i)] = sp.clatterEnvelope();
        r.maxHit = std::max(r.maxHit, sp.hit());
        r.maxJolt = std::max(r.maxJolt, std::fabs(r.jolt[pos]));
        r.maxAllpass = std::max(r.maxAllpass, std::fabs(sp.allpassDelta()));
    }
    r.impacts = sp.impactCount();
    r.strokes = sp.strokeCount();
    return r;
}

double energy(const Buf& x)
{
    double s = 0;
    for (float v : x) s += double(v) * v;
    return s;
}
float peakOf(const Buf& x)
{
    float p = 0;
    for (float v : x) p = std::max(p, std::fabs(v));
    return p;
}
double db(double p) { return 10.0 * std::log10(p + 1e-30); }

// Share of energy between lo and hi Hz (plain FFT, zero-padded).
double bandShare(const Buf& x, float lo, float hi, float fs)
{
    size_t n = 1;
    while (n < x.size()) n <<= 1;
    std::vector<float> re(n, 0.0f), im(n, 0.0f);
    std::copy(x.begin(), x.end(), re.begin());
    rv::fft::transform(re, im, false);
    double in = 0, all = 0;
    for (size_t k = 1; k < n / 2; ++k) {
        const double p = double(re[k]) * re[k] + double(im[k]) * im[k];
        const double f = double(k) * fs / double(n);
        all += p;
        if (f >= lo && f <= hi) in += p;
    }
    return all > 0 ? in / all : 0.0;
}

// Pitch (cents per pass) of a Loop delay offset jolt × L.
struct Lurch {
    double down = 0, up = 0; // most negative, most positive cents
    double slope = 0;        // max |ΔD| (samples per sample)
};
Lurch lurch(const Buf& jolt, float Lsamples, size_t from = 0, size_t to = ~size_t(0))
{
    Lurch l;
    to = std::min(to, jolt.size());
    for (size_t i = std::max<size_t>(from, 1); i < to; ++i) {
        const double d = double(jolt[i] - jolt[i - 1]) * Lsamples;
        const double c = 1200.0 * std::log2(1.0 - d);
        l.down = std::min(l.down, c);
        l.up = std::max(l.up, c);
        l.slope = std::max(l.slope, std::fabs(d));
    }
    return l;
}

size_t firstNonZero(const Buf& x)
{
    for (size_t i = 0; i < x.size(); ++i)
        if (x[i] != 0.0f) return i;
    return x.size();
}

} // namespace

int main()
{
    using namespace rv;
    const float levels[3] = {-6.0f, -12.0f, -18.0f};

    // ---- Hit monotonic with level ------------------------------------------------
    {
        bool mono = true;
        for (bool rim : {false, true})
            for (float s : {0.0f, 0.5f, 1.0f}) {
                float h[3];
                for (int l = 0; l < 3; ++l) h[l] = run(hit(levels[l], rim, 1.0f, kFs), 1, s).maxHit;
                std::printf("      %s SPLASH %.1f: Hit %.3f / %.3f / %.3f at -6 / -12 / -18 dBFS\n", rim ? "rim  " : "snare",
                            s, h[0], h[1], h[2]);
                mono &= h[0] > h[1] && h[1] > h[2];
            }
        check(mono, "Hit rises with input level (snare and rim, SPLASH 0 / 0.5 / 1)");
        // Sensitivity: SPLASH raises Hit for the same hit.
        const float h0 = run(hit(-12, false, 1.0f, kFs), 1, 0.0f).maxHit, h1 = run(hit(-12, false, 1.0f, kFs), 1, 1.0f).maxHit;
        std::snprintf(msg, sizeof msg, "SPLASH sets sensitivity: -12 dBFS snare Hit %.2f at SPLASH 0 -> %.2f at SPLASH 1", h0, h1);
        check(h1 > 2.0f * h0, msg);
        // Sustained sound is not a Hit, and gets no Clang or Bite: held tones
        // at -6 dBFS, SPLASH 1, after the onset.
        float late[2] = {0, 0}, lateE[2] = {0, 0};
        int strokes[2] = {0, 0};
        const float freqs[2] = {1000.0f, 110.0f};
        for (int f = 0; f < 2; ++f) {
            Buf tone(size_t(2.0f * kFs));
            for (size_t i = 0; i < tone.size(); ++i)
                tone[i] = 0.5f * std::sin(2.0f * map::kPi * freqs[f] * float(i) / kFs) * std::min(1.0f, float(i) / 2400.0f);
            rv::dsp::Splash sp;
            sp.prepare(kFs, 7u);
            sp.set(att(2), 1.0f);
            Buf c(tone.size()), j(tone.size()), cl(tone.size()), bt(tone.size());
            int atSettle = 0;
            for (size_t pos = 0; pos < tone.size(); pos += 48) {
                sp.process(tone.data() + pos, cl.data() + pos, bt.data() + pos, c.data() + pos, nullptr, nullptr,
                           j.data() + pos, 48);
                if (pos > size_t(0.5f * kFs)) {
                    late[f] = std::max(late[f], sp.hit());
                    for (size_t i = pos; i < pos + 48; ++i) lateE[f] = std::max({lateE[f], cl[i], bt[i]});
                }
                if (pos == size_t(0.3f * kFs)) atSettle = sp.strokeCount();
            }
            strokes[f] = sp.strokeCount() - atSettle; // strokes once sustained (from 0.3 s)
        }
        std::snprintf(msg, sizeof msg, "held tones (-6 dBFS, 50 ms fade-in) are not Hits once sustained: max Hit %.3f (1 kHz), %.3f (110 Hz), Clang / Bite %.3f / %.3f after 0.5 s; strokes after 0.3 s: %d / %d",
                      late[0], late[1], lateE[0], lateE[1], strokes[0], strokes[1]);
        check(late[0] < 0.02f && late[1] < 0.02f && lateE[0] < 0.01f && lateE[1] < 0.01f && strokes[0] == 0
                  && strokes[1] == 0,
              msg);
    }

    // ---- Level-adaptive Hit (M8, SplashVoicing.h) ------------------------------------
    // An isolated quiet hit still registers at SPLASH 1 (the Jolt); the same
    // hit half a second after a loud one (a ghost note) barely does.
    {
        const float iso = run(hit(-18, true, 1.0f, kFs), 1, 1.0f).maxHit;
        Buf pair = hit(-6, false, 1.2f, kFs, 0.2f);
        const Buf g = hit(-18, true, 1.2f, kFs, 0.7f);
        for (size_t i = 0; i < pair.size(); ++i) pair[i] += g[i];
        rv::dsp::Splash sp;
        sp.prepare(kFs, 7u);
        sp.set(att(1), 1.0f);
        Buf c(pair.size()), j(pair.size());
        float ghostHit = 0.0f, prog = 0.0f;
        for (size_t pos = 0; pos < pair.size(); pos += 32) {
            sp.process(pair.data() + pos, c.data() + pos, j.data() + pos, 32);
            if (pos >= size_t(0.69f * kFs) && pos < size_t(0.8f * kFs)) {
                ghostHit = std::max(ghostHit, sp.hit());
                prog = std::max(prog, sp.detector().programLevel());
            }
        }
        std::snprintf(msg, sizeof msg,
                      "level-adaptive Hit (DRIVEN, SPLASH 1): isolated -18 dBFS rim %.2f (>= 0.8); the same rim 0.5 s after "
                      "a -6 dBFS snare %.2f (< 0.25; program level %.3f)",
                      iso, ghostHit, prog);
        check(iso >= 0.8f && ghostHit < 0.25f, msg);
    }

    // ---- The Clang follows the hit's level; ghost notes barely get it (ADR 0032) ---------
    // e = SPLASH x sudden x loud, loud re splash::kLoudRef on the input after
    // the INPUT gain (here 1: DRIVE 0). A -6 dBFS DAW-level hit reaches the
    // loud floor; -12 dBFS gets a quarter of it at most, a -18 dBFS one 6 %.
    // Clang energy (the amount, squared and summed): -12 under -6, -18 under
    // a quarter of -6 (the M7 ghost bar), every ATTITUDE, SPLASH 0.5 / 1.
    {
        bool ok = true;
        for (int a : {0, 1, 2})
            for (bool rim : {false, true})
                for (float s : {0.5f, 1.0f}) {
                    const double e6 = energy(run(hit(-6, rim, 1.0f, kFs), a, s).clang);
                    const double e12 = energy(run(hit(-12, rim, 1.0f, kFs), a, s).clang);
                    const double e18 = energy(run(hit(-18, rim, 1.0f, kFs), a, s).clang);
                    const double r = e18 / e6;
                    std::printf("      %-6s %s SPLASH %.1f: Clang -12 dBFS %6.1f dB, -18 dBFS %6.1f dB re -6 dBFS (ghost %.1f %%)\n",
                                kAttName[a], rim ? "rim  " : "snare", s, db(e12 / e6), db(r), 100.0 * r);
                    ok &= e6 > 0 && r < 0.25 && e12 < e6;
                }
        check(ok, "-18 dBFS ghost gives < 25 % of the -6 dBFS hit's Clang (every ATTITUDE, SPLASH 0.5/1, snare+rim, INPUT 0 dB)");
    }

    // ---- The Bite: short, cracking hits bite; chord stabs clang (ADR 0032) ----------------
    // "Short" = the share of the hit's peak envelope above 2 kHz (SplashVoicing.h).
    // DRIVEN / KICKED, SPLASH 1: the snare and the rim count as short (>= 0.8
    // where the envelope peaks: Bite / kBiteGain vs Clang / Voice::clang), the
    // chord stab next to none (< 5 % of the snare's Bite energy) but a Clang
    // at least half the snare's; CLEAN gets no Bite at all.
    {
        bool ok = true;
        for (int a : {0, 1, 2}) {
            const Run sn = run(hit(-6, false, 1.0f, kFs), a, 1.0f), rm = run(hit(-6, true, 1.0f, kFs), a, 1.0f),
                      st = run(stab(1.0f, kFs), a, 1.0f);
            const float bs = peakOf(sn.bite), br = peakOf(rm.bite), bst = peakOf(st.bite), cst = peakOf(st.clang);
            const double eStab = db(energy(st.bite) / std::max(1e-30, energy(sn.bite)));
            std::printf("      %-6s SPLASH 1: Bite peak snare %.2f, rim %.2f, stab %.3f (energy %.1f dB re the snare's); Clang peak stab %.2f, snare %.2f\n",
                        kAttName[a], bs, br, bst, eStab, cst, peakOf(sn.clang));
            if (a == 0) ok &= bs == 0.0f && br == 0.0f && bst == 0.0f && cst >= 0.5f * peakOf(sn.clang);
            else
                // Bite peak / kBiteGain vs Clang peak / clang = how short (0..1) at e's peak.
                ok &= bs / splash::kBiteGain >= 0.8f * peakOf(sn.clang) / splash::kVoice[size_t(a)].clang
                   && br / splash::kBiteGain >= 0.8f * peakOf(rm.clang) / splash::kVoice[size_t(a)].clang && eStab < -13.0
                   && cst >= 0.5f * peakOf(sn.clang);
        }
        check(ok, "Bite on short hits only: snare and rim count as short (>= 0.8 at the envelope's peak), a chord stab gets < 5 % of the snare's Bite energy but "
                  "half its Clang or more; CLEAN never bites (SPLASH 1)");
    }

    // ---- Per-ATTITUDE behaviour ------------------------------------------------------
    {
        const Buf hard = hit(-6, false, 1.5f, kFs);
        const Run d0 = run(hard, 1, 0.0f), d1 = run(hard, 1, 1.0f), k1 = run(hard, 2, 1.0f), k0 = run(hard, 2, 0.0f);
        // SPLASH 0: no Clang, no Bite and no Clatter in any ATTITUDE (the
        // old Clatter floor read as a click on every hard hit). The Jolt
        // floor stays: a slight pitch lurch, no transient.
        std::snprintf(msg, sizeof msg,
                      "SPLASH 0: no Clang, Bite or Clatter on a hard hit (DRIVEN, KICKED); Jolt floor stays (peak %.4f / %.4f of L)",
                      d0.maxJolt, k0.maxJolt);
        check(energy(d0.clang) == 0.0 && energy(d0.bite) == 0.0 && energy(d0.clatter) == 0.0 && energy(k0.clang) == 0.0
                  && energy(k0.bite) == 0.0 && energy(k0.clatter) == 0.0 && d0.maxJolt > 0.0f && k0.maxJolt > d0.maxJolt,
              msg);
        // Hits fire no Clatter (ADR 0032: the Kick's crash only) and so no
        // rattle impacts: one impact (the Jolt) per stroke.
        std::snprintf(msg, sizeof msg,
                      "SPLASH 1: no Clatter on hits (energy %.1e / %.1e), one impact per stroke (DRIVEN %d / %d, KICKED %d / %d)",
                      energy(d1.clatter), energy(k1.clatter), d1.impacts, d1.strokes, k1.impacts, k1.strokes);
        check(energy(d1.clatter) == 0.0 && energy(k1.clatter) == 0.0 && d1.impacts == 1 && d1.strokes == 1
                  && k1.impacts == 1 && k1.strokes == 1,
              msg);
        std::snprintf(msg, sizeof msg, "KICKED > DRIVEN at SPLASH 1: Jolt peak %.4f vs %.4f of L",
                      *std::max_element(k1.jolt.begin(), k1.jolt.end()), *std::max_element(d1.jolt.begin(), d1.jolt.end()));
        check(*std::max_element(k1.jolt.begin(), k1.jolt.end()) > 1.5f * *std::max_element(d1.jolt.begin(), d1.jolt.end()), msg);

        // CLEAN (ADR 0025, 0032): nothing at SPLASH 0; at SPLASH 1 the same
        // Clang as DRIVEN (the owner's C2 in every ATTITUDE), no Bite, and a
        // Jolt (Loop and allpass) at most a third of DRIVEN's.
        const Run c1 = run(hard, 0, 1.0f), c0 = run(hard, 0, 0.0f);
        const bool silent0 = energy(c0.clang) == 0.0 && energy(c0.bite) == 0.0 && c0.maxJolt == 0.0f && c0.maxAllpass == 0.0f;
        check(silent0, "CLEAN SPLASH 0: no Clang, no Bite, no Jolt (hi-fi unless asked)");
        std::snprintf(msg, sizeof msg,
                      "CLEAN SPLASH 1: Clang %+.1f dB re DRIVEN's (the same, within 0.1), no Bite; Jolt %.4f of L (DRIVEN %.4f, "
                      "<= 1/3), allpass %.4f (DRIVEN %.4f, <= 1/3)",
                      db(energy(c1.clang) / energy(d1.clang)), c1.maxJolt, d1.maxJolt, c1.maxAllpass, d1.maxAllpass);
        check(std::fabs(db(energy(c1.clang) / energy(d1.clang))) < 0.1 && energy(c1.bite) == 0.0 && c1.maxJolt > 0.0f
                  && c1.maxJolt <= d1.maxJolt / 3.0f && c1.maxAllpass > 0.0f && c1.maxAllpass <= d1.maxAllpass / 3.0f,
              msg);
    }

    // ---- Clatter (the Kick's crash): band, decay; the Jolt's timing jitter ------------------------
    {
        const Buf quiet(size_t(1.5f * kFs), 0.0f);
        const Run k1 = run(quiet, 2, 1.0f, 48, kFs, long(0.2f * kFs));
        const double share = bandShare(k1.clatter, 800.0f, 8000.0f, kFs), core = bandShare(k1.clatter, 1000.0f, 6000.0f, kFs);
        std::snprintf(msg, sizeof msg, "Kick's Clatter is band-passed noise: %.0f %% of energy in 0.8-8 kHz, %.0f %% in 1-6 kHz", 100 * share, 100 * core);
        check(share > 0.8 && core > 0.55, msg);
        std::snprintf(msg, sizeof msg, "KICKED Kick rattle: %d impacts from one strike (DRIVEN %d), one stroke each", k1.impacts,
                      run(quiet, 1, 1.0f, 48, kFs, long(0.2f * kFs)).impacts);
        check(k1.impacts > run(quiet, 1, 1.0f, 48, kFs, long(0.2f * kFs)).impacts && k1.strokes == 1, msg);

        bool decays = true;
        for (float ms : {5.0f, 30.0f}) {
            rv::dsp::Clatter c;
            c.prepare(kFs, 3u);
            c.impact(1.0f, ms);
            int n = 0;
            while (c.envelope() > std::exp(-1.0f) && n < 48000) {
                c.process();
                ++n;
            }
            std::printf("      Clatter burst set to %.0f ms: envelope 1/e after %.2f ms\n", ms, 1000.0f * float(n) / kFs);
            decays &= std::fabs(1000.0f * float(n) / kFs - ms) < 0.1f;
        }
        const float dMin = splash::kVoice[1].clatterDecayMinMs, dMax = splash::kVoice[2].clatterDecayMaxMs;
        std::snprintf(msg, sizeof msg, "Clatter bursts decay in 5-30 ms (DRIVEN weak %.0f ms ... KICKED Hit-1 %.0f ms)", dMin, dMax);
        check(decays && dMin >= 5.0f && dMax <= 30.0f, msg);

        // Timing jitter: onset of a hit's Jolt after the hit, across seeds.
        const Buf hard = hit(-6, false, 0.5f, kFs);
        double lo = 1e9, hi = 0;
        for (uint32_t seed = 1; seed <= 16; ++seed) {
            const Run r = run(hard, 2, 1.0f, 1, kFs, -1, seed);
            const double ms = 1000.0 * (double(firstNonZero(r.jolt)) - 0.2 * kFs) / kFs;
            lo = std::min(lo, ms);
            hi = std::max(hi, ms);
        }
        std::snprintf(msg, sizeof msg, "few-ms timing jitter: the Jolt's onset %.2f..%.2f ms after the hit over 16 seeds (0.5 .. %.1f)", lo, hi,
                      splash::kJitterMaxMs + splash::kMaxRiseMs + 2000.0 * splash::kControlInterval / kFs);
        // Since ADR 0032 a hit's first impact is its Jolt (no Clatter on hits).
        // Bound, from the design: the jitter (<= kJitterMaxMs) runs from the
        // Hit's peak, which may come up to kMaxRiseMs after the onset, plus
        // up to two control ticks (the onset's and the re-check): 6.3 ms.
        const double maxMs = splash::kJitterMaxMs + splash::kMaxRiseMs + 2000.0 * splash::kControlInterval / kFs;
        check(lo >= 0.5 && hi <= maxMs && hi - lo >= 1.5, msg);
    }

    // ---- Jolt: pitch lurch that settles within ~1 s -----------------------------------------
    {
        const float L = map::tensionLoopDelaySeconds(0.5f) * kFs;
        const Buf hard = hit(-6, false, 2.0f, kFs);
        for (int a : {1, 2}) {
            const Run r = run(hard, a, 1.0f);
            const Lurch all = lurch(r.jolt, L), late = lurch(r.jolt, L, size_t(1.2f * kFs));
            float peak = 0, after = 0;
            for (size_t i = 0; i < r.jolt.size(); ++i) {
                peak = std::max(peak, std::fabs(r.jolt[i]));
                if (i > size_t(1.2f * kFs)) after = std::max(after, std::fabs(r.jolt[i]));
            }
            std::printf("      %-6s SPLASH 1 hard snare: Jolt peak %.4f of L (%.1f samples at L = 55 ms), lurch %.0f / %+.0f cents per pass, |Δa| %.3f\n",
                        kAttName[a], peak, peak * L, all.down, all.up, r.maxAllpass);
            std::snprintf(msg, sizeof msg, "%s: Jolt settles within 1 s of the hit: %.2f %% of peak, %.2f cents after 1 s",
                          kAttName[a], 100.0 * after / peak, std::max(-late.down, late.up));
            check(after < 0.02f * peak && std::max(-late.down, late.up) < 1.0, msg);
            if (a == 2) {
                std::snprintf(msg, sizeof msg, "KICKED: clear pitch lurch (%.0f cents per pass >= 20), |Δa| %.3f",
                              std::max(-all.down, all.up), r.maxAllpass);
                check(std::max(-all.down, all.up) >= 20.0 && r.maxAllpass > 0.05f, msg);
            }
        }
        // Worst case slope stays within Spring::kLoopSlewPerSample (0.08) at the longest L (100 ms × max detune 1.08).
        const Run k = run(hard, 2, 1.0f, 48, kFs, long(0.2f * kFs));
        const Lurch w = lurch(k.jolt, 0.100f * 1.08f * kFs);
        std::snprintf(msg, sizeof msg, "Jolt slope %.3f samples/sample at L = 108 ms (hit + Kick strike) stays under the Loop slew limit 0.08", w.slope);
        check(w.slope < 0.08, msg);
    }

    // ---- Kick strike, rolls, determinism, block size ------------------------------------------
    {
        const Buf quiet(size_t(1.0f * kFs), 0.0f);
        // The burst starts on the strike's sample: its envelope, read per
        // sample at block 1 (the sparse excitation's first click lands
        // somewhere in its first cell); every other block size gives a
        // bit-identical Clatter, so the same onset.
        const Run one = run(quiet, 2, 0.0f, 1, kFs, 12345);
        bool exact = firstNonZero(one.env) == 12345;
        for (int b : {7, 32, 48, 333, 1024}) exact &= run(quiet, 2, 0.0f, b, kFs, 12345).clatter == one.clatter;
        check(exact, "Kick strike: the Clatter burst starts on the exact sample (envelope, block 1); blocks 7, 32, 48, 333, "
                     "1024 bit-identical");
        const Run s0 = run(quiet, 2, 0.0f, 48, kFs, 12345), s1 = run(quiet, 2, 1.0f, 48, kFs, 12345);
        check(s0.clatter == s1.clatter && s0.strokes == 1, "Kick strike is maximal whatever SPLASH is (forced Hit 1, SPLASH 1)");

        // Snare roll: 8 strokes 100 ms apart -> 8 strokes.
        Buf roll(size_t(1.5f * kFs), 0.0f);
        for (int h = 0; h < 8; ++h) {
            const Buf one = hit(-6, false, 1.5f, kFs, 0.1f + 0.1f * float(h), uint32_t(h + 1));
            for (size_t i = 0; i < roll.size(); ++i) roll[i] += one[i];
        }
        const Run r = run(roll, 2, 1.0f);
        std::snprintf(msg, sizeof msg, "snare roll, 8 strokes 100 ms apart: %d strokes detected", r.strokes);
        check(r.strokes == 8, msg);

        Buf hard = hit(-6, false, 1.0f, kFs);
        for (size_t i = 0; i < hard.size(); ++i) hard[i] += quiet[std::min(i, quiet.size() - 1)];
        const Run ref = run(hard, 2, 1.0f, 48, kFs, long(0.6f * kFs));
        const Run again = run(hard, 2, 1.0f, 48, kFs, long(0.6f * kFs));
        check(again.clatter == ref.clatter && again.clang == ref.clang && again.bite == ref.bite && again.jolt == ref.jolt,
              "deterministic: same seed, same Clang, Bite, Clatter and Jolt");
        bool blocks = true;
        for (int b : {1, 7, 32, 333, 1024}) {
            const Run x = run(hard, 2, 1.0f, b, kFs, long(0.6f * kFs));
            blocks &= x.clatter == ref.clatter && x.jolt == ref.jolt && x.clang == ref.clang && x.bite == ref.bite;
        }
        check(blocks, "block-size independent: Clang, Bite, Clatter and Jolt bit-identical for blocks 1, 7, 32, 333, 1024");
        rv::dsp::Splash sp;
        sp.prepare(kFs, 7u);
        sp.set(att(2), 1.0f);
        Buf c(hard.size()), j(hard.size()), cl1(hard.size()), cl2(hard.size()), bt(hard.size());
        sp.process(hard.data(), cl1.data(), bt.data(), c.data(), nullptr, nullptr, j.data(), int(hard.size()));
        sp.reset();
        sp.process(hard.data(), cl2.data(), bt.data(), c.data(), nullptr, nullptr, j.data(), int(hard.size()));
        check(cl1 == cl2, "reset() replays identically");
    }

    // ---- Sample-rate aware ---------------------------------------------------------------
    {
        // Rate-independent stimulus: a 2 kHz tone burst, 30 ms decay.
        auto burst = [](float peak, float fs) {
            Buf x(size_t(fs), 0.0f);
            for (size_t i = 0; i < size_t(0.2f * fs); ++i) {
                const float t = float(i) / fs;
                x[size_t(0.2f * fs) + i] = peak * std::sin(2.0f * map::kPi * 2000.0f * t) * std::exp(-t / 0.03f);
            }
            return x;
        };
        const Run r48 = run(burst(0.5f, 48000.0f), 2, 0.5f, 48, 48000.0f), r96 = run(burst(0.5f, 96000.0f), 2, 0.5f, 48, 96000.0f);
        const float g48 = run(burst(0.125f, 48000.0f), 2, 0.5f, 48, 48000.0f).maxHit;
        const float g96 = run(burst(0.125f, 96000.0f), 2, 0.5f, 48, 96000.0f).maxHit;
        const float c48 = peakOf(r48.clang), c96 = peakOf(r96.clang);
        std::snprintf(msg, sizeof msg,
                      "same Hit and Clang at 48 and 96 kHz (2 kHz tone burst): Hit -6 dBFS %.3f / %.3f, -18 dBFS %.3f / %.3f; "
                      "Clang peak %.3f / %.3f",
                      r48.maxHit, r96.maxHit, g48, g96, c48, c96);
        check(std::fabs(r48.maxHit - r96.maxHit) < 0.05f && std::fabs(g48 - g96) < 0.05f && std::fabs(c48 - c96) < 0.05f * c48,
              msg);
    }

    // ---- The real stimulus: test_audio/stimulus/02_hits.wav --------------------------------
    // Snare at -6 / -12 / -18 dBFS from 1 s, then rim, 6 s apart
    // (tools/make_stimulus.py). Skipped if the stimulus is not generated.
    {
        wav::Audio a;
        std::string err;
        bool loaded = false;
        for (const char* prefix : {"../test_audio/stimulus/", "test_audio/stimulus/"})
            if (!loaded) loaded = wav::read(std::string(prefix) + "02_hits.wav", a, err);
        if (!loaded) {
            std::printf("SKIP  02_hits.wav not found (run python3 tools/make_stimulus.py)\n");
        } else {
            const Buf& x = a.channels[0];
            bool ok = true;
            for (int at : {1, 2})
                for (float sv : {0.5f, 1.0f}) {
                    const Run r = run(x, at, sv, 48);
                    double e[6];
                    for (int h = 0; h < 6; ++h) {
                        const size_t from = size_t((1.0f + 6.0f * float(h)) * kFs), to = from + size_t(6.0f * kFs);
                        e[h] = 0;
                        for (size_t i = from; i < std::min(to, r.clang.size()); ++i)
                            e[h] += double(r.clang[i]) * r.clang[i] + double(r.bite[i]) * r.bite[i];
                    }
                    std::printf("      02_hits %-6s SPLASH %.1f: snare -12/-18 %.1f / %.1f dB, rim -12/-18 %.1f / %.1f dB re -6 dBFS; %d strokes\n",
                                kAttName[at], sv, db(e[1] / e[0]), db(e[2] / e[0]), db(e[4] / e[3]), db(e[5] / e[3]), r.strokes);
                    ok &= e[0] > e[1] && e[1] >= e[2] && e[2] < 0.25 * e[0] && e[3] > e[4] && e[4] >= e[5] && e[5] < 0.25 * e[3] &&
                          r.strokes <= 6;
                }
            check(ok, "02_hits.wav: Clang + Bite fall with level, ghost < 25 % of the hard hit, at most one stroke per hit (DRIVEN, KICKED)");
        }
    }

    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
