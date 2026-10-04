// Kick through the whole Tank (SPEC §4.6, §7 M2 + M7; ADR 0005, 0013, 0016;
// docs/m7-integration.md).
//
// M2 checked "Kick at N == input impulse at N". Since M7 the Kick is no
// longer an input impulse (it is a thump + burst injected after the drive,
// plus a forced Splash), so the timing contract is:
//   1. onset sample-accurate: the first output sample that differs from the
//      same render without the Kick is at N + a fixed wet latency (the
//      DriveOut oversampler), identical for every block size;
//   2. the whole Kick render is bit-identical for every block size;
//   3. offsets past the block end clamp to the last sample.
// Plus the M7 criteria at Tank level: < 100 Hz energy down >= 20 dB within
// 300 ms at DECAY max (every ATTITUDE), and a 12/s gate train gives exactly
// one Kick (and one forced Splash stroke) per edge, on its exact sample.
//
// "Onset" is measured against a no-Kick render: the Springs' -200 dB
// denormal guard noise means the wet is never exactly 0, but two renders
// that differ only by the Kick are identical until the Kick arrives.

#include "dsp/Filters.h"
#include "dsp/Tank.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
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
constexpr float kFs     = 48000.0f;
constexpr int   kLength = 48000;
constexpr int   kKickAt = 12345;

struct Settings {
    float decay = 0.5f, attitude = 0.5f, splash = 0.3f, wobble = 0.45f, springs = 0.5f;
    float tone = -1.0f; // < 0: the ParamSpec default
    int   outBits = -1;  // output_bits_voicing: -1 the default (ADR 0042's mu-law box), 0 before the box (a test hook)
};

struct Out {
    Buf l, r;
    std::vector<long> starts; // absolute samples where a Kick started (block 1 renders)
};

// Silence in, MIX 1, Kicks at the given absolute samples, fixed block size.
Out render(int block, const std::vector<long>& kicks, size_t length, const Settings& st, bool track = false)
{
    rv::Tank tank;
    tank.prepare(kFs, block);
    tank.setParam(rv::ParamId::Mix, 1.0f);
    tank.setParam(rv::ParamId::Decay, st.decay);
    tank.setParam(rv::ParamId::Attitude, st.attitude);
    tank.setParam(rv::ParamId::Splash, st.splash);
    tank.setParam(rv::ParamId::Wobble, st.wobble);
    tank.setParam(rv::ParamId::Springs, st.springs);
    if (st.tone >= 0.0f) tank.setParam(rv::ParamId::Tone, st.tone);
    if (st.outBits >= 0) tank.setOutputBitsVoicing(st.outBits);
    Buf in(length, 0.0f);
    Out o{Buf(length), Buf(length), {}};
    int started = 0;
    for (size_t pos = 0; pos < length; pos += size_t(block)) {
        const int n = int(std::min(size_t(block), length - pos));
        for (long k : kicks)
            if (k >= long(pos) && k < long(pos) + n) tank.kick(int(k - long(pos)));
        tank.process(in.data() + pos, in.data() + pos, o.l.data() + pos, o.r.data() + pos, n);
        if (track && tank.kickVoice().kicksStarted() != started) {
            started = tank.kickVoice().kicksStarted();
            o.starts.push_back(long(pos));
        }
    }
    return o;
}

size_t firstDifference(const Out& a, const Out& b)
{
    for (size_t i = 0; i < a.l.size(); ++i)
        if (a.l[i] != b.l[i] || a.r[i] != b.r[i]) return i;
    return a.l.size();
}

Buf lowBand(const Buf& x, float hz = 100.0f)
{
    rv::dsp::Biquad a, b;
    a.setLowpass(hz, 0.707f, kFs);
    b.setLowpass(hz, 0.707f, kFs);
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
// Largest energy in any 50 ms window (hop 25 ms) starting at or after `from`.
double worstWindow(const Buf& x, size_t from)
{
    const size_t w = size_t(0.05f * kFs);
    double worst = 0;
    for (size_t s = from; s + w <= x.size(); s += w / 2) worst = std::max(worst, energy(x, s, s + w));
    return worst;
}

const char* kAttName[3] = {"CLEAN", "DRIVEN", "KICKED"};

} // namespace

int main()
{
    // ---- 1 + 2. Onset at N + fixed latency, bit-identical for every block size ----
    {
        const Settings st;
        const Out silent = render(48, {}, kLength, st);
        const Out ref    = render(48, {kKickAt}, kLength, st);
        const size_t onsetRef = firstDifference(ref, silent);
        const long   latency  = long(onsetRef) - kKickAt;
        std::snprintf(msg, sizeof msg, "Kick at N=%d: onset at N + %ld samples (wet latency, fixed; limit 48 = 1 ms)", kKickAt,
                      latency);
        check(latency >= 0 && latency <= 48, msg);

        bool allOnset = true, allSame = true;
        for (int block : {1, 7, 32, 48, 128, 512, 1024}) {
            const Out a = render(block, {kKickAt}, kLength, st);
            const Out s = render(block, {}, kLength, st);
            const long onset = long(firstDifference(a, s)) - kKickAt;
            const bool same  = a.l == ref.l && a.r == ref.r;
            allOnset &= onset == latency;
            allSame &= same;
            std::printf("      block %4d: onset N + %ld, output %s block 48\n", block, onset, same ? "==" : "!=");
        }
        check(allOnset, "Kick onset at N + the same fixed latency for blocks 1, 7, 32, 48, 128, 512, 1024");
        check(allSame, "Kick output bit-identical for blocks 1, 7, 32, 48, 128, 512, 1024");
    }

    // ---- 3. Offsets past the block end clamp to the last sample instead of vanishing ----
    {
        rv::Tank tank;
        tank.prepare(kFs, 64);
        tank.setParam(rv::ParamId::Mix, 1.0f);
        Buf in(size_t(kLength), 0.0f), out(in.size(), 0.0f), outR(in.size(), 0.0f);
        tank.kick(1000);
        for (int pos = 0; pos < kLength; pos += 64) tank.process(in.data() + pos, in.data() + pos, out.data() + pos, outR.data() + pos, 64);
        double peak = 0.0;
        for (float v : out) peak = std::max(peak, double(std::fabs(v)));
        std::snprintf(msg, sizeof msg, "Kick offset beyond the block is clamped, not dropped (%d Kick started, peak %.2f)",
                      tank.kickVoice().kicksStarted(), peak);
        check(tank.kickVoice().kicksStarted() == 1 && peak > 0.01, msg);
    }

    // ---- 4. Tight low end through the Tank (ADR 0016, SPEC §7 M7) ----------------------------
    // DECAY max (the longest Loop), every ATTITUDE, 2 Springs. < 100 Hz of the
    // mono sum: the 50 ms window at the Kick vs the loudest 50 ms window from
    // 300 ms on. KICKED at DECAY max is inside the Howl zone (a Kick there
    // starts a Howl); KICKED is also checked just below the zone, DECAY 0.88.
    // At the default TONE and right of noon (0.85, 1): since the Big Knob
    // moved after the Springs (ADR 0036 amendment, 4 Oct 2026) it thins the
    // Kick's direct thump with the wet, and its makeup swells the tail back.
    // Read before the output's mu-law box (ADR 0042; output_bits_voicing 0 as a
    // test hook): this is the Tank's low end. The box's grain follows the whole
    // output's level, broadband, so in KICKED (10-bit since 5 Oct 2026, ~45 dB under the signal; 8-bit ~33)
    // with TONE's low cut thinning the thump, the grain of the still-ringing
    // highs fills the < 100 Hz band after 300 ms; printed as INFO.
    for (float tone : {-1.0f, 0.85f, 1.0f}) {
        const size_t len = size_t(3.0f * kFs), s0 = size_t(0.1f * kFs), w = size_t(0.05f * kFs);
        struct Case { int att; float decay; bool required; };
        const Case cases[] = {{0, 1.0f, true}, {1, 1.0f, true}, {2, 0.88f, true}, {2, 1.0f, true}};
        for (const Case& c : cases) {
            Settings st;
            st.decay    = c.decay;
            st.tone     = tone;
            st.attitude = rv::switchToNormalised(c.att);
            auto lowDrop = [&](int outBits) {
                Settings sb = st;
                sb.outBits  = outBits;
                const Out o = render(48, {long(s0)}, len, sb);
                Buf mono(len);
                for (size_t i = 0; i < len; ++i) mono[i] = 0.5f * (o.l[i] + o.r[i]);
                const Buf low = lowBand(mono);
                return db(energy(low, s0, s0 + w) / worstWindow(low, s0 + size_t(0.3f * kFs)));
            };
            const double drop = lowDrop(0), shipped = lowDrop(-1);
            char toneTxt[24] = "";
            if (tone >= 0.0f) std::snprintf(toneTxt, sizeof toneTxt, " TONE %.2f", tone);
            std::snprintf(msg, sizeof msg,
                          "%s DECAY %.2f%s: Kick < 100 Hz energy down %.1f dB within 300 ms before the output box (>= 20)%s; INFO with "
                          "the box %.1f dB",
                          kAttName[c.att], c.decay, toneTxt, drop, c.required ? "" : " [Howl zone: report only]", shipped);
            if (c.required) check(drop >= 20.0, msg);
            else std::printf("INFO  %s\n", msg);
        }
    }

    // ---- 5. Gate train at 12/s: exactly one Kick per edge, on its sample -------------------
    {
        std::vector<long> gates;
        const long period = long(kFs / 12.0f); // 4000 samples: 16ths at 180 bpm
        for (long t = 2000; t < long(3.0f * kFs) + 2000; t += period) gates.push_back(t);
        const size_t len = size_t(3.5f * kFs);
        for (int att = 0; att < 3; ++att) {
            Settings st;
            st.attitude = rv::switchToNormalised(att);
            st.splash   = 0.0f; // silence in, so every Splash stroke is a forced one
            const Out o = render(1, gates, len, st, true);
            bool exact = o.starts.size() == gates.size();
            for (size_t i = 0; exact && i < gates.size(); ++i) exact = o.starts[i] == gates[i];
            std::snprintf(msg, sizeof msg, "%s: 12 Kicks/s for 3 s -> %zu Kicks started (want %zu), each on its gate sample: %s",
                          kAttName[att], o.starts.size(), gates.size(), exact ? "yes" : "no");
            check(exact, msg);
        }
        // Block-size independence of the whole train (and the forced Splash strokes).
        Settings st;
        st.attitude = 1.0f;
        const Out a = render(48, gates, len, st), b = render(333, gates, len, st), c = render(7, gates, len, st);
        check(a.l == b.l && a.r == b.r && a.l == c.l && a.r == c.r, "12/s Kick train, KICKED: bit-identical for blocks 7, 48, 333");
        rv::Tank t;
        t.prepare(kFs, 48);
        t.setParam(rv::ParamId::Attitude, 1.0f);
        t.setParam(rv::ParamId::Splash, 0.0f);
        Buf in(len, 0.0f), l(len), r(len);
        for (size_t pos = 0; pos < len; pos += 48) {
            for (long g : gates)
                if (g >= long(pos) && g < long(pos) + 48) t.kick(int(g - long(pos)));
            t.process(in.data() + pos, in.data() + pos, l.data() + pos, r.data() + pos, 48);
        }
        std::snprintf(msg, sizeof msg, "12/s Kick train: %d forced Splash strokes for %zu Kicks (one each)",
                      t.splash().strokeCount(), gates.size());
        check(t.splash().strokeCount() == int(gates.size()), msg);
    }

    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
