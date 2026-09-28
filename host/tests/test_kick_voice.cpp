// KickVoice tests (SPEC §4.6, §7 M7; ADRs 0005, 0013, 0016). Stand-alone:
// the Kick voice alone, and fed into one real Spring (as the Tank will feed
// it after integration, docs/m7-integration.md), not the Tank.
//
// "Low end": energy below 100 Hz = output through two 2nd-order 100 Hz
// low-passes (24 dB/oct), in 50 ms windows. "Onset" = first non-zero sample.

#include "dsp/Filters.h"
#include "dsp/Kick.h"
#include "dsp/Spring.h"
#include "params/Mappings.h"
#include "params/SplashVoicing.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
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

struct Out {
    Buf loop, direct;
    std::vector<long> jolts; // absolute sample of each joltOffset() report
    int started = 0;
};

// Kicks at absolute samples `at`; block sizes cycle through `blocks`.
Out render(const std::vector<long>& at, size_t length, std::vector<int> blocks, int attitude = 2, float fs = kFs)
{
    rv::dsp::KickVoice k;
    k.prepare(fs, 11u);
    k.setAttitude(att(attitude));
    Out o{Buf(length), Buf(length), {}, 0};
    size_t bi = 0;
    for (size_t pos = 0; pos < length;) {
        const int n = int(std::min(size_t(blocks[bi++ % blocks.size()]), length - pos));
        for (long a : at)
            if (a >= long(pos) && a < long(pos) + n) k.trigger(int(a - long(pos)));
        k.process(o.loop.data() + pos, o.direct.data() + pos, n);
        if (k.joltOffset() >= 0) o.jolts.push_back(long(pos) + k.joltOffset());
        pos += size_t(n);
    }
    o.started = k.kicksStarted();
    return o;
}

size_t firstNonZero(const Buf& x)
{
    for (size_t i = 0; i < x.size(); ++i)
        if (x[i] != 0.0f) return i;
    return x.size();
}

Buf lowBand(const Buf& x, float fs, float hz = 100.0f)
{
    rv::dsp::Biquad a, b;
    a.setLowpass(hz, 0.707f, fs);
    b.setLowpass(hz, 0.707f, fs);
    Buf y(x.size());
    for (size_t i = 0; i < x.size(); ++i) y[i] = b.process(a.process(x[i]));
    return y;
}
Buf highBand(const Buf& x, float fs, float hz)
{
    rv::dsp::Biquad a, b;
    a.setHighpass(hz, 0.707f, fs);
    b.setHighpass(hz, 0.707f, fs);
    Buf y(x.size());
    for (size_t i = 0; i < x.size(); ++i) y[i] = b.process(a.process(x[i]));
    return y;
}
double energy(const Buf& x, size_t from, size_t to)
{
    double s = 0;
    for (size_t i = from; i < std::min(to, x.size()); ++i) s += double(x[i]) * x[i];
    return s;
}
double db(double p) { return 10.0 * std::log10(p + 1e-30); }

// Largest < 100 Hz energy in any 50 ms window starting at or after `from`.
double worstWindow(const Buf& low, size_t from, float fs)
{
    const size_t w = size_t(0.05f * fs);
    double worst = 0;
    for (size_t s = from; s + w <= low.size(); s += w / 2) worst = std::max(worst, energy(low, s, s + w));
    return worst;
}

// One Spring at DECAY max (longest, most resonant Loop), KICKED LoopSat.
// wet = Spring(loopFeed) + direct: the Tank after integration, minus the
// Tank-level output stages.
Buf throughSpring(const Buf& loopFeed, const Buf& direct, float fs)
{
    rv::Spring s;
    std::vector<float> pool(rv::Spring::requiredFloats(fs));
    s.prepare(fs, pool.data(), 0x9E3779B9u);
    rv::SpringSettings st;
    st.loopDelaySeconds = rv::map::decayLoopDelaySeconds(1.0f);
    st.t60Seconds       = rv::map::decayT60Seconds(1.0f);
    st.transitionHz     = rv::map::decayTransitionHz(1.0f);
    st.allpassCoeff     = rv::map::boingCoefficient(0.5f);
    st.stages           = rv::map::boingStages(0.5f);
    st.dampingHz        = rv::map::toneDampingHz(0.5f);
    st.highPathLevel    = rv::map::toneHighPathLevel(0.5f);
    st.loopSatAmount    = 1.0f;
    st.loopSatKPos      = 1.6f;
    st.loopSatKNeg      = 2.6f;
    s.setSettings(st, true);
    Buf out(loopFeed.size());
    for (size_t pos = 0; pos < out.size(); pos += 32) {
        const int n = int(std::min<size_t>(32, out.size() - pos));
        s.process(loopFeed.data() + pos, out.data() + pos, n);
    }
    for (size_t i = 0; i < out.size(); ++i) out[i] += direct[i];
    return out;
}

} // namespace

int main()
{
    using namespace rv;
    const size_t len = size_t(2.0f * kFs);

    // ---- Sample-accurate onset, block-size independent ------------------------------
    {
        const long at = 12345;
        const Out ref = render({at}, len, {48});
        bool exact = true, same = true;
        for (int b : {1, 7, 32, 48, 333, 1024}) {
            const Out o = render({at}, len, {b});
            exact &= firstNonZero(o.loop) == size_t(at) && firstNonZero(o.direct) == size_t(at) && o.jolts.size() == 1 &&
                     o.jolts[0] == at;
            same &= o.loop == ref.loop && o.direct == ref.direct;
        }
        check(exact, "Kick onset on the exact sample N (both outputs, and the forceJolt offset) for blocks 1, 7, 32, 48, 333, 1024");
        check(same, "Kick output bit-identical for every block size");
        // Clamp: an offset past the block end lands on the block's last sample, not dropped (as Tank::kick).
        rv::dsp::KickVoice k;
        k.prepare(kFs, 11u);
        Buf a(64), b(64);
        k.trigger(1000);
        k.process(a.data(), b.data(), 64);
        check(firstNonZero(a) == 63 && k.kicksStarted() == 1, "offset beyond the block is clamped to its last sample, not dropped");
    }

    // ---- Thump + burst shape --------------------------------------------------------------
    {
        const Out o = render({long(0.1f * kFs)}, len, {48});
        // Thump frequency from zero crossings in its first 60 ms.
        const size_t s0 = size_t(0.1f * kFs);
        int crossings = 0;
        for (size_t i = s0 + 1; i < s0 + size_t(0.06f * kFs); ++i) crossings += (o.direct[i - 1] < 0) != (o.direct[i] < 0);
        const float hz = float(crossings) / 2.0f / 0.06f;
        // Decay: peak in 5 ms windows, time to fall 40 dB.
        float p0 = 0;
        size_t t40 = 0;
        for (size_t w = 0; w < 60; ++w) {
            float p = 0;
            for (size_t i = s0 + w * 240; i < s0 + (w + 1) * 240; ++i) p = std::max(p, std::fabs(o.direct[i]));
            if (w == 0) p0 = p;
            if (!t40 && p < 0.01f * p0) t40 = w * 5;
        }
        const float tau = float(t40) / 4.6f;
        std::snprintf(msg, sizeof msg, "KICKED thump: %.0f Hz average over 60 ms (40..80), -40 dB after %zu ms (1/e ~%.0f ms, 20..40)", hz,
                      t40, tau);
        check(hz >= 40.0f && hz <= 80.0f && tau >= 18.0f && tau <= 42.0f, msg);
        // Burst: > 1 kHz part of the loop feed, 90 % of its energy within 10 ms.
        const Buf hi = highBand(o.loop, kFs, 1000.0f);
        const double all = energy(hi, s0, s0 + size_t(0.2f * kFs)), first = energy(hi, s0, s0 + size_t(0.010f * kFs));
        std::snprintf(msg, sizeof msg, "broadband burst ~10 ms: %.1f %% of the > 1 kHz energy within 10 ms", 100.0 * first / all);
        check(first > 0.9 * all, msg);
    }

    // ---- Tight low end (ADR 0016) ------------------------------------------------------------
    {
        const size_t s0 = size_t(0.1f * kFs), w = size_t(0.05f * kFs);
        const Out o = render({long(s0)}, len, {48});
        Buf sum(len);
        for (size_t i = 0; i < len; ++i) sum[i] = o.loop[i] + o.direct[i];
        const Buf low = lowBand(sum, kFs);
        const double e0 = energy(low, s0, s0 + w), e300 = worstWindow(low, s0 + size_t(0.3f * kFs), kFs);
        std::snprintf(msg, sizeof msg, "voice alone: < 100 Hz energy down %.0f dB 300 ms after the Kick (>= 20)", db(e0 / e300));
        check(db(e0 / e300) >= 20.0, msg);

        // Loop feed vs direct below 100 Hz: the Kick-path high-pass keeps the thump out of the Loop.
        const double loopLow = energy(lowBand(o.loop, kFs), s0, s0 + size_t(0.2f * kFs));
        const double directLow = energy(lowBand(o.direct, kFs), s0, s0 + size_t(0.2f * kFs));
        std::snprintf(msg, sizeof msg, "Kick-path high-pass: loop feed carries %.1f dB less < 100 Hz energy than the direct thump (>= 12)",
                      db(directLow / loopLow));
        check(db(directLow / loopLow) >= 12.0, msg);

        // Through a real Spring at DECAY max (9 s tail) + direct thump.
        const Buf wet = throughSpring(o.loop, o.direct, kFs);
        const Buf wlow = lowBand(wet, kFs);
        const double we0 = energy(wlow, s0, s0 + w), we300 = worstWindow(wlow, s0 + size_t(0.3f * kFs), kFs);
        // For contrast: the whole thump fed into the Loop (no split, the M2 placeholder path).
        const Buf none(len, 0.0f);
        const Buf naive = lowBand(throughSpring(sum, none, kFs), kFs);
        const double ne0 = worstWindow(naive, s0, kFs), ne300 = worstWindow(naive, s0 + size_t(0.3f * kFs), kFs);
        std::snprintf(msg, sizeof msg,
                      "through a Spring at DECAY max, KICKED: < 100 Hz energy down %.1f dB within 300 ms (>= 20); whole thump into the Loop instead: %.1f dB",
                      db(we0 / we300), db(ne0 / ne300));
        check(db(we0 / we300) >= 20.0, msg);
    }

    // ---- ATTITUDE scaling, fixed strength ---------------------------------------------------------
    {
        double e[3];
        for (int a = 0; a < 3; ++a) {
            const Out o = render({4800}, len, {48}, a);
            e[a] = energy(o.loop, 0, len) + energy(o.direct, 0, len);
        }
        std::snprintf(msg, sizeof msg, "scaled by ATTITUDE: Kick energy CLEAN %.1f dB, DRIVEN %.1f dB re KICKED", db(e[0] / e[2]), db(e[1] / e[2]));
        check(e[0] < e[1] && e[1] < e[2], msg);
        const Out a = render({4800}, len, {48}), b = render({4800 + 9600}, len + 9600, {48});
        // The burst noise differs from Kick to Kick (it runs on); the thump never does.
        double ea = energy(a.direct, 0, len), eb = energy(b.direct, 0, len + 9600);
        std::snprintf(msg, sizeof msg, "fixed strength: every Kick's thump is identical (energy %.6f vs %.6f)", ea, eb);
        check(std::fabs(ea - eb) < 1e-6 * ea, msg);
    }

    // ---- Gate trains: exactly one Kick per edge ------------------------------------------------------
    {
        // 12 gates/s for 3 s (16ths at 180 bpm), irregular block sizes.
        std::vector<long> gates;
        for (int g = 0; g < 36; ++g) gates.push_back(long(0.05f * kFs) + long(float(g) * kFs / 12.0f));
        const size_t n = size_t(3.2f * kFs);
        const Out o = render(gates, n, {48, 17, 256, 1, 999, 32});
        bool onTime = o.jolts.size() == gates.size();
        for (size_t g = 0; onTime && g < gates.size(); ++g) onTime &= o.jolts[g] == gates[g];
        // Each gate: output energy in the 1 ms after the edge, nothing new before it.
        bool oneEach = true;
        for (long g : gates) oneEach &= firstNonZero(Buf(o.loop.begin() + g, o.loop.begin() + g + 48)) == 0;
        std::snprintf(msg, sizeof msg, "12 gates/s, irregular blocks: %d Kicks for %zu gates, every onset on its gate sample", o.started,
                      gates.size());
        check(o.started == int(gates.size()) && onTime && oneEach, msg);

        // Several gates inside one big block are all kept, in order.
        const Out big = render(gates, n, {4096});
        check(big.started == int(gates.size()), "12 gates/s in 4096-sample blocks (several per block): one Kick each");

        // Bounce: two edges 2 ms apart are one Kick; 6 ms apart are two.
        const Out bounce = render({4800, 4800 + 96}, len, {48});
        const Out two = render({4800, 4800 + 288}, len, {48});
        std::snprintf(msg, sizeof msg, "bounce guard: edges 2 ms apart -> %d Kick, 6 ms apart -> %d Kicks", bounce.started, two.started);
        check(bounce.started == 1 && two.started == 2, msg);
    }

    // ---- Determinism, reset, sample rate --------------------------------------------------------------
    {
        const Out a = render({1000, 30000}, len, {48}), b = render({1000, 30000}, len, {48});
        check(a.loop == b.loop && a.direct == b.direct, "deterministic: same triggers, same output");
        rv::dsp::KickVoice k;
        k.prepare(kFs, 11u);
        k.setAttitude(att(2));
        Buf l1(len), d1(len), l2(len), d2(len);
        k.trigger(1000);
        k.process(l1.data(), d1.data(), int(len));
        k.reset();
        k.trigger(1000);
        k.process(l2.data(), d2.data(), int(len));
        check(l1 == l2 && d1 == d2, "reset() replays identically");

        auto thumpHz = [](float fs) {
            const Out o = render({long(0.1f * fs)}, size_t(fs), {48}, 2, fs);
            int c = 0;
            const size_t s0 = size_t(0.1f * fs);
            for (size_t i = s0 + 1; i < s0 + size_t(0.06f * fs); ++i) c += (o.direct[i - 1] < 0) != (o.direct[i] < 0);
            return float(c) / 2.0f / 0.06f;
        };
        std::snprintf(msg, sizeof msg, "sample-rate aware: thump %.0f Hz at 48 kHz, %.0f Hz at 96 kHz", thumpHz(48000.0f), thumpHz(96000.0f));
        check(std::fabs(thumpHz(48000.0f) - thumpHz(96000.0f)) < 2.0f, msg);
    }

    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
