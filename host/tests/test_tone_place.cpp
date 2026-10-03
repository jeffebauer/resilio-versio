// TONE placement prototype (DriveVoicing.h "TONE placement"; docs/prototypes/
// tone-place/; docs/research/dub-lens-critique.md §3.2, direction B).
// Renderer-only key tone_place_voicing: 0 = the Big Knob before the Springs
// (today), 1 = on the wet (the return), 2 = split (1st-order section before,
// 2nd-order section on the wet).
//   identical  placement 0 is today's Tank, bit for bit (hits, skank; CLEAN /
//              KICKED; TONE 0.3 / 0.85 / 1 and a TONE sweep), and placements
//              1-2 are too at TONE <= 0.5.
//   loudness   hits / skank / held chords at TONE 0.7 / 0.85 / 1 within ±3 dB
//              of noon, CLEAN / DRIVEN / KICKED, the owner's settings (2
//              Springs, DECAY 0.6, TENSION noon, SPLASH 0.3, DRIVE 0.25, MIX 1).
//   clicks     a fast TONE move (noon -> 1 -> noon, one step each, the 5 ms
//              smoothing only) on a ringing tail: no clicks.
//   thins      how much the tail's lows (< 250 Hz) drop 0.25 s after that
//              move, per placement (INFO: post / split act on the tail now).
//   cpu        the whole Tank at TONE 1, ns/sample, per placement (INFO).

#include "Wav.h"
#include "dsp/Tank.h"
#include "params/DriveVoicing.h"
#include "params/Mappings.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <future>
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
constexpr float kFs = 48000.0f;
const char* const kAttName[3] = {"CLEAN", "DRIVEN", "KICKED"};
const char* const kPlaceName[3] = {"0 pre", "1 post", "2 split"};

struct Stereo {
    Buf l, r;
};

struct Settings {
    int   att = 0, place = 0;
    float tone = 0.5f, drive = 0.25f;
};

void apply(rv::Tank& t, const Settings& s)
{
    t.setParam(rv::ParamId::Springs, rv::switchToNormalised(1));
    t.setParam(rv::ParamId::Decay, 0.6f);
    t.setParam(rv::ParamId::Tension, 0.5f);
    t.setParam(rv::ParamId::Splash, 0.3f);
    t.setParam(rv::ParamId::Drive, s.drive);
    t.setParam(rv::ParamId::Wobble, 0.5f);
    t.setParam(rv::ParamId::Mix, 1.0f);
    t.setParam(rv::ParamId::Attitude, rv::switchToNormalised(s.att));
    t.setParam(rv::ParamId::Tone, s.tone);
    if (s.place != 0) t.setTonePlaceVoicing(s.place);
}

// TONE moves: (sample, value), applied at the start of the block holding it.
using Moves = std::vector<std::pair<size_t, float>>;

Stereo render(const Settings& s, const Buf& in, const Moves& moves = {}, int block = 16)
{
    rv::Tank t;
    t.prepare(kFs, block);
    apply(t, s);
    const size_t n = in.size();
    Stereo o{Buf(n), Buf(n)};
    size_t next = 0;
    for (size_t pos = 0; pos < n; pos += size_t(block)) {
        while (next < moves.size() && moves[next].first < pos + size_t(block)) {
            t.setParam(rv::ParamId::Tone, moves[next].second);
            ++next;
        }
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

// BS.1770 loudness (as test_drive).
Buf kWeight(const Buf& x)
{
    const double b1[3] = {1.53512485958697, -2.69169618940638, 1.19839281085285};
    const double a1[3] = {1.0, -1.69065929318241, 0.73248077421585};
    const double b2[3] = {1.0, -2.0, 1.0};
    const double a2[3] = {1.0, -1.99004745483398, 0.99007225036621};
    Buf y(x.size());
    double s1 = 0, s2 = 0, t1 = 0, t2 = 0;
    for (size_t i = 0; i < x.size(); ++i) {
        const double in = x[i];
        const double m  = b1[0] * in + s1;
        s1 = b1[1] * in - a1[1] * m + s2;
        s2 = b1[2] * in - a1[2] * m;
        const double o = b2[0] * m + t1;
        t1 = b2[1] * m - a2[1] * o + t2;
        t2 = b2[2] * m - a2[2] * o;
        y[i] = float(o);
    }
    return y;
}
double loudness(const Stereo& o)
{
    const Buf kl = kWeight(o.l), kr = kWeight(o.r);
    const size_t block = size_t(0.4f * kFs), hop = block / 4, n = kl.size();
    std::vector<double> z;
    for (size_t s = 0; s + block <= n; s += hop) z.push_back(power(kl, s, s + block) + power(kr, s, s + block));
    auto gated = [&](double thr) {
        double acc = 0;
        int cnt = 0;
        for (double v : z)
            if (-0.691 + db(v) > thr) { acc += v; ++cnt; }
        return cnt ? acc / cnt : 0.0;
    };
    const double absMean = gated(-70.0);
    if (absMean <= 0) return -100.0;
    return -0.691 + db(gated(std::max(-70.0, -0.691 + db(absMean) - 10.0)));
}

// Click detector as test_tank / test_spring: |second difference| > 20 dB
// above its local ±10 ms RMS and above 1e-3.
int countClicks(const Buf& x, size_t from, double* maxRatio)
{
    const size_t n = x.size();
    std::vector<double> d2(n, 0.0), pre(n + 1, 0.0);
    for (size_t i = 2; i < n; ++i) d2[i] = double(x[i]) - 2.0 * x[i - 1] + x[i - 2];
    for (size_t i = 0; i < n; ++i) pre[i + 1] = pre[i] + d2[i] * d2[i];
    const size_t half = size_t(0.010 * kFs);
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

bool readStimulus(const std::string& name, rv::wav::Audio& out)
{
    std::string error;
    for (const std::string& prefix : {"../test_audio/stimulus/", "test_audio/stimulus/"})
        if (rv::wav::read(prefix + name, out, error)) return true;
    std::fprintf(stderr, "could not read stimulus %s: %s\n", name.c_str(), error.c_str());
    return false;
}
Buf mono(const rv::wav::Audio& a, double seconds)
{
    Buf m(std::min(a.frames(), size_t(seconds * kFs)));
    for (size_t i = 0; i < m.size(); ++i) {
        float acc = 0;
        for (const auto& c : a.channels) acc += c[i];
        m[i] = acc / float(a.channels.size());
    }
    return m;
}

// One synthetic snare (as test_drive's snareHits) at 0.2 s, then silence.
Buf oneSnare(size_t n)
{
    Buf out(n, 0.0f);
    rv::dsp::Rng rng;
    rng.seed(1u);
    const size_t len = size_t(0.25f * kFs), at = size_t(0.2f * kFs);
    float lp = 0, lp2 = 0;
    const float cl = 1.0f - std::exp(-2.0f * rv::map::kPi * 7000.0f / kFs);
    const float ch = 1.0f - std::exp(-2.0f * rv::map::kPi * 800.0f / kFs);
    for (size_t i = 0; i < len && at + i < n; ++i) {
        const float t = float(i) / kFs;
        lp += cl * (rng.bipolar() - lp);
        lp2 += ch * (lp - lp2);
        out[at + i] = 0.5f * (0.6f * std::sin(2.0f * rv::map::kPi * 185.0f * t) * std::exp(-t / 0.03f)
                              + 1.2f * (lp - lp2) * std::exp(-t / 0.06f));
    }
    return out;
}

Buf lowBand(const Stereo& o, float hz)
{
    const float c = 1.0f - std::exp(-2.0f * rv::map::kPi * hz / kFs);
    Buf y(o.l.size());
    float a = 0, b = 0;
    for (size_t i = 0; i < y.size(); ++i) {
        const float x = 0.5f * (o.l[i] + o.r[i]);
        a += c * (x - a);
        b += c * (a - b);
        y[i] = b;
    }
    return y;
}

void identical(const Buf& hits, const Buf& skank)
{
    int cells = 0, diff = 0;
    for (int att : {0, 2})
        for (float tn : {0.0f, 0.3f, 0.5f})
            for (const Buf* in : {&hits, &skank}) {
                Settings s;
                s.att = att;
                s.tone = tn;
                const Stereo ref = render(s, *in);
                for (int p = 1; p < 3; ++p) {
                    s.place = p;
                    const Stereo o = render(s, *in);
                    ++cells;
                    if (o.l != ref.l || o.r != ref.r) ++diff;
                }
            }
    std::snprintf(msg, sizeof msg, "TONE placement 1-2 at TONE 0 / 0.3 / 0.5 are bit for bit placement 0 (hits, skank; CLEAN, KICKED): %d of %d differ", diff, cells);
    check(diff == 0, msg);

    // Placement 0 set explicitly is the default Tank, TONE right of noon and swept too.
    cells = diff = 0;
    const Moves sweep = {{size_t(1.0f * kFs), 1.0f}, {size_t(3.0f * kFs), 0.5f}};
    for (int att : {0, 2})
        for (float tn : {0.85f, 1.0f})
            for (const Buf* in : {&hits, &skank}) {
                Settings s;
                s.att = att;
                s.tone = tn;
                const Stereo ref = render(s, *in, tn == 1.0f ? sweep : Moves{});
                rv::Tank t;
                t.prepare(kFs, 16);
                apply(t, s);
                t.setTonePlaceVoicing(0);
                const size_t n = in->size();
                Stereo o{Buf(n), Buf(n)};
                size_t next = 0;
                const Moves mv = tn == 1.0f ? sweep : Moves{};
                for (size_t pos = 0; pos < n; pos += 16) {
                    while (next < mv.size() && mv[next].first < pos + 16) t.setParam(rv::ParamId::Tone, mv[next++].second);
                    const int k = int(std::min(size_t(16), n - pos));
                    t.process(in->data() + pos, in->data() + pos, o.l.data() + pos, o.r.data() + pos, k);
                }
                ++cells;
                if (o.l != ref.l || o.r != ref.r) ++diff;
            }
    std::snprintf(msg, sizeof msg, "TONE placement 0 set explicitly is the default Tank bit for bit (TONE 0.85 / 1 and a sweep): %d of %d differ", diff, cells);
    check(diff == 0, msg);
}

void loudnessAcross(const Buf& hits, const Buf& skank, const Buf& held)
{
    const float tones[4] = {0.5f, 0.7f, 0.85f, 1.0f};
    const char* const matName[3] = {"hits", "skank", "held"};
    const Buf* mats[3] = {&hits, &skank, &held};
    for (int p = 0; p < 3; ++p)
        for (int att = 0; att < 3; ++att) {
            double worst = 0;
            char line[240] = {}, at[64] = {};
            std::future<double> fut[3][4];
            for (int m = 0; m < 3; ++m)
                for (int i = 0; i < 4; ++i)
                    fut[m][i] = std::async(std::launch::async, [&, m, i] {
                        Settings s;
                        s.att = att;
                        s.place = p;
                        s.tone = tones[i];
                        return loudness(render(s, *mats[m], {}, 48));
                    });
            for (int m = 0; m < 3; ++m) {
                double lev[4];
                for (int i = 0; i < 4; ++i) lev[i] = fut[m][i].get();
                char part[64];
                std::snprintf(part, sizeof part, " %s %+.1f/%+.1f/%+.1f", matName[m], lev[1] - lev[0], lev[2] - lev[0], lev[3] - lev[0]);
                std::strncat(line, part, sizeof line - std::strlen(line) - 1);
                for (int i = 1; i < 4; ++i)
                    if (std::fabs(lev[i] - lev[0]) > std::fabs(worst)) {
                        worst = lev[i] - lev[0];
                        std::snprintf(at, sizeof at, "%s TONE %.2f", matName[m], tones[i]);
                    }
            }
            std::snprintf(msg, sizeof msg, "TONE placement %s %s: loudness vs noon at TONE 0.7/0.85/1 (dB):%s; worst %+.1f (%s; limit ±3)",
                          kPlaceName[p], kAttName[att], line, worst, at);
            check(std::fabs(worst) <= 3.0, msg);
        }
}

void fastSweep()
{
    const size_t n = size_t(3.0f * kFs);
    const Buf in = oneSnare(n);
    const size_t up = size_t(0.5f * kFs), down = size_t(1.2f * kFs), probe = size_t(0.25f * kFs);
    for (int p = 0; p < 3; ++p) {
        int clicks = 0;
        double worst = 0, lowDrop[3] = {0, 0, 0};
        for (int att = 0; att < 3; ++att) {
            Settings s;
            s.att = att;
            s.place = p;
            const Stereo o = render(s, in, {{up, 1.0f}, {down, 0.5f}});
            for (const Buf* ch : {&o.l, &o.r}) {
                double r = 0;
                clicks += countClicks(*ch, up - size_t(0.1f * kFs), &r);
                worst = std::max(worst, r);
            }
            // The tail's lows just after the move vs the same moment without it.
            const Stereo still = render(s, in);
            const Buf a = lowBand(o, 250.0f), b = lowBand(still, 250.0f);
            const size_t w = size_t(0.05f * kFs);
            lowDrop[att] = db(power(a, up + probe, up + probe + w)) - db(power(b, up + probe, up + probe + w));
        }
        std::printf("INFO  TONE placement %s: noon -> 1 on a ringing tail, its lows (< 250 Hz) 0.25 s later vs no move: "
                    "CLEAN %+.1f / DRIVEN %+.1f / KICKED %+.1f dB\n",
                    kPlaceName[p], lowDrop[0], lowDrop[1], lowDrop[2]);
        std::snprintf(msg, sizeof msg, "TONE placement %s: fast TONE noon -> 1 -> noon on a ringing tail, every ATTITUDE: %d clicks (worst ratio %.1f, limit 10)",
                      kPlaceName[p], clicks, worst);
        check(clicks == 0, msg);
    }
}

void cpu(const Buf& hits)
{
    double ns[3];
    for (int p = 0; p < 3; ++p) {
        double best = 1e9;
        for (int rep = 0; rep < 3; ++rep) {
            Settings s;
            s.att = 2;
            s.place = p;
            s.tone = 1.0f;
            rv::Tank t;
            t.prepare(kFs, 48);
            apply(t, s);
            Buf l(48), r(48);
            const auto t0 = std::chrono::steady_clock::now();
            for (size_t pos = 0; pos + 48 <= hits.size(); pos += 48)
                t.process(hits.data() + pos, hits.data() + pos, l.data(), r.data(), 48);
            const auto t1 = std::chrono::steady_clock::now();
            best = std::min(best, std::chrono::duration<double, std::nano>(t1 - t0).count() / double(hits.size()));
        }
        ns[p] = best;
    }
    std::printf("INFO  TONE placement CPU, whole Tank (KICKED, 2 Springs, TONE 1, hits; best of 3, desktop): "
                "pre %.1f ns/sample, post %.1f (%+.1f %%), split %.1f (%+.1f %%)\n",
                ns[0], ns[1], 100.0 * (ns[1] / ns[0] - 1.0), ns[2], 100.0 * (ns[2] / ns[0] - 1.0));
}

} // namespace

int main()
{
    rv::wav::Audio hitsA, skankA, heldA;
    if (!readStimulus("02_hits.wav", hitsA) || !readStimulus("04_skank.wav", skankA) || !readStimulus("08_held_tones.wav", heldA)) {
        std::printf("FAIL  stimulus readable\n");
        return 1;
    }
    const Buf hits = mono(hitsA, 8.0), skank = mono(skankA, 8.0), held = mono(heldA, 12.0);
    identical(hits, skank);
    loudnessAcross(hits, skank, held);
    fastSweep();
    cpu(hits);
    std::printf("%s (%d failure%s)\n", failures ? "FAILED" : "ALL PASSED", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
