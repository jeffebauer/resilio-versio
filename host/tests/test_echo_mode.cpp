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
//   feedback  DECAY 0 = one repeat; noon 0.5; the top the same in every
//             ATTITUDE (kFeedbackTop = KICKED's old DECAY 0.92; ADR 0041
//             amendment, owner 5 Oct 2026), below the rise bit for bit as
//             before. At DECAY 1, every ATTITUDE: a single rim's repeats
//             persist at a steady level held by the tape (no fade, no
//             growth, under the limiter); with continuous input bounded
//             (limiter threshold), not growing; backing DECAY off to noon:
//             >= 30 dB down within 3.5 s, no clicks while riding DECAY.
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
//             repeats fade outside the top's persistent zone (KICKED DECAY
//             0.89 and 1, CLEAN / DRIVEN DECAY 1).
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
//             deterministic, no clicks, KICKED's top bounded and dying, CLEAN's
//             long build (DECAY 0.9) fades, M6 (Ringing and steady tone) clean.
//   bits      The repeats' bit depth (echo_bits_voicing A-D, on BBD A):
//             level per repeat, the echo time, no new narrow (pitched) peaks
//             in repeats 2-6 beyond A's, tails ending in silence (no stuck
//             buzz), deterministic, no clicks, runaway, M6.
//   tapewear  PROTOTYPE tape wear (echo_wear_voicing 5 tape saturation +
//             roll-off, 6 / 7 + crinkle subtle / obvious; owner 5 Oct): the
//             wear's own response (above 300 Hz never over 0 dB, the head bump
//             within its number), the loop's peak per-pass gain not raised (no
//             added energy), level per repeat (repeat 1 untouched, none above
//             +1 dB), no new pitches (a loud tone's inharmonic energy, a rim's
//             new narrow peaks, a hot 15 kHz tone's folds), deterministic,
//             blocks 1 / 7 / 333 bit for bit, no clicks, the held top steady in
//             every ATTITUDE, KICKED's top dying when backed off, extremes, M6,
//             cost (desktop, reported).
//   cost      Desktop ns/sample, SPRINGS 3 echo vs SPRINGS 2 vs the coupled
//             reference. Reported.
//   blend     PROTOTYPE springs blend (echo_springs_voicing; owner 6 Oct 2026,
//             EchoVoicing.h "Springs blend", dsp/EchoDirect.h): A (0) bit for
//             bit the default; any voicing in SPRINGS 1 / 2 bit for bit today;
//             1-6 deterministic, block 48 = 333, finite, no clicks (hits, and
//             flipping SPRINGS 2 <-> 3 mid-tail); D (3, 6: no springs) has no
//             spring tail (the wet before the first repeat and after the only
//             one is silent, A's rings); ping-pong alternates (repeat 1 left,
//             2 right ...); wide's right head kWideMs behind the left. Mono
//             fold-down and desktop cost reported.

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

// The held top's build (1 s windows over 10-30 s): within 3 dB through the
// springs only (their wash evens the windows); the shipped blend's repeats are
// distinct, so a window's level depends on how many land in it: 4 dB.
const double kHeldBuildDb = rv::echo::kSpringsBlend[rv::echo::kSpringsBlendDefault].style != rv::echo::DirectStyle::None ? 4.0 : 3.0;

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
    int   wear = -1; // echo_wear_voicing; -1 = the default (tape saturation since 6 Oct; BBD grit 4-6 Oct)
    int   outBits = -1; // output_bits_voicing: -1 the default (ADR 0042's mu-law box on the wet), 0 without it (a test hook)
};

void apply(rv::Tank& t, const Settings& s)
{
    using rv::ParamId;
    t.setEchoMode(s.echo);
    t.setHostTempo(s.hostBpm);
    if (s.wear >= 0) t.setEchoWearVoicing(s.wear);
    if (s.outBits >= 0) t.setOutputBitsVoicing(s.outBits);
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

// Renders with echo_springs_voicing v (-1 = the default), optionally flipping
// SPRINGS 3 <-> 2 every flipAt samples (section "blend").
Stereo renderBlend(const Settings& s, int v, const Buf& in, int block = 48, size_t flipAt = 0);

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
    // Since the springs blend (6 Oct 2026) the first repeat is heard directly,
    // at the echo time after the input's own hit; through the springs only
    // (voicing A) it came the springs' ~27 ms transit later, after their splash
    // of the hit, which was the reference then.
    if (rv::echo::kSpringsBlend[rv::echo::kSpringsBlendDefault].style != rv::echo::DirectStyle::None)
        return (double(onset(diff)) - double(onset(in))) / kFs;
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

const char* const kAttName[3] = {"CLEAN", "DRIVEN", "KICKED"};

// ---- feedback -------------------------------------------------------------------------------
Buf rimLike(double seconds, double at, float peak = 0.5f); // the bits section's rim
// A held C minor pad (soft saws C3 Eb3 G3 C4, `hold` s from 0.5 s, slow in and out), -9 dBFS.
Buf padLike(double hold)
{
    Buf x(sec(hold + 1.5), 0.0f);
    const size_t n = sec(hold), att = sec(0.4), rel = sec(0.8);
    for (double f : {130.81, 155.56, 196.0, 261.63})
        for (size_t i = 0; i < n; ++i) {
            const double t = double(i) / kFs, env = std::min({1.0, double(i) / double(att), double(n - 1 - i) / double(rel)});
            x[sec(0.5) + i] += float(env * (2 * std::fmod(f * t, 1.0) - 1) / 4);
        }
    lowpass(x, 1800.0);
    normalise(x, -9.0f);
    return x;
}

void feedback()
{
    // The curve (ADR 0041 amendment, owner 5 Oct 2026): the base curve
    // (0.95 x DECAY^0.926) up to each ATTITUDE's start, then a smoothstep
    // rise to the same top in every ATTITUDE, kFeedbackTop = what KICKED's
    // DECAY 0.92 gave before (when its rise went to 1.25 and ran away).
    {
        using namespace rv::echo;
        const float f0 = feedbackClean(0.0f), fn = feedbackClean(0.5f), f1 = feedbackClean(1.0f), k1 = feedbackKicked(1.0f);
        // KICKED's curve as it was (its top 1.25), at DECAY 0.92.
        const float b92 = feedbackBase(0.92f), u92 = (0.92f - kKickedFrom) / (1.0f - kKickedFrom);
        const float old92 = b92 + u92 * u92 * (3.0f - 2.0f * u92) * (1.25f - b92);
        // Bit for bit as before below each start (the base curve is the old CLEAN curve).
        bool below = true;
        for (int k = 0; k <= 1000; ++k) {
            const float d = k / 1000.0f;
            if (d <= kCleanFrom) below &= feedbackClean(d) == feedbackBase(d);
            if (d <= kKickedFrom) below &= feedbackKicked(d) == feedbackBase(d);
        }
        // Even travel: rising all the way (no flat, dead end), no jump.
        bool  rising  = true;
        float maxStep = 0.0f;
        for (int k = 1; k <= 100; ++k) {
            const float a = (k - 1) / 100.0f, b = k / 100.0f;
            for (float (*f)(float) : {&feedbackClean, &feedbackKicked}) {
                rising &= f(b) > f(a);
                maxStep = std::max(maxStep, f(b) - f(a));
            }
        }
        std::snprintf(msg, sizeof msg,
                      "Feedback: DECAY 0 / noon / 1 = %.3f / %.3f / %.3f, KICKED 1 %.3f (want 0, 0.5, the top %.3f in every ATTITUDE = "
                      "KICKED's old DECAY 0.92 %.4f); bit for bit as before below DECAY %.2f (CLEAN, DRIVEN) / %.2f (KICKED) %d; rising "
                      "to the end %d (largest step per 1 %% of the knob %.3f; DECAY 0.92 -> 1 adds %.3f)",
                      double(f0), double(fn), double(f1), double(k1), double(kFeedbackTop), double(old92), double(kCleanFrom),
                      double(kKickedFrom), int(below), int(rising), double(maxStep), double(k1 - feedbackKicked(0.92f)));
        check(f0 == 0.0f && std::fabs(fn - 0.5f) < 1e-4f && f1 == kFeedbackTop && k1 == kFeedbackTop
                  && std::fabs(kFeedbackTop - old92) < 0.002f && below && rising && maxStep < 0.05f
                  && k1 - feedbackKicked(0.92f) > 0.05f,
              msg);
        // Each pass's gain at the heads' peak (~0.965): where repeats stop fading.
        float grow[2] = {-1.0f, -1.0f};
        for (int k = 0; k <= 1000; ++k) {
            if (grow[0] < 0 && feedbackClean(k / 1000.0f) * 0.965f > 1.0f) grow[0] = k / 1000.0f;
            if (grow[1] < 0 && feedbackKicked(k / 1000.0f) * 0.965f > 1.0f) grow[1] = k / 1000.0f;
        }
        std::snprintf(msg, sizeof msg,
                      "Feedback: a pass gains (x 0.965, the heads' peak) from DECAY %.3f in CLEAN / DRIVEN, %.3f in KICKED (was 0.873 in "
                      "KICKED, never in CLEAN / DRIVEN)",
                      double(grow[0]), double(grow[1]));
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
    // DECAY 1, every ATTITUDE: a single rim (-6 dBFS, TENSION noon: 0.4 s)
    // and 30 s. The repeats persist (no fade), settle to a steady level held
    // by the tape (1 s windows over 10-30 s within 3 dB: +-1.5), don't keep
    // growing (25-30 s within 1 dB of 20-25 s) and stay under the limiter
    // (its gain never moves). Read without the wet's mu-law box (ADR 0042: its
    // grain is not the echo); the shipped output checked finite.
    for (int att = 0; att < 3; ++att) {
        Settings s;
        s.att = att, s.decay = 1.0f, s.tension = 0.5f, s.outBits = 0;
        const Buf in = rimLike(30.0, 0.5, 0.5f);
        rv::Tank t;
        t.prepare(kFs, 48);
        apply(t, s);
        Stereo o{Buf(in.size()), Buf(in.size())};
        float lim = 1.0f;
        for (size_t pos = 0; pos < in.size(); pos += 48) {
            t.process(in.data() + pos, in.data() + pos, o.l.data() + pos, o.r.data() + pos, 48);
            lim = std::min(lim, t.limiterGain());
        }
        s.outBits = -1;
        const Stereo sh = render(s, in);
        const double hit = stereoDb(o, sec(0.5), sec(0.85)), early = stereoDb(o, sec(2.0), sec(4.0));
        double lo = 1e9, hi = -1e9;
        for (int w = 10; w < 30; ++w) {
            const double v = stereoDb(o, sec(w), sec(w + 1));
            lo = std::min(lo, v), hi = std::max(hi, v);
        }
        const double a = stereoDb(o, sec(20.0), sec(25.0)), b = stereoDb(o, sec(25.0), sec(30.0));
        std::snprintf(msg, sizeof msg,
                      "Feedback %s DECAY 1, a single rim: repeats persist at %.1f dB re the hit (%.1f dBFS; 2-4 s %+.1f), 1 s windows "
                      "over 10-30 s within %.1f dB (want <= %.0f), 25-30 s vs 20-25 s %+.2f dB (want < +1: no growth); peak %.3f, "
                      "limiter gain never under %.3f (want 1: under the limiter); shipped finite %d",
                      kAttName[att], b - hit, b, early - hit, hi - lo, kHeldBuildDb, b - a, double(peakOf(o)), double(lim), int(finite(sh)));
        check(b > early && hi - lo <= kHeldBuildDb && b - a < 1.0 && lim >= 1.0f && finite(o) && finite(sh), msg);
    }
    // DECAY 1 with continuous input (skank stabs, 30 s; DRIVE 0 and 1):
    // bounded (peak under the limiter's threshold, without the wet's mu-law box) and not
    // growing (the last 5 s within 1 dB of the 5 s before). The limiter's
    // deepest gain is reported.
    {
        const Buf in = stabs(30.0);
        bool ok = true;
        char line[420] = "";
        for (int att = 0; att < 3; ++att)
            for (float dr : {0.0f, 1.0f}) {
                Settings s;
                s.att = att, s.decay = 1.0f, s.tension = 0.5f, s.drive = dr, s.outBits = 0;
                rv::Tank t;
                t.prepare(kFs, 48);
                apply(t, s);
                Stereo o{Buf(in.size()), Buf(in.size())};
                float lim = 1.0f;
                for (size_t pos = 0; pos < in.size(); pos += 48) {
                    t.process(in.data() + pos, in.data() + pos, o.l.data() + pos, o.r.data() + pos, 48);
                    lim = std::min(lim, t.limiterGain());
                }
                const double grow = stereoDb(o, sec(25.0), sec(30.0)) - stereoDb(o, sec(20.0), sec(25.0));
                const float  pk   = peakOf(o);
                ok &= finite(o) && pk <= rv::Tank::kLimitThreshold + 1e-3f && grow < 1.0;
                char one[100];
                std::snprintf(one, sizeof one, "%s%s DRIVE %.0f peak %.3f limiter %.1f dB growth %+.2f", line[0] ? "; " : "",
                              kAttName[att], double(dr), double(pk), 20.0 * std::log10(double(lim)), grow);
                std::strncat(line, one, sizeof line - std::strlen(line) - 1);
            }
        std::snprintf(msg, sizeof msg, "Feedback DECAY 1, skank for 30 s (bounded <= %.2f without the wet's mu-law box, growth < 1 dB): %s",
                      double(rv::Tank::kLimitThreshold), line);
        check(ok, msg);
    }
    // Backing off (ADR 0018's spirit): DECAY 1 for 10 s, then noon, every
    // ATTITUDE at DRIVE 1: >= 30 dB down within 3.5 s. Riding DECAY up and
    // down (0.5 -> 1 -> 0.5 ..., 4 s legs) adds no clicks to the same note
    // at DECAY 1 held.
    for (int att = 0; att < 3; ++att) {
        Settings s;
        s.att = att, s.decay = 1.0f, s.tension = 0.5f, s.drive = 1.0f;
        s.outBits = 0; // the echo itself, without the wet's mu-law box (its 12 / 10-bit steps read as edges)
        const size_t n  = sec(20.0);
        const Buf    rim = rimLike(20.0, 0.5, 0.5f);
        // For the clicks, a smooth note (a Hann-shaped 0.3 s chord, -6 dBFS):
        // a rim's own edge, repeated and saturated, reads as a click.
        Buf note(n, 0.0f);
        for (size_t i = 0; i < sec(0.3); ++i) {
            const double t = double(i) / kFs, w = 0.5 - 0.5 * std::cos(2 * kPi * t / 0.3);
            note[sec(0.5) + i] = float(0.25 * w * (std::sin(2 * kPi * 330 * t) + std::sin(2 * kPi * 495 * t)));
        }
        auto go = [&](int mode, const Buf& in) {
            rv::Tank t;
            t.prepare(kFs, 48);
            apply(t, s);
            Stereo o{Buf(n), Buf(n)};
            for (size_t pos = 0; pos < n; pos += 48) {
                if (mode == 1 && pos == sec(10.0)) t.setParam(rv::ParamId::Decay, 0.5f);
                if (mode == 2) {
                    const double ts = double(pos) / kFs, ph = std::fmod(ts, 8.0) / 4.0;
                    t.setParam(rv::ParamId::Decay, float(ph < 1.0 ? 0.5 + 0.5 * ph : 1.0 - 0.5 * (ph - 1.0)));
                }
                t.process(in.data() + pos, in.data() + pos, o.l.data() + pos, o.r.data() + pos, 48);
            }
            return o;
        };
        const Stereo back = go(1, rim), held = go(0, note), ride = go(2, note);
        const double before = stereoDb(back, sec(8.0), sec(10.0)), after = stereoDb(back, sec(13.2), sec(13.5));
        double w0 = 0, w1 = 0;
        const int cHeld = clicksBoth(held, sec(0.4), &w0), cRide = clicksBoth(ride, sec(0.4), &w1);
        std::snprintf(msg, sizeof msg,
                      "Feedback %s DECAY 1 (DRIVE 1), back to noon at 10 s: 8-10 s %.1f dB, 13.2-13.5 s %.1f dB (want >= 30 dB under); "
                      "riding DECAY 0.5 <-> 1: %d clicks (worst ratio %.1f) vs %d held at 1 (%.1f); peak %.3f, finite %d",
                      kAttName[att], before, after, cRide, w1, cHeld, w0, double(std::max(peakOf(back), peakOf(ride))),
                      int(finite(back) && finite(ride)));
        check(after < before - 30.0 && cRide <= cHeld && finite(back) && finite(ride), msg);
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
    // Voicing A (the whole wet through the springs, ADR 0041 "The first
    // repeat"): window 0 is the springs' splash of the hit, and every step,
    // the first included, is geometric. The shipped blend (C, 6 Oct 2026):
    // the hit's splash is mostly gone from the wet, so at MIX 1 the first
    // step (the springs' quarter of the hit -> the direct repeat) is no longer
    // the tape's step; the repeats' steps (2nd, 3rd) still are, and DECAY 0
    // is still one repeat. The first step is reported at MIX 1 and with the
    // dry hit (MIX 0.6, the springs-blend page's).
    for (int v : {0, rv::echo::kSpringsBlendDefault}) {
        const bool a = v == 0;
        bool ok = true;
        char line[500] = "", first[300] = "";
        for (float dc : {0.0f, 0.3f, 0.5f, 0.7f, 0.9f}) {
            Settings s;
            s.tension = 0.0f, s.decay = dc, s.splash = 0.0f, s.att = 0;
            const Stereo o = renderBlend(s, v, in);
            const double s1 = win(o, 1) - win(o, 0), s2 = win(o, 2) - win(o, 1), s3 = win(o, 3) - win(o, 2);
            const double g = 20.0 * std::log10(std::max(double(rv::echo::feedbackClean(dc)), 1e-9));
            if (dc == 0.0f) ok &= (a ? std::fabs(s1 + 10.0) < 2.0 : s1 < 0.0) && s2 < -15.0;
            else ok &= (a ? s1 < -0.5 && std::fabs(s1 - s2) < 1.5 : s2 < -0.5 && std::fabs(s2 - g) < 1.5) && std::fabs(s2 - s3) < 1.5;
            char one[90];
            std::snprintf(one, sizeof one, "%sDECAY %.1f (g %.1f dB): %+.1f %+.1f %+.1f", line[0] ? "; " : "", double(dc), g, s1, s2, s3);
            std::strncat(line, one, sizeof line - std::strlen(line) - 1);
            if (!a) {
                s.mix = 0.6f;
                const Stereo m = renderBlend(s, v, in);
                std::snprintf(one, sizeof one, "%sDECAY %.1f %+.1f", first[0] ? ", " : "", double(dc), win(m, 1) - win(m, 0));
                std::strncat(first, one, sizeof first - std::strlen(first) - 1);
            }
        }
        if (a)
            std::snprintf(msg, sizeof msg,
                          "Steps (A, springs only): every repeat a step down from the hit, the first included (repeat k vs k-1, dB): "
                          "%s. Want steps within 1.5 dB of each other, below 0; DECAY 0 one repeat at ~-10 dB, then nothing", line);
        else
            std::snprintf(msg, sizeof msg,
                          "Steps (C, the shipped blend), repeat k vs k-1 at MIX 1, dB: %s. Want the 2nd and 3rd steps the tape's (within "
                          "1.5 dB of g and of each other), DECAY 0 one repeat then nothing; the first step (reported) at MIX 0.6, with "
                          "the dry hit: %s",
                          line, first);
        check(ok, msg);
    }
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
// The shipped output: the wet's mu-law box (ADR 0042) sits before the
// limiter since its 5 Oct amendment, so the peaks are limited again.
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
    // everywhere but the top's persistent repeats (KICKED DECAY 0.89 and 1, where a pass
    // gains or nearly; CLEAN / DRIVEN DECAY 1; ADR 0041 amendment).
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
                        const bool runaway = dc > (a == 2 ? 0.87f : 0.93f); // the top: persistent by design
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
                      "noise, 36 s): finite, peak < 1 (worst %.3f), repeats fade >= 10 dB over 30 s outside "
                      "the top's persistent zone (least %.1f dB); %d bad (first: %s)",
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
RepeatStats tapeRepeats(int voicing, float fb, double secs, int wear = 0, int bbd = 0, const Buf* stim = nullptr, int bits = 0)
{
    rv::dsp::TapeEcho e;
    std::vector<float> tapeBuf(rv::Tank::requiredTapeFloats(kFs));
    e.prepare(kFs, 0x1234u, tapeBuf.data(), tapeBuf.size());
    e.setDiffuseVoicing(voicing);
    e.setWearVoicing(wear);
    e.setBbdVoicing(bbd);
    e.setBitsVoicing(bits);
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
    // DECAY 0.85 and 1 (KICKED 1 included since its top is persistent
    // repeats rather than a runaway, ADR 0041 amendment), TENSION 0 / 0.5. Flags and worst ringing_db per voicing;
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
        std::snprintf(msg, sizeof msg, "Diffuse, KICKED DECAY 1 (the held top) then DECAY noon at 10 s (want peak < 1, >= 30 dB down by 13.5 s): %s", line);
        check(ok, msg);
    }
    // Extremes per voicing (the shipped output: since ADR 0042's 5 Oct
    // amendment the wet's mu-law box is before the limiter). (Since CLEAN and
    // DRIVEN hold at the top of DECAY, ADR 0041 amendment, their wet sits at
    // the limiter here, as KICKED's did.)
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
        std::snprintf(msg, sizeof msg,
                      "Diffuse 1-3 extremes (DECAY 1, DRIVE 1, every ATTITUDE, TENSION 0/1): finite, worst peak %.3f (< 1)", double(worstPk));
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
constexpr int     kWearRound1  = 5; // the 4 Oct round (none ... crushed); the tape wear round (5-7) has its own section

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
    RepeatStats st[kWearRound1];
    for (int v = 0; v < kWearRound1; ++v) st[v] = tapeRepeats(0, 0.8f, 0.6, v);
    for (int v = 1; v < kWearRound1; ++v) {
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
    for (int v = 1; v < kWearRound1; ++v) {
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
            // The shipped output, as "stability" and the diffuse extremes.
            bool ok = true;
            float worstPk = 0.0f;
            const Buf hh = hits(10.0);
            for (int a2 = 0; a2 < 3; ++a2)
                for (float tn : {0.0f, 1.0f}) {
                        Settings x;
                        x.att = a2, x.decay = 1.0f, x.drive = 1.0f, x.tension = tn, x.wobble = 0.0f;
                        rv::Tank t;
                        t.prepare(kFs, 48);
                        apply(t, x);
                        t.setEchoWearVoicing(v);
                        Stereo o{Buf(hh.size()), Buf(hh.size())};
                        for (size_t pos = 0; pos < hh.size(); pos += 48)
                            t.process(hh.data() + pos, hh.data() + pos, o.l.data() + pos, o.r.data() + pos, 48);
                        worstPk = std::max(worstPk, peakOf(o));
                        ok &= finite(o) && peakOf(o) < 1.0f;
                    }
            std::snprintf(msg, sizeof msg, "Wear %s extremes (DECAY 1, DRIVE 1, every ATTITUDE, TENSION 0/1): finite, worst peak %.3f (< 1)",
                          kWearName[v], double(worstPk));
            check(ok, msg);
        }
    }

    // M6 Ringing at SPRINGS 3 per voicing: click + burst, every ATTITUDE x
    // DECAY .85 / 1 (every ATTITUDE: KICKED's top no longer runs away) x TENSION 0 / .5.
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
            t.setEchoWearVoicing(rv::echo::kWearBbd); // as built (the default 4-6 Oct)
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
        t.setEchoWearVoicing(rv::echo::kWearBbd); // as built (the default 4-6 Oct)
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
            t.setEchoWearVoicing(rv::echo::kWearBbd); // as built (the default 4-6 Oct)
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
            t.setEchoWearVoicing(rv::echo::kWearBbd); // as built (the default 4-6 Oct)
            t.setBbdVoicing(v);
            Stereo o{Buf(in.size()), Buf(in.size())};
            for (size_t pos = 0; pos < in.size(); pos += 48) {
                if (pos == sec(10.0)) t.setParam(rv::ParamId::Decay, 0.5f);
                t.process(in.data() + pos, in.data() + pos, o.l.data() + pos, o.r.data() + pos, 48);
            }
            const double held = stereoDb(o, sec(8.0), sec(10.0)), after = stereoDb(o, sec(12.7), sec(13.0));
            std::snprintf(msg, sizeof msg,
                          "BBD %s, KICKED DECAY 1 (DRIVE 1): held 8-10 s %.1f dB, peak %.2f; DECAY to noon at 10 s: 12.7-13 s %.1f dB "
                          "(want >= 30 dB under); CLEAN DECAY 0.9 fades: see below",
                          kName[v], held, double(peakOf(o)), after);
            check(finite(o) && peakOf(o) < 1.0f && after < held - 30.0, msg);
        }
        {   // CLEAN's long build at a long echo (TENSION 0.15): the repeats fade.
            // DECAY 0.9 (feedback 0.94): what DECAY 1 gave before CLEAN's top
            // became persistent repeats (0.95; ADR 0041 amendment), which
            // "feedback" checks.
            const Buf in = burst(30.0, 0.5);
            Settings k;
            k.decay = 0.9f, k.tension = 0.15f;
            rv::Tank t;
            t.prepare(kFs, 48);
            apply(t, k);
            t.setEchoWearVoicing(rv::echo::kWearBbd); // as built (the default 4-6 Oct)
            t.setBbdVoicing(v);
            Stereo o{Buf(in.size()), Buf(in.size())};
            for (size_t pos = 0; pos < in.size(); pos += 48)
                t.process(in.data() + pos, in.data() + pos, o.l.data() + pos, o.r.data() + pos, 48);
            const double early = stereoDb(o, sec(2.0), sec(5.0)), late = stereoDb(o, sec(26.0), sec(29.0));
            std::snprintf(msg, sizeof msg, "BBD %s, CLEAN DECAY 0.9 at 1.2 s echo: 2-5 s %.1f dB, 26-29 s %.1f dB (want >= 10 dB lower)",
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
                            Settings x;
                            x.att = a, x.decay = dc, x.tension = tn;
                            rv::Tank t;
                            t.prepare(kFs, 48);
                            apply(t, x);
                            t.setEchoWearVoicing(rv::echo::kWearBbd); // as built (the default 4-6 Oct)
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
                t.setEchoWearVoicing(v == 4 ? 0 : rv::echo::kWearBbd); // as built (the default 4-6 Oct)
                if (v != 4) t.setBbdVoicing(v);
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

// ---- bits (the repeats' bit depth, PROTOTYPE) ----------------------------------------------
// A rim-like hit: a click plus three decaying partials (420, 1150, 2300 Hz).
Buf rimLike(double seconds, double at, float peak)
{
    Buf b(sec(seconds), 0.0f);
    rv::dsp::Rng rng;
    rng.seed(99u);
    for (size_t i = 0; i < sec(0.12); ++i) {
        const double t = double(i) / kFs;
        const double v = 0.5 * std::exp(-t / 0.002) * rng.bipolar() + 0.35 * std::exp(-t / 0.03) * std::sin(2 * kPi * 420 * t)
                       + 0.3 * std::exp(-t / 0.02) * std::sin(2 * kPi * 1150 * t) + 0.2 * std::exp(-t / 0.012) * std::sin(2 * kPi * 2300 * t);
        b[sec(at) + i] = float(v);
    }
    normalise(b, 20.0f * std::log10(peak));
    return b;
}

// New narrow peaks in repeat k that repeat 1 didn't have: Hann-windowed
// Goertzel every 10 Hz, 200 Hz-6 kHz, over each repeat's first 0.25 s. A bin
// counts when it stands above its +-1/3-octave median both in repeat k itself
// and in repeat k's spectrum relative to repeat 1's (so darkening, which
// moves whole regions, doesn't count; a new pitch, narrow, does). Returns the
// largest such prominence (dB) over repeats 2-6 and where.
double newPeakDb(const RepeatStats& r, double secs, double* atHz, int* atRep)
{
    const int nF = 581; // 200 .. 6000 Hz
    auto spec = [&](int k, std::vector<double>& out) {
        const size_t d = sec(secs), a = sec(0.1) + size_t(k + 1) * d - sec(0.02), b = a + sec(0.25);
        out.assign(size_t(nF), 0.0);
        for (int j = 0; j < nF; ++j) {
            const double hz = 200.0 + 10.0 * j, c = 2 * std::cos(2 * kPi * hz / kFs);
            double s1 = 0, s2 = 0;
            for (size_t i = a; i < b; ++i) {
                const double w = 0.5 - 0.5 * std::cos(2 * kPi * double(i - a) / double(b - a));
                const double y = w * r.play[i] + c * s1 - s2;
                s2 = s1, s1 = y;
            }
            out[size_t(j)] = db(s1 * s1 + s2 * s2 - c * s1 * s2);
        }
    };
    auto prominence = [&](const std::vector<double>& v, int j) {
        const double hz = 200.0 + 10.0 * j, lo = hz / 1.26, hi = hz * 1.26;
        std::vector<double> nb;
        for (int m = 0; m < nF; ++m) {
            const double f = 200.0 + 10.0 * m;
            if (f >= lo && f <= hi && std::abs(m - j) > 2) nb.push_back(v[size_t(m)]);
        }
        if (nb.empty()) return 0.0;
        std::nth_element(nb.begin(), nb.begin() + long(nb.size() / 2), nb.end());
        return v[size_t(j)] - nb[nb.size() / 2];
    };
    std::vector<double> s1, sk, rel;
    rel.assign(size_t(nF), 0.0);
    spec(0, s1);
    double worst = 0;
    for (int k = 1; k < 6; ++k) {
        spec(k, sk);
        const double floorDb = *std::max_element(sk.begin(), sk.end()) - 60.0; // ignore the float floor
        for (int j = 0; j < nF; ++j) rel[size_t(j)] = sk[size_t(j)] - s1[size_t(j)];
        for (int j = 0; j < nF; ++j) {
            if (sk[size_t(j)] < floorDb) continue;
            const double p = std::min(prominence(sk, j), prominence(rel, j));
            if (p > worst) {
                worst = p;
                if (atHz) *atHz = 200.0 + 10.0 * j;
                if (atRep) *atRep = k + 1;
            }
        }
    }
    return worst;
}

void bits()
{
    const char* const kName[4] = {"A none", "B 24k/12-bit", "C 24k/8-bit", "D 24k/8-bit mu-law"};
    const int bbd = rv::echo::kWearBbd;
    // On the tape (BBD A, 0.4 s echo, feedback 0.75; a rim at -6 dBFS):
    // level per repeat vs A, the echo time, new pitches.
    const Buf rim = rimLike(9.0, 0.1);
    const RepeatStats base = tapeRepeats(0, 0.75f, 0.4, bbd, 0, &rim, 0);
    double peakA = 0;
    for (int v = 0; v < 4; ++v) {
        const RepeatStats r = v == 0 ? base : tapeRepeats(0, 0.75f, 0.4, bbd, 0, &rim, v);
        double up = -99, lvl[6], drift = 0, hz = 0;
        int    at = 0;
        for (int k = 0; k < 6; ++k) {
            lvl[k] = r.db[k] - base.db[k];
            up     = std::max(up, lvl[k]);
            drift  = std::max(drift, std::fabs(repeatCentreMs(r, k, 0.4) - repeatCentreMs(base, k, 0.4)));
        }
        const double pk = newPeakDb(r, 0.4, &hz, &at);
        if (v == 0) peakA = pk;
        std::snprintf(msg, sizeof msg,
                      "Bits %s on the tape (BBD A, rim -6 dBFS, 0.4 s, feedback 0.75): level per repeat 1-6 vs A %+.1f %+.1f %+.1f "
                      "%+.1f %+.1f %+.1f dB (never above +1); timing %.2f ms off A's (limit 1.5); new narrow peak in repeats 2-6 %.1f dB "
                      "(%.0f Hz, repeat %d; limit A's %.1f + 3)",
                      kName[v], lvl[0], lvl[1], lvl[2], lvl[3], lvl[4], lvl[5], drift, pk, hz, at, peakA);
        check(up <= 1.0 && drift <= 1.5 && pk <= peakA + 3.0, msg);
    }
    // The same metric on the BBD round's rejected B and C (the owner's "higher
    // pitched chirp"), to show it sees what the owner heard.
    {
        double hzB = 0, hzC = 0;
        int    kB = 0, kC = 0;
        const double pB = newPeakDb(tapeRepeats(0, 0.75f, 0.4, bbd, 1, &rim, 0), 0.4, &hzB, &kB);
        const double pC = newPeakDb(tapeRepeats(0, 0.75f, 0.4, bbd, 2, &rim, 0), 0.4, &hzC, &kC);
        std::snprintf(msg, sizeof msg,
                      "Bits: the new-peak check on the rejected BBD B / C (owner heard a pitched chirp): %.1f dB at %.0f Hz (repeat %d) / "
                      "%.1f dB at %.0f Hz (repeat %d), vs BBD A %.1f",
                      pB, hzB, kB, pC, hzC, kC, peakA);
        info(msg);
    }

    // In the Tank, per version: deterministic, block-size free, no clicks;
    // the tail ends in silence (no stuck buzz or low tone): a rim at DECAY
    // 0.85, 30 s; the last seconds keep falling and end under -100 dBFS,
    // and M6 flags no steady tone; KICKED's runaway bounded and dying.
    for (int v = 0; v < 4; ++v) {
        {
            const Buf h = hits(8.0);
            Settings x;
            x.decay = 0.85f, x.tension = 0.6f;
            auto go = [&](int block) {
                rv::Tank t;
                t.prepare(kFs, block);
                apply(t, x);
                t.setEchoWearVoicing(rv::echo::kWearBbd); // as built (on BBD A, the default 4-6 Oct)
                t.setEchoBitsVoicing(v);
                Stereo o{Buf(h.size()), Buf(h.size())};
                for (size_t pos = 0; pos < h.size(); pos += size_t(block)) {
                    const int m = int(std::min<size_t>(size_t(block), h.size() - pos));
                    t.process(h.data() + pos, h.data() + pos, o.l.data() + pos, o.r.data() + pos, m);
                }
                return o;
            };
            const Stereo a = go(48), b = go(48), c = go(333);
            double w = 0;
            const int clicks = clicksBoth(a, sec(0.5), &w);
            std::snprintf(msg, sizeof msg, "Bits %s in the Tank (hits, DECAY 0.85): the same twice %d, block 48 = 333 %d, finite %d, %d clicks "
                                           "(worst ratio %.1f)",
                          kName[v], int(same(a, b)), int(same(a, c)), int(finite(a)), clicks, w);
            check(same(a, b) && same(a, c) && finite(a) && clicks == 0, msg);
        }
        {
            const Buf in = rimLike(30.0, 0.5);
            for (int att : {0, 2}) {
                Settings x;
                x.decay = 0.85f, x.tension = 0.5f, x.att = att;
                if (att == 2) x.decay = 0.8f; // under KICKED's runaway
                rv::Tank t;
                t.prepare(kFs, 48);
                apply(t, x);
                t.setEchoWearVoicing(rv::echo::kWearBbd); // as built (on BBD A, the default 4-6 Oct)
                t.setEchoBitsVoicing(v);
                Stereo o{Buf(in.size()), Buf(in.size())};
                for (size_t pos = 0; pos < in.size(); pos += 48)
                    t.process(in.data() + pos, in.data() + pos, o.l.data() + pos, o.r.data() + pos, 48);
                const double l1 = stereoDb(o, sec(26.0), sec(28.0)), l2 = stereoDb(o, sec(28.0), sec(30.0));
                const auto m = rv::metrics::compute({o.l, o.r}, kFs);
                std::snprintf(msg, sizeof msg,
                              "Bits %s, a rim's tail (%s DECAY %.2f, 0.4 s, 30 s): 26-28 s %.1f dB, 28-30 s %.1f dB (falling, under -100); "
                              "steady tone %d, ringing_db %.1f (%.0f Hz)",
                              kName[v], att ? "KICKED" : "CLEAN", double(x.decay), l1, l2, int(m.steadyTone), m.ringingDb, m.ringingHz);
                // Ringing reported only: on a tail that drops to silence in a few
                // seconds (8-bit) it reads the float floor (a probe put the flagged
                // 7.7 kHz bin level with its neighbours, ~90 dB under the tail).
                check(l2 <= l1 + 0.5 && l2 < -100.0 && !m.steadyTone, msg);
            }
        }
        {
            const Buf in = burst(16.0, 0.5);
            Settings k;
            k.att = 2, k.decay = 1.0f, k.tension = 0.75f, k.drive = 1.0f;
            rv::Tank t;
            t.prepare(kFs, 48);
            apply(t, k);
            t.setEchoWearVoicing(rv::echo::kWearBbd); // as built (on BBD A, the default 4-6 Oct)
            t.setEchoBitsVoicing(v);
            Stereo o{Buf(in.size()), Buf(in.size())};
            for (size_t pos = 0; pos < in.size(); pos += 48) {
                if (pos == sec(10.0)) t.setParam(rv::ParamId::Decay, 0.5f);
                t.process(in.data() + pos, in.data() + pos, o.l.data() + pos, o.r.data() + pos, 48);
            }
            const double held = stereoDb(o, sec(8.0), sec(10.0)), after = stereoDb(o, sec(12.7), sec(13.0));
            std::snprintf(msg, sizeof msg,
                          "Bits %s, KICKED DECAY 1 (DRIVE 1): held 8-10 s %.1f dB, peak %.2f; DECAY to noon at 10 s: 12.7-13 s %.1f dB "
                          "(want >= 30 dB under)",
                          kName[v], held, double(peakOf(o)), after);
            check(finite(o) && peakOf(o) < 1.0f && after < held - 30.0, msg);
        }
    }
    // M6 at SPRINGS 3 per version (click + burst, ATTITUDE x DECAY .85/1 x TENSION 0/.5).
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
                            Settings x;
                            x.att = a, x.decay = dc, x.tension = tn;
                            rv::Tank t;
                            t.prepare(kFs, 48);
                            apply(t, x);
                            t.setEchoWearVoicing(rv::echo::kWearBbd); // as built (on BBD A, the default 4-6 Oct)
                            t.setEchoBitsVoicing(v);
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
                      "Bits, M6 at SPRINGS 3 (%d cells each): flagged (steady tone) / worst ringing_db: A %d (%d) / %.1f, B %d (%d) / %.1f, "
                      "C %d (%d) / %.1f, D %d (%d) / %.1f",
                      cells, flagged[0], steady[0], worst[0], flagged[1], steady[1], worst[1], flagged[2], steady[2], worst[2], flagged[3],
                      steady[3], worst[3]);
        check(flagged[0] + flagged[1] + flagged[2] + flagged[3] == 0, msg);
    }
    // Cost (desktop).
    {
        const Buf in = hits(6.0);
        double ns[4];
        for (int v = 0; v < 4; ++v) {
            double best = 1e30;
            for (int run2 = 0; run2 < 3; ++run2) {
                rv::Tank t;
                t.prepare(kFs, 48);
                Settings x;
                x.att = 2, x.drive = 1.0f, x.decay = 1.0f, x.tone = 1.0f, x.tension = 0.0f;
                apply(t, x);
                t.setEchoWearVoicing(rv::echo::kWearBbd); // as built (on BBD A, the default 4-6 Oct)
                t.setEchoBitsVoicing(v);
                Buf l(in.size()), r(in.size());
                const auto t0 = std::chrono::steady_clock::now();
                for (size_t pos = 0; pos < in.size(); pos += 48)
                    t.process(in.data() + pos, in.data() + pos, l.data() + pos, r.data() + pos, 48);
                const auto t1 = std::chrono::steady_clock::now();
                best = std::min(best, std::chrono::duration<double, std::nano>(t1 - t0).count() / double(in.size()));
            }
            ns[v] = best;
        }
        std::snprintf(msg, sizeof msg, "Bits cost (desktop, SPRINGS 3 worst case): A %.1f, B %.1f, C %.1f, D %.1f ns/sample", ns[0], ns[1],
                      ns[2], ns[3]);
        info(msg);
    }
}

// ---- tapewear (PROTOTYPE, owner 5 Oct 2026) ---------------------------------------------------
// The tape wear round (EchoVoicing.h "Tape wear"): B tape saturation +
// roll-off (5), C1 / C2 the same + crinkle (6 / 7), against A = BBD grit (3,
// today's default) and none. No new pitches, no added energy (the loop's
// peak gain, which sets the held top, never raised), the held top steady,
// deterministic, block-size free.
const int         kTapeWearV[3]    = {rv::echo::kWearTapeSat, rv::echo::kWearCrinkle, rv::echo::kWearCrinkleHeavy};
const char* const kTapeWearName[3] = {"B tape sat", "C1 crinkle", "C2 crinkle"};

// A sine's per-pass gain on the tape (dB per pass, repeats 2 -> 5), quiet (-40 dBFS).
double perPassDb(int wear, double hz)
{
    Buf s(sec(9.0), 0.0f);
    const size_t len = sec(0.2);
    for (size_t i = 0; i < len; ++i)
        s[sec(0.1) + i] = float(0.01 * std::sin(2 * kPi * hz * double(i) / kFs) * std::sin(kPi * double(i) / double(len)));
    const RepeatStats r = tapeRepeats(0, 0.8f, 0.6, wear, 0, &s);
    return (r.db[4] - r.db[1]) / 3.0;
}

void tapeWear()
{
    using namespace rv::echo;
    // The held top's lock (kTapeHold*) comes in only where a pass gains:
    // below, the feedback path is bit for bit as without it.
    {
        float from[2] = {-1.0f, -1.0f};
        bool  zeroBelow = true;
        for (int k = 0; k <= 1000; ++k) {
            const float d = k / 1000.0f, fc = feedbackClean(d), fk = feedbackKicked(d);
            if (from[0] < 0 && holdWeight(fc) > 0.0f) from[0] = d;
            if (from[1] < 0 && holdWeight(fk) > 0.0f) from[1] = d;
            zeroBelow &= (fc * kHeadsPeakGain <= 1.0f) == (holdWeight(fc) == 0.0f);
        }
        std::snprintf(msg, sizeof msg,
                      "Tape wear held-top lock: comes in from DECAY %.3f (CLEAN / DRIVEN), %.3f (KICKED), where a pass gains; 0 below "
                      "%d; 1 at the top (%.2f)",
                      double(from[0]), double(from[1]), int(zeroBelow), double(holdWeight(kFeedbackTop)));
        check(zeroBelow && from[0] > 0.9f && from[1] > 0.85f && holdWeight(kFeedbackTop) == 1.0f, msg);
    }
    // The wear on its own, quiet sines (-40 dBFS): its gain vs frequency.
    {
        const double fs[] = {30, 50, 80, 120, 200, 400, 700, 1500, 3000, 6000, 10000};
        for (int k = 0; k < 3; ++k) {
            char   line[400] = "";
            double over300 = -99, peak = -99;
            for (double hz : fs) {
                rv::dsp::TapeWear w;
                w.prepare(kFs, 0x5EEDu);
                w.setVoicing(kTapeWearV[k]);
                Buf x(sec(2.0));
                for (size_t i = 0; i < x.size(); ++i) x[i] = float(0.01 * std::sin(2 * kPi * hz * double(i) / kFs));
                Buf y = x;
                w.process(y.data(), int(y.size()));
                const double g = db(power(y, sec(1.0), sec(2.0)) / power(x, sec(1.0), sec(2.0)));
                peak = std::max(peak, g);
                if (hz >= 300) over300 = std::max(over300, g);
                char one[24];
                std::snprintf(one, sizeof one, "%s%.0f %+.2f", line[0] ? ", " : "", hz, g);
                std::strncat(line, one, sizeof line - std::strlen(line) - 1);
            }
            std::snprintf(msg, sizeof msg,
                          "Tape wear %s alone, quiet sines (Hz dB): %s; above 300 Hz never over %+.2f dB (want <= +0.05), the bump's peak "
                          "%+.2f dB (want <= kTapeSatBumpDb %.1f + 0.2)",
                          kTapeWearName[k], line, over300, peak, double(kTapeSatBumpDb));
            check(over300 <= 0.05 && peak <= double(kTapeSatBumpDb) + 0.2, msg);
        }
    }
    // In the loop (with the heads): per-pass gain per frequency; its peak is
    // what holds the top, so it must not rise (no added energy).
    {
        const double fs[] = {50, 80, 120, 200, 400, 700, 1000, 2000, 3500, 6000};
        double none[10], peakNone = -99;
        for (int j = 0; j < 10; ++j) peakNone = std::max(peakNone, none[j] = perPassDb(0, fs[j]));
        for (int k = 0; k < 3; ++k) {
            char   line[420] = "";
            double peak = -99;
            for (int j = 0; j < 10; ++j) {
                const double g = perPassDb(kTapeWearV[k], fs[j]);
                peak = std::max(peak, g);
                char one[32];
                std::snprintf(one, sizeof one, "%s%.0f %+.2f", line[0] ? ", " : "", fs[j], g - none[j]);
                std::strncat(line, one, sizeof line - std::strlen(line) - 1);
            }
            std::snprintf(msg, sizeof msg,
                          "Tape wear %s in the loop (feedback 0.8, quiet): per-pass gain vs none (Hz dB/pass): %s; the loop's peak %.2f "
                          "dB/pass vs none's %.2f (want <= +0.05: no added energy)",
                          kTapeWearName[k], line, peak, peakNone);
            check(peak <= peakNone + 0.05, msg);
        }
    }
    // Level per repeat on the tape (broadband burst, 0.6 s, feedback 0.8), vs none
    // and vs A (BBD grit); the first repeat untouched; the echo time kept.
    {
        const RepeatStats n = tapeRepeats(0, 0.8f, 0.6, 0), a = tapeRepeats(0, 0.8f, 0.6, kWearBbd);
        for (int k = 0; k < 3; ++k) {
            const RepeatStats r = tapeRepeats(0, 0.8f, 0.6, kTapeWearV[k]);
            const bool first = std::memcmp(r.play.data(), n.play.data(), sec(0.1 + 2 * 0.6 - 0.05) * sizeof(float)) == 0;
            double up = -99, lvl[6], vsA[6], drift = 0;
            for (int j = 0; j < 6; ++j) {
                lvl[j] = r.db[j] - n.db[j], vsA[j] = r.db[j] - a.db[j];
                up     = std::max(up, lvl[j]);
                drift  = std::max(drift, std::fabs(repeatCentreMs(r, j, 0.6) - repeatCentreMs(n, j, 0.6)));
            }
            std::snprintf(msg, sizeof msg,
                          "Tape wear %s on the tape (burst, 0.6 s, feedback 0.8): repeat 1 untouched %d; level per repeat 1-6 vs none "
                          "%+.1f %+.1f %+.1f %+.1f %+.1f %+.1f dB (never above +1), vs A (BBD) %+.1f %+.1f %+.1f %+.1f %+.1f %+.1f; "
                          "timing %.2f ms off none's (limit 3.5)",
                          kTapeWearName[k], int(first), lvl[0], lvl[1], lvl[2], lvl[3], lvl[4], lvl[5], vsA[0], vsA[1], vsA[2], vsA[3],
                          vsA[4], vsA[5], drift);
            check(first && up <= 1.0 && drift <= 3.5, msg);
        }
    }
    // No new pitches. (1) A loud 1.7 kHz tone (0.4 and 0.8 peak: the tape
    // saturates): inharmonic energy in 0.3-5 kHz on repeats 2-4 ("bbd"'s
    // aliasDb), vs none and A. (2) New narrow peaks in a rim's repeats 2-6
    // ("bits"'s newPeakDb), vs none's.
    {
        for (double amp : {0.4, 0.8}) {
            Buf tone(sec(9.0), 0.0f);
            for (size_t i = 0; i < sec(0.06); ++i)
                tone[sec(0.1) + i] = float(amp * std::sin(2 * kPi * 1700.0 * double(i) / kFs) * std::sin(kPi * double(i) / double(sec(0.06))));
            const RepeatStats n = tapeRepeats(0, 0.8f, 0.6, 0, 0, &tone), a = tapeRepeats(0, 0.8f, 0.6, kWearBbd, 0, &tone);
            double an[3], aa[3];
            for (int j = 0; j < 3; ++j) an[j] = aliasDb(n, j + 1, 0.6), aa[j] = aliasDb(a, j + 1, 0.6);
            for (int k = 0; k < 3; ++k) {
                const RepeatStats r = tapeRepeats(0, 0.8f, 0.6, kTapeWearV[k], 0, &tone);
                double ar[3], worse = -99;
                for (int j = 0; j < 3; ++j) ar[j] = aliasDb(r, j + 1, 0.6), worse = std::max(worse, ar[j] - an[j]);
                std::snprintf(msg, sizeof msg,
                              "Tape wear %s, a %.1f peak 1.7 kHz tone: inharmonic energy repeats 2 / 3 / 4 %.1f / %.1f / %.1f dB re the "
                              "tone (none %.1f / %.1f / %.1f, A BBD %.1f / %.1f / %.1f); worst vs none %+.1f dB (%s)",
                              kTapeWearName[k], amp, ar[0], ar[1], ar[2], an[0], an[1], an[2], aa[0], aa[1], aa[2], worse,
                              k == 0 ? (amp < 0.5 ? "want <= +3" : "reported: the envelope's skirt") : "reported: the crinkle's dips spread the tone's own energy, not a pitch");
                // Gated at 0.4; at 0.8 the saturation flattens the burst's
                // envelope, widening the tone's own skirt past the +-75 Hz the
                // metric skips (not a pitch: "folds" below reads the folds).
                if (k == 0 && amp < 0.5) check(worse <= 3.0, msg);
                else info(msg);
            }
        }
        const Buf rim = rimLike(9.0, 0.1);
        double hzN = 0;
        int    atN = 0;
        const double pn = newPeakDb(tapeRepeats(0, 0.75f, 0.4, 0, 0, &rim), 0.4, &hzN, &atN);
        double hzA = 0;
        int    atA = 0;
        const double pa = newPeakDb(tapeRepeats(0, 0.75f, 0.4, kWearBbd, 0, &rim), 0.4, &hzA, &atA);
        for (int k = 0; k < 3; ++k) {
            double hz = 0;
            int    at = 0;
            const double p = newPeakDb(tapeRepeats(0, 0.75f, 0.4, kTapeWearV[k], 0, &rim), 0.4, &hz, &at);
            std::snprintf(msg, sizeof msg,
                          "Tape wear %s, a rim (-6 dBFS, 0.4 s, feedback 0.75): new narrow peak in repeats 2-6 %.1f dB (%.0f Hz, repeat %d); "
                          "none %.1f (%.0f Hz), A BBD %.1f (%.0f Hz); want <= none + 3",
                          kTapeWearName[k], p, hz, at, pn, hzN, pa, hzA);
            check(p <= pn + 3.0, msg);
        }
    }
    // Folds, worst case: a steady loud tone straight into the wear (no heads
    // in front, so far more top than the feedback ever carries). Its odd
    // harmonics above 24 kHz would fold back to inharmonic frequencies; the
    // strongest of those (Goertzel at each predicted fold), re the tone.
    {
        // B only: C1 / C2 are B plus a gain that dips (no harmonics of its own;
        // its dips spread a tone's energy around it, read here as "folds").
        for (int k = 0; k < 1; ++k) {
            char line[200] = "";
            double worstAll = -999;
            for (double hz : {1700.0, 4100.0}) {
                rv::dsp::TapeWear w;
                w.prepare(kFs, 0x5EEDu);
                w.setVoicing(kTapeWearV[k]);
                Buf y(sec(1.5));
                for (size_t i = 0; i < y.size(); ++i) y[i] = float(0.5 * std::sin(2 * kPi * hz * double(i) / kFs));
                w.process(y.data(), int(y.size()));
                auto g = [&](double f) {
                    const double c = 2 * std::cos(2 * kPi * f / kFs);
                    double s1 = 0, s2 = 0;
                    const size_t a = sec(0.5), b = sec(1.5);
                    for (size_t i = a; i < b; ++i) {
                        const double wv = 0.5 - 0.5 * std::cos(2 * kPi * double(i - a) / double(b - a));
                        const double v  = wv * y[i] + c * s1 - s2;
                        s2 = s1, s1 = v;
                    }
                    return db(s1 * s1 + s2 * s2 - c * s1 * s2);
                };
                const double tone = g(hz);
                double worst = -999;
                for (int h = 3; h <= 61; h += 2) {
                    double f = std::fmod(h * hz, double(kFs));
                    if (f > 0.5 * kFs) f = double(kFs) - f;
                    if (h * hz < 0.5 * kFs) continue; // a real harmonic, not a fold
                    bool nearHarm = false;
                    for (int m = 1; m * hz < 0.5 * kFs; ++m) nearHarm |= std::fabs(f - m * hz) < 20.0;
                    if (!nearHarm && f > 20.0) worst = std::max(worst, g(f) - tone);
                }
                worstAll = std::max(worstAll, worst);
                char one[60];
                std::snprintf(one, sizeof one, "%s%.0f Hz %.1f dB", line[0] ? ", " : "", hz, worst);
                std::strncat(line, one, sizeof line - std::strlen(line) - 1);
            }
            std::snprintf(msg, sizeof msg,
                          "Tape wear %s alone, a steady 0.5 tone straight in (no heads): strongest fold re the tone %s (want < -60: "
                          "the feedback never carries this much top)",
                          kTapeWearName[k], line);
            check(worstAll < -60.0, msg);
        }
    }
    // A hot 15 kHz tone ("hothighs", KICKED DRIVE 1, DECAY 0.8, 80 ms): the
    // echo's share of its folds at 3 and 9 kHz under -90 dBFS.
    for (int k = 0; k < 3; ++k) {
        const size_t n = sec(3.0);
        Buf in(n);
        for (size_t i = 0; i < n; ++i) in[i] = float(std::sin(2 * kPi * 15000.0 * double(i) / kFs));
        Settings s;
        s.att = 2, s.drive = 1.0f, s.decay = 0.8f, s.tension = 1.0f, s.wear = kTapeWearV[k];
        std::vector<float> pool(rv::Tank::requiredPoolFloats(kFs)), tapeBuf(rv::Tank::requiredTapeFloats(kFs));
        Stereo o[2];
        for (int j = 0; j < 2; ++j) {
            rv::Tank t;
            std::fill(pool.begin(), pool.end(), 0.0f);
            t.prepare(kFs, 48, pool.data(), pool.size(), j ? tapeBuf.data() : nullptr, j ? tapeBuf.size() : 0);
            apply(t, s);
            o[j] = Stereo{Buf(n), Buf(n)};
            for (size_t pos = 0; pos < n; pos += 48)
                t.process(in.data() + pos, in.data() + pos, o[j].l.data() + pos, o[j].r.data() + pos, 48);
        }
        auto goertzel = [&](double hz) {
            const double w = 2 * kPi * hz / kFs, c = 2 * std::cos(w);
            double s1 = 0, s2 = 0;
            for (size_t i = sec(1.0); i < sec(3.0); ++i) {
                const double x = 0.5 * ((o[1].l[i] - o[0].l[i]) + (o[1].r[i] - o[0].r[i]));
                const double y = x + c * s1 - s2;
                s2 = s1, s1 = y;
            }
            return db(s1 * s1 + s2 * s2 - c * s1 * s2) - db(0.25 * double(sec(2.0)) * double(sec(2.0)));
        };
        const double a3 = goertzel(3000.0), a9 = goertzel(9000.0);
        std::snprintf(msg, sizeof msg, "Tape wear %s, 15 kHz at 0 dBFS (KICKED DRIVE 1, DECAY 0.8, 80 ms): the echo's 3 kHz %.1f dBFS, 9 kHz %.1f dBFS (< -90)",
                      kTapeWearName[k], a3, a9);
        check(a3 < -90.0 && a9 < -90.0, msg);
    }

    for (int k = 0; k < 3; ++k) {
        const int v = kTapeWearV[k];
        // Deterministic, block-size free (1 / 7 / 333 = 48 bit for bit), no clicks.
        {
            const Buf h = hits(8.0);
            Settings s;
            s.decay = 0.85f, s.tension = 0.6f, s.wear = v;
            const Stereo a = render(s, h, 48), b = render(s, h, 48);
            bool blocksSame = true;
            for (int blk : {1, 7, 333}) blocksSame &= same(a, render(s, h, blk));
            double worst = 0;
            const int clicks = clicksBoth(a, sec(0.5), &worst);
            std::snprintf(msg, sizeof msg,
                          "Tape wear %s in the Tank (hits, DECAY 0.85): the same twice %d, blocks 1 / 7 / 333 = 48 %d, finite %d, %d clicks "
                          "(worst ratio %.1f)",
                          kTapeWearName[k], int(same(a, b)), int(blocksSame), int(finite(a)), clicks, worst);
            check(same(a, b) && blocksSame && finite(a) && clicks == 0, msg);
        }
        // The held top, locked like the BBD's (owner, 6 Oct 2026: "lock in
        // like today"; EchoVoicing.h kTapeHold*), every ATTITUDE, 120 s: a
        // single rim, and for B (the default) also the skank (13 s of stabs)
        // and a held pad (4 s). The rim as "feedback": persists, 1 s windows
        // over 10-30 s within 3 dB. Every material: no slow creep, 10 s
        // windows over 30-120 s within 1 dB, the last 10 s within 0.5 dB of
        // the 10 s before; under the limiter (its gain never moves). The pad
        // is still settling after 30 s even with A (BBD, the default until 6
        // Oct: its held pad falls ~4.5 dB over 30-120 s, a sustained wash
        // slowly thinning to held repeats), so there the bar is A's own drift
        // (at least 1 dB); A is rendered alongside for every material.
        auto heldRun = [&](int wear, int mat, int att, float* limOut) {
            Settings s;
            s.att = att, s.decay = 1.0f, s.tension = 0.5f, s.outBits = 0, s.wear = wear;
            Buf in = mat == 0 ? rimLike(120.0, 0.5, 0.5f) : (mat == 1 ? stabs(13.0) : padLike(4.0));
            in.resize(sec(120.0), 0.0f);
            rv::Tank t;
            t.prepare(kFs, 48);
            apply(t, s);
            Stereo o{Buf(in.size()), Buf(in.size())};
            float lim = 1.0f;
            for (size_t pos = 0; pos < in.size(); pos += 48) {
                t.process(in.data() + pos, in.data() + pos, o.l.data() + pos, o.r.data() + pos, 48);
                lim = std::min(lim, t.limiterGain());
            }
            if (limOut) *limOut = lim;
            return o;
        };
        auto drift30 = [&](const Stereo& o) {
            double lo = 1e9, hi = -1e9;
            for (int w = 30; w < 120; w += 10) {
                const double x = stereoDb(o, sec(w), sec(w + 10));
                lo = std::min(lo, x), hi = std::max(hi, x);
            }
            return hi - lo;
        };
        for (int mat = 0; mat < (k == 0 ? 3 : 1); ++mat)
            for (int att = 0; att < 3; ++att) {
                float lim = 1.0f;
                const Stereo o = heldRun(v, mat, att, &lim);
                const double aDrift = k == 0 ? drift30(heldRun(rv::echo::kWearBbd, mat, att, nullptr)) : -1.0;
                double lo = 1e9, hi = -1e9;
                for (int w = 10; w < 30; ++w) {
                    const double x = stereoDb(o, sec(w), sec(w + 1));
                    lo = std::min(lo, x), hi = std::max(hi, x);
                }
                const double build = hi - lo;
                const double drift = drift30(o), last = stereoDb(o, sec(110.0), sec(120.0)) - stereoDb(o, sec(100.0), sec(110.0));
                const double held = stereoDb(o, sec(110.0), sec(120.0)), early = stereoDb(o, sec(2.0), sec(4.0));
                const char* const kMatName[3] = {"a single rim", "the skank", "a held pad"};
                std::snprintf(msg, sizeof msg,
                              "Tape wear %s, %s DECAY 1, %s (120 s): held at %.1f dBFS; %s1 s windows over 10-30 s within %.1f dB%s; 10 s "
                              "windows over 30-120 s within %.2f dB (want <= %.2f; A %.2f), the last 10 s %+.2f dB (want within 0.5); "
                              "peak %.3f, limiter %.3f (want 1)",
                              kTapeWearName[k], kAttName[att], kMatName[mat], held, mat == 0 ? "" : "(reported) ", build,
                              mat == 0 ? (kHeldBuildDb > 3.0 ? " (want <= 4)" : " (want <= 3)") : "", drift, mat == 2 ? std::max(1.0, aDrift) : 1.0, aDrift, last,
                              double(peakOf(o)), double(lim));
                const double bar = mat == 2 ? std::max(1.0, aDrift) : 1.0;
                check((mat != 0 || (held > early && build <= kHeldBuildDb)) && drift <= bar && std::fabs(last) <= 0.5 && lim >= 1.0f && finite(o), msg);
            }
        // KICKED's top bounded and dying when DECAY comes back to noon; extremes finite.
        {
            const Buf in = burst(16.0, 0.5);
            Settings x;
            x.att = 2, x.decay = 1.0f, x.tension = 0.75f, x.drive = 1.0f, x.wear = v;
            rv::Tank t;
            t.prepare(kFs, 48);
            apply(t, x);
            Stereo o{Buf(in.size()), Buf(in.size())};
            for (size_t pos = 0; pos < in.size(); pos += 48) {
                if (pos == sec(10.0)) t.setParam(rv::ParamId::Decay, 0.5f);
                t.process(in.data() + pos, in.data() + pos, o.l.data() + pos, o.r.data() + pos, 48);
            }
            const double held = stereoDb(o, sec(8.0), sec(10.0)), after = stereoDb(o, sec(12.7), sec(13.0));
            bool ok = true;
            float worstPk = 0.0f;
            const Buf hh = hits(10.0);
            for (int a2 = 0; a2 < 3; ++a2)
                for (float tn : {0.0f, 1.0f}) {
                    Settings e;
                    e.att = a2, e.decay = 1.0f, e.drive = 1.0f, e.tension = tn, e.wobble = 0.0f, e.wear = v;
                    const Stereo r = render(e, hh);
                    worstPk = std::max(worstPk, peakOf(r));
                    ok &= finite(r) && peakOf(r) < 1.0f;
                }
            std::snprintf(msg, sizeof msg,
                          "Tape wear %s: KICKED DECAY 1 (DRIVE 1) held 8-10 s %.1f dB, peak %.2f, DECAY to noon: 12.7-13 s %.1f dB (want "
                          ">= 30 under); extremes (DECAY 1, DRIVE 1, every ATTITUDE, TENSION 0/1) finite, worst peak %.3f (< 1)",
                          kTapeWearName[k], held, double(peakOf(o)), after, double(worstPk));
            check(finite(o) && peakOf(o) < 1.0f && after < held - 30.0 && ok, msg);
        }
    }

    // M6 Ringing at SPRINGS 3 (as "wear"): none, A (BBD), B, C1, C2.
    {
        Buf clk(sec(14.0), 0.0f);
        clk[sec(0.5)] = clk[sec(0.5) + 1] = 0.5f;
        Buf nb(sec(14.0), 0.0f);
        {
            const Buf z = noise(sec(0.5), 0.43f, 77u);
            std::copy(z.begin(), z.end(), nb.begin() + long(sec(0.5)));
        }
        const int vs[5] = {0, kWearBbd, kWearTapeSat, kWearCrinkle, kWearCrinkleHeavy};
        int    flagged[5] = {0, 0, 0, 0, 0}, cells = 0;
        double worst[5]   = {0, 0, 0, 0, 0};
        for (int j = 0; j < 5; ++j)
            for (int a = 0; a < 3; ++a)
                for (float dc : {0.85f, 1.0f})
                    for (float tn : {0.0f, 0.5f})
                        for (const Buf* in : {static_cast<const Buf*>(&clk), static_cast<const Buf*>(&nb)}) {
                            Settings x;
                            x.att = a, x.decay = dc, x.tension = tn, x.wear = vs[j];
                            const Stereo o = render(x, *in);
                            const auto m = rv::metrics::compute({o.l, o.r}, kFs);
                            if (m.ringing || m.steadyTone) ++flagged[j];
                            if (!std::isnan(m.ringingDb)) worst[j] = std::max(worst[j], m.ringingDb);
                            if (j == 0) ++cells;
                        }
        std::snprintf(msg, sizeof msg,
                      "Tape wear, M6 Ringing at SPRINGS 3 (%d cells each): flagged / worst ringing_db: none %d / %.1f, A BBD %d / %.1f, "
                      "B %d / %.1f, C1 %d / %.1f, C2 %d / %.1f (want B, C1, C2 no more flagged than none)",
                      cells, flagged[0], worst[0], flagged[1], worst[1], flagged[2], worst[2], flagged[3], worst[3], flagged[4], worst[4]);
        check(flagged[2] <= flagged[0] && flagged[3] <= flagged[0] && flagged[4] <= flagged[0], msg);
    }

    // Cost (desktop). The wear alone (a micro-bench: 10 s of a hot, decaying
    // signal through TapeWear::process, 32-sample blocks as the Tank's grid),
    // and the whole Tank, SPRINGS 3 worst case. Desktop estimates have run
    // ~2x low against the chip: only a chip run (profile build) is trustworthy.
    {
        const int vs[5] = {0, kWearBbd, kWearTapeSat, kWearCrinkle, kWearCrinkleHeavy};
        const char* const nm[5] = {"none", "A BBD", "B tape sat", "C1 crinkle", "C2 crinkle"};
        Buf sig(sec(10.0));
        {
            rv::dsp::Rng rng;
            rng.seed(7u);
            for (size_t i = 0; i < sig.size(); ++i)
                sig[i] = 0.5f * std::exp(-float(i % sec(0.5)) / (0.15f * kFs))
                       * (0.6f * float(std::sin(2 * kPi * 330.0 * double(i) / kFs)) + 0.4f * rng.bipolar());
        }
        double alone[5], tank[5];
        for (int j = 0; j < 5; ++j) {
            double best = 1e30;
            for (int run = 0; run < 5; ++run) {
                rv::dsp::TapeWear w;
                w.prepare(kFs, 0x5EEDu);
                w.setVoicing(vs[j]);
                Buf y = sig;
                const auto t0 = std::chrono::steady_clock::now();
                for (size_t pos = 0; pos + 32 <= y.size(); pos += 32) w.process(y.data() + pos, 32);
                const auto t1 = std::chrono::steady_clock::now();
                volatile float sink = y[y.size() / 2];
                (void)sink;
                best = std::min(best, std::chrono::duration<double, std::nano>(t1 - t0).count() / double(y.size()));
            }
            alone[j] = best;
        }
        const Buf in = hits(6.0);
        for (int j = 0; j < 5; ++j) {
            double best = 1e30;
            for (int run = 0; run < 3; ++run) {
                Settings x;
                x.att = 2, x.drive = 1.0f, x.decay = 1.0f, x.tone = 1.0f, x.tension = 0.0f, x.wear = vs[j];
                rv::Tank t;
                t.prepare(kFs, 48);
                apply(t, x);
                Buf l(in.size()), r(in.size());
                const auto t0 = std::chrono::steady_clock::now();
                for (size_t pos = 0; pos < in.size(); pos += 48)
                    t.process(in.data() + pos, in.data() + pos, l.data() + pos, r.data() + pos, 48);
                const auto t1 = std::chrono::steady_clock::now();
                best = std::min(best, std::chrono::duration<double, std::nano>(t1 - t0).count() / double(in.size()));
            }
            tank[j] = best;
        }
        char line[400] = "";
        for (int j = 0; j < 5; ++j) {
            char one[80];
            std::snprintf(one, sizeof one, "%s%s %.2f ns alone / %.1f ns Tank", j ? "; " : "", nm[j], alone[j], tank[j]);
            std::strncat(line, one, sizeof line - std::strlen(line) - 1);
        }
        std::snprintf(msg, sizeof msg, "Tape wear cost (desktop, per sample; SPRINGS 3 KICKED DRIVE 1 DECAY 1 TENSION 0 for the Tank): %s", line);
        info(msg);
    }
}

// ---- cost -----------------------------------------------------------------------------------
// ---- blend (PROTOTYPE) ------------------------------------------------------------------------
Stereo renderBlend(const Settings& s, int v, const Buf& in, int block, size_t flipAt)
{
    rv::Tank t;
    t.prepare(kFs, block);
    apply(t, s);
    if (v >= 0) t.setEchoSpringsVoicing(v);
    const size_t n = in.size();
    Stereo o{Buf(n), Buf(n)};
    int pos3 = 1; // flipAt: SPRINGS 3 <-> 2 every flipAt samples (switching mid-tail)
    for (size_t pos = 0; pos < n; pos += size_t(block)) {
        if (flipAt > 0) {
            const int want = (pos / flipAt) % 2 == 0 ? 1 : 0;
            if (want != pos3) {
                pos3 = want;
                t.setParam(rv::ParamId::Springs, rv::switchToNormalised(want ? 2 : 1));
            }
        }
        const int m = int(std::min<size_t>(size_t(block), n - pos));
        t.process(in.data() + pos, in.data() + pos, o.l.data() + pos, o.r.data() + pos, m);
    }
    return o;
}

void blend()
{
    static const char* kName[rv::echo::kNumSpringsBlendVoicings] = {"A springs",  "B-wide 50 %", "C-wide 25 %", "D-wide 0 %",
                                                                    "B-pp 50 %", "C-pp 25 %",   "D-pp 0 %"};
    const Buf h = hits(10.0), st = stabs(8.0);
    // The default is C (voicing 2, the owner's pick 6 Oct 2026), bit for bit, every ATTITUDE (hits and stabs).
    {
        bool ok = rv::echo::kSpringsBlendDefault == 2;
        for (int att = 0; att < 3; ++att) {
            Settings s;
            s.att = att, s.drive = 0.5f, s.decay = 0.8f;
            ok = ok && same(renderBlend(s, -1, h), renderBlend(s, 2, h)) && same(renderBlend(s, -1, st), renderBlend(s, 2, st));
        }
        check(ok, "Blend: the default is C (voicing 2), bit for bit (hits + stabs, DECAY 0.8, every ATTITUDE)");
    }
    // SPRINGS 1 and 2: any voicing changes nothing.
    {
        bool ok = true;
        for (int sp = 0; sp < 2; ++sp)
            for (int v = 1; v < rv::echo::kNumSpringsBlendVoicings; ++v) {
                Settings s;
                s.springs = sp, s.decay = 0.7f;
                ok = ok && same(renderBlend(s, 0, h), renderBlend(s, v, h));
            }
        check(ok, "Blend voicings 1-6 in SPRINGS 1 and 2: bit for bit A (the blend only acts in echo mode)");
    }
    for (int v = 1; v < rv::echo::kNumSpringsBlendVoicings; ++v) {
        for (int att = 0; att < 3; ++att) {
            Settings s;
            s.att = att, s.decay = 0.85f, s.tension = 0.6f, s.drive = 0.5f;
            const Stereo a = renderBlend(s, v, h), b = renderBlend(s, v, h), c = renderBlend(s, v, h, 333);
            const Stereo f = renderBlend(s, v, st, 48, sec(1.37));
            double w1 = 0, w2 = 0;
            const int c1 = clicksBoth(a, sec(0.5), &w1), c2 = clicksBoth(f, sec(0.5), &w2);
            std::snprintf(msg, sizeof msg,
                          "Blend %s %s (hits, DECAY 0.85): the same twice %d, block 48 = 333 %d, finite %d, peak %.2f, %d clicks "
                          "(worst %.1f); flipping SPRINGS 3 <-> 2 every 1.37 s on stabs: %d clicks (worst %.1f)",
                          kName[v], att == 0 ? "CLEAN" : (att == 1 ? "DRIVEN" : "KICKED"), int(same(a, b)), int(same(a, c)),
                          int(finite(a) && finite(f)), double(peakOf(a)), c1, w1, c2, w2);
            check(same(a, b) && same(a, c) && finite(a) && finite(f) && peakOf(a) < 1.0f && c1 == 0 && c2 == 0, msg);
        }
    }
    // D: no spring tail. A burst at 0.5 s, DECAY 0 (one repeat, 0.4 s later), MIX 1 (the wet alone):
    // before the repeat and after it D is silent; A rings there.
    {
        const Buf b = burst(2.0, 0.5);
        Settings s;
        s.decay = 0.0f, s.mix = 1.0f;
        const Stereo a = renderBlend(s, 0, b);
        const double aPre = stereoDb(a, sec(0.55), sec(0.88)), aPost = stereoDb(a, sec(1.05), sec(1.6));
        for (int v : {3, 6}) {
            const Stereo d = renderBlend(s, v, b);
            const double pre = stereoDb(d, sec(0.55), sec(0.88)), rep = stereoDb(d, sec(0.9), sec(0.95)),
                         post = stereoDb(d, sec(1.05), sec(1.6));
            std::snprintf(msg, sizeof msg,
                          "Blend %s, no spring tail (burst, DECAY 0, MIX 1): before the repeat %.0f dB, the repeat %.0f dB, after it "
                          "%.0f dB (want before / after under -90 and the repeat over -60; A there: %.0f / %.0f dB)",
                          kName[v], pre, rep, post, aPre, aPost);
            check(pre < -90.0 && post < -90.0 && rep > -60.0 && aPre > -60.0 && aPost > -60.0, msg);
        }
    }
    // Ping-pong: repeat 1 left, 2 right, 3 left ... (D-pp, burst, DECAY noon, 0.4 s).
    {
        const Buf b = burst(4.5, 0.5);
        Settings s;
        s.mix = 1.0f;
        const Stereo d = renderBlend(s, 6, b);
        bool ok = true;
        std::string lr;
        for (int k = 1; k <= 6; ++k) {
            const size_t t0 = sec(0.5 + 0.4 * k - 0.02), t1 = sec(0.5 + 0.4 * k + 0.1);
            const double l = db(power(d.l, t0, t1)), r = db(power(d.r, t0, t1));
            const double lead = (k % 2 ? l - r : r - l);
            ok = ok && lead > 40.0;
            char one[48];
            std::snprintf(one, sizeof one, " %d:%s %+.0f dB", k, k % 2 ? "L" : "R", lead);
            lr += one;
        }
        std::snprintf(msg, sizeof msg, "Blend D-pp alternates (burst, DECAY noon): each repeat's side over the other by%s (want > 40)",
                      lr.c_str());
        check(ok, msg);
    }
    // Wide: R's highs kWideMs behind L's (cross-correlation of the first repeat, above ~1 kHz).
    {
        const Buf b = burst(1.5, 0.5);
        Settings s;
        s.mix = 1.0f;
        const Stereo d = renderBlend(s, 3, b);
        Buf l(d.l.begin() + long(sec(0.85)), d.l.begin() + long(sec(1.05)));
        Buf r(d.r.begin() + long(sec(0.85)), d.r.begin() + long(sec(1.05)));
        for (Buf* x : {&l, &r}) { // crude high-pass: x - LP(x)
            Buf lo = *x;
            lowpass(lo, 1000.0);
            for (size_t i = 0; i < x->size(); ++i) (*x)[i] -= lo[i];
        }
        int    best  = 0;
        double bestC = -1e30;
        for (int lag = -int(sec(0.02)); lag <= int(sec(0.02)); ++lag) {
            double c = 0;
            for (size_t i = 0; i < l.size(); ++i) {
                const long j = long(i) + lag;
                if (j >= 0 && j < long(r.size())) c += double(l[i]) * r[size_t(j)];
            }
            if (c > bestC) bestC = c, best = lag;
        }
        const double ms = 1000.0 * best / kFs;
        std::snprintf(msg, sizeof msg, "Blend D-wide: R's highs %.2f ms behind L's (want %.1f +- 0.5)", ms, double(rv::echo::kWideMs));
        check(std::fabs(ms - rv::echo::kWideMs) <= 0.5, msg);
    }
    // Mono fold-down (reported): K-weighted mono vs stereo loudness, the wet alone, hits DECAY noon.
    {
        Settings s;
        s.mix = 1.0f;
        std::string out;
        for (int v = 0; v < rv::echo::kNumSpringsBlendVoicings; ++v) {
            const Stereo o = renderBlend(s, v, h);
            char one[80];
            std::snprintf(one, sizeof one, " %s %+.1f dB (stereo %.1f);", kName[v], monoLoudnessDb(o) - loudnessDb(o), loudnessDb(o));
            out += one;
        }
        std::snprintf(msg, sizeof msg, "Blend mono fold-down, the wet alone (hits, MIX 1, DECAY noon): (L+R)/2 vs stereo loudness:%s",
                      out.c_str());
        info(msg);
    }
    // Cost (desktop, reported): KICKED, DRIVE 1, DECAY 1, TENSION 0, as "cost".
    {
        const Buf in = hits(6.0);
        auto ns = [&](int v) {
            double best = 1e30;
            for (int run = 0; run < 5; ++run) {
                rv::Tank t;
                t.prepare(kFs, 48);
                Settings s;
                s.att = 2, s.drive = 1.0f, s.decay = 1.0f, s.tone = 1.0f, s.tension = 0.0f;
                apply(t, s);
                t.setEchoSpringsVoicing(v);
                Buf l(in.size()), r(in.size());
                const auto t0 = std::chrono::steady_clock::now();
                for (size_t pos = 0; pos < in.size(); pos += 48)
                    t.process(in.data() + pos, in.data() + pos, l.data() + pos, r.data() + pos, 48);
                const auto t1 = std::chrono::steady_clock::now();
                best = std::min(best, std::chrono::duration<double, std::nano>(t1 - t0).count() / double(in.size()));
            }
            return best;
        };
        const double a = ns(0), w = ns(2), pp = ns(6);
        std::snprintf(msg, sizeof msg,
                      "Blend cost (desktop, KICKED, DRIVE 1, DECAY 1, TENSION 0): A %.1f ns/sample, C (shipped, wide) %.1f (%+.1f %%), ping-pong "
                      "%.1f (%+.1f %%)",
                      a, w, 100.0 * (w / a - 1.0), pp, 100.0 * (pp / a - 1.0));
        info(msg);
    }
}

// ---- blendlevel: the shipped blend (C) as loud as A was --------------------------------------
// The owner picked C on a level-matched page, so C must be as loud as A
// (K-weighted, stereo), in every ATTITUDE, across MIX, DECAY, TENSION and
// DRIVE, on hits, stabs and a held pad. Each cell C - A; per ATTITUDE the
// mean within +-1 dB and every cell within +-4 dB (MIX 1, the wet alone,
// is the hardest: below it the dry carries the level). What is left is the
// material: without most of the springs' splash a hit's wet is ~1-2 dB
// under A's, and a held pad's overlapping direct repeats ~1-3 dB over it.
void blendLevelOrBalance(bool level)
{
    const Buf h = hits(10.0), st = stabs(8.0);
    Buf pad(sec(9.0), 0.0f); // a held C minor pad, 4 s
    for (double f : {130.81, 155.56, 196.0, 261.63})
        for (size_t i = 0; i < sec(4.0); ++i) {
            const double t = double(i) / kFs, env = std::min({1.0, t / 0.4, (4.0 - t) / 0.8});
            pad[sec(1.0) + i] += float(env * (2 * std::fmod(f * t, 1.0) - 1) / 4);
        }
    lowpass(pad, 1800.0);
    normalise(pad, -9.0f);
    const Buf* mats[3]       = {&h, &st, &pad};
    const char* matName[3]   = {"hits", "stabs", "pad"};
    const char* attName[3]   = {"CLEAN", "DRIVEN", "KICKED"};
    for (int att = 0; level && att < 3; ++att) {
        double sum = 0, worst = 0, sumWet = 0;
        int    cells = 0, cellsWet = 0;
        char   worstAt[120] = "";
        auto cell = [&](float mix, float decay, float tension, float drive, int m) {
            Settings s;
            s.att = att, s.mix = mix, s.decay = decay, s.tension = tension, s.drive = drive;
            const double d = loudnessDb(renderBlend(s, rv::echo::kSpringsBlendDefault, *mats[m])) - loudnessDb(renderBlend(s, 0, *mats[m]));
            sum += d, ++cells;
            if (mix >= 1.0f) sumWet += d, ++cellsWet;
            if (std::getenv("RV_BLEND_VERBOSE"))
                std::printf("INFO  %s MIX %.1f DECAY %.1f TENSION %.2f DRIVE %.1f %s: C - A %+.2f dB\n", attName[att], double(mix),
                            double(decay), double(tension), double(drive), matName[m], d);
            if (std::fabs(d) > std::fabs(worst)) {
                worst = d;
                std::snprintf(worstAt, sizeof worstAt, "MIX %.1f DECAY %.1f TENSION %.2f DRIVE %.1f %s", double(mix), double(decay),
                              double(tension), double(drive), matName[m]);
            }
        };
        for (int m = 0; m < 3; ++m) {
            for (float decay : {0.0f, 0.5f, 0.8f})
                for (float tension : {0.25f, 0.5f, 0.75f})
                    for (float drive : {0.0f, 0.5f, 1.0f}) cell(1.0f, decay, tension, drive, m);
            for (float mix : {0.3f, 0.6f}) cell(mix, 0.5f, 0.5f, 0.25f, m);
        }
        const double mean = sum / cells, meanWet = sumWet / cellsWet;
        std::snprintf(msg, sizeof msg,
                      "Blend level %s: C - A K-weighted over %d cells (MIX 1: DECAY 0 / 0.5 / 0.8 x TENSION 0.25 / 0.5 / 0.75 x DRIVE "
                      "0 / 0.5 / 1; MIX 0.3, 0.6) x hits / stabs / pad: mean %+.2f dB (MIX 1 %+.2f; want within +-1), worst %+.2f "
                      "at %s (want within +-4)",
                      attName[att], cells, mean, meanWet, worst, worstAt);
        check(std::fabs(mean) <= 1.0 && std::fabs(meanWet) <= 1.0 && std::fabs(worst) <= 4.0, msg);
    }
    if (level) return;
    // L / R balance, the wet alone: the wide heads on their own (D-wide, no
    // springs) centred, on average over the three within +-0.3 dB, each within
    // +-1; the shipped C and A reported (the springs' own image leans a little,
    // differently per material).
    {
        std::string out;
        double sumD = 0, worstD = 0;
        for (int m = 0; m < 3; ++m) {
            double b[3];
            const int vs[3] = {0, rv::echo::kSpringsBlendDefault, 3};
            for (int k = 0; k < 3; ++k) {
                Settings s;
                const Stereo o = renderBlend(s, vs[k], *mats[m]);
                b[k] = db(power(o.l, 0, SIZE_MAX)) - db(power(o.r, 0, SIZE_MAX));
            }
            sumD += b[2] / 3.0;
            worstD = std::max(worstD, std::fabs(b[2]));
            char one[96];
            std::snprintf(one, sizeof one, " %s heads %+.2f, C %+.2f, A %+.2f;", matName[m], b[2], b[1], b[0]);
            out += one;
        }
        std::snprintf(msg, sizeof msg,
                      "Blend L - R level, the wet alone (CLEAN, DECAY noon):%s dB; the heads' mean %+.2f (want within +-0.3, each "
                      "within +-1)",
                      out.c_str(), sumD);
        check(std::fabs(sumD) <= 0.3 && worstD <= 1.0, msg);
    }
}

void blendLevel() { blendLevelOrBalance(true); }
void blendBalance() { blendLevelOrBalance(false); }

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
                                {"diffuse", diffuse}, {"wear", wear}, {"bbd", bbd}, {"bits", bits}, {"tapewear", tapeWear}, {"blend", blend}, {"blendlevel", blendLevel}, {"blendbalance", blendBalance}, {"cost", cost}};
    for (const auto& s : sections) {
        if (only && std::strcmp(only, s.name) != 0) continue;
        std::printf("== %s\n", s.name);
        s.fn();
    }
    std::printf("%s (%d failure%s)\n", failures ? "FAILED" : "ALL PASSED", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
