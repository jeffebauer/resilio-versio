// THROW (ADR 0039) and HOLD (ADR 0040): params/ThrowHold.h.
// Dependency-free: prints PASS/FAIL lines, returns nonzero on any failure.
//
// THROW: the gate opens the Springs' send while high, from its first rising
// edge after power-up (unpatched it reads low, so nothing changes).
//   1. untouched: gate-low events and THROW off leave the Tank bit for bit
//      as before (the latch never switches on);
//   2. latch: the send stays open until the first rising edge, then follows
//      the gate (open within 3 ms, closed within 20 ms); reset() = power-off;
//   3. the tail rings on after the send closes, and input after the close
//      never reaches the wet;
//   4. the Kick is never gated;
//   5. clicks: a hot (0 dBFS) sustained low chord thrown on and off, every
//      ATTITUDE, MIX 1: the Renderer's click detector reads 0;
//   6. the THROW param is the gate (Plugin, Renderer).
// HOLD: CLEAN and DRIVEN, DECAY 0.9 -> 1.
//   7. the zone: weight 0 below DECAY 0.9 and in KICKED (the Howl unchanged),
//      1 at CLEAN / DRIVEN DECAY 1; T60 continuous and rising through it;
//   8. it holds: a hit at DECAY 1 loses <= 6 dB from 5 s to 20 s, never
//      grows (no self-oscillation), stays under the limiter;
//   9. ducking: the held wet dips >= 8 dB under new input and comes back
//      within ~1 s;
//  10. freeze (voicing A): new input does not enter the held bed; an open
//      throw does (how new sound gets into a frozen bed);
//  11. layer (voicing B): reported (creep over 60 s of a sustained pad).
// test_antires judges the T60 and evenness outside the zone (DECAY <= 0.9
// for CLEAN / DRIVEN since ADR 0040).

#include "Metrics.h"
#include "dsp/Tank.h"
#include "params/Mappings.h"
#include "params/ThrowHold.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>

namespace {

int  failures = 0;
char msg[512];

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++failures;
}

constexpr float kFs    = 48000.0f;
constexpr int   kBlock = 48;
constexpr float kPi    = 3.14159265358979f;
using Buf              = std::vector<float>;

struct Setup {
    float decay = 0.5f, attitude = 0.0f, mix = 1.0f, splash = 0.0f, drive = 0.0f;
    int   springs = 2, voicing = rv::throwhold::kDefaultVoicing;
};

std::unique_ptr<rv::Tank> make(const Setup& s)
{
    auto t = std::make_unique<rv::Tank>();
    t->prepare(kFs, kBlock);
    t->setHoldVoicing(s.voicing);
    t->setParam(rv::ParamId::Decay, s.decay);
    t->setParam(rv::ParamId::Tension, 0.5f);
    t->setParam(rv::ParamId::Tone, 0.5f);
    t->setParam(rv::ParamId::Splash, s.splash);
    t->setParam(rv::ParamId::Wobble, 0.5f); // noon: still (ADR 0034)
    t->setParam(rv::ParamId::Mix, s.mix);
    t->setParam(rv::ParamId::Springs, rv::switchToNormalised(s.springs - 1));
    t->setParam(rv::ParamId::Attitude, s.attitude);
    t->setParam(rv::ParamId::Drive, s.drive);
    return t;
}

// A gate change at a sample, or a Kick.
struct Event {
    size_t at;
    int    what; // 0 = gate low, 1 = gate high, 2 = Kick
};

// Mono render (L), events placed at their sample within the block.
Buf render(rv::Tank& t, const Buf& in, const std::vector<Event>& ev = {})
{
    const size_t n = in.size();
    Buf l(n), r(n);
    size_t e = 0;
    for (size_t p = 0; p < n; p += kBlock) {
        const int m = int(std::min<size_t>(kBlock, n - p));
        while (e < ev.size() && ev[e].at < p + size_t(m)) {
            const int off = int(ev[e].at - p);
            if (ev[e].what == 2) t.kick(off);
            else t.gate(ev[e].what == 1, off);
            ++e;
        }
        t.process(&in[p], &in[p], &l[p], &r[p], m);
    }
    return l;
}

size_t sec(float s) { return size_t(s * kFs); }

// Low A minor-ish chord (tools/make_stimulus.py's 08 chord an octave down),
// 10 ms fades, at gainDb, from a to b seconds within len seconds.
Buf chord(float gainDb, float a, float b, float len, double f0 = 110.0)
{
    Buf x(sec(len), 0.0f);
    const double g = std::pow(10.0, double(gainDb) / 20.0);
    const double f[3] = {f0, f0 * 1.1892, f0 * 1.4983};
    const size_t i0 = sec(a), i1 = std::min(sec(b), x.size()), fade = sec(0.01f);
    for (size_t i = i0; i < i1; ++i) {
        const double env = std::min({1.0, double(i - i0) / double(fade), double(i1 - 1 - i) / double(fade)});
        double s = 0.0;
        for (double hz : f) s += std::sin(2.0 * double(kPi) * hz * double(i) / double(kFs));
        x[i] = float(g * env * s / 3.0); // <= 0 dBFS peak at gainDb 0
    }
    return x;
}

// A short noise-ish hit (snare-like: decaying band of noise), at t seconds.
void addHit(Buf& x, float t, float gainDb = -6.0f)
{
    unsigned s = 12345u + unsigned(t * 1000.0f);
    const double g = std::pow(10.0, double(gainDb) / 20.0);
    const size_t i0 = sec(t);
    for (size_t i = 0; i < sec(0.12f) && i0 + i < x.size(); ++i) {
        s = s * 1664525u + 1013904223u;
        const double n = double(int(s >> 9) - (1 << 22)) / double(1 << 22);
        x[i0 + i] += float(g * n * std::exp(-double(i) / (0.025 * double(kFs))));
    }
}

double rmsDb(const Buf& x, float a, float b)
{
    const size_t i0 = sec(a), i1 = std::min(sec(b), x.size());
    double acc = 0.0;
    for (size_t i = i0; i < i1; ++i) acc += double(x[i]) * double(x[i]);
    return 10.0 * std::log10(acc / double(std::max<size_t>(1, i1 - i0)) + 1e-20);
}

double peak(const Buf& x)
{
    double p = 0.0;
    for (float v : x) p = std::max(p, double(std::fabs(v)));
    return p;
}

long clicks(const Buf& l) { return rv::metrics::compute({l, l}, kFs).clickCount; }

// ---- THROW --------------------------------------------------------------------

void untouched()
{
    Buf in = chord(-12.0f, 0.1f, 1.0f, 3.0f);
    addHit(in, 1.5f);
    bool same = true;
    for (float att : {0.0f, 0.5f, 1.0f}) {
        Setup s;
        s.attitude = att;
        s.splash   = 0.5f;
        auto a = make(s), b = make(s);
        const Buf ya = render(*a, in);
        // Gate-low changes only (an unpatched jack): the latch never switches on.
        const Buf yb = render(*b, in, {{sec(0.3f), 0}, {sec(1.2f), 0}});
        for (size_t i = 0; i < ya.size(); ++i) same &= ya[i] == yb[i];
        same &= !b->throwOn() && b->sendGain() == 1.0f;
    }
    check(same, "THROW untouched: gate-low only (unpatched) is bit for bit the Tank without THROW, every ATTITUDE");
}

void latch()
{
    Setup s;
    auto t = make(s);
    const Buf silence(sec(0.1f), 0.0f);
    render(*t, silence);
    const bool openBefore = t->sendGain() == 1.0f && !t->throwOn();
    // High at 0, low at 50 ms: watch the send per block.
    auto t2 = make(s);
    Buf in(sec(0.2f), 0.0f);
    float openAt = -1.0f, closedAt = -1.0f;
    for (size_t p = 0; p < in.size(); p += kBlock) {
        if (p == 0) t2->gate(true, 0);
        if (p == sec(0.05f)) t2->gate(false, 0);
        float l[kBlock], r[kBlock];
        t2->process(&in[p], &in[p], l, r, kBlock);
        const float now = float(p + kBlock) / kFs;
        if (openAt < 0.0f && t2->sendGain() >= 0.999f && p < sec(0.05f)) openAt = now;
        if (p >= sec(0.05f) && closedAt < 0.0f && t2->sendGain() <= 1e-6f) closedAt = now - 0.05f;
    }
    const bool on = t2->throwOn();
    t2->reset();
    const bool offAfterReset = !t2->throwOn() && t2->sendGain() == 1.0f;
    std::snprintf(msg, sizeof msg,
                  "THROW latch: send open before the first rising edge; then opens in %.1f ms (<= 3), closes in %.1f ms "
                  "(<= 20); reset() = power-off (throw off, send open)",
                  1000.0f * openAt, 1000.0f * closedAt);
    check(openBefore && on && openAt > 0.0f && openAt <= 0.003f && closedAt > 0.0f && closedAt <= 0.020f && offAfterReset,
          msg);
}

void tailRingsOn()
{
    // A pad from 0 to 4 s; a 1-sample blip of the gate at 0 latches the
    // throw on (closed from ~15 ms), then it is open 0.5 .. 1.0 s. Compare
    // with the pad stopping at 1.0 s: after the close the input must not
    // reach the wet.
    Setup s;
    s.decay = 0.75f;
    const Buf padLong  = chord(-12.0f, 0.0f, 4.0f, 5.0f);
    Buf       padShort = padLong;
    // The close ramp's 15 ms still let the pad in, fading: padShort keeps them.
    std::fill(padShort.begin() + long(sec(1.0f + rv::throwhold::kThrowCloseSeconds) + 1), padShort.end(), 0.0f);
    const std::vector<Event> ev = {{0, 1}, {1, 0}, {sec(0.5f), 1}, {sec(1.0f), 0}};
    auto a = make(s), b = make(s);
    const Buf ya = render(*a, padLong, ev), yb = render(*b, padShort, ev);
    double err = 0.0, ref = 0.0;
    for (size_t i = sec(1.05f); i < ya.size(); ++i) {
        err += double(ya[i] - yb[i]) * double(ya[i] - yb[i]);
        ref += double(yb[i]) * double(yb[i]);
    }
    const double leakDb = 10.0 * std::log10(err / ref + 1e-30);
    const double tailDb = rmsDb(ya, 1.5f, 2.0f) - rmsDb(ya, 0.7f, 1.0f);
    const double before = rmsDb(ya, 0.25f, 0.45f); // the first 15 ms got in: let it ring down (DECAY 0.75)
    std::snprintf(msg, sizeof msg,
                  "THROW tail: rings on after the close (%.1f dB 0.5-1 s later, > -30), input after the close leaks "
                  "%.1f dB (< -80), little before the open (%.1f dBFS 0.25 s after the blip, < -30)",
                  tailDb, leakDb, before);
    check(tailDb > -30.0 && leakDb < -80.0 && before < -30.0, msg);
}

void kickNotGated()
{
    Setup s;
    s.decay = 0.75f;
    auto t = make(s);
    const Buf silence(sec(2.0f), 0.0f);
    // Throw on (gate up and down), then a Kick with the gate low.
    const Buf y = render(*t, silence, {{sec(0.1f), 1}, {sec(0.2f), 0}, {sec(0.5f), 2}});
    const double lvl = rmsDb(y, 0.5f, 1.0f);
    std::snprintf(msg, sizeof msg, "THROW never gates the Kick: a Kick with the gate low rings at %.1f dBFS (> -50)", lvl);
    check(t->throwOn() && lvl > -50.0, msg);
}

void throwClicks()
{
    // 0 dBFS sustained low chord, thrown on and off on and off the beat (not
    // on zero crossings), at every ATTITUDE.
    const Buf in = chord(0.0f, 0.0f, 6.0f, 8.0f);
    std::vector<Event> ev;
    for (int k = 0; k < 8; ++k) {
        ev.push_back({sec(0.3f + 0.7f * float(k)) + size_t(k * 37), 1});
        ev.push_back({sec(0.3f + 0.7f * float(k) + 0.23f + 0.05f * float(k % 3)) + size_t(k * 11), 0});
    }
    long worst = 0;
    for (float att : {0.0f, 0.5f, 1.0f})
        for (float d : {0.5f, 0.95f}) {
            Setup s;
            s.attitude = att;
            s.decay    = d;
            auto      t = make(s);
            const long c = clicks(render(*t, in, ev));
            worst = std::max(worst, c);
            if (c) {
                auto      u  = make(s);
                const long c0 = clicks(render(*u, in));
                std::printf("INFO    clicks %ld at ATTITUDE %.1f DECAY %.2f (%ld without the throw)\n", c, att, d, c0);
            }
        }
    std::snprintf(msg, sizeof msg,
                  "THROW clicks: 0 dBFS low chord thrown 8x on / off, CLEAN / DRIVEN / KICKED x DECAY 0.5 / 0.95, MIX 1: "
                  "worst click_count %ld (must be 0)",
                  worst);
    check(worst == 0, msg);
}

void throwParam()
{
    Setup s;
    s.decay = 0.75f;
    const Buf in = chord(-12.0f, 0.0f, 2.0f, 3.0f);
    auto a = make(s), b = make(s);
    const Buf ya = render(*a, in, {{0, 1}, {sec(1.0f), 0}});
    // The param: on at block 0, off at 1.0 s (a block edge: 1.0 s = 1000 blocks).
    Buf yb(in.size()), r(in.size());
    for (size_t p = 0; p < in.size(); p += kBlock) {
        if (p == 0) b->setParam(rv::ParamId::Throw, 1.0f);
        if (p == sec(1.0f)) b->setParam(rv::ParamId::Throw, 0.0f);
        b->process(&in[p], &in[p], &yb[p], &r[p], kBlock);
    }
    bool same = true;
    for (size_t i = 0; i < ya.size(); ++i) same &= ya[i] == yb[i];
    check(same && b->throwOn(), "THROW param (Plugin, Renderer) is the gate: same output as gate() at the block's start");
}

// ---- HOLD ---------------------------------------------------------------------

void zone()
{
    bool ok = true;
    float worstJump = 0.0f;
    for (float att : {0.0f, 0.5f, 1.0f}) {
        for (float d : {0.85f, 0.9f, 0.95f, 1.0f}) {
            Setup s;
            s.attitude = att;
            s.decay    = d;
            auto t = make(s);
            render(*t, Buf(sec(0.05f), 0.0f));
            const float w = t->holdWeight();
            const float want = att == 1.0f ? 0.0f : rv::throwhold::zone(d);
            ok &= std::fabs(w - want) < 1e-4f;
        }
    }
    // T60 continuous and rising: the zone's T60 over DECAY 0.85 .. 1.
    float prev = rv::map::decayT60Seconds(0.85f);
    bool rising = true;
    for (int k = 1; k <= 150; ++k) {
        const float d = 0.85f + 0.001f * float(k);
        const float t = rv::throwhold::t60Seconds(rv::map::decayT60Seconds(d), rv::throwhold::zone(d));
        rising &= t > prev;
        worstJump = std::max(worstJump, t / prev);
        prev = t;
    }
    const float top = rv::throwhold::t60Seconds(rv::map::decayT60Seconds(1.0f), 1.0f);
    std::snprintf(msg, sizeof msg,
                  "HOLD zone: weight = zone in CLEAN / DRIVEN, 0 in KICKED and below DECAY 0.9; T60 rises smoothly "
                  "(largest step x%.3f per 0.001 DECAY, <= x1.06) to %.0f s at DECAY 1",
                  worstJump, top);
    check(ok && rising && worstJump <= 1.06f && std::fabs(top - rv::throwhold::kTopT60Seconds) < 1.0f, msg);
}

// How much a held bed may lose from 5 s to 20 s. The design's 240 s T60
// (-3.75 dB / 15 s) applies at the Loop's peak; the rest of the spectrum
// (the damping's highs) goes faster, so a broadband hit reads more early on.
constexpr double kMaxLoss = 8.0;

void holds()
{
    bool ok = true;
    for (float att : {0.0f, 0.5f})
        for (int sp : {1, 2, 3}) {
            Setup s;
            s.attitude = att;
            s.decay    = 1.0f;
            s.springs  = sp;
            auto t = make(s);
            // Freeze voicing: the bed is filled while ... the send is closed at
            // DECAY 1, so fill it with a throw (an open throw overrides the
            // freeze), then let it hold.
            Buf in(sec(41.0f), 0.0f);
            addHit(in, 0.2f, -3.0f);
            addHit(in, 0.45f, -3.0f);
            const Buf y = render(*t, in, {{sec(0.15f), 1}, {sec(0.6f), 0}});
            const double l5 = rmsDb(y, 4.0f, 5.0f), l20 = rmsDb(y, 19.0f, 20.0f), l2 = rmsDb(y, 1.5f, 2.5f),
                         l40 = rmsDb(y, 39.0f, 40.0f);
            double grow = -100.0;
            for (float a = 3.0f; a < 40.0f; a += 1.0f) grow = std::max(grow, rmsDb(y, a, a + 1.0f) - l2);
            const double p = peak(y);
            const bool good = l5 - l20 <= kMaxLoss && l5 - l20 >= 0.5 && grow < 0.5 && p <= double(rv::Tank::kLimitThreshold) + 1e-3 && l5 > -60.0;
            std::printf("INFO    %s %d Spring%s DECAY 1: %.1f dBFS at 5 s, %.1f at 20 s (loss %.2f dB), %.1f at 40 s "
                        "(%.2f dB / 10 s from 20 s), max growth over 1.5-2.5 s %+.2f dB, peak %.3f\n",
                        att == 0.0f ? "CLEAN " : "DRIVEN", sp, sp > 1 ? "s" : " ", l5, l20, l5 - l20, l40,
                        0.5 * (l20 - l40), grow, p);
            ok &= good;
        }
    std::snprintf(msg, sizeof msg,
                  "HOLD holds: CLEAN / DRIVEN DECAY 1, every SPRINGS: loses 0.5-%.0f dB from 5 s to 20 s, never grows "
                  "(< +0.5 dB over 40 s), stays under the limiter",
                  kMaxLoss);
    check(ok, msg);
}

void ducking()
{
    Setup s;
    s.decay = 1.0f;
    auto t = make(s);
    Buf in(sec(10.0f), 0.0f);
    addHit(in, 0.2f, -3.0f);
    // New input 4 .. 6 s (a quiet riff: hits every 125 ms at -12 dBFS).
    for (float a = 4.0f; a < 6.0f; a += 0.125f) addHit(in, a, -12.0f);
    Buf bedOnly(in.size(), 0.0f);
    addHit(bedOnly, 0.2f, -3.0f);
    auto ref = make(s);
    const std::vector<Event> fill = {{sec(0.15f), 1}, {sec(0.4f), 0}};
    const Buf y = render(*t, in, fill), yr = render(*ref, bedOnly, fill);
    // Freeze voicing: the riff doesn't enter, so the wet is the bed alone,
    // ducked: compare with the bed rendered without the riff.
    const double dip  = rmsDb(yr, 4.5f, 5.9f) - rmsDb(y, 4.5f, 5.9f);
    const double back = rmsDb(yr, 7.0f, 7.5f) - rmsDb(y, 7.0f, 7.5f);
    std::snprintf(msg, sizeof msg,
                  "HOLD ducking: the held wet dips %.1f dB under new input (>= 8), back within %.1f dB 1-1.5 s after "
                  "it stops (<= 1)",
                  dip, back);
    check(dip >= 8.0 && back <= 1.0, msg);
}

void freezeAndThrowIn()
{
    Setup s;
    s.decay = 1.0f;
    Buf bed(sec(8.0f), 0.0f);
    addHit(bed, 0.2f, -3.0f);
    Buf withRiff = bed;
    addHit(withRiff, 3.0f, -3.0f);
    addHit(withRiff, 3.3f, -3.0f);
    const std::vector<Event> fill = {{sec(0.15f), 1}, {sec(0.4f), 0}};
    auto a = make(s), b = make(s), c = make(s);
    const Buf ya = render(*a, bed, fill), yb = render(*b, withRiff, fill);
    std::vector<Event> fill2 = fill;
    fill2.push_back({sec(2.95f), 1});
    fill2.push_back({sec(3.5f), 0});
    const Buf yc = render(*c, withRiff, fill2);
    // After the ducking has recovered (from 5 s), the frozen bed must be the
    // same with or without the riff; thrown in, the riff must be there.
    double eB = 0.0, eC = 0.0, ref = 0.0;
    for (size_t i = sec(5.0f); i < ya.size(); ++i) {
        ref += double(ya[i]) * double(ya[i]);
        eB += double(yb[i] - ya[i]) * double(yb[i] - ya[i]);
        eC += double(yc[i] - ya[i]) * double(yc[i] - ya[i]);
    }
    const double frozen = 10.0 * std::log10(eB / ref + 1e-30), thrown = 10.0 * std::log10(eC / ref + 1e-30);
    std::snprintf(msg, sizeof msg,
                  "HOLD freeze (voicing A): new input leaves the held bed alone (difference %.1f dB, < -60); thrown in, "
                  "it enters (difference %.1f dB, > -20)",
                  frozen, thrown);
    check(frozen < -60.0 && thrown > -20.0, msg);
}

void layerCreep()
{
    // Voicing B: a sustained pad into the held bed for 60 s. Reported: the
    // level over each 10 s (ADR 0040: the bed keeper only if > 1 dB / 10 s).
    for (float att : {0.0f, 0.5f}) {
        Setup s;
        s.decay   = 1.0f;
        s.attitude = att;
        s.voicing = rv::throwhold::kVoicingLayer;
        auto t = make(s);
        const Buf in = chord(-12.0f, 0.0f, 60.0f, 60.0f, 220.0);
        const Buf y = render(*t, in);
        double worst = -100.0;
        std::printf("INFO    layer %s pad -12 dBFS, wet per 10 s:", att == 0.0f ? "CLEAN " : "DRIVEN");
        double prev = 0.0;
        for (int k = 0; k < 6; ++k) {
            const double l = rmsDb(y, 10.0f * float(k) + 8.0f, 10.0f * float(k) + 10.0f);
            std::printf(" %.1f", l);
            if (k >= 2) worst = std::max(worst, l - prev);
            prev = l;
        }
        std::printf(" dBFS; worst creep from 20 s %+.2f dB / 10 s; peak %.3f\n", worst, peak(y));
    }
}

} // namespace

int main()
{
    untouched();
    latch();
    tailRingsOn();
    kickNotGated();
    throwClicks();
    throwParam();
    zone();
    holds();
    ducking();
    freezeAndThrowIn();
    layerCreep();
    std::printf("%s (%d failure%s)\n", failures ? "FAILED" : "ALL PASSED", failures, failures == 1 ? "" : "s");
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
