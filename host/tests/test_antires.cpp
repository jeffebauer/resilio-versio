// AntiRes tests for M6 (SPEC §4.10, §7 M6; ADR 0010, 0018, 0019;
// docs/m6-metric-calibration.md). Dependency-free: prints PASS/FAIL/INFO
// lines, returns nonzero on any failure. Optional argv[1] filter by name.
//
// What is checked here (the full M6 grid runs through the Renderer, see
// presets/sweeps/m6_*.json and docs/m6-metric-calibration.md):
//   micromod   Layer 2 shape: peak depth, slow, smooth, independent per
//              Spring, deterministic (reset() restarts it), on at WOBBLE 0.
//   pitch      Layer 2 inaudible on held tones (08_held_tones.wav): pitch
//              deviation of the Tank's wet output at WOBBLE 0, in cents.
//   evenness   Layer 1: per-trip Loop gain and T60 across frequency at every
//              ATTITUDE x TONE x BOING x DECAY corner: below target everywhere,
//              and no band standing above its neighbours.
//   tail       The Ringing metric on real Tank tails: passes as built, and
//              flags the same tail with a slow mode injected (not toothless).
//   howl       KICKED Howl zone: ADR 0019 criterion; exit by pulling DECAY
//              drops >= 30 dB within 3 s (ADR 0018); ATTITUDE-flip exit
//              reported (open owner decision, not changed).
//   cpu        Cost of the modulation per Spring.

#include "Metrics.h"
#include "Wav.h"
#include "dsp/Tank.h"
#include "params/AntiRes.h"
#include "params/DriveVoicing.h"
#include "params/Mappings.h"
#include "params/SpringModes.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

int failures = 0;
char msg[512];

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++failures;
}

using Buf = std::vector<float>;
constexpr float  kFs = 48000.0f;
constexpr double kPi = 3.14159265358979323846;
const char* const kAttName[3] = {"CLEAN", "DRIVEN", "KICKED"};

struct Settings {
    float decay = 1.0f, boing = 0.5f, tone = 0.5f, mix = 1.0f, drive = 0.5f, wobble = 0.0f;
    int   att = 0, springs = 2;
};

void apply(rv::Tank& t, const Settings& s)
{
    t.setParam(rv::ParamId::Decay, s.decay);
    t.setParam(rv::ParamId::Boing, s.boing);
    t.setParam(rv::ParamId::Tone, s.tone);
    t.setParam(rv::ParamId::Mix, s.mix);
    t.setParam(rv::ParamId::Drive, s.drive);
    t.setParam(rv::ParamId::Wobble, s.wobble);
    t.setParam(rv::ParamId::Attitude, rv::switchToNormalised(s.att));
    t.setParam(rv::ParamId::Springs, rv::switchToNormalised(s.springs));
}

struct Stereo {
    Buf l, r;
};

Stereo render(rv::Tank& t, const Buf& in, int block = 48)
{
    const size_t n = in.size();
    Stereo o{Buf(n), Buf(n)};
    for (size_t pos = 0; pos < n; pos += size_t(block)) {
        const int k = int(std::min(size_t(block), n - pos));
        t.process(in.data() + pos, in.data() + pos, o.l.data() + pos, o.r.data() + pos, k);
    }
    return o;
}

double db(double p) { return 10.0 * std::log10(p + 1e-30); }
double power(const Buf& x, size_t from, size_t to)
{
    double s = 0;
    to = std::min(to, x.size());
    for (size_t i = from; i < to; ++i) s += double(x[i]) * x[i];
    return to > from ? s / double(to - from) : 0.0;
}
double stereoPowerDb(const Stereo& o, size_t from, size_t to) { return db(0.5 * (power(o.l, from, to) + power(o.r, from, to))); }

bool readStimulus(const std::string& name, rv::wav::Audio& out)
{
    std::string error;
    for (const std::string& prefix : {"../test_audio/stimulus/", "test_audio/stimulus/"})
        if (rv::wav::read(prefix + name, out, error)) return true;
    std::fprintf(stderr, "could not read stimulus %s: %s\n", name.c_str(), error.c_str());
    return false;
}

Buf monoOf(const rv::wav::Audio& a)
{
    Buf m(a.frames(), 0.0f);
    for (const auto& c : a.channels)
        for (size_t i = 0; i < m.size(); ++i) m[i] += c[i] / float(a.channels.size());
    return m;
}

// One click (-6 dBFS, 2 samples) at 1 s, like 07_click_single.wav.
Buf click(double seconds)
{
    Buf b(size_t(seconds * kFs), 0.0f);
    b[size_t(kFs)] = b[size_t(kFs) + 1] = 0.5f;
    return b;
}

// ---- 1. Micro-mod floor shape ------------------------------------------------------------------
void microMod()
{
    rv::Tank t;
    t.prepare(kFs, 48);
    Settings s;
    s.springs = 2;
    apply(t, s);
    const size_t n = size_t(120.0f * kFs);
    std::vector<Buf> m(3, Buf(n / 48));
    Buf zero(48, 0.0f), ol(48), orr(48);
    for (size_t b = 0; b < n / 48; ++b) {
        t.process(zero.data(), zero.data(), ol.data(), orr.data(), 48);
        for (int sp = 0; sp < 3; ++sp) m[size_t(sp)][b] = t.spring(sp).lengthModulation() - 1.0f;
    }
    double peak = 0, maxStep = 0;
    long crossings = 0;
    for (int sp = 0; sp < 3; ++sp) {
        const Buf& x = m[size_t(sp)];
        for (size_t i = 1; i < x.size(); ++i) {
            peak = std::max(peak, double(std::fabs(x[i])));
            maxStep = std::max(maxStep, double(std::fabs(x[i] - x[i - 1])) / 48.0); // per sample
            if ((x[i] > 0) != (x[i - 1] > 0)) ++crossings;
        }
    }
    const double rateHz = double(crossings) / 3.0 / 120.0 / 2.0; // zero-crossings / 2 = "cycles"
    auto corr = [&](const Buf& a, const Buf& b) {
        double ab = 0, aa = 0, bb = 0;
        for (size_t i = 0; i < a.size(); ++i) { ab += double(a[i]) * b[i]; aa += double(a[i]) * a[i]; bb += double(b[i]) * b[i]; }
        return ab / std::sqrt(aa * bb + 1e-30);
    };
    const double c01 = corr(m[0], m[1]), c02 = corr(m[0], m[2]), c12 = corr(m[1], m[2]);
    const double depth = double(rv::antires::kMicroModDepth);
    std::snprintf(msg, sizeof msg,
                  "Micro-mod floor at WOBBLE 0: peak %.3f %% of L over 120 s (target %.3f %%), ~%.2f Hz, "
                  "max change %.2g of L per sample (smooth), Springs independent (corr %.2f / %.2f / %.2f)",
                  peak * 100, depth * 100, rateHz, maxStep, c01, c02, c12);
    check(peak > 0.7 * depth && peak < 1.3 * depth && rateHz < 2.0 && maxStep < 1e-7 && std::fabs(c01) < 0.3
              && std::fabs(c02) < 0.3 && std::fabs(c12) < 0.3,
          msg);

    // Deterministic: reset() restarts the modulation from the same seed.
    Buf in = click(3.0);
    rv::Tank a;
    a.prepare(kFs, 48);
    apply(a, s);
    const Stereo first = render(a, in);
    a.reset();
    const Stereo again = render(a, in);
    bool same = first.l == again.l && first.r == again.r;
    for (int block : {1, 7, 512}) {
        rv::Tank b;
        b.prepare(kFs, 512);
        apply(b, s);
        const Stereo o = render(b, in, block);
        same &= o.l == first.l && o.r == first.r;
    }
    check(same, "Micro-mod floor deterministic: reset() restarts it; block sizes 1, 7, 48, 512 bit-identical");
}

// ---- 2. Held-tone pitch at WOBBLE 0 --------------------------------------------------------------
// Pitch of one partial: complex demodulation at f0 over 200 ms Hann windows
// every 20 ms; the phase advance between windows gives the frequency
// offset. Returns the largest |deviation| in cents and the 95th percentile
// (the largest can come from a momentary amplitude dip, where phase is
// ill-defined), over [from, to). level (optional) = the partial's mean power.
void pitchCents(const Buf& x, double f0, size_t from, size_t to, double& maxC, double& p95C, double* level = nullptr)
{
    const size_t W = size_t(0.2 * kFs), step = size_t(0.02 * kFs);
    std::vector<double> win(W);
    for (size_t i = 0; i < W; ++i) win[i] = 0.5 - 0.5 * std::cos(2 * kPi * double(i) / double(W));
    std::vector<std::complex<double>> z;
    for (size_t s = from; s + W <= to; s += step) {
        std::complex<double> acc = 0;
        for (size_t i = 0; i < W; ++i) {
            const double ph = -2 * kPi * f0 * double(s + i) / double(kFs);
            acc += win[i] * double(x[s + i]) * std::complex<double>(std::cos(ph), std::sin(ph));
        }
        z.push_back(acc);
    }
    std::vector<double> c;
    if (level) {
        *level = 0;
        for (const auto& v : z) *level += std::norm(v) / double(z.size());
    }
    for (size_t i = 1; i < z.size(); ++i) {
        const double dph = std::arg(z[i] * std::conj(z[i - 1]));
        const double df = dph / (2 * kPi * double(step) / double(kFs));
        c.push_back(std::fabs(1200.0 * std::log2((f0 + df) / f0)));
    }
    maxC = c.empty() ? 0 : *std::max_element(c.begin(), c.end());
    std::sort(c.begin(), c.end());
    p95C = c.empty() ? 0 : c[size_t(0.95 * double(c.size() - 1))];
}

void heldTonePitch()
{
    rv::wav::Audio a;
    if (!readStimulus("08_held_tones.wav", a)) { check(false, "08_held_tones.wav readable"); return; }
    const Buf in = monoOf(a);
    // 1 kHz from 1 s to 9 s, A minor chord (220, 261.63, 329.63 Hz) from 10 s to 18 s.
    // Measure from 2 s into each (build-up done) to 0.5 s before its end.
    const size_t s = size_t(kFs);
    struct Part { double f; size_t from, to; };
    const Part parts[] = {{1000.0, 3 * s, 8 * s + s / 2}, {220.0, 12 * s, 17 * s + s / 2},
                          {261.63, 12 * s, 17 * s + s / 2}, {329.63, 12 * s, 17 * s + s / 2}};
    // Estimator floor: the dry input itself.
    double floorMax = 0, floorP95 = 0;
    for (const Part& p : parts) {
        double mx, p95;
        pitchCents(in, p.f, p.from, p.to, mx, p95);
        floorMax = std::max(floorMax, mx);
        floorP95 = std::max(floorP95, p95);
    }
    std::printf("INFO  held-tone pitch estimator on the dry input: max %.3f, p95 %.3f cents\n", floorMax, floorP95);

    // (a) What the Micro-mod floor itself adds: one Spring (Spring A's
    // detune, as in 1-Spring mode) with and without it, same input. The
    // difference is the floor's own pitch movement; the rest is there without
    // it too (a held tone through a long tail is not a pure tone yet: the
    // onset's free-ringing modes beat against it, which a phase-based pitch
    // estimate reads as a few cents of wobble).
    double worstAdd = 0, worstWith = 0, worstWithout = 0;
    for (float d : {0.5f, 1.0f})
        for (float b : {0.0f, 1.0f}) {
            double p95[2] = {0, 0};
            for (int on = 0; on < 2; ++on) {
                rv::SpringSettings ss;
                ss.loopDelaySeconds = rv::map::decayLoopDelaySeconds(d) * rv::modes::kDetune[0].loopDelay;
                ss.t60Seconds       = rv::map::decayT60Seconds(d);
                ss.transitionHz     = rv::map::decayTransitionHz(d) * rv::modes::kDetune[0].transition;
                ss.allpassCoeff     = rv::map::boingCoefficient(b) * rv::modes::kDetune[0].allpassCoeff;
                ss.stages           = rv::map::boingStages(b);
                ss.dampingHz        = rv::map::toneDampingHz(0.5f);
                ss.highPathLevel    = rv::map::toneHighPathLevel(0.5f);
                ss.tapRatio         = rv::modes::kPickupTap[0];
                ss.modDepth         = on ? rv::antires::kMicroModDepth : 0.0f;
                std::vector<float> pool(rv::Spring::requiredFloats(kFs), 0.0f);
                rv::Spring spr;
                spr.prepare(kFs, pool.data(), 0x9E3779B9u);
                spr.setSettings(ss, true);
                Buf out(in.size());
                for (size_t pos = 0; pos < in.size(); pos += 32) {
                    const int k = int(std::min<size_t>(32, in.size() - pos));
                    spr.process(in.data() + pos, out.data() + pos, k);
                }
                for (const Part& p : parts) {
                    double mx, q;
                    pitchCents(out, p.f, p.from, p.to, mx, q);
                    p95[on] = std::max(p95[on], q);
                }
            }
            std::printf("INFO    one Spring DECAY %.1f BOING %.0f: p95 %.2f cents without the floor, %.2f with\n", d, b,
                        p95[0], p95[1]);
            worstAdd = std::max(worstAdd, p95[1] - p95[0]);
            worstWith = std::max(worstWith, p95[1]);
            worstWithout = std::max(worstWithout, p95[0]);
        }

    // (b) The Tank as the owner hears it (WOBBLE 0, wet only), every SPRINGS
    // mode, DECAY 0.5 and 1, CLEAN and DRIVEN: the mono sum, and each
    // channel where it carries the partial. A partial more than 20 dB weaker
    // in one channel than in the other (the width stage, L = mid + side + wD,
    // R = mid - side - wD, can all but cancel one steady partial on one side,
    // M8) is heard from the other channel; there its phase is dominated by
    // the tail's neighbouring modes and reads as cents of "pitch" that nobody
    // hears (M8 measured 23 cents p95 on a partial 33 dB down in R). Those
    // channel readings are skipped and counted.
    double worstMax = 0, worstP95 = 0;
    char worstAt[96] = {};
    for (int m = 0; m < 3; ++m)
        for (float d : {0.5f, 1.0f})
            for (int att : {0, 1}) {
                Settings st;
                st.springs = m;
                st.decay = d;
                st.att = att;
                rv::Tank t;
                t.prepare(kFs, 48);
                apply(t, st);
                const Stereo o = render(t, in);
                double cellMax = 0, cellP95 = 0;
                int skipped = 0;
                Buf mid(o.l.size());
                for (size_t i = 0; i < mid.size(); ++i) mid[i] = 0.5f * (o.l[i] + o.r[i]);
                for (const Part& p : parts) {
                    double mx[3], p95[3], lv[3];
                    const Buf* chans[3] = {&o.l, &o.r, &mid};
                    for (int k = 0; k < 3; ++k) pitchCents(*chans[k], p.f, p.from, p.to, mx[k], p95[k], &lv[k]);
                    for (int k = 0; k < 3; ++k) {
                        if (k < 2 && lv[k] < 0.01 * lv[1 - k]) { ++skipped; continue; } // > 20 dB down: not heard here
                        cellMax = std::max(cellMax, mx[k]);
                        cellP95 = std::max(cellP95, p95[k]);
                    }
                }
                std::printf("INFO    Tank %d Spring%s DECAY %.1f %-6s: max %.2f, p95 %.2f cents (%d channel reading%s "
                            "skipped: partial > 20 dB down)\n",
                            m + 1, m ? "s" : " ", d, kAttName[att], cellMax, cellP95, skipped, skipped == 1 ? "" : "s");
                if (cellP95 > worstP95) {
                    worstP95 = cellP95;
                    std::snprintf(worstAt, sizeof worstAt, "%d Spring(s) DECAY %.1f %s", m + 1, d, kAttName[att]);
                }
                worstMax = std::max(worstMax, cellMax);
            }
    std::snprintf(msg, sizeof msg,
                  "Micro-mod floor inaudible on held tones at WOBBLE 0 (1 kHz + A minor chord): it adds at most %.2f "
                  "cents p95 to one Spring (%.2f with vs %.2f without; limit +1 cent); the whole Tank reads p95 %.2f "
                  "cents (worst %s, max %.2f; limit 3, most of it the tail's own beating, see above)",
                  worstAdd, worstWith, worstWithout, worstP95, worstAt, worstMax);
    check(worstAdd <= 1.0 && worstP95 <= 3.0, msg);
}

// ---- 3. Layer 1: even Loop gain ----------------------------------------------------------------
// Per Spring, small signal, from its current coefficients:
//   P(f)   = g |H(f)|  (per trip)       must be < 1 everywhere outside the Howl zone
//   T60(f) = the Loop's decay time      must not exceed the DECAY target anywhere
// and neither may have a band standing above its neighbourhood: each is
// compared with its own median over +-1/3 octave (a smooth curve reads
// ~0 dB; a narrow bump that could Ring would stand out).
double medianOf(std::vector<double> v)
{
    std::nth_element(v.begin(), v.begin() + long(v.size() / 2), v.end());
    return v[v.size() / 2];
}

void loopEvenness()
{
    int cells = 0, bad = 0, fcBinds = 0;
    double shortBump = 0;
    double worstP = 0, worstPBump = 0, worstT60Ratio = 0, worstT60Bump = 0;
    char atP[128] = {}, atPB[128] = {}, atT[128] = {}, atTB[128] = {};
    std::vector<double> freqs;
    for (double f = 30.0; f < 0.49 * double(kFs); f *= std::pow(2.0, 1.0 / 48.0)) freqs.push_back(f);
    const size_t half = 16; // 1/3 octave at 48 points per octave
    for (int a = 0; a < 3; ++a)
        for (float tn : {0.0f, 0.5f, 1.0f})
            for (float d : {0.0f, 0.5f, 0.75f, 0.89f, 1.0f})
                for (float b : {0.0f, 0.5f, 1.0f}) {
                    if (a == 2 && d >= rv::drive::kHowlZoneStart) continue; // judged by the Howl criterion
                    Settings s;
                    s.att = a;
                    s.tone = tn;
                    s.decay = d;
                    s.boing = b;
                    s.springs = 2;
                    rv::Tank t;
                    t.prepare(kFs, 48);
                    apply(t, s);
                    render(t, Buf(4800, 0.0f));
                    const double target = double(rv::map::decayT60Seconds(d));
                    for (int sp = 0; sp < 3; ++sp) {
                        const rv::Spring& spr = t.spring(sp);
                        std::vector<double> P, T;
                        for (double f : freqs) {
                            P.push_back(double(spr.feedbackGain() * spr.loopMagnitude(float(f))));
                            T.push_back(double(spr.t60AtSeconds(float(f))));
                        }
                        ++cells;
                        char at[128];
                        std::snprintf(at, sizeof at, "%s TONE %.1f DECAY %.2f BOING %.1f Spring %d", kAttName[a], tn, d, b, sp);
                        double pMax = 0, pBump = -1e9, tMax = 0, tBump = -1e9;
                        for (size_t i = 0; i < freqs.size(); ++i) {
                            pMax = std::max(pMax, P[i]);
                            tMax = std::max(tMax, T[i]);
                        }
                        for (size_t i = 0; i < freqs.size(); ++i) {
                            const size_t lo = i > half ? i - half : 0, hi = std::min(freqs.size() - 1, i + half);
                            const double pm = medianOf(std::vector<double>(P.begin() + long(lo), P.begin() + long(hi) + 1));
                            const double tm = medianOf(std::vector<double>(T.begin() + long(lo), T.begin() + long(hi) + 1));
                            // Bumps only count where they could matter: in the
                            // audible band, and (for T60) in bands that last at
                            // least half as long as the longest one. A "bump"
                            // on a band that dies in a tenth of the tail's time
                            // can never outlive the tail.
                            if (freqs[i] < 16000.0) {
                                pBump = std::max(pBump, 20.0 * std::log10(P[i] / pm));
                                if (T[i] >= 0.5 * tMax) tBump = std::max(tBump, T[i] / tm);
                            }
                        }
                        // The design deliberately makes the slowest band ring
                        // kT60DesignScale longer than DECAY's T60 (Spring.h),
                        // so that is the ceiling.
                        const double tRatio = tMax / (target * double(rv::Spring::kT60DesignScale));
                        // Do the fC design points (Spring.cpp kDesignFcRatios) set g here?
                        {
                            float gFc = 1e9f;
                            for (float r : {0.6f, 0.75f, 0.85f, 0.92f, 1.0f}) {
                                const float hz = r * float(rv::map::decayTransitionHz(d)) * rv::modes::kDetune[size_t(sp)].transition;
                                gFc = std::min(gFc, std::exp(-3.0f * 2.302585093f * spr.roundTripSamples(hz)
                                                             / (float(target) * rv::Spring::kT60DesignScale * kFs))
                                                        / spr.loopMagnitude(hz));
                            }
                            if (gFc <= spr.feedbackGain() * 1.000001f) ++fcBinds;
                        }
                        // The T60-bump rule applies from DECAY 0.5 up (tails of
                        // ~2 s and longer, where a band could audibly Ring; the
                        // M6 grid is DECAY 0.75 and 1). Below that it is
                        // reported: with a > 0 (highs later) the tightest slap
                        // at max BOING has its Chirp's top edge lasting ~1.5x
                        // its neighbours (0.6 s vs 0.4 s), a "ping", not Ringing.
                        if (d < 0.5f) shortBump = std::max(shortBump, tBump);
                        if (pMax >= 1.0 || tRatio > 1.05 || pBump > 0.5 || (d >= 0.5f && tBump > 1.15)) ++bad;
                        if (d < 0.5f) tBump = 0.0;
                        if (pMax > worstP) { worstP = pMax; std::snprintf(atP, sizeof atP, "%s", at); }
                        if (pBump > worstPBump) { worstPBump = pBump; std::snprintf(atPB, sizeof atPB, "%s", at); }
                        if (tRatio > worstT60Ratio) { worstT60Ratio = tRatio; std::snprintf(atT, sizeof atT, "%s", at); }
                        if (tBump > worstT60Bump) { worstT60Bump = tBump; std::snprintf(atTB, sizeof atTB, "%s", at); }
                    }
                }
    std::printf("INFO  Loop evenness, %d Spring cells (BOING sign: a in [%.2f, %.2f]):\n", cells,
                double(rv::map::kBoingCoeffMin), double(rv::map::kBoingCoeffMax));
    std::printf("INFO    per-trip peak g|H| %.4f (%s)\n", worstP, atP);
    std::printf("INFO    per-trip bump over its 1/3-oct median %.3f dB (%s)\n", worstPBump, atPB);
    std::printf("INFO    longest T60(f) / (DECAY target x design scale %.2f) %.3f (%s)\n",
                double(rv::Spring::kT60DesignScale), worstT60Ratio, atT);
    std::printf("INFO    T60(f) bump over its 1/3-oct median x%.3f (%s), DECAY >= 0.5\n", worstT60Bump, atTB);
    std::printf("INFO    T60(f) bump below DECAY 0.5 (reported only) x%.3f\n", shortBump);
    std::printf("INFO    cells where the design points under fC set g: %d of %d (%s)\n", fcBinds, cells,
                rv::map::kHighsLater ? "a > 0: they may bind" : "0 expected while a < 0");
    std::snprintf(msg, sizeof msg,
                  "Layer 1, even Loop gain: every Spring at every ATTITUDE x TONE x BOING x DECAY corner outside the Howl "
                  "zone has per-trip gain < 1 (worst %.3f), no per-trip bump > 0.5 dB over its 1/3 octave (worst %.2f), "
                  "no band ringing longer than designed (worst x%.3f, limit 1.05), no T60 bump > x1.15 from DECAY 0.5 up (worst x%.3f); "
                  "%d of %d cells out",
                  worstP, worstPBump, worstT60Ratio, worstT60Bump, bad, cells);
    check(bad == 0, msg);
}

// ---- 4. The Ringing metric on real Tank tails ---------------------------------------------------
Buf noiseBurst(double seconds)
{
    Buf b(size_t(seconds * kFs), 0.0f);
    rv::dsp::Rng rng;
    rng.seed(2u);
    float y = 0;
    const float c = 1.0f - std::exp(-2.0f * float(kPi) * 3000.0f / kFs);
    const size_t at = size_t(kFs), len = size_t(0.3f * kFs);
    for (size_t i = 0; i < len; ++i) {
        y += c * (rng.bipolar() - y);
        const float fade = std::min(1.0f, std::min(float(i), float(len - 1 - i)) / (0.002f * kFs));
        b[at + i] = 1.2f * y * fade;
    }
    return b;
}

void tankTails()
{
    int n = 0, flagged = 0;
    double worst = 0;
    char worstAt[128] = {};
    for (int inp = 0; inp < 2; ++inp)
        for (int a = 0; a < 3; ++a)
            for (int m = 0; m < 3; ++m) {
                Settings s;
                s.att = a;
                s.springs = m;
                s.decay = a == 2 ? 0.75f : 1.0f; // KICKED DECAY 1 is the Howl zone
                s.boing = 1.0f;
                rv::Tank t;
                t.prepare(kFs, 48);
                apply(t, s);
                const Stereo o = render(t, inp ? noiseBurst(14.0) : click(14.0));
                const auto mm = rv::metrics::compute({o.l, o.r}, kFs);
                ++n;
                if (mm.ringing || mm.steadyTone) ++flagged;
                if (!std::isnan(mm.ringingDb) && mm.ringingDb > worst) {
                    worst = mm.ringingDb;
                    std::snprintf(worstAt, sizeof worstAt, "%s %s %d Spring(s)", inp ? "burst" : "click", kAttName[a], m + 1);
                }
            }
    std::snprintf(msg, sizeof msg,
                  "Ringing metric on Tank tails (click + noise burst, ATTITUDE x SPRINGS, DECAY max, BOING 1, WOBBLE 0): "
                  "%d of %d flagged, worst ringing_db %.1f (%s; limit %.0f)",
                  flagged, n, worst, worstAt, rv::metrics::kRingingGrowthDb);
    check(flagged == 0, msg);

    // Not toothless: the same kind of tail with one slow mode added (1.3 kHz,
    // T60 30 s vs the tail's ~9 s, starting 30 dB under the tail's peak
    // level) must be flagged; the bare tail must not.
    Settings s;
    s.springs = 2;
    rv::Tank t;
    t.prepare(kFs, 48);
    apply(t, s);
    Stereo o = render(t, noiseBurst(14.0));
    const auto bare = rv::metrics::compute({o.l, o.r}, kFs);
    double pk = 0;
    for (float v : o.l) pk = std::max(pk, double(std::fabs(v)));
    const double amp = pk * std::pow(10.0, -30.0 / 20.0);
    for (size_t i = size_t(1.3 * kFs); i < o.l.size(); ++i) {
        const double tt = double(i) / double(kFs) - 1.3;
        const float v = float(amp * std::pow(10.0, -3.0 * tt / 30.0) * std::sin(2 * kPi * 1300.0 * tt));
        o.l[i] += v;
        o.r[i] += v;
    }
    const auto ring = rv::metrics::compute({o.l, o.r}, kFs);
    std::snprintf(msg, sizeof msg,
                  "Ringing metric not toothless on a Tank tail: bare %.1f dB (not flagged), + slow 1.3 kHz mode "
                  "(T60 30 s, -30 dB re peak) %.1f dB at %.0f Hz (flagged)",
                  bare.ringingDb, ring.ringingDb, ring.ringingHz);
    check(!bare.ringing && ring.ringing && std::fabs(ring.ringingHz - 1300.0) < 20.0, msg);
}

// ---- 5. Howl zone ------------------------------------------------------------------------------
void howlZone()
{
    const size_t sec = size_t(kFs);
    for (int m = 0; m < 3; ++m) {
        // DRIVE 0.5 (noon), BOING/TONE noon, click input: like the M6 grid.
        const size_t n = 16 * sec, pullAt = 10 * sec;
        const Buf in = click(16.0);
        rv::Tank t;
        t.prepare(kFs, 48);
        Settings s;
        s.att = 2;
        s.springs = m;
        apply(t, s);
        Stereo o{Buf(n), Buf(n)};
        for (size_t pos = 0; pos < n; pos += 48) {
            if (pos == pullAt) t.setParam(rv::ParamId::Decay, 0.75f);
            t.process(in.data() + pos, in.data() + pos, o.l.data() + pos, o.r.data() + pos, 48);
        }
        // ADR 0019 on the sustained part (4-10 s).
        const Buf l(o.l.begin() + long(4 * sec), o.l.begin() + long(pullAt));
        const Buf r(o.r.begin() + long(4 * sec), o.r.begin() + long(pullAt));
        const auto hm = rv::metrics::compute({l, r}, kFs);
        const double before = stereoPowerDb(o, pullAt - sec / 2, pullAt);
        const double after3 = stereoPowerDb(o, pullAt + 3 * sec - sec / 4, pullAt + 3 * sec + sec / 4);
        std::snprintf(msg, sizeof msg,
                      "Howl KICKED DECAY 1 DRIVE 0.5, %d Spring%s (ADR 0019): sustains at %.1f dBFS; floor %.1f dB "
                      "(limit %.0f), steadiest 2 s moves %.2f %% / %.1f dB (limit %.1f %% or %.0f dB); DECAY -> 0.75: "
                      "%.1f dB lower 3 s later (ADR 0018, limit 30)",
                      m + 1, m ? "s" : "", before, hm.howlFloorDb, rv::metrics::kHowlFloorMinDb, hm.howlMovePct,
                      hm.howlMoveDb, rv::metrics::kHowlMovePct, rv::metrics::kHowlMoveDb, before - after3);
        check(before > -40.0 && hm.howlOk && before - after3 >= 30.0, msg);
    }

    // Exit by flipping ATTITUDE (KICKED -> DRIVEN / CLEAN) at DECAY max: an
    // open owner decision (ADR 0018 says ~1-2 s; DECAY 1's T60 is ~9 s).
    // Reported, not checked, not changed.
    for (int to : {1, 0}) {
        const size_t n = 20 * sec, flipAt = 10 * sec;
        const Buf in = click(20.0);
        rv::Tank t;
        t.prepare(kFs, 48);
        Settings s;
        s.att = 2;
        apply(t, s);
        Stereo o{Buf(n), Buf(n)};
        for (size_t pos = 0; pos < n; pos += 48) {
            if (pos == flipAt) t.setParam(rv::ParamId::Attitude, rv::switchToNormalised(to));
            t.process(in.data() + pos, in.data() + pos, o.l.data() + pos, o.r.data() + pos, 48);
        }
        const double before = stereoPowerDb(o, flipAt - sec / 2, flipAt);
        double d3 = before - stereoPowerDb(o, flipAt + 3 * sec - sec / 4, flipAt + 3 * sec + sec / 4);
        double d6 = before - stereoPowerDb(o, flipAt + 6 * sec - sec / 4, flipAt + 6 * sec + sec / 4);
        double d9 = before - stereoPowerDb(o, flipAt + 9 * sec - sec / 4, flipAt + 9 * sec + sec / 4);
        std::printf("INFO  Howl exit via ATTITUDE KICKED -> %s at DECAY 1 (open owner decision, unchanged): "
                    "%.1f dB lower after 3 s, %.1f after 6 s, %.1f after 9 s (ADR 0018 test asks >= 30 within ~3 s)\n",
                    kAttName[to], d3, d6, d9);
    }
}

// ---- 6. CPU --------------------------------------------------------------------------------------
void cpu()
{
    // Same method as M1/M4/M5: desktop ns/sample x (15..25 x slower per
    // sample on a Cortex-M7 @ 480 MHz) x 0.48 cycles/ns. The modulation
    // alone, replicated (two one-poles, the magic-circle sine, a counter, one
    // multiply), timed over 3 Springs' worth of calls.
    const size_t n = size_t(8.0f * kFs);
    float t1 = 0, y1 = 0, y2 = 0, sn = 0, cs = 1, acc = 0;
    const float c = 1.0f - std::exp(-1.0f / (rv::antires::kMicroModHoldSeconds * kFs)),
                e = 2.0f * std::sin(float(kPi) * 0.35f / kFs);
    int count = 0;
    rv::dsp::Rng rng;
    const auto a0 = std::chrono::steady_clock::now();
    for (size_t i = 0; i < 3 * n; ++i) {
        if (--count <= 0) { count = int(rv::antires::kMicroModHoldSeconds * kFs); t1 = rng.bipolar(); }
        y1 += c * (t1 - y1);
        y2 += c * (y1 - y2);
        sn += e * cs;
        cs -= e * sn;
        acc += 1.0f + 0.0012f * y2 + 0.0f * sn;
    }
    const auto a1 = std::chrono::steady_clock::now();
    const double ns = std::chrono::duration<double, std::nano>(a1 - a0).count() / double(n); // per output sample, 3 Springs
    std::printf("INFO  Micro-mod floor (3 Springs): %.2f ns/sample desktop -> %.0f-%.0f Daisy cycles/sample "
                "(checksum %.3g). The delay reads were already fractional.\n",
                ns, ns * 15 * 0.48, ns * 25 * 0.48, double(acc));
}

} // namespace

int main(int argc, char** argv)
{
    const std::string only = argc > 1 ? argv[1] : "";
    struct T {
        const char* name;
        void (*fn)();
    };
    const T tests[] = {{"micromod", microMod}, {"pitch", heldTonePitch}, {"evenness", loopEvenness},
                       {"tail", tankTails},     {"howl", howlZone},       {"cpu", cpu}};
    for (const T& t : tests)
        if (only.empty() || std::string(t.name).find(only) != std::string::npos) t.fn();
    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
