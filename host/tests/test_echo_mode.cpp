// SPRINGS 3 echo mode tests (ADR 0041; core/params/EchoVoicing.h,
// core/dsp/Echo.h, core/dsp/EchoClock.h). Dependency-free: prints
// PASS/FAIL/INFO lines, returns nonzero on any failure. Optional argv[1]
// filter by section name.
//
//   identity  SPRINGS 1 and 2 bit for bit with echo mode off (the coupled
//             reference, where Spring C still runs): echo mode changes
//             nothing there (hits + stabs, every ATTITUDE, WOBBLE 0 / 1).
//   free      Unclocked, TENSION 0 / noon / 1 = 2 s / 0.4 s / 80 ms, log in
//             between; the tape's delay reaches it; the first repeat lands
//             there on the output.
//   clock     One gate pulse = one beat: TENSION's seven zones = 1/2, dotted
//             1/4, 1/4, dotted 1/8, 1/8, dotted 1/16, 1/16 at 120 bpm;
//             longer than 2 s plays at half (30 bpm); a knob on a border
//             doesn't flicker (hysteresis); a jittery clock (+-1 %) doesn't
//             move the time; a bounce is ignored; lost after 2.25 beats
//             (back to free time); edges anywhere in a block, any block size.
//   host      The Plugin's tempo (setHostTempo) sets the beat, overrides the gate.
//   swoop     A time change glides (~0.3 s), never faster than 0.5 samples
//             per sample, and doesn't click (TENSION jumps, the clock arriving
//             and being lost).
//   feedback  DECAY 0 = one repeat; noon 0.5; the top 0.95 (CLEAN, DRIVEN):
//             repeats fade at DECAY 1 (no runaway); KICKED's top runs away,
//             held under 1 by the tape and the limiter, finite, and dies
//             away (>= 30 dB within 4 s) when DECAY comes back to noon.
//   steps     The level fix: every repeat a step down from the hit (the
//             first included), geometric; DECAY 0 one repeat at ~-10 dB.
//   springs   The springs behind it are fixed: DECAY and TENSION don't touch
//             them (bit for bit up to the first / second repeat); their tail
//             rings ~1.5-2 s (owner: "one classic medium tail").
//   tape      A fresh tape on the way in (nothing from before plays); a
//             short tape caps the time; no tape = silent echo, no crash.
//   level     SPRINGS 3 (DECAY, TENSION noon) within +-2 dB of SPRINGS 2 on
//             hits and stabs, K-weighted loudness, stereo and mono.
//   switching SPRINGS 2 <-> 3 mid-tail (held + decaying): no clicks; rapid
//             flipping: no clicks.
//   stability Extremes (DECAY 1, every ATTITUDE, DRIVE 1, TENSION 0 / 1,
//             WOBBLE 0 / 1, a clock): finite, peaks under 1.
//             And the grid as test_tank / test_drive: finite, peak < 1, the
//             repeats fade outside KICKED's runaway.
//   hothighs  The tape's saturation on a hot 15 kHz tone: its folds under -90 dBFS.
//   blocks    Block size 16 / 48 / 333 / 512 render the same (with clock edges).
//   diffuse   PROTOTYPE diffuse repeats (echo_diffuse_voicing 1-3): the first
//             repeat bit for bit as voicing 0, each later one more smeared
//             (the tape alone: spread per repeat), level per repeat as voicing
//             0 (+-1.5 dB, allpasses add no energy); M6 Ringing on position 3
//             at DECAY 0.85 / 1, every ATTITUDE, no worse than voicing 0;
//             KICKED's runaway bounded and dying away; extremes finite.
//   wear      PROTOTYPE break-up (echo_wear_voicing 1-4: worn tape, radio
//             band, BBD grit, crushed), inside the feedback: repeat 1
//             untouched, no repeat louder than none's, the echo time kept;
//             deterministic, block-size independent, no clicks; KICKED's
//             runaway bounded and >= 30 dB down ~3 s after backing off;
//             extremes finite; M6 no worse than none.
//   bbd       The BBD's strength (bbd_voicing A-D; BBD grit is the default
//             wear since 4 Oct): aliasing measured on a tone (inharmonic energy
//             in 0.3-5 kHz), A < B < C and B-D clearly above crushed; level per
//             repeat steady; D's clock follows the echo time and swoops;
//             deterministic, no clicks, runaway bounded and dying, CLEAN DECAY 1
//             fades, M6 (Ringing and steady tone) clean.
//   cost      Desktop ns/sample, SPRINGS 3 echo vs SPRINGS 2 vs the coupled
//             reference. Reported.

#include "dsp/Filters.h"
#include "dsp/Tank.h"
#include "params/EchoVoicing.h"
#include "params/Mappings.h"
#include "params/ParamSpec.h"
#include "Metrics.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <vector>

namespace {

int  failures = 0;
char msg[600];

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++failures;
}
void info(const char* what) { std::printf("INFO  %s\n", what); }

using Buf = std::vector<float>;
constexpr float  kFs = 48000.0f;
constexpr double kPi = 3.14159265358979323846;
size_t sec(double s) { return size_t(s * double(kFs)); }

struct Settings {
    float decay = 0.5f, tension = 0.5f, tone = 0.5f, mix = 1.0f, drive = 0.0f, wobble = 0.5f, splash = 0.5f;
    int   att = 0, springs = 2;
    bool  echo = true;
    float hostBpm = 0.0f;
    int   wear = -1; // echo_wear_voicing; -1 = the default (BBD grit since 4 Oct)
};

void apply(rv::Tank& t, const Settings& s)
{
    using rv::ParamId;
    t.setEchoMode(s.echo);
    t.setHostTempo(s.hostBpm);
    if (s.wear >= 0) t.setEchoWearVoicing(s.wear);
    t.setParam(ParamId::Decay, s.decay);
    t.setParam(ParamId::Tension, s.tension);
    t.setParam(ParamId::Tone, s.tone);
    t.setParam(ParamId::Mix, s.mix);
    t.setParam(ParamId::Drive, s.drive);
    t.setParam(ParamId::Wobble, s.wobble);
    t.setParam(ParamId::Splash, s.splash);
    t.setParam(ParamId::Attitude, rv::switchToNormalised(s.att));
    t.setParam(ParamId::Springs, rv::switchToNormalised(s.springs));
}

struct Stereo {
    Buf l, r;
};

// Renders `in` (mono into both inputs); clock edges at absolute samples.
Stereo render(const Settings& s, const Buf& in, int block = 48, const std::vector<size_t>& clocks = {})
{
    rv::Tank t;
    t.prepare(kFs, block);
    apply(t, s);
    const size_t n = in.size();
    Stereo o{Buf(n), Buf(n)};
    size_t c = 0;
    for (size_t pos = 0; pos < n; pos += size_t(block)) {
        const int m = int(std::min<size_t>(size_t(block), n - pos));
        while (c < clocks.size() && clocks[c] < pos + size_t(m)) {
            if (clocks[c] >= pos) t.clock(int(clocks[c] - pos));
            ++c;
        }
        t.process(in.data() + pos, in.data() + pos, o.l.data() + pos, o.r.data() + pos, m);
    }
    return o;
}

std::vector<size_t> steadyClock(double bpm, double from, double to)
{
    std::vector<size_t> c;
    for (double t = from; t < to; t += 60.0 / bpm) c.push_back(size_t(std::lround(t * kFs)));
    return c;
}

double db(double p) { return 10.0 * std::log10(p + 1e-30); }
double power(const Buf& x, size_t from, size_t to)
{
    double s = 0;
    to = std::min(to, x.size());
    for (size_t i = from; i < to; ++i) s += double(x[i]) * x[i];
    return to > from ? s / double(to - from) : 0.0;
}
double stereoDb(const Stereo& o, size_t from, size_t to) { return db(0.5 * (power(o.l, from, to) + power(o.r, from, to))); }
float peakOf(const Stereo& o, size_t from = 0)
{
    float p = 0.0f;
    for (size_t i = from; i < o.l.size(); ++i) p = std::max({p, std::fabs(o.l[i]), std::fabs(o.r[i])});
    return p;
}
bool finite(const Stereo& o)
{
    for (size_t i = 0; i < o.l.size(); ++i)
        if (!std::isfinite(o.l[i]) || !std::isfinite(o.r[i])) return false;
    return true;
}
bool same(const Stereo& a, const Stereo& b, size_t to = SIZE_MAX)
{
    to = std::min({to, a.l.size(), b.l.size()});
    return std::memcmp(a.l.data(), b.l.data(), to * sizeof(float)) == 0
        && std::memcmp(a.r.data(), b.r.data(), to * sizeof(float)) == 0;
}

// BS.1770 K-weighting at 48 kHz (as test_springs3).
double kWeightedDb(const Buf& x)
{
    static constexpr double b1[3] = {1.53512485958697, -2.69169618940638, 1.19839281085285};
    static constexpr double a1[3] = {1.0, -1.69065929318241, 0.73248077421585};
    static constexpr double b2[3] = {1.0, -2.0, 1.0};
    static constexpr double a2[3] = {1.0, -1.99004745483398, 0.99007225036621};
    double s1[2] = {0, 0}, s2[2] = {0, 0}, acc = 0;
    for (float v : x) {
        const double y1 = b1[0] * v + s1[0];
        s1[0] = b1[1] * v - a1[1] * y1 + s1[1];
        s1[1] = b1[2] * v - a1[2] * y1;
        const double y2 = b2[0] * y1 + s2[0];
        s2[0] = b2[1] * y1 - a2[1] * y2 + s2[1];
        s2[1] = b2[2] * y1 - a2[2] * y2;
        acc += y2 * y2;
    }
    return db(acc / double(std::max<size_t>(1, x.size())));
}
double loudnessDb(const Stereo& o) { return db(0.5 * (std::pow(10.0, kWeightedDb(o.l) / 10) + std::pow(10.0, kWeightedDb(o.r) / 10))); }
double monoLoudnessDb(const Stereo& o)
{
    Buf m(o.l.size());
    for (size_t i = 0; i < m.size(); ++i) m[i] = 0.5f * (o.l[i] + o.r[i]);
    return kWeightedDb(m);
}

void normalise(Buf& x, float peakDb)
{
    float p = 0.0f;
    for (float v : x) p = std::max(p, std::fabs(v));
    const float g = std::pow(10.0f, peakDb / 20.0f) / std::max(p, 1.0e-9f);
    for (float& v : x) v *= g;
}
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

// Snare-like hits (noise + 185 Hz body), 3 s apart, -6 dBFS peak.
Buf hits(double seconds)
{
    Buf x(sec(seconds), 0.0f);
    std::mt19937 rng(1);
    std::uniform_real_distribution<float> u(-1.0f, 1.0f);
    for (int h = 0; h < 8; ++h) {
        const size_t at = sec(1.0 + 3.0 * h);
        for (size_t i = 0; i < sec(0.25) && at + i < x.size(); ++i) {
            const double t = double(i) / kFs;
            x[at + i] = float(0.6 * std::sin(2 * kPi * 185 * t) * std::exp(-t / 0.03) + 1.2 * u(rng) * std::exp(-t / 0.06));
        }
    }
    normalise(x, -6.0f);
    return x;
}
// Skank stabs (offbeats at 75 bpm, A minor / D major), -6 dBFS peak.
Buf stabs(double seconds)
{
    Buf x(sec(seconds), 0.0f);
    const double beat = 60.0 / 75.0;
    for (int k = 0; 1.0 + k * beat + beat / 2 + 0.12 < seconds; ++k) {
        const size_t at = sec(1.0 + k * beat + beat / 2);
        static constexpr double kAm[3] = {220.0, 261.63, 329.63}, kD[3] = {293.66, 369.99, 440.0};
        const double* ch = (k / 4) % 2 ? kD : kAm;
        for (size_t i = 0; i < sec(0.12) && at + i < x.size(); ++i) {
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
// A short noise burst (8 ms decay, -6 dBFS) at `at` seconds.
Buf burst(double seconds, double at = 0.5)
{
    Buf b(sec(seconds), 0.0f);
    rv::dsp::Rng rng;
    rng.seed(4242u);
    for (size_t i = 0; i < sec(0.03); ++i) b[sec(at) + i] = 0.5f * std::exp(-float(i) / (0.008f * kFs)) * rng.bipolar();
    return b;
}
Buf noise(size_t n, float amp, uint32_t seed)
{
    Buf b(n);
    rv::dsp::Rng rng;
    rng.seed(seed);
    for (auto& x : b) x = amp * rng.bipolar();
    return b;
}

// Click detector as in test_tank / test_springs3: |second difference| > 10x
// its local +-10 ms RMS and above 1e-3.
int countClicks(const Buf& x, size_t from, double* maxRatio)
{
    const size_t n = x.size();
    std::vector<double> d2(n, 0.0), pre(n + 1, 0.0);
    for (size_t i = 2; i < n; ++i) d2[i] = double(x[i]) - 2.0 * x[i - 1] + x[i - 2];
    for (size_t i = 0; i < n; ++i) pre[i + 1] = pre[i] + d2[i] * d2[i];
    const size_t half = sec(0.010);
    int clicks = 0;
    double worst = 0;
    for (size_t i = std::max<size_t>(from, half); i + half < n; ++i) {
        const double rms = std::sqrt((pre[i + half] - pre[i - half]) / double(2 * half));
        if (rms <= 0) continue;
        const double ratio = std::fabs(d2[i]) / rms;
        if (std::fabs(d2[i]) > 1e-3) worst = std::max(worst, ratio);
        if (ratio > 10.0 && std::fabs(d2[i]) > 1e-3) ++clicks;
    }
    if (maxRatio) *maxRatio = worst;
    return clicks;
}
int clicksBoth(const Stereo& o, size_t from, double* worst)
{
    double a = 0, b = 0;
    const int c = countClicks(o.l, from, &a) + countClicks(o.r, from, &b);
    if (worst) *worst = std::max(a, b);
    return c;
}

// Runs a Tank for `seconds` of silence (or `in`) and returns it, for reading
// the echo's state.
void run(rv::Tank& t, double seconds, int block = 48, const std::vector<size_t>& clocks = {}, size_t clockFrom = 0)
{
    const size_t n = sec(seconds);
    Buf z(size_t(block), 0.0f), l(z.size()), r(z.size());
    size_t c = 0;
    while (c < clocks.size() && clocks[c] < clockFrom) ++c;
    for (size_t pos = 0; pos < n; pos += size_t(block)) {
        const int m = int(std::min<size_t>(size_t(block), n - pos));
        const size_t abs = clockFrom + pos;
        while (c < clocks.size() && clocks[c] < abs + size_t(m)) {
            if (clocks[c] >= abs) t.clock(int(clocks[c] - abs));
            ++c;
        }
        t.process(z.data(), z.data(), l.data(), r.data(), m);
    }
}

// ---- identity -------------------------------------------------------------------------------
void identity()
{
    const Buf h = hits(8.0), st = stabs(8.0);
    bool ok = true;
    int  cases = 0;
    for (int sp = 0; sp < 2; ++sp)
        for (int a = 0; a < 3; ++a)
            for (float wob : {0.0f, 1.0f})
                for (const Buf* in : {&h, &st}) {
                    Settings s;
                    s.springs = sp, s.att = a, s.wobble = wob, s.decay = 0.7f, s.tension = 0.3f;
                    s.echo = false;
                    const Stereo ref = render(s, *in);
                    s.echo = true;
                    ok &= same(ref, render(s, *in));
                    ++cases;
                }
    std::snprintf(msg, sizeof msg,
                  "Identity: SPRINGS 1 and 2 bit for bit with echo mode off (Spring C running there), %d cases "
                  "(hits + stabs, every ATTITUDE, WOBBLE 0 / 1)", cases);
    check(ok, msg);

    // While knobs move (DECAY, TENSION, TONE swept; the Springs take new
    // settings on their turns) and the gate clocks: still bit for bit.
    bool moving = true;
    for (int sp = 0; sp < 2; ++sp)
        for (int a = 0; a < 3; ++a) {
            Stereo o[2];
            for (int k = 0; k < 2; ++k) {
                rv::Tank t;
                t.prepare(kFs, 48);
                Settings s;
                s.springs = sp, s.att = a, s.echo = k == 1;
                apply(t, s);
                o[k] = Stereo{Buf(h.size()), Buf(h.size())};
                const auto clk = steadyClock(120.0, 0.3, 8.0);
                size_t c = 0;
                for (size_t pos = 0; pos < h.size(); pos += 48) {
                    const float u = float(pos) / float(h.size());
                    t.setParam(rv::ParamId::Decay, 0.2f + 0.8f * u);
                    t.setParam(rv::ParamId::Tension, 0.9f - 0.8f * u);
                    t.setParam(rv::ParamId::Tone, 0.5f + 0.4f * std::sin(6.0f * u));
                    while (c < clk.size() && clk[c] < pos + 48) {
                        if (k == 1 && clk[c] >= pos) t.clock(int(clk[c] - pos));
                        ++c;
                    }
                    t.process(h.data() + pos, h.data() + pos, o[k].l.data() + pos, o[k].r.data() + pos, 48);
                }
            }
            moving &= same(o[0], o[1]);
        }
    check(moving, "Identity: SPRINGS 1 and 2 bit for bit with echo mode off while DECAY / TENSION / TONE move and the gate clocks "
                  "(every ATTITUDE)");
}

// ---- free time ------------------------------------------------------------------------------
// The first repeat on the output: the same render with a tape and without
// one (the echo plays silence, everything else identical); their difference
// is the repeats through the springs. Its onset (first sample over 1 % of its
// peak) minus the onset of the hit itself through the springs (the no-tape
// render) = the echo time.
size_t onset(const Buf& x, size_t from = 0)
{
    float pk = 0.0f;
    for (size_t i = from; i < x.size(); ++i) pk = std::max(pk, std::fabs(x[i]));
    for (size_t i = from; i < x.size(); ++i)
        if (std::fabs(x[i]) > 0.01f * pk) return i;
    return x.size();
}
double repeatDelaySeconds(const Settings& s, const Buf& in)
{
    std::vector<float> pool(rv::Tank::requiredPoolFloats(kFs)), tapeBuf(rv::Tank::requiredTapeFloats(kFs));
    Stereo o[2];
    for (int k = 0; k < 2; ++k) {
        rv::Tank t;
        std::fill(pool.begin(), pool.end(), 0.0f);
        t.prepare(kFs, 48, pool.data(), pool.size(), k ? tapeBuf.data() : nullptr, k ? tapeBuf.size() : 0);
        apply(t, s);
        o[k] = Stereo{Buf(in.size()), Buf(in.size())};
        for (size_t pos = 0; pos < in.size(); pos += 48)
            t.process(in.data() + pos, in.data() + pos, o[k].l.data() + pos, o[k].r.data() + pos, 48);
    }
    Buf diff(in.size()), dry(in.size());
    for (size_t i = 0; i < in.size(); ++i) {
        diff[i] = 0.5f * ((o[1].l[i] - o[0].l[i]) + (o[1].r[i] - o[0].r[i]));
        dry[i]  = 0.5f * (o[0].l[i] + o[0].r[i]);
    }
    return (double(onset(diff)) - double(onset(dry))) / kFs;
}

void freeTime()
{
    const float tensions[5] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
    bool ok = true;
    char line[400] = "";
    for (float tn : tensions) {
        const double want = rv::echo::kFreeLongSeconds
                          * std::pow(double(rv::echo::kFreeShortSeconds / rv::echo::kFreeLongSeconds), double(tn));
        rv::Tank t;
        t.prepare(kFs, 48);
        Settings s;
        s.tension = tn;
        apply(t, s);
        run(t, 1.5);
        const double aimed = t.echoSeconds(), got = t.tapeEcho().delaySamples() / kFs;
        ok &= std::fabs(aimed - want) < 1e-3 * want && std::fabs(got - want) < 2e-3 * want + 1.0 / kFs && t.echoDivision() < 0;
        char one[80];
        std::snprintf(one, sizeof one, "%s%.2f -> %.3f s (%.3f)", line[0] ? ", " : "", tn, got, want);
        std::strncat(line, one, sizeof line - std::strlen(line) - 1);
    }
    std::snprintf(msg, sizeof msg, "Free time, TENSION -> the tape's delay (aimed): %s; 2 s / 0.4 s / 80 ms, log", line);
    check(ok, msg);

    // On the output: TENSION noon (0.4 s) and 1 (80 ms), DECAY 0.
    for (float tn : {0.5f, 1.0f}) {
        Settings s;
        s.tension = tn, s.decay = 0.0f, s.splash = 0.0f, s.wobble = 0.5f;
        const double d = tn == 0.5f ? 0.4 : 0.08;
        const double at = repeatDelaySeconds(s, burst(2.0));
        std::snprintf(msg, sizeof msg, "Free time on the output, TENSION %.1f: first repeat %.4f s after the hit (want %.3f, +-2 ms)",
                      double(tn), at, d);
        check(std::fabs(at - d) < 0.002, msg);
    }
}

// ---- clock ----------------------------------------------------------------------------------
void clockDivisions()
{
    // 120 bpm: beat 0.5 s. TENSION at each zone's centre.
    const double beat = 0.5;
    bool ok = true;
    char line[500] = "";
    for (int z = 0; z < rv::echo::kNumDivisions; ++z) {
        rv::Tank t;
        t.prepare(kFs, 48);
        Settings s;
        s.tension = (float(z) + 0.5f) / float(rv::echo::kNumDivisions);
        apply(t, s);
        const auto clk = steadyClock(120.0, 0.1, 4.0);
        run(t, 3.0, 48, clk);
        const double want = rv::echo::kDivisionBeats[z] * beat;
        const double got  = t.tapeEcho().delaySamples() / kFs;
        ok &= t.clockLocked() && t.echoDivision() == z && std::fabs(t.echoSeconds() - want) < 1e-4
           && std::fabs(got - want) < 1e-3;
        char one[60];
        std::snprintf(one, sizeof one, "%s%d: %.4f s", line[0] ? ", " : "", z, got);
        std::strncat(line, one, sizeof line - std::strlen(line) - 1);
    }
    std::snprintf(msg, sizeof msg,
                  "Clock 120 bpm, TENSION's zones long -> short (1/2, 1/4., 1/4, 1/8., 1/8, 1/16., 1/16 = 1, .75, .5, "
                  ".375, .25, .1875, .125 s): %s", line);
    check(ok, msg);

    // 30 bpm: beat 2 s; 1/2 = 4 s -> 2 s (half), dotted 1/4 = 3 s -> 1.5 s, 1/4 = 2 s stays.
    {
        bool ok2 = true;
        double got[3];
        for (int z = 0; z < 3; ++z) {
            rv::Tank t;
            t.prepare(kFs, 48);
            Settings s;
            s.tension = (float(z) + 0.5f) / float(rv::echo::kNumDivisions);
            apply(t, s);
            run(t, 9.0, 48, steadyClock(30.0, 0.1, 10.0));
            got[z] = t.echoSeconds();
        }
        ok2 = std::fabs(got[0] - 2.0) < 1e-3 && std::fabs(got[1] - 1.5) < 1e-3 && std::fabs(got[2] - 2.0) < 1e-3;
        std::snprintf(msg, sizeof msg, "Clock 30 bpm: 1/2 %.3f s (4 s plays at half: 2), 1/4. %.3f (3 -> 1.5), 1/4 %.3f (2)",
                      got[0], got[1], got[2]);
        check(ok2, msg);
    }

    // Hysteresis: TENSION resting on the 2|3 border with a little wobble.
    {
        rv::Tank t;
        t.prepare(kFs, 48);
        Settings s;
        const float border = 3.0f / float(rv::echo::kNumDivisions);
        s.tension = border - 0.01f;
        apply(t, s);
        const auto clk = steadyClock(120.0, 0.1, 8.0);
        run(t, 2.0, 48, clk);
        const int first = t.echoDivision();
        int changes = 0, last = first;
        for (int k = 0; k < 40; ++k) {
            t.setParam(rv::ParamId::Tension, border + (k % 2 ? 0.012f : -0.012f));
            run(t, 0.1, 48, clk, sec(2.0 + 0.1 * k));
            if (t.echoDivision() != last) ++changes, last = t.echoDivision();
        }
        t.setParam(rv::ParamId::Tension, border + 0.06f); // well past: it changes
        run(t, 0.3, 48, clk, sec(6.0));
        std::snprintf(msg, sizeof msg,
                      "Clock: TENSION wobbling +-0.012 on a zone border: %d zone changes (limit 0); well past it: zone %d -> %d",
                      changes, first, t.echoDivision());
        check(changes == 0 && t.echoDivision() == first + 1, msg);
    }

    // Jitter: +-1 % per pulse at 100 bpm, the time doesn't move; a bounce
    // (a second edge 2 ms after one) is ignored.
    {
        rv::Tank t;
        t.prepare(kFs, 48);
        Settings s;
        s.tension = 2.5f / 7.0f; // 1/4
        apply(t, s);
        std::vector<size_t> clk;
        std::mt19937 rng(7);
        std::uniform_real_distribution<double> u(-0.01, 0.01);
        for (int k = 0; k < 30; ++k) {
            const double tt = 0.1 + 0.6 * k + 0.6 * u(rng);
            clk.push_back(sec(tt));
            if (k == 12) clk.push_back(sec(tt + 0.002)); // bounce
        }
        std::sort(clk.begin(), clk.end());
        run(t, 3.0, 48, clk);
        const double ref = t.echoSeconds();
        double lo = ref, hi = ref;
        for (int k = 0; k < 30; ++k) {
            run(t, 0.5, 48, clk, sec(3.0 + 0.5 * k));
            lo = std::min(lo, double(t.echoSeconds())), hi = std::max(hi, double(t.echoSeconds()));
        }
        std::snprintf(msg, sizeof msg,
                      "Clock 100 bpm +-1 %% jitter and one bounce: 1/4 between %.4f and %.4f s (0.6 s; moves <= 2 %%, locked %d)",
                      lo, hi, int(t.clockLocked()));
        check(t.clockLocked() && hi - lo <= 0.021 * 0.6 && std::fabs(ref - 0.6) < 0.02 * 0.6, msg);
    }

    // Lost: pulses stop; after 2.25 beats the echo glides back to free time.
    {
        rv::Tank t;
        t.prepare(kFs, 48);
        Settings s;
        s.tension = 0.5f; // zone 3 clocked (dotted 1/8 = 0.375 s at 60 bpm... 1.0 s beat), 0.4 s free
        apply(t, s);
        run(t, 4.0, 48, steadyClock(60.0, 0.1, 3.2)); // last pulse at 3.1 s
        const bool lockedBefore = t.clockLocked();
        run(t, 1.0, 48, {}, sec(4.0)); // 4..5 s: 1.9 beats since the last: still locked
        const bool stillLocked = t.clockLocked();
        run(t, 1.0, 48, {}, sec(5.0)); // 5..6 s: 2.9 beats: lost
        const bool lost = !t.clockLocked() && t.echoDivision() < 0 && std::fabs(t.echoSeconds() - 0.4f) < 1e-3;
        std::snprintf(msg, sizeof msg, "Clock lost after 2.25 beats at 60 bpm: locked %d, at 1.9 beats %d, at 2.9 beats lost %d (free 0.4 s)",
                      int(lockedBefore), int(stillLocked), int(lost));
        check(lockedBefore && stillLocked && lost, msg);
    }

    // Edges anywhere in a block: the beat is the same at block 16, 48, 512
    // (pulses on odd samples).
    {
        double b[3];
        const int blocks[3] = {16, 48, 512};
        std::vector<size_t> clk;
        for (int k = 0; k < 10; ++k) clk.push_back(size_t(4801 + 23999 * k)); // 0.49998 s: 120.005 bpm
        for (int i = 0; i < 3; ++i) {
            rv::Tank t;
            t.prepare(kFs, blocks[i]);
            Settings s;
            apply(t, s);
            run(t, 4.0, blocks[i], clk);
            b[i] = t.clockBeatSeconds();
        }
        std::snprintf(msg, sizeof msg, "Clock edges on their exact samples: beat %.6f / %.6f / %.6f s at block 16 / 48 / 512 (want %.6f)",
                      b[0], b[1], b[2], 23999.0 / kFs);
        check(std::fabs(b[0] - 23999.0 / kFs) < 1e-6 && b[0] == b[1] && b[1] == b[2], msg);
    }
}

// ---- host tempo -----------------------------------------------------------------------------
void hostTempo()
{
    rv::Tank t;
    t.prepare(kFs, 48);
    Settings s;
    s.tension = 2.5f / 7.0f; // 1/4
    s.hostBpm = 90.0f;
    apply(t, s);
    run(t, 1.0, 48, steadyClock(120.0, 0.1, 2.0)); // a gate clock too: the host wins
    const double a = t.echoSeconds();
    t.setHostTempo(0.0f);
    run(t, 0.2, 48, steadyClock(120.0, 0.1, 2.0), sec(1.0));
    const double b = t.echoSeconds();
    std::snprintf(msg, sizeof msg, "Host tempo 90 bpm: 1/4 = %.4f s (want 0.6667, over a 120 bpm gate); host off: the gate's %.4f s (0.5)",
                  a, b);
    check(std::fabs(a - 60.0 / 90.0) < 1e-4 && std::fabs(b - 0.5) < 1e-3, msg);
}

// ---- swoop ----------------------------------------------------------------------------------
void swoop()
{
    // TENSION 0.5 -> 1 (0.4 s -> 80 ms) and back -> 0 (2 s): the delay's
    // per-sample step never exceeds kMaxSlew; the move takes ~0.3 s to get
    // most of the way (63 % within 0.2-0.45 s for a small move).
    {
        rv::Tank t;
        t.prepare(kFs, 32);
        Settings s;
        apply(t, s);
        run(t, 1.0, 32);
        double maxStep = 0, prev = t.tapeEcho().delaySamples();
        const double from = prev, to = 0.08 * kFs, small = 0.32 * kFs;
        t.setParam(rv::ParamId::Tension, 0.6f); // 0.4 -> 0.32 s: a small move, inside the slew limit
        double t63 = -1;
        for (int k = 0; k < 1500; ++k) { // 1 s of 32-sample ticks
            run(t, 32.0 / kFs, 32);
            const double d = t.tapeEcho().delaySamples();
            maxStep = std::max(maxStep, std::fabs(d - prev) / 32.0);
            prev = d;
            if (t63 < 0 && std::fabs(d - from) >= 0.632 * std::fabs(small - from)) t63 = (k + 1) * 32.0 / kFs;
        }
        // Now the big move (slew-limited).
        t.setParam(rv::ParamId::Tension, 1.0f);
        for (int k = 0; k < 6000; ++k) { // 4 s
            run(t, 32.0 / kFs, 32);
            const double d = t.tapeEcho().delaySamples();
            maxStep = std::max(maxStep, std::fabs(d - prev) / 32.0);
            prev = d;
        }
        const bool reached = std::fabs(prev - to) < 1.0;
        std::snprintf(msg, sizeof msg,
                      "Swoop: a TENSION move glides (63 %% of a small move in %.2f s, want 0.2-0.45), steepest %.3f samples of "
                      "delay per sample (limit %.2f), reaches 80 ms %d",
                      t63, maxStep, double(rv::echo::kMaxSlew), int(reached));
        check(t63 >= 0.2 && t63 <= 0.45 && maxStep <= rv::echo::kMaxSlew + 1e-4 && reached, msg);
    }
    // No clicks: repeats ringing (DECAY 0.8) on a held tone and on hits,
    // TENSION jumps 0.5 -> 0 -> 1, a clock arrives and is lost.
    {
        const size_t n = sec(12.0);
        Buf tone(n);
        for (size_t i = 0; i < n; ++i) tone[i] = i < sec(1.0) ? float(0.25 * std::sin(2 * kPi * 330 * double(i) / kFs)) : 0.0f;
        const Buf h = hits(12.0);
        int clicks = 0;
        double worst = 0;
        for (const Buf* in : {static_cast<const Buf*>(&tone), &h}) {
            rv::Tank t;
            t.prepare(kFs, 48);
            Settings s;
            s.decay = 0.8f;
            apply(t, s);
            Stereo o{Buf(n), Buf(n)};
            const auto clk = steadyClock(110.0, 7.0, 9.0);
            size_t c = 0;
            for (size_t pos = 0; pos < n; pos += 48) {
                if (pos == sec(2.0) / 48 * 48) t.setParam(rv::ParamId::Tension, 0.0f);
                if (pos == sec(4.0) / 48 * 48) t.setParam(rv::ParamId::Tension, 1.0f);
                if (pos == sec(5.5) / 48 * 48) t.setParam(rv::ParamId::Tension, 0.3f);
                while (c < clk.size() && clk[c] < pos + 48) {
                    if (clk[c] >= pos) t.clock(int(clk[c] - pos));
                    ++c;
                }
                t.process(in->data() + pos, in->data() + pos, o.l.data() + pos, o.r.data() + pos, 48);
            }
            double w = 0;
            clicks += clicksBoth(o, sec(1.5), &w);
            worst = std::max(worst, w);
        }
        std::snprintf(msg, sizeof msg,
                      "Swoop: TENSION jumps, a clock arriving and lost, repeats ringing (tone, hits): %d clicks (worst ratio %.1f)",
                      clicks, worst);
        check(clicks == 0, msg);
    }
}

// ---- feedback -------------------------------------------------------------------------------
void feedback()
{
    // The curve.
    {
        const float f0 = rv::echo::feedbackClean(0.0f), fn = rv::echo::feedbackClean(0.5f), f1 = rv::echo::feedbackClean(1.0f);
        const float k1 = rv::echo::feedbackKicked(1.0f), k7 = rv::echo::feedbackKicked(0.7f);
        std::snprintf(msg, sizeof msg,
                      "Feedback: DECAY 0 / noon / 1 = %.3f / %.3f / %.3f (0, 0.5, 0.95); KICKED 0.7 %.3f (as CLEAN %.3f), 1 %.3f (1.25)",
                      double(f0), double(fn), double(f1), double(k7), double(rv::echo::feedbackClean(0.7f)), double(k1));
        check(f0 == 0.0f && std::fabs(fn - 0.5f) < 1e-4f && std::fabs(f1 - 0.95f) < 1e-4f && k7 == rv::echo::feedbackClean(0.7f)
                  && std::fabs(k1 - 1.25f) < 1e-4f,
              msg);
        // Each pass's gain at the heads' peak (~0.965): where KICKED runs away.
        float runaway = -1.0f;
        for (int k = 0; k <= 1000 && runaway < 0; ++k)
            if (rv::echo::feedbackKicked(k / 1000.0f) * 0.965f > 1.0f) runaway = k / 1000.0f;
        std::snprintf(msg, sizeof msg, "Feedback: KICKED's repeats grow from DECAY %.3f (x 0.965, the heads' peak); CLEAN never",
                      double(runaway));
        info(msg);
    }
    // DECAY 0 = one repeat: a burst, TENSION 1 (80 ms); the echo's share
    // after the first repeat (pos 3 minus the same with the echo's
    // feedback...) is measured on the tape: the 2nd repeat's window holds
    // nothing more than the springs' own tail.
    {
        const Buf in = burst(1.5, 0.2);
        Settings s;
        s.tension = 0.0f, s.decay = 0.0f, s.splash = 0.0f; // 2 s: the repeat far from the hit
        // Compare DECAY 0 and DECAY 0.5 up to the first repeat (2.2 s): identical (DECAY
        // sets the repeats' level only), and after the 2nd's time DECAY 0 has nothing new.
        Buf in6(sec(6.5), 0.0f);
        std::copy(in.begin(), in.end(), in6.begin());
        const Stereo a = render(s, in6);
        s.decay = 0.5f;
        const Stereo b = render(s, in6);
        const bool sameTo2nd = same(a, b, sec(2.15));
        const double aRise = stereoDb(a, sec(4.2), sec(4.4)) - stereoDb(a, sec(3.9), sec(4.1));
        const double bRise = stereoDb(b, sec(4.2), sec(4.4)) - stereoDb(b, sec(3.9), sec(4.1));
        std::snprintf(msg, sizeof msg,
                      "Feedback: DECAY 0 = one repeat (2 s echo: DECAY 0 and noon bit for bit until the 1st repeat %d; at 4.2 s "
                      "DECAY 0 %+.1f dB (no repeat: falling), noon %+.1f dB (a repeat))",
                      int(sameTo2nd), aRise, bRise);
        check(sameTo2nd && aRise < 0.0 && bRise > 6.0, msg);
    }
    // CLEAN / DRIVEN, DECAY 1: the repeats fade (a hit, then 30 s).
    for (int att = 0; att < 2; ++att) {
        Settings s;
        s.att = att, s.decay = 1.0f, s.tension = 0.75f; // ~0.18 s: many passes
        const Buf in = burst(30.0, 0.5);
        const Stereo o = render(s, in);
        const double early = stereoDb(o, sec(2.0), sec(4.0)), late = stereoDb(o, sec(26.0), sec(28.0));
        std::snprintf(msg, sizeof msg, "Feedback %s DECAY 1: repeats fade, 2-4 s %.1f dB, 26-28 s %.1f dB (want >= 20 dB lower)",
                      att ? "DRIVEN" : "CLEAN", early, late);
        check(late < early - 20.0 && finite(o), msg);
    }
    // KICKED DECAY 1: a runaway, held under 1, and it dies when DECAY comes down.
    {
        Settings s;
        s.att = 2, s.decay = 1.0f, s.tension = 0.75f, s.drive = 1.0f;
        const size_t n = sec(20.0);
        const Buf in = burst(20.0, 0.5);
        rv::Tank t;
        t.prepare(kFs, 48);
        apply(t, s);
        Stereo o{Buf(n), Buf(n)};
        for (size_t pos = 0; pos < n; pos += 48) {
            if (pos == sec(10.0)) t.setParam(rv::ParamId::Decay, 0.5f);
            t.process(in.data() + pos, in.data() + pos, o.l.data() + pos, o.r.data() + pos, 48);
        }
        const double early = stereoDb(o, sec(1.0), sec(2.0)), held = stereoDb(o, sec(8.0), sec(10.0));
        const double after = stereoDb(o, sec(13.5), sec(14.0));
        const float  pk    = peakOf(o);
        std::snprintf(msg, sizeof msg,
                      "Feedback KICKED DECAY 1 (DRIVE 1): runaway (8-10 s %.1f dB vs 1-2 s %.1f), peak %.3f (< 1), finite %d; "
                      "DECAY back to noon at 10 s: 13.5-14 s %.1f dB (want >= 30 dB under)",
                      held, early, double(pk), int(finite(o)), after);
        check(held > early && pk < 1.0f && finite(o) && after < held - 30.0, msg);
    }
}

// ---- steps (the level fix) ------------------------------------------------------------------
// Every repeat a step down from the hit, the first included: a band-limited
// burst (200 Hz-2 kHz, where the heads barely touch it), 2 s echo (each
// repeat's window holds it and its springs; the one before has rung ~28 dB
// down), CLEAN, SPLASH 0. Window k = repeat k (0 = the hit). Steps
// W1 - W0, W2 - W1, W3 - W2 within 1.5 dB of each other (geometric), all
// below 0; DECAY 0: one repeat at about -10 dB, then nothing.
void steps()
{
    Buf in(sec(8.4), 0.0f);
    {
        const Buf nz = noise(sec(0.06), 0.5f, 31u);
        Buf b(nz);
        for (size_t i = 0; i < b.size(); ++i) b[i] *= std::exp(-float(i) / (0.015f * kFs));
        lowpass(b, 2000.0);
        // high-pass 200 Hz: subtract a 200 Hz low-pass
        Buf lp(b);
        lowpass(lp, 200.0);
        for (size_t i = 0; i < b.size(); ++i) b[i] -= lp[i];
        std::copy(b.begin(), b.end(), in.begin() + long(sec(0.2)));
    }
    auto win = [&](const Stereo& o, int k) { return stereoDb(o, sec(0.15 + 2.0 * k), sec(0.15 + 2.0 * (k + 1))); };
    bool ok = true;
    char line[500] = "";
    for (float dc : {0.0f, 0.3f, 0.5f, 0.7f, 0.9f}) {
        Settings s;
        s.tension = 0.0f, s.decay = dc, s.splash = 0.0f, s.att = 0;
        const Stereo o = render(s, in);
        const double s1 = win(o, 1) - win(o, 0), s2 = win(o, 2) - win(o, 1), s3 = win(o, 3) - win(o, 2);
        const double g = 20.0 * std::log10(std::max(double(rv::echo::feedbackClean(dc)), 1e-9));
        if (dc == 0.0f) ok &= std::fabs(s1 + 10.0) < 2.0 && s2 < -15.0;
        else ok &= s1 < -0.5 && std::fabs(s1 - s2) < 1.5 && std::fabs(s2 - s3) < 1.5;
        char one[90];
        std::snprintf(one, sizeof one, "%sDECAY %.1f (g %.1f dB): %+.1f %+.1f %+.1f", line[0] ? "; " : "", double(dc), g, s1, s2, s3);
        std::strncat(line, one, sizeof line - std::strlen(line) - 1);
    }
    std::snprintf(msg, sizeof msg,
                  "Steps: every repeat a step down from the hit, the first included (repeat k vs k-1, dB): %s. Want steps "
                  "within 1.5 dB of each other, below 0; DECAY 0 one repeat at ~-10 dB, then nothing", line);
    check(ok, msg);
}

// ---- springs --------------------------------------------------------------------------------
void springs()
{
    const Buf in = burst(4.0, 0.2);
    // DECAY doesn't touch the springs: 2 s echo, DECAY 0 vs 1: the same until the first repeat
    // (whose level is DECAY's since the level fix).
    {
        Settings s;
        s.tension = 0.0f, s.decay = 0.0f;
        const Stereo a = render(s, in);
        s.decay = 1.0f;
        const Stereo b = render(s, in);
        std::snprintf(msg, sizeof msg, "Springs fixed: DECAY 0 vs 1 bit for bit until the first repeat (2 s echo)");
        check(same(a, b, sec(2.15)), msg);
    }
    // TENSION doesn't touch the springs: 0 vs 1, the same until the 80 ms repeat.
    {
        Settings s;
        s.tension = 0.0f;
        const Stereo a = render(s, in);
        s.tension = 1.0f;
        const Stereo b = render(s, in);
        std::snprintf(msg, sizeof msg, "Springs fixed: TENSION 0 vs 1 bit for bit until the first repeat (0.2 s + 80 ms)");
        check(same(a, b, sec(0.2 + 0.078)), msg);
    }
    // Their tail: a click's decay before the 2 s repeat, from the -5 to -25 dB
    // points of the envelope (T20 x 3).
    {
        Settings s;
        s.tension = 0.0f, s.decay = 0.0f, s.splash = 0.0f, s.wobble = 0.5f;
        Buf c(sec(2.1), 0.0f);
        c[sec(0.05)] = c[sec(0.05) + 1] = 0.5f;
        const Stereo o = render(s, c);
        const size_t w = sec(0.02);
        std::vector<double> env;
        for (size_t i = sec(0.05); i + w < sec(2.0); i += w) env.push_back(stereoDb(o, i, i + w));
        // Schroeder-ish: the time from the peak to 20 dB under it, x 3.
        size_t pk = 0;
        for (size_t k = 0; k < env.size(); ++k) if (env[k] > env[pk]) pk = k;
        size_t k5 = pk, k25 = pk;
        while (k5 < env.size() && env[k5] > env[pk] - 5.0) ++k5;
        while (k25 < env.size() && env[k25] > env[pk] - 25.0) ++k25;
        const double t60 = 3.0 * double(k25 - k5) * 0.02;
        std::snprintf(msg, sizeof msg, "Springs: the fixed tank's tail T60 ~ %.2f s on a click (T20 x 3; want 1.3-2.3, ~1.7)", t60);
        check(t60 >= 1.3 && t60 <= 2.3, msg);
    }
}

// ---- tape -----------------------------------------------------------------------------------
void tape()
{
    // A fresh tape: 2 s echo; a burst at 0.2 s in SPRINGS 3; out to 2 at
    // 0.6 s, back to 3 at 1.0 s. The old burst would play at 2.2 s: its
    // window must hold no more than a render where the burst never reached the
    // tape (the same with SPRINGS 2 until 1.0 s).
    {
        const size_t n = sec(3.0);
        const Buf in = burst(3.0, 0.2);
        auto go = [&](int firstPos) {
            rv::Tank t;
            t.prepare(kFs, 48);
            Settings s;
            s.tension = 0.0f, s.decay = 0.0f, s.springs = firstPos;
            apply(t, s);
            Stereo o{Buf(n), Buf(n)};
            for (size_t pos = 0; pos < n; pos += 48) {
                if (pos == sec(0.6) / 48 * 48) t.setParam(rv::ParamId::Springs, rv::switchToNormalised(1));
                if (pos == sec(1.0) / 48 * 48) t.setParam(rv::ParamId::Springs, rv::switchToNormalised(2));
                t.process(in.data() + pos, in.data() + pos, o.l.data() + pos, o.r.data() + pos, 48);
            }
            return o;
        };
        const Stereo a = go(2);
        Settings s;
        s.tension = 0.0f, s.decay = 0.0f;
        const Stereo stay = render(s, in); // stays in 3: the hit repeats at 2.2 s
        const double ea = stereoDb(a, sec(2.15), sec(2.5)), es = stereoDb(stay, sec(2.15), sec(2.5));
        std::snprintf(msg, sizeof msg,
                      "Tape: back in SPRINGS 3 on a fresh tape: at 2.2 s (where the old hit would repeat) %.1f dB; staying "
                      "in 3 (it repeats) %.1f dB (want >= 60 dB under)", ea, es);
        check(ea < es - 60.0, msg);
    }
    // Memory: 2 s + margin; a short tape caps the time; none = no echo.
    {
        const size_t need = rv::Tank::requiredTapeFloats(kFs);
        rv::Tank a;
        std::vector<float> pool(rv::Tank::requiredPoolFloats(kFs)), shortTape(sec(0.5));
        a.prepare(kFs, 48, pool.data(), pool.size(), shortTape.data(), shortTape.size());
        Settings s;
        s.tension = 0.0f;
        apply(a, s);
        run(a, 1.0);
        const double capped = a.tapeEcho().delaySamples() / kFs;
        rv::Tank b;
        std::vector<float> pool2(rv::Tank::requiredPoolFloats(kFs));
        b.prepare(kFs, 48, pool2.data(), pool2.size());
        apply(b, s);
        const Buf in = burst(3.0, 0.1);
        Stereo o{Buf(in.size()), Buf(in.size())};
        for (size_t pos = 0; pos < in.size(); pos += 48)
            b.process(in.data() + pos, in.data() + pos, o.l.data() + pos, o.r.data() + pos, 48);
        std::snprintf(msg, sizeof msg,
                      "Tape: %zu floats at 48 kHz (%.0f KB; 2 s + %d); a 0.5 s tape caps the echo at %.3f s; no tape: plays, "
                      "finite %d, no echo %d",
                      need, double(need) * 4.0 / 1024.0, rv::echo::kTapeMargin, capped, int(finite(o)), int(!b.tapeEcho().ok()));
        check(need == size_t(96000 + rv::echo::kTapeMargin) && capped <= 0.5 && capped > 0.45 && finite(o) && !b.tapeEcho().ok(), msg);
    }
}

// ---- level ----------------------------------------------------------------------------------
void level()
{
    const Buf h = hits(14.0), st = stabs(12.0);
    bool ok = true;
    char line[400] = "";
    for (int a = 0; a < 3; ++a)
        for (const Buf* in : {&h, &st}) {
            Settings s;
            s.att = a, s.springs = 1;
            const Stereo two = render(s, *in);
            s.springs = 2;
            const Stereo three = render(s, *in);
            const double d  = loudnessDb(three) - loudnessDb(two);
            const double dm = monoLoudnessDb(three) - monoLoudnessDb(two);
            ok &= std::fabs(d) <= 2.0 && std::fabs(dm) <= 2.0;
            char one[80];
            std::snprintf(one, sizeof one, "%s%s %s %+.1f/%+.1f", line[0] ? ", " : "", a == 0 ? "CLEAN" : a == 1 ? "DRIVEN" : "KICKED",
                          in == &h ? "hits" : "stabs", d, dm);
            std::strncat(line, one, sizeof line - std::strlen(line) - 1);
        }
    std::snprintf(msg, sizeof msg, "Level: SPRINGS 3 (DECAY, TENSION noon) vs 2, K-weighted stereo/mono: %s dB (limit +-2)", line);
    check(ok, msg);
}

// ---- switching ------------------------------------------------------------------------------
void switching()
{
    const size_t n = sec(5.0);
    const Buf held = noise(n, 0.1f, 3u);
    const Buf hit  = [&] {
        Buf b(n, 0.0f);
        rv::dsp::Rng rng;
        rng.seed(4242u);
        for (int h = 0; h < 4; ++h) {
            const size_t at = sec(0.1) + size_t(h) * sec(1.0);
            for (size_t i = 0; i < sec(0.03); ++i) b[at + i] = 0.5f * std::exp(-float(i) / (0.008f * kFs)) * rng.bipolar();
        }
        return b;
    }();
    const int pairs[4][2] = {{1, 2}, {2, 1}, {0, 2}, {2, 0}};
    bool ok = true;
    char line[400] = "";
    for (const auto& p : pairs) {
        int clicks = 0;
        double worst = 0;
        for (int input = 0; input < 2; ++input) {
            const Buf& in = input == 0 ? held : hit;
            const size_t at = input == 0 ? sec(2.0) : sec(3.55);
            rv::Tank t;
            t.prepare(kFs, 16);
            Settings s;
            s.decay = 0.6f, s.springs = p[0];
            apply(t, s);
            Stereo o{Buf(n), Buf(n)};
            for (size_t pos = 0; pos < n; pos += 16) {
                if (pos == (at / 16) * 16) t.setParam(rv::ParamId::Springs, rv::switchToNormalised(p[1]));
                t.process(in.data() + pos, in.data() + pos, o.l.data() + pos, o.r.data() + pos, 16);
            }
            double w = 0;
            clicks += clicksBoth(o, at - sec(0.1), &w);
            worst = std::max(worst, w);
        }
        ok &= clicks == 0;
        char one[60];
        std::snprintf(one, sizeof one, "%s%d->%d %d (%.1f)", line[0] ? ", " : "", p[0] + 1, p[1] + 1, clicks, worst);
        std::strncat(line, one, sizeof line - std::strlen(line) - 1);
    }
    std::snprintf(msg, sizeof msg, "Switching mid-tail (held + decaying): clicks (worst ratio) %s", line);
    check(ok, msg);

    rv::Tank t;
    t.prepare(kFs, 16);
    Settings s;
    s.decay = 0.6f, s.springs = 1;
    apply(t, s);
    Stereo o{Buf(n), Buf(n)};
    int flip = 0;
    for (size_t pos = 0; pos < n; pos += 16) {
        if (pos >= sec(1.0) && pos < sec(2.0) && pos % 240 == 0)
            t.setParam(rv::ParamId::Springs, rv::switchToNormalised(1 + (++flip % 2)));
        t.process(held.data() + pos, held.data() + pos, o.l.data() + pos, o.r.data() + pos, 16);
    }
    double w = 0;
    const int c = clicksBoth(o, sec(0.9), &w);
    std::snprintf(msg, sizeof msg, "Switching: SPRINGS 2 <-> 3 every 5 ms for 1 s: %d clicks (worst ratio %.1f)", c, w);
    check(c == 0 && finite(o), msg);
}

// ---- stability ------------------------------------------------------------------------------
void stability()
{
    const Buf h = hits(14.0);
    bool ok = true;
    float worst = 0.0f;
    int cases = 0;
    for (int a = 0; a < 3; ++a)
        for (float tn : {0.0f, 1.0f})
            for (float wob : {0.0f, 1.0f})
                for (int clocked = 0; clocked < 2; ++clocked) {
                    Settings s;
                    s.att = a, s.decay = 1.0f, s.drive = 1.0f, s.tension = tn, s.wobble = wob, s.tone = tn;
                    const Stereo o = render(s, h, 48, clocked ? steadyClock(140.0, 0.0, 14.0) : std::vector<size_t>{});
                    const float pk = peakOf(o);
                    worst = std::max(worst, pk);
                    ok &= finite(o) && pk < 1.0f;
                    ++cases;
                }
    std::snprintf(msg, sizeof msg, "Stability: %d extremes (DECAY 1, DRIVE 1, every ATTITUDE, TENSION/TONE 0 1, WOBBLE 0 1, "
                                   "clocked or not): finite, worst peak %.3f (< 1)", cases, double(worst));
    check(ok, msg);

    // The grid (as test_tank / test_drive): impulse + 1 s full-scale noise,
    // ATTITUDE x DRIVE {0, 1} x DECAY {0, .5, .89, 1} x TENSION {0, .5, 1}:
    // finite, peak < 1, and the repeats fade (>= 10 dB from 3-5 s to 33-35 s)
    // everywhere but KICKED's runaway (DECAY >= kKickedFrom + its top).
    {
        Buf in(sec(36.0), 0.0f);
        in[sec(0.1)] = 1.0f;
        const Buf nz = noise(sec(1.0), 1.0f, 99u);
        std::copy(nz.begin(), nz.end(), in.begin() + long(sec(0.5)));
        int cells = 0, bad = 0;
        float pkWorst = 0.0f;
        double fadeWorst = 1e9;
        char badAt[160] = "none";
        for (int a = 0; a < 3; ++a)
            for (float dr : {0.0f, 1.0f})
                for (float dc : {0.0f, 0.5f, 0.89f, 1.0f})
                    for (float tn : {0.0f, 0.5f, 1.0f}) {
                        Settings st;
                        st.att = a, st.drive = dr, st.decay = dc, st.tension = tn;
                        const Stereo o = render(st, in, 48);
                        const float pk = peakOf(o);
                        const bool runaway = a == 2 && dc > 0.87f;
                        const double fall = stereoDb(o, sec(3.0), sec(5.0)) - stereoDb(o, sec(33.0), sec(35.0));
                        const bool good = finite(o) && pk < 1.0f && (runaway || fall >= 10.0);
                        pkWorst = std::max(pkWorst, pk);
                        if (!runaway) fadeWorst = std::min(fadeWorst, fall);
                        if (!good && bad++ == 0)
                            std::snprintf(badAt, sizeof badAt, "%d DRIVE %.0f DECAY %.2f TENSION %.1f: peak %.3f fall %.1f dB", a,
                                          double(dr), double(dc), double(tn), double(pk), fall);
                        ++cells;
                    }
        std::snprintf(msg, sizeof msg,
                      "Stability grid ATTITUDE x DRIVE x DECAY {0,.5,.89,1} x TENSION {0,.5,1} (%d cells, impulse + 1 s full-scale "
                      "noise, 36 s): finite, peak < 1 (worst %.3f), repeats fade >= 10 dB over 30 s outside KICKED's runaway "
                      "(least %.1f dB); %d bad (first: %s)",
                      cells, double(pkWorst), fadeWorst, bad, badAt);
        check(bad == 0, msg);
    }
}

// ---- hothighs -------------------------------------------------------------------------------
// The tape's record head saturates (softClip): a hot high tone's aliases in
// the echo's share (with a tape minus without one), reported.
void hotHighs()
{
    const size_t n = sec(3.0);
    for (float amp : {1.0f, 0.5f}) {
        Buf in(n);
        for (size_t i = 0; i < n; ++i) in[i] = amp * float(std::sin(2 * kPi * 15000.0 * double(i) / kFs));
        Settings s;
        s.att = 2, s.drive = 1.0f, s.decay = 0.8f, s.tension = 1.0f;
        std::vector<float> pool(rv::Tank::requiredPoolFloats(kFs)), tapeBuf(rv::Tank::requiredTapeFloats(kFs));
        Stereo o[2];
        for (int k = 0; k < 2; ++k) {
            rv::Tank t;
            std::fill(pool.begin(), pool.end(), 0.0f);
            t.prepare(kFs, 48, pool.data(), pool.size(), k ? tapeBuf.data() : nullptr, k ? tapeBuf.size() : 0);
            apply(t, s);
            o[k] = Stereo{Buf(n), Buf(n)};
            for (size_t pos = 0; pos < n; pos += 48)
                t.process(in.data() + pos, in.data() + pos, o[k].l.data() + pos, o[k].r.data() + pos, 48);
        }
        auto goertzel = [&](double hz) {
            const double w = 2 * kPi * hz / kFs, c = 2 * std::cos(w);
            double s1 = 0, s2 = 0;
            const size_t from = sec(1.0), to = sec(3.0);
            for (size_t i = from; i < to; ++i) {
                const double x = 0.5 * ((o[1].l[i] - o[0].l[i]) + (o[1].r[i] - o[0].r[i]));
                const double y = x + c * s1 - s2;
                s2 = s1, s1 = y;
            }
            return db(s1 * s1 + s2 * s2 - c * s1 * s2);
        };
        // Goertzel power -> a sine's amplitude in dBFS (N = 2 s).
        const double toDbfs = -db(0.25 * double(sec(2.0)) * double(sec(2.0)));
        const double a3 = goertzel(3000.0) + toDbfs, a9 = goertzel(9000.0) + toDbfs;
        std::snprintf(msg, sizeof msg,
                      "Alias, the echo's share, 15 kHz at %.0f dBFS (KICKED DRIVE 1, DECAY 0.8, 80 ms): 3 kHz %.1f dBFS, 9 kHz %.1f dBFS",
                      20 * std::log10(double(amp)), a3, a9);
        check(a3 < -90.0 && a9 < -90.0, msg); // the record head's low-pass (EchoVoicing.h kRecLpHz): -48 dBFS without it
    }
}

// ---- blocks ---------------------------------------------------------------------------------
void blocks()
{
    const Buf h = hits(8.0);
    const auto clk = steadyClock(97.0, 0.3, 8.0);
    Settings s;
    s.decay = 0.8f, s.tension = 0.6f, s.wobble = 0.2f;
    const Stereo ref = render(s, h, 16, clk);
    bool ok = true;
    for (int b : {48, 333, 512}) ok &= same(ref, render(s, h, b, clk));
    check(ok, "Blocks: 16 / 48 / 333 / 512 bit for bit (clocked, WOBBLE Drift, repeats)");
}

// ---- diffuse (PROTOTYPE) ----------------------------------------------------------------------
// The tape on its own, looped as the Tank loops it (rec = input + fb x play,
// diffused when the voicing has it): a burst, then each repeat's energy and
// its spread (the time between 10 % and 90 % of its energy).
struct RepeatStats {
    double db[8], spreadMs[8];
    Buf    play;
};
RepeatStats tapeRepeats(int voicing, float fb, double secs, int wear = 0, int bbd = 0, const Buf* stim = nullptr)
{
    rv::dsp::TapeEcho e;
    std::vector<float> tapeBuf(rv::Tank::requiredTapeFloats(kFs));
    e.prepare(kFs, 0x1234u, tapeBuf.data(), tapeBuf.size());
    e.setDiffuseVoicing(voicing);
    e.setWearVoicing(wear);
    e.setBbdVoicing(bbd);
    const size_t n = sec(9.0), d = sec(secs);
    const Buf in = stim ? *stim : burst(9.0, 0.1);
    RepeatStats r{};
    r.play.assign(n, 0.0f);
    constexpr int G = rv::dsp::TapeEcho::kGrid;
    float pl[G], rec[G], fbs[G];
    for (size_t pos = 0; pos < n; pos += G) {
        e.tick(float(secs), 0.5f, pos == 0);
        e.play(pl, G);
        for (int i = 0; i < G; ++i) fbs[i] = fb * pl[i];
        e.diffuse(fbs, G);
        float xs[G];
        for (int i = 0; i < G; ++i) xs[i] = in[pos + size_t(i)];
        if (e.wearActive()) {
            e.wear(fbs, G);
            e.delayInput(xs, G);
        }
        for (int i = 0; i < G; ++i) {
            rec[i]            = xs[i] + fbs[i];
            r.play[pos + size_t(i)] = pl[i];
        }
        e.record(rec, G);
    }
    for (int k = 0; k < 8; ++k) {
        const size_t a = sec(0.1) + size_t(k + 1) * d - sec(0.02), b = std::min(n, a + d);
        double tot = 0;
        for (size_t i = a; i < b; ++i) tot += double(r.play[i]) * r.play[i];
        double acc = 0, t10 = -1, t90 = -1;
        for (size_t i = a; i < b; ++i) {
            acc += double(r.play[i]) * r.play[i];
            if (t10 < 0 && acc >= 0.1 * tot) t10 = double(i);
            if (t90 < 0 && acc >= 0.9 * tot) t90 = double(i);
        }
        r.db[k]       = db(tot);
        r.spreadMs[k] = (t90 - t10) * 1000.0 / kFs;
    }
    return r;
}

void diffuse()
{
    // As built (wear none): the diffuse prototype predates BBD grit as the default.
    // On the tape: 0.6 s echo, feedback 0.8.
    RepeatStats st[rv::echo::kNumDiffuseVoicings];
    for (int v = 0; v < rv::echo::kNumDiffuseVoicings; ++v) st[v] = tapeRepeats(v, 0.8f, 0.6);
    for (int v = 1; v < rv::echo::kNumDiffuseVoicings; ++v) {
        bool first = std::memcmp(st[v].play.data(), st[0].play.data(), sec(0.1 + 2 * 0.6 - 0.05) * sizeof(float)) == 0;
        double worstLvl = 0;
        for (int k = 0; k < 6; ++k) worstLvl = std::max(worstLvl, std::fabs(st[v].db[k] - st[0].db[k]));
        std::snprintf(msg, sizeof msg,
                      "Diffuse %d on the tape (0.6 s, feedback 0.8): repeat 1 bit for bit as none %d; spread per repeat 1-6 "
                      "%.1f %.1f %.1f %.1f %.1f %.1f ms (none: %.1f %.1f %.1f %.1f %.1f %.1f); level per repeat vs none, worst %.2f dB "
                      "(limit 1.5)",
                      v, int(first), st[v].spreadMs[0], st[v].spreadMs[1], st[v].spreadMs[2], st[v].spreadMs[3], st[v].spreadMs[4],
                      st[v].spreadMs[5], st[0].spreadMs[0], st[0].spreadMs[1], st[0].spreadMs[2], st[0].spreadMs[3],
                      st[0].spreadMs[4], st[0].spreadMs[5], worstLvl);
        check(first && worstLvl <= 1.5 && st[v].spreadMs[4] > st[v].spreadMs[1] && st[v].spreadMs[4] > st[v - 1].spreadMs[4], msg);
    }

    // In the Tank: voicing 0 set explicitly = the default, bit for bit; the
    // first repeat bit for bit in every voicing (TENSION 0.25: 0.89 s).
    {
        const Buf in = burst(2.4, 0.2);
        Settings s;
            s.wear = 0; // the diffuse round was built without wear
        s.tension = 0.25f, s.decay = 0.85f;
        const Stereo ref = render(s, in);
        bool ok = true;
        for (int v = 0; v < rv::echo::kNumDiffuseVoicings; ++v) {
            rv::Tank t;
            t.prepare(kFs, 48);
            apply(t, s);
            t.setEchoDiffuseVoicing(v);
            Stereo o{Buf(in.size()), Buf(in.size())};
            for (size_t pos = 0; pos < in.size(); pos += 48)
                t.process(in.data() + pos, in.data() + pos, o.l.data() + pos, o.r.data() + pos, 48);
            ok &= same(ref, o, v == 0 ? SIZE_MAX : sec(0.2 + 2 * 0.894 - 0.01));
        }
        check(ok, "Diffuse in the Tank: voicing 0 bit for bit as the default; voicings 1-3 bit for bit until the 2nd repeat");
    }

    // M6 Ringing on position 3 tails: click + noise burst, every ATTITUDE,
    // DECAY 0.85 and 1 (KICKED 1 is the runaway: Howl, not Ringing; KICKED
    // 0.85 instead), TENSION 0 / 0.5. Flags and worst ringing_db per voicing;
    // no voicing worse than none.
    {
        Buf clk(sec(14.0), 0.0f);
        clk[sec(0.5)] = clk[sec(0.5) + 1] = 0.5f;
        const Buf nb = [&] {
            Buf b(sec(14.0), 0.0f);
            const Buf z = noise(sec(0.5), 0.43f, 77u);
            std::copy(z.begin(), z.end(), b.begin() + long(sec(0.5)));
            return b;
        }();
        int    flagged[4] = {0, 0, 0, 0}, cells = 0;
        double worst[4]   = {0, 0, 0, 0};
        for (int v = 0; v < 4; ++v)
            for (int a = 0; a < 3; ++a)
                for (float dc : {0.85f, 1.0f})
                    for (float tn : {0.0f, 0.5f})
                        for (const Buf* in : {static_cast<const Buf*>(&clk), &nb}) {
                            if (a == 2 && dc > 0.9f) continue; // the runaway: checked below
                            Settings s;
            s.wear = 0; // the diffuse round was built without wear
                            s.att = a, s.decay = dc, s.tension = tn;
                            rv::Tank t;
                            t.prepare(kFs, 48);
                            apply(t, s);
                            t.setEchoDiffuseVoicing(v);
                            Stereo o{Buf(in->size()), Buf(in->size())};
                            for (size_t pos = 0; pos < in->size(); pos += 48)
                                t.process(in->data() + pos, in->data() + pos, o.l.data() + pos, o.r.data() + pos, 48);
                            const auto m = rv::metrics::compute({o.l, o.r}, kFs);
                            if (m.ringing || m.steadyTone) ++flagged[v];
                            if (!std::isnan(m.ringingDb)) worst[v] = std::max(worst[v], m.ringingDb);
                            if (v == 0) ++cells;
                        }
        std::snprintf(msg, sizeof msg,
                      "Diffuse, M6 Ringing at SPRINGS 3 (%d cells each: click + burst, ATTITUDE x DECAY .85/1 x TENSION 0/.5): "
                      "flagged / worst ringing_db: none %d / %.1f, light %d / %.1f, medium %d / %.1f, heavy %d / %.1f",
                      cells, flagged[0], worst[0], flagged[1], worst[1], flagged[2], worst[2], flagged[3], worst[3]);
        check(flagged[1] <= flagged[0] && flagged[2] <= flagged[0] && flagged[3] <= flagged[0], msg);
    }

    // KICKED's runaway per voicing: bounded, and dying away when DECAY comes back.
    {
        bool ok = true;
        char line[300] = "";
        const Buf in = burst(20.0, 0.5);
        for (int v = 0; v < 4; ++v) {
            Settings s;
            s.wear = 0; // the diffuse round was built without wear
            s.att = 2, s.decay = 1.0f, s.tension = 0.75f, s.drive = 1.0f;
            rv::Tank t;
            t.prepare(kFs, 48);
            apply(t, s);
            t.setEchoDiffuseVoicing(v);
            Stereo o{Buf(in.size()), Buf(in.size())};
            for (size_t pos = 0; pos < in.size(); pos += 48) {
                if (pos == sec(10.0)) t.setParam(rv::ParamId::Decay, 0.5f);
                t.process(in.data() + pos, in.data() + pos, o.l.data() + pos, o.r.data() + pos, 48);
            }
            const double held = stereoDb(o, sec(8.0), sec(10.0)), after = stereoDb(o, sec(13.5), sec(14.0));
            const float  pk   = peakOf(o);
            ok &= finite(o) && pk < 1.0f && after < held - 30.0;
            char one[80];
            std::snprintf(one, sizeof one, "%s%d: %.1f dB held, peak %.2f, %.1f dB after", line[0] ? "; " : "", v, held, double(pk), after);
            std::strncat(line, one, sizeof line - std::strlen(line) - 1);
        }
        std::snprintf(msg, sizeof msg, "Diffuse, KICKED DECAY 1 runaway then DECAY noon at 10 s (want peak < 1, >= 30 dB down by 13.5 s): %s", line);
        check(ok, msg);
    }
    // Extremes per voicing.
    {
        const Buf h = hits(10.0);
        bool ok = true;
        float worstPk = 0.0f;
        for (int v = 1; v < 4; ++v)
            for (int a = 0; a < 3; ++a)
                for (float tn : {0.0f, 1.0f}) {
                    Settings s;
            s.wear = 0; // the diffuse round was built without wear
                    s.att = a, s.decay = 1.0f, s.drive = 1.0f, s.tension = tn, s.wobble = 0.0f;
                    rv::Tank t;
                    t.prepare(kFs, 48);
                    apply(t, s);
                    t.setEchoDiffuseVoicing(v);
                    Stereo o{Buf(h.size()), Buf(h.size())};
                    for (size_t pos = 0; pos < h.size(); pos += 48)
                        t.process(h.data() + pos, h.data() + pos, o.l.data() + pos, o.r.data() + pos, 48);
                    worstPk = std::max(worstPk, peakOf(o));
                    ok &= finite(o) && peakOf(o) < 1.0f;
                }
        std::snprintf(msg, sizeof msg, "Diffuse 1-3 extremes (DECAY 1, DRIVE 1, every ATTITUDE, TENSION 0/1): finite, worst peak %.3f", double(worstPk));
        check(ok, msg);
    }
    // Cost (desktop): position 3, KICKED, DRIVE 1, DECAY 1, TENSION 0, per voicing.
    {
        const Buf in = hits(6.0);
        double ns[4];
        for (int v = 0; v < 4; ++v) {
            double best = 1e30;
            for (int run = 0; run < 3; ++run) {
                rv::Tank t;
                t.prepare(kFs, 48);
                Settings s;
            s.wear = 0; // the diffuse round was built without wear
                s.att = 2, s.drive = 1.0f, s.decay = 1.0f, s.tone = 1.0f, s.tension = 0.0f;
                apply(t, s);
                t.setEchoDiffuseVoicing(v);
                Buf l(in.size()), r(in.size());
                const auto t0 = std::chrono::steady_clock::now();
                for (size_t pos = 0; pos < in.size(); pos += 48)
                    t.process(in.data() + pos, in.data() + pos, l.data() + pos, r.data() + pos, 48);
                const auto t1 = std::chrono::steady_clock::now();
                best = std::min(best, std::chrono::duration<double, std::nano>(t1 - t0).count() / double(in.size()));
            }
            ns[v] = best;
        }
        std::snprintf(msg, sizeof msg, "Diffuse cost (desktop, SPRINGS 3 worst case): none %.1f, light %.1f, medium %.1f, heavy %.1f ns/sample",
                      ns[0], ns[1], ns[2], ns[3]);
        info(msg);
    }
}

// ---- wear (PROTOTYPE) -------------------------------------------------------------------------
const char* const kWearName[5] = {"none", "worn tape", "radio band", "BBD grit", "crushed"};

// Energy-weighted centre (ms) of repeat k's window on the tape.
double repeatCentreMs(const RepeatStats& r, int k, double secs)
{
    const size_t d = sec(secs), a = sec(0.1) + size_t(k + 1) * d - sec(0.02), b = std::min(r.play.size(), a + d);
    double e = 0, t = 0;
    for (size_t i = a; i < b; ++i) {
        const double p = double(r.play[i]) * r.play[i];
        e += p, t += p * double(i - a);
    }
    return e > 0 ? 1000.0 * t / e / kFs : 0.0;
}

void wear()
{
    // On the tape (0.6 s, feedback 0.8): the first repeat untouched, level per
    // repeat vs none (no added energy: no repeat louder than none's + 1 dB),
    // the echo time kept (each repeat's centre within 1 ms of none's).
    RepeatStats st[rv::echo::kNumWearVoicings];
    for (int v = 0; v < rv::echo::kNumWearVoicings; ++v) st[v] = tapeRepeats(0, 0.8f, 0.6, v);
    for (int v = 1; v < rv::echo::kNumWearVoicings; ++v) {
        const bool first = std::memcmp(st[v].play.data(), st[0].play.data(), sec(0.1 + 2 * 0.6 - 0.05) * sizeof(float)) == 0;
        double up = -99, lvl[6], drift = 0;
        for (int k = 0; k < 6; ++k) {
            lvl[k] = st[v].db[k] - st[0].db[k];
            up     = std::max(up, lvl[k]);
            drift  = std::max(drift, std::fabs(repeatCentreMs(st[v], k, 0.6) - repeatCentreMs(st[0], k, 0.6)));
        }
        // Steady: once the first pass has shaped it (radio: broadband -> band), each later pass within 1 dB of none's step.
        double unsteady = 0;
        for (int k = 2; k < 6; ++k) unsteady = std::max(unsteady, std::fabs(lvl[k] - lvl[k - 1]));
        std::snprintf(msg, sizeof msg,
                      "Wear %s on the tape (0.6 s, feedback 0.8): repeat 1 untouched %d; level per repeat 1-6 vs none %+.1f %+.1f "
                      "%+.1f %+.1f %+.1f %+.1f dB (never above +1; from the 2nd on, each step within 1 dB of none's: worst %.1f); "
                      "timing, worst repeat centre %.2f ms off none's (limit 3.5: the filters' own delay, a few tenths of a ms a pass)",
                      kWearName[v], int(first), lvl[0], lvl[1], lvl[2], lvl[3], lvl[4], lvl[5], unsteady, drift);
        check(first && up <= 1.0 && unsteady <= 1.0 && drift <= 3.5, msg);
    }

    // In the Tank, per voicing.
    for (int v = 1; v < rv::echo::kNumWearVoicings; ++v) {
        // Deterministic and block-size independent (seeded randomness).
        const Buf h = hits(8.0);
        Settings s;
        s.decay = 0.85f, s.tension = 0.6f;
        auto go = [&](int block) {
            rv::Tank t;
            t.prepare(kFs, block);
            apply(t, s);
            t.setEchoWearVoicing(v);
            Stereo o{Buf(h.size()), Buf(h.size())};
            for (size_t pos = 0; pos < h.size(); pos += size_t(block)) {
                const int m = int(std::min<size_t>(size_t(block), h.size() - pos));
                t.process(h.data() + pos, h.data() + pos, o.l.data() + pos, o.r.data() + pos, m);
            }
            return o;
        };
        const Stereo a = go(48), b = go(48), c = go(333);
        double worst = 0;
        const int clicks = clicksBoth(a, sec(0.5), &worst);
        std::snprintf(msg, sizeof msg, "Wear %s in the Tank (hits, DECAY 0.85): the same twice %d, block 48 = 333 %d, finite %d, "
                                       "%d clicks (worst ratio %.1f)",
                      kWearName[v], int(same(a, b)), int(same(a, c)), int(finite(a)), clicks, worst);
        check(same(a, b) && same(a, c) && finite(a) && clicks == 0, msg);

        // KICKED's runaway: bounded, and >= 30 dB down ~3 s after DECAY comes back to noon.
        {
            const Buf in = burst(16.0, 0.5);
            Settings k;
            k.att = 2, k.decay = 1.0f, k.tension = 0.75f, k.drive = 1.0f;
            rv::Tank t;
            t.prepare(kFs, 48);
            apply(t, k);
            t.setEchoWearVoicing(v);
            Stereo o{Buf(in.size()), Buf(in.size())};
            for (size_t pos = 0; pos < in.size(); pos += 48) {
                if (pos == sec(10.0)) t.setParam(rv::ParamId::Decay, 0.5f);
                t.process(in.data() + pos, in.data() + pos, o.l.data() + pos, o.r.data() + pos, 48);
            }
            const double held = stereoDb(o, sec(8.0), sec(10.0)), after = stereoDb(o, sec(12.7), sec(13.0));
            const float  pk   = peakOf(o);
            std::snprintf(msg, sizeof msg,
                          "Wear %s, KICKED DECAY 1 (DRIVE 1): held 8-10 s %.1f dB, peak %.2f (< 1); DECAY to noon at 10 s: "
                          "12.7-13 s %.1f dB (want >= 30 dB under)",
                          kWearName[v], held, double(pk), after);
            check(finite(o) && pk < 1.0f && after < held - 30.0, msg);
        }
        // Extremes.
        {
            bool ok = true;
            float worstPk = 0.0f;
            for (int a2 = 0; a2 < 3; ++a2)
                for (float tn : {0.0f, 1.0f}) {
                    Settings x;
                    x.att = a2, x.decay = 1.0f, x.drive = 1.0f, x.tension = tn, x.wobble = 0.0f;
                    rv::Tank t;
                    t.prepare(kFs, 48);
                    apply(t, x);
                    t.setEchoWearVoicing(v);
                    const Buf hh = hits(10.0);
                    Stereo o{Buf(hh.size()), Buf(hh.size())};
                    for (size_t pos = 0; pos < hh.size(); pos += 48)
                        t.process(hh.data() + pos, hh.data() + pos, o.l.data() + pos, o.r.data() + pos, 48);
                    worstPk = std::max(worstPk, peakOf(o));
                    ok &= finite(o) && peakOf(o) < 1.0f;
                }
            std::snprintf(msg, sizeof msg, "Wear %s extremes (DECAY 1, DRIVE 1, every ATTITUDE, TENSION 0/1): finite, worst peak %.3f",
                          kWearName[v], double(worstPk));
            check(ok, msg);
        }
    }

    // M6 Ringing at SPRINGS 3 per voicing: click + burst, every ATTITUDE x
    // DECAY .85 / 1 (KICKED 1 is the runaway: checked above) x TENSION 0 / .5.
    {
        Buf clk(sec(14.0), 0.0f);
        clk[sec(0.5)] = clk[sec(0.5) + 1] = 0.5f;
        Buf nb(sec(14.0), 0.0f);
        {
            const Buf z = noise(sec(0.5), 0.43f, 77u);
            std::copy(z.begin(), z.end(), nb.begin() + long(sec(0.5)));
        }
        int    flagged[5] = {0, 0, 0, 0, 0}, cells = 0;
        double worst[5]   = {0, 0, 0, 0, 0};
        char   at[5][80]  = {"", "", "", "", ""};
        for (int v = 0; v < 5; ++v)
            for (int a = 0; a < 3; ++a)
                for (float dc : {0.85f, 1.0f})
                    for (float tn : {0.0f, 0.5f})
                        for (const Buf* in : {static_cast<const Buf*>(&clk), static_cast<const Buf*>(&nb)}) {
                            if (a == 2 && dc > 0.9f) continue;
                            Settings x;
                            x.att = a, x.decay = dc, x.tension = tn;
                            rv::Tank t;
                            t.prepare(kFs, 48);
                            apply(t, x);
                            t.setEchoWearVoicing(v);
                            Stereo o{Buf(in->size()), Buf(in->size())};
                            for (size_t pos = 0; pos < in->size(); pos += 48)
                                t.process(in->data() + pos, in->data() + pos, o.l.data() + pos, o.r.data() + pos, 48);
                            const auto m = rv::metrics::compute({o.l, o.r}, kFs);
                            if (m.ringing || m.steadyTone) {
                                if (flagged[v]++ == 0)
                                    std::snprintf(at[v], sizeof at[v], " (first: att %d DECAY %.2f TENSION %.1f %s, %.0f Hz)", a,
                                                  double(dc), double(tn), in == &clk ? "click" : "burst", m.ringingHz);
                            }
                            if (!std::isnan(m.ringingDb)) worst[v] = std::max(worst[v], m.ringingDb);
                            if (v == 0) ++cells;
                        }
        std::snprintf(msg, sizeof msg,
                      "Wear, M6 Ringing at SPRINGS 3 (%d cells each): flagged / worst ringing_db: none %d / %.1f%s, worn tape %d / %.1f%s, "
                      "radio %d / %.1f%s, BBD %d / %.1f%s, crushed %d / %.1f%s",
                      cells, flagged[0], worst[0], at[0], flagged[1], worst[1], at[1], flagged[2], worst[2], at[2], flagged[3],
                      worst[3], at[3], flagged[4], worst[4], at[4]);
        check(flagged[1] <= flagged[0] && flagged[2] <= flagged[0] && flagged[3] <= flagged[0] && flagged[4] <= flagged[0], msg);
    }
    // Cost (desktop): SPRINGS 3 worst case per voicing.
    {
        const Buf in = hits(6.0);
        double ns[5];
        for (int v = 0; v < 5; ++v) {
            double best = 1e30;
            for (int run = 0; run < 3; ++run) {
                rv::Tank t;
                t.prepare(kFs, 48);
                Settings x;
                x.att = 2, x.drive = 1.0f, x.decay = 1.0f, x.tone = 1.0f, x.tension = 0.0f;
                apply(t, x);
                t.setEchoWearVoicing(v);
                Buf l(in.size()), r(in.size());
                const auto t0 = std::chrono::steady_clock::now();
                for (size_t pos = 0; pos < in.size(); pos += 48)
                    t.process(in.data() + pos, in.data() + pos, l.data() + pos, r.data() + pos, 48);
                const auto t1 = std::chrono::steady_clock::now();
                best = std::min(best, std::chrono::duration<double, std::nano>(t1 - t0).count() / double(in.size()));
            }
            ns[v] = best;
        }
        std::snprintf(msg, sizeof msg,
                      "Wear cost (desktop, SPRINGS 3 worst case): none %.1f, worn tape %.1f, radio %.1f, BBD %.1f, crushed %.1f ns/sample",
                      ns[0], ns[1], ns[2], ns[3], ns[4]);
        info(msg);
    }
}

// ---- bbd (the BBD's strength, PROTOTYPE) ------------------------------------------------------
// Inharmonic energy in 0.3-5 kHz (where it's heard, under the heads) on
// repeat k of a 1.7 kHz tone burst on the tape, dB re the tone's own energy
// there: Goertzel every 25 Hz, skipping +-75 Hz around the tone's harmonics.
double aliasDb(const RepeatStats& r, int k, double secs)
{
    const size_t d = sec(secs), a = sec(0.1) + size_t(k + 1) * d - sec(0.03), b = a + sec(0.2);
    auto g = [&](double hz) { // Hann-windowed (the window's own leakage stays ~60 dB down)
        const double c = 2 * std::cos(2 * kPi * hz / kFs);
        double s1 = 0, s2 = 0;
        for (size_t i = a; i < b; ++i) {
            const double w = 0.5 - 0.5 * std::cos(2 * kPi * double(i - a) / double(b - a));
            const double y = w * r.play[i] + c * s1 - s2;
            s2 = s1, s1 = y;
        }
        return s1 * s1 + s2 * s2 - c * s1 * s2;
    };
    double tone = 0, other = 0;
    for (double f = 300; f <= 5000; f += 25) {
        bool harm = false;
        for (int h = 1; h <= 3; ++h) harm |= std::fabs(f - 1700.0 * h) <= 75.0;
        const double p = g(f);
        if (std::fabs(f - 1700.0) <= 75.0) tone += p;
        else if (!harm) other += p;
    }
    return db(other) - db(tone);
}

void bbd()
{
    const char* const kName[4] = {"A today", "B stronger", "C strongest", "D follows time"};
    // A 1.7 kHz tone burst (60 ms) on the tape, 0.6 s echo, feedback 0.8.
    Buf tone(sec(9.0), 0.0f);
    for (size_t i = 0; i < sec(0.06); ++i)
        tone[sec(0.1) + i] = float(0.4 * std::sin(2 * kPi * 1700.0 * double(i) / kFs) * std::sin(kPi * double(i) / double(sec(0.06))));
    RepeatStats none = tapeRepeats(0, 0.8f, 0.6, 0, 0, &tone), crushed = tapeRepeats(0, 0.8f, 0.6, rv::echo::kWearCrushed, 0, &tone);
    const double crushedAlias = aliasDb(crushed, 2, 0.6), noneAlias = aliasDb(none, 2, 0.6);
    double alias3[4];
    for (int v = 0; v < 4; ++v) {
        const RepeatStats r = tapeRepeats(0, 0.8f, 0.6, rv::echo::kWearBbd, v, &tone);
        const RepeatStats b = tapeRepeats(0, 0.8f, 0.6, rv::echo::kWearBbd, v); // broadband burst: level per repeat
        const RepeatStats n = tapeRepeats(0, 0.8f, 0.6, 0, 0);
        double up = -99, unsteady = 0, lvl[6];
        for (int k = 0; k < 6; ++k) {
            lvl[k] = b.db[k] - n.db[k];
            up     = std::max(up, lvl[k]);
            if (k >= 2) unsteady = std::max(unsteady, std::fabs(lvl[k] - lvl[k - 1]));
        }
        const double a1 = aliasDb(r, 1, 0.6), a2 = aliasDb(r, 2, 0.6), a3 = aliasDb(r, 3, 0.6);
        alias3[v] = a2;
        std::snprintf(msg, sizeof msg,
                      "BBD %s (0.6 s echo, feedback 0.8): aliasing re the tone on repeats 2 / 3 / 4: %.1f / %.1f / %.1f dB "
                      "(none %.1f, crushed %.1f on repeat 3); level per repeat 1-6 vs no wear %+.1f %+.1f %+.1f %+.1f %+.1f %+.1f dB "
                      "(never above +1; from the 2nd to the 6th, on average within 1 dB a pass of no wear's steps: %.2f; "
                      "single steps wobble up to %.1f dB with the aliasing and pumping)",
                      kName[v], a1, a2, a3, noneAlias, crushedAlias, lvl[0], lvl[1], lvl[2], lvl[3], lvl[4], lvl[5],
                      (lvl[5] - lvl[1]) / 4.0, unsteady);
        check(up <= 1.0 && std::fabs(lvl[5] - lvl[1]) / 4.0 <= 1.0, msg);
    }
    std::snprintf(msg, sizeof msg,
                  "BBD aliasing gets more obvious A < B < C, and B, C, D clearly above crushed (+6 dB) on repeat 3: A %.1f, B %.1f, "
                  "C %.1f, D %.1f, crushed %.1f dB",
                  alias3[0], alias3[1], alias3[2], alias3[3], crushedAlias);
    check(alias3[1] > alias3[0] + 6.0 && alias3[2] > alias3[1] && alias3[1] > crushedAlias + 6.0 && alias3[2] > crushedAlias + 6.0
              && alias3[3] > crushedAlias + 6.0,
          msg);

    // D: the clock follows the echo time (and swoops with it).
    {
        auto clockAt = [&](float tension, double secs, const std::vector<size_t>& clk = {}) {
            rv::Tank t;
            t.prepare(kFs, 48);
            Settings x;
            x.tension = tension;
            apply(t, x);
            t.setBbdVoicing(3);
            run(t, secs, 48, clk);
            return double(t.bbdClockHz());
        };
        const double noon = clockAt(0.5f, 1.5), longest = clockAt(0.0f, 3.0), shortest = clockAt(1.0f, 1.5);
        // Clocked at 100 bpm: TENSION from 1/2 (1.2 s) to 1/16 (0.15 s): the clock during the swoop.
        rv::Tank t;
        t.prepare(kFs, 48);
        Settings x;
        x.tension = 0.07f;
        apply(t, x);
        t.setBbdVoicing(3);
        const auto clk = steadyClock(100.0, 0.1, 6.0);
        run(t, 2.0, 48, clk);
        const double before = t.bbdClockHz();
        t.setParam(rv::ParamId::Tension, 0.93f);
        run(t, 0.5, 48, clk, sec(2.0));
        const double mid = t.bbdClockHz();
        run(t, 2.0, 48, clk, sec(2.5));
        const double after = t.bbdClockHz();
        std::snprintf(msg, sizeof msg,
                      "BBD D: clock %.0f Hz at TENSION noon (0.4 s; as B), %.0f at 2 s (C or lower), %.0f at 80 ms; clocked 1/2 -> "
                      "1/16 at 100 bpm: %.0f -> %.0f (mid-swoop) -> %.0f Hz",
                      noon, longest, shortest, before, mid, after);
        check(std::fabs(noon - double(rv::echo::kBbd[1].clockHz)) < 100.0 && longest <= double(rv::echo::kBbd[2].clockHz)
                  && shortest > 7000.0 && mid > before + 100.0 && after > mid + 100.0,
              msg);
    }

    // In the Tank, per strength: deterministic, no clicks, runaway bounded and dying,
    // extremes, M6 (Ringing and steady tone: an aliasing loop must not leave a stuck tone).
    for (int v = 0; v < 4; ++v) {
        const Buf h = hits(8.0);
        Settings x;
        x.decay = 0.85f, x.tension = 0.6f;
        auto go = [&](int block, float tension) {
            rv::Tank t;
            t.prepare(kFs, block);
            Settings y = x;
            y.tension = tension;
            apply(t, y);
            t.setBbdVoicing(v);
            Stereo o{Buf(h.size()), Buf(h.size())};
            for (size_t pos = 0; pos < h.size(); pos += size_t(block)) {
                const int m = int(std::min<size_t>(size_t(block), h.size() - pos));
                t.process(h.data() + pos, h.data() + pos, o.l.data() + pos, o.r.data() + pos, m);
            }
            return o;
        };
        const Stereo a = go(48, 0.6f), b = go(48, 0.6f), c = go(333, 0.6f), lng = go(48, 0.15f);
        double w1 = 0, w2 = 0;
        const int clicks = clicksBoth(a, sec(0.5), &w1) + clicksBoth(lng, sec(0.5), &w2);
        std::snprintf(msg, sizeof msg, "BBD %s in the Tank (hits, DECAY 0.85, TENSION 0.6 and 0.15): the same twice %d, block 48 = 333 %d, "
                                       "finite %d, %d clicks (worst ratio %.1f)",
                      kName[v], int(same(a, b)), int(same(a, c)), int(finite(a) && finite(lng)), clicks, std::max(w1, w2));
        check(same(a, b) && same(a, c) && finite(a) && finite(lng) && clicks == 0, msg);

        {
            const Buf in = burst(16.0, 0.5);
            Settings k;
            k.att = 2, k.decay = 1.0f, k.tension = 0.75f, k.drive = 1.0f;
            rv::Tank t;
            t.prepare(kFs, 48);
            apply(t, k);
            t.setBbdVoicing(v);
            Stereo o{Buf(in.size()), Buf(in.size())};
            for (size_t pos = 0; pos < in.size(); pos += 48) {
                if (pos == sec(10.0)) t.setParam(rv::ParamId::Decay, 0.5f);
                t.process(in.data() + pos, in.data() + pos, o.l.data() + pos, o.r.data() + pos, 48);
            }
            const double held = stereoDb(o, sec(8.0), sec(10.0)), after = stereoDb(o, sec(12.7), sec(13.0));
            std::snprintf(msg, sizeof msg,
                          "BBD %s, KICKED DECAY 1 (DRIVE 1): held 8-10 s %.1f dB, peak %.2f; DECAY to noon at 10 s: 12.7-13 s %.1f dB "
                          "(want >= 30 dB under); CLEAN DECAY 1 fades: see below",
                          kName[v], held, double(peakOf(o)), after);
            check(finite(o) && peakOf(o) < 1.0f && after < held - 30.0, msg);
        }
        {   // CLEAN DECAY 1 at a long echo (TENSION 0.15): the repeats fade.
            const Buf in = burst(30.0, 0.5);
            Settings k;
            k.decay = 1.0f, k.tension = 0.15f;
            rv::Tank t;
            t.prepare(kFs, 48);
            apply(t, k);
            t.setBbdVoicing(v);
            Stereo o{Buf(in.size()), Buf(in.size())};
            for (size_t pos = 0; pos < in.size(); pos += 48)
                t.process(in.data() + pos, in.data() + pos, o.l.data() + pos, o.r.data() + pos, 48);
            const double early = stereoDb(o, sec(2.0), sec(5.0)), late = stereoDb(o, sec(26.0), sec(29.0));
            std::snprintf(msg, sizeof msg, "BBD %s, CLEAN DECAY 1 at 1.2 s echo: 2-5 s %.1f dB, 26-29 s %.1f dB (want >= 10 dB lower)",
                          kName[v], early, late);
            check(late < early - 10.0, msg);
        }
    }
    {
        Buf clk(sec(14.0), 0.0f);
        clk[sec(0.5)] = clk[sec(0.5) + 1] = 0.5f;
        Buf nb(sec(14.0), 0.0f);
        {
            const Buf z = noise(sec(0.5), 0.43f, 77u);
            std::copy(z.begin(), z.end(), nb.begin() + long(sec(0.5)));
        }
        int    flagged[4] = {0, 0, 0, 0}, steady[4] = {0, 0, 0, 0}, cells = 0;
        double worst[4]   = {0, 0, 0, 0};
        for (int v = 0; v < 4; ++v)
            for (int a = 0; a < 3; ++a)
                for (float dc : {0.85f, 1.0f})
                    for (float tn : {0.0f, 0.5f})
                        for (const Buf* in : {static_cast<const Buf*>(&clk), static_cast<const Buf*>(&nb)}) {
                            if (a == 2 && dc > 0.9f) continue;
                            Settings x;
                            x.att = a, x.decay = dc, x.tension = tn;
                            rv::Tank t;
                            t.prepare(kFs, 48);
                            apply(t, x);
                            t.setBbdVoicing(v);
                            Stereo o{Buf(in->size()), Buf(in->size())};
                            for (size_t pos = 0; pos < in->size(); pos += 48)
                                t.process(in->data() + pos, in->data() + pos, o.l.data() + pos, o.r.data() + pos, 48);
                            const auto m = rv::metrics::compute({o.l, o.r}, kFs);
                            if (m.ringing || m.steadyTone) ++flagged[v];
                            if (m.steadyTone) ++steady[v];
                            if (!std::isnan(m.ringingDb)) worst[v] = std::max(worst[v], m.ringingDb);
                            if (v == 0) ++cells;
                        }
        std::snprintf(msg, sizeof msg,
                      "BBD, M6 at SPRINGS 3 (%d cells each: click + burst, ATTITUDE x DECAY .85/1 x TENSION 0/.5): flagged (steady tone) / "
                      "worst ringing_db: A %d (%d) / %.1f, B %d (%d) / %.1f, C %d (%d) / %.1f, D %d (%d) / %.1f",
                      cells, flagged[0], steady[0], worst[0], flagged[1], steady[1], worst[1], flagged[2], steady[2], worst[2], flagged[3],
                      steady[3], worst[3]);
        check(flagged[0] + flagged[1] + flagged[2] + flagged[3] == 0, msg);
    }
    // Cost (desktop): SPRINGS 3 worst case per strength.
    {
        const Buf in = hits(6.0);
        double ns[5];
        for (int v = 0; v < 5; ++v) {
            double best = 1e30;
            for (int run2 = 0; run2 < 3; ++run2) {
                rv::Tank t;
                t.prepare(kFs, 48);
                Settings x;
                x.att = 2, x.drive = 1.0f, x.decay = 1.0f, x.tone = 1.0f, x.tension = 0.0f;
                apply(t, x);
                if (v == 4) t.setEchoWearVoicing(0);
                else t.setBbdVoicing(v);
                Buf l(in.size()), r(in.size());
                const auto t0 = std::chrono::steady_clock::now();
                for (size_t pos = 0; pos < in.size(); pos += 48)
                    t.process(in.data() + pos, in.data() + pos, l.data() + pos, r.data() + pos, 48);
                const auto t1 = std::chrono::steady_clock::now();
                best = std::min(best, std::chrono::duration<double, std::nano>(t1 - t0).count() / double(in.size()));
            }
            ns[v] = best;
        }
        std::snprintf(msg, sizeof msg, "BBD cost (desktop, SPRINGS 3 worst case): A %.1f, B %.1f, C %.1f, D %.1f, no wear %.1f ns/sample",
                      ns[0], ns[1], ns[2], ns[3], ns[4]);
        info(msg);
    }
}

// ---- cost -----------------------------------------------------------------------------------
void cost()
{
    const Buf in = hits(6.0);
    auto ns = [&](Settings s) {
        double best = 1e30;
        for (int run = 0; run < 3; ++run) {
            rv::Tank t;
            t.prepare(kFs, 48);
            apply(t, s);
            Buf l(in.size()), r(in.size());
            const auto t0 = std::chrono::steady_clock::now();
            for (size_t pos = 0; pos < in.size(); pos += 48)
                t.process(in.data() + pos, in.data() + pos, l.data() + pos, r.data() + pos, 48);
            const auto t1 = std::chrono::steady_clock::now();
            best = std::min(best, std::chrono::duration<double, std::nano>(t1 - t0).count() / double(in.size()));
        }
        return best;
    };
    Settings s;
    s.att = 2, s.drive = 1.0f, s.decay = 1.0f, s.tone = 1.0f, s.tension = 0.0f;
    s.springs = 1;
    const double two = ns(s);
    s.springs = 2;
    const double echo = ns(s);
    s.echo = false;
    const double coupled = ns(s);
    s.springs = 1;
    const double twoOld = ns(s);
    std::snprintf(msg, sizeof msg,
                  "Cost (desktop, KICKED, DRIVE 1, DECAY 1, TENSION 0): SPRINGS 3 echo %.1f ns/sample, SPRINGS 2 %.1f (%.1f with "
                  "Spring C running, echo mode off), the coupled reference %.1f",
                  echo, two, twoOld, coupled);
    info(msg);
}

} // namespace

int main(int argc, char** argv)
{
    const char* only = argc > 1 ? argv[1] : nullptr;
    struct Section {
        const char* name;
        void (*fn)();
    };
    const Section sections[] = {{"identity", identity}, {"free", freeTime},   {"clock", clockDivisions}, {"host", hostTempo},
                                {"swoop", swoop},       {"feedback", feedback}, {"steps", steps}, {"springs", springs},   {"tape", tape},
                                {"level", level},       {"switching", switching}, {"stability", stability}, {"hothighs", hotHighs}, {"blocks", blocks},
                                {"diffuse", diffuse}, {"wear", wear}, {"bbd", bbd}, {"cost", cost}};
    for (const auto& s : sections) {
        if (only && std::strcmp(only, s.name) != 0) continue;
        std::printf("== %s\n", s.name);
        s.fn();
    }
    std::printf("%s (%d failure%s)\n", failures ? "FAILED" : "ALL PASSED", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
