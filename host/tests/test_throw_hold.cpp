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
//   4. (was: the Kick is never gated; the Kick went in ADR 0043. The
//      button's own throw is test_button.)
//   5. clicks: a hot (0 dBFS) sustained low chord thrown on and off, every
//      ATTITUDE, MIX 1: the Renderer's click detector reads 0;
//   6. the THROW param is the gate (Plugin, Renderer).
//   6b. leaving throw mode (exitThrowMode(); the button's double tap and
//      hold calls the same, test_button): the send reopens over the open
//      ramp without a click, the latch clears, the next rising edge switches
//      it on again; an exit with throw mode off changes nothing, bit for
//      bit; the Renderer's "throw_exits" reaches the Tank.
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

#include "Automation.h"
#include "Json.h"
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

// A gate change at a sample.
struct Event {
    size_t at;
    int    what; // 0 = gate low, 1 = gate high
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
            t.gate(ev[e].what == 1, off);
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

// Gate and exit events in time order, then the render.
// what: 0 / 1 gate, 3 exitThrowMode() (at the block's start).
Buf renderEx(rv::Tank& t, const Buf& in, const std::vector<Event>& ev, bool* exitReturned = nullptr)
{
    const size_t n = in.size();
    Buf l(n), r(n);
    size_t e = 0;
    for (size_t p = 0; p < n; p += kBlock) {
        const int m = int(std::min<size_t>(kBlock, n - p));
        while (e < ev.size() && ev[e].at < p + size_t(m)) {
            const int off = int(ev[e].at - p);
            if (ev[e].what == 3) {
                const bool was = t.exitThrowMode();
                if (exitReturned) *exitReturned = was;
            } else t.gate(ev[e].what == 1, off);
            ++e;
        }
        t.process(&in[p], &in[p], &l[p], &r[p], m);
    }
    return l;
}

void leaveThrowMode()
{
    // 1. Exit with the gate low (the send closed) under a 0 dBFS low chord,
    // every ATTITUDE, DECAY 0.5 and 0.95 (in the Hold, freeze: the plain
    // send there is the freeze's): the send must go back to the plain send
    // within 3 ms, no click, the latch cleared.
    const Buf in = chord(0.0f, 0.0f, 4.0f, 5.0f);
    long worstClicks = 0;
    float worstOpen  = 0.0f;
    bool  cleared = true, returned = true;
    for (float att : {0.0f, 0.5f, 1.0f})
        for (float d : {0.5f, 0.95f}) {
            Setup s;
            s.attitude = att;
            s.decay    = d;
            auto t = make(s);
            bool ret = false;
            // Latch at 0.3 s (closes at 0.5 s), exit at 1.5 s (mid-chord).
            const Buf y = renderEx(*t, in, {{sec(0.3f), 1}, {sec(0.5f), 0}, {sec(1.5f), 3}}, &ret);
            worstClicks = std::max(worstClicks, clicks(y));
            returned &= ret;
            cleared &= !t->throwOn() && (d > 0.9f || t->sendGain() == 1.0f);
            // How long the reopening takes, per block (outside the Hold: in
            // its freeze the plain send is closed, so the exit goes there).
            if (d > 0.9f) continue;
            auto u = make(s);
            const Buf pre = renderEx(*u, Buf(sec(1.5f), 0.0f), {{sec(0.3f), 1}, {sec(0.5f), 0}});
            (void)pre;
            u->exitThrowMode();
            Buf z(kBlock, 0.0f), zl(kBlock), zr(kBlock);
            float openIn = -1.0f;
            for (int b = 0; b < 20 && openIn < 0.0f; ++b) {
                u->process(z.data(), z.data(), zl.data(), zr.data(), kBlock);
                if (u->sendGain() >= 1.0f && !u->throwOn()) openIn = float((b + 1) * kBlock) / kFs;
            }
            worstOpen = std::max(worstOpen, openIn < 0.0f ? 1.0f : openIn);
        }
    std::snprintf(msg, sizeof msg,
                  "THROW exit (exitThrowMode()): send reopens in %.1f ms (<= 3), latch cleared, 0 dBFS chord: worst "
                  "click_count %ld (must be 0), exitThrowMode() reports throw mode was on",
                  1000.0f * worstOpen, worstClicks);
    check(worstClicks == 0 && worstOpen <= 0.003f && cleared && returned, msg);

    // 2. After the exit the input reaches the springs again; the next rising
    // edge latches again (send follows the gate).
    {
        Setup s;
        s.decay = 0.75f;
        auto t = make(s);
        renderEx(*t, Buf(sec(0.1f), 0.0f), {{0, 1}, {sec(0.05f), 0}, {sec(0.08f), 3}});
        const bool off = !t->throwOn();
        renderEx(*t, Buf(sec(0.1f), 0.0f), {{sec(0.02f), 1}, {sec(0.04f), 0}});
        const bool relatched = t->throwOn() && t->sendGain() <= 1e-6f;
        std::snprintf(msg, sizeof msg, "THROW exit: off after the exit, and the next rising edge switches it on again "
                                       "(the send then follows the gate: closed after the fall)");
        check(off && relatched, msg);
    }

    // 3. (Was: a short press of KICK only kicks. The button's presses that
    // don't exit are test_button's.)

    // 4. An exit with throw mode off changes nothing, bit for bit
    // (outside and inside the Hold, every ATTITUDE).
    {
        Buf x = chord(-12.0f, 0.1f, 1.5f, 3.0f);
        addHit(x, 2.0f);
        bool same = true, none = true;
        for (float att : {0.0f, 0.5f, 1.0f})
            for (float d : {0.5f, 1.0f}) {
                Setup s;
                s.attitude = att;
                s.decay    = d;
                auto a = make(s), b = make(s);
                bool ret = true;
                const Buf ya = renderEx(*a, x, {}), yb = renderEx(*b, x, {{sec(1.0f), 3}, {sec(2.2f), 3}}, &ret);
                for (size_t i = 0; i < ya.size(); ++i) same &= ya[i] == yb[i];
                none &= !ret;
            }
        check(same && none, "exitThrowMode() with throw mode off: nothing changes, bit for bit (DECAY 0.5 and the Hold, every ATTITUDE)");
    }

    // 5. The Renderer's "throw_exits" automation reaches the Tank.
    {
        rv::json::Value v;
        rv::automation::Automation au;
        std::string err;
        const bool ok = rv::json::parse(R"({"throw_exits": [2.5, 1.0], "gates": [[0.5, 0.75]]})", v, err)
                        && rv::automation::parse(v, au, err);
        check(ok && au.throwExitsSeconds.size() == 2 && au.throwExitsSeconds[0] == 1.0,
              "Renderer automation \"throw_exits\": parsed, in time order");
    }
}

// SPRINGS 3 = echo mode (ADR 0041): the gate is the echo's clock there, so
// it never latches the throw, and a throw latched in positions 1-2 rests
// open (the send glides open); DECAY is the echo's feedback, so no Hold.
void echoModeRoles()
{
    Setup s;
    s.springs = 3;
    s.decay   = 1.0f;
    auto t = make(s);
    renderEx(*t, Buf(sec(1.0f), 0.0f), {{sec(0.2f), 1}, {sec(0.3f), 0}, {sec(0.5f), 1}, {sec(0.6f), 0}});
    const bool noLatch = !t->throwOn() && t->sendGain() == 1.0f && t->holdWeight() == 0.0f;
    // Latched in position 2 (send closed), then position 3: the send opens.
    Setup s2;
    s2.springs = 2;
    s2.decay   = 0.5f;
    auto u = make(s2);
    renderEx(*u, Buf(sec(0.5f), 0.0f), {{sec(0.1f), 1}, {sec(0.2f), 0}});
    const bool closed = u->throwOn() && u->sendGain() <= 1e-6f;
    u->setParam(rv::ParamId::Springs, rv::switchToNormalised(2));
    renderEx(*u, Buf(sec(0.5f), 0.0f), {});
    const bool opened = u->sendGain() >= 0.999f;
    u->setParam(rv::ParamId::Springs, rv::switchToNormalised(1));
    renderEx(*u, Buf(sec(0.5f), 0.0f), {});
    const bool closedAgain = u->sendGain() <= 1e-6f;
    check(noLatch && closed && opened && closedAgain,
          "Echo mode (SPRINGS 3): the gate is the clock (never latches the throw), no Hold at DECAY 1; a throw "
          "latched in positions 1-2 rests open in 3 and follows the gate again back in 2");
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
            // Position 3 is echo mode (ADR 0041), where DECAY is the echo's
            // feedback and there is no Hold (echoModeRoles): the three-Spring
            // reference holds as positions 1-2 do.
            if (sp == 3) t->setEchoMode(false);
            // The bed is filled with a throw (in the freeze voicing the
            // send is closed at DECAY 1 and an open throw overrides it), then
            // let it hold.
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

// Drum parts for the ducking checks (sine kick, noise hats, a sine bass).
void addKick(Buf& x, float t, float gainDb = -6.0f)
{
    const double g = std::pow(10.0, double(gainDb) / 20.0);
    double ph = 0.0;
    const size_t i0 = sec(t);
    for (size_t i = 0; i < sec(0.3f) && i0 + i < x.size(); ++i) {
        const double tt = double(i) / double(kFs);
        ph += 2.0 * double(kPi) * (50.0 + 90.0 * std::exp(-tt / 0.03)) / double(kFs);
        x[i0 + i] += float(g * std::sin(ph) * std::exp(-tt / 0.09));
    }
}
void addHat(Buf& x, float t, float gainDb = -12.0f)
{
    unsigned s = 777u + unsigned(t * 1000.0f);
    const double g = std::pow(10.0, double(gainDb) / 20.0);
    double prev = 0.0;
    const size_t i0 = sec(t);
    for (size_t i = 0; i < sec(0.05f) && i0 + i < x.size(); ++i) {
        s = s * 1664525u + 1013904223u;
        const double n = double(int(s >> 9) - (1 << 22)) / double(1 << 22);
        x[i0 + i] += float(g * (n - prev) * 0.5 * std::exp(-double(i) / (0.01 * double(kFs)))); // first difference: highs
        prev = n;
    }
}
void addBass(Buf& x, float t, float len, double hz, float gainDb = -12.0f)
{
    const double g = std::pow(10.0, double(gainDb) / 20.0);
    const size_t i0 = sec(t), n = sec(len), fade = sec(0.01f);
    for (size_t i = 0; i < n && i0 + i < x.size(); ++i) {
        const double env = std::min({1.0, double(i) / double(fade), double(n - 1 - i) / double(fade)});
        x[i0 + i] += float(g * env * std::sin(2.0 * double(kPi) * hz * double(i) / double(kFs)));
    }
}

// For measuring: the wet's lows (4th-order low-pass, 200 Hz by default) and
// highs (4th-order high-pass at 1 kHz).
void split(const Buf& y, Buf& lo, Buf& hi, float loHz = 200.0f)
{
    rv::dsp::Biquad l[2], h[2];
    for (auto& f : l) f.setLowpass(loHz, 0.70710678f, kFs);
    for (auto& f : h) f.setHighpass(1000.0f, 0.70710678f, kFs);
    lo.resize(y.size());
    hi.resize(y.size());
    for (size_t i = 0; i < y.size(); ++i) {
        lo[i] = l[1].process(l[0].process(y[i]));
        hi[i] = h[1].process(h[0].process(y[i]));
    }
}

// A snare like tools/make_stimulus.py's (185 Hz body + band of noise).
void addSnare(Buf& x, float t, float gainDb = -6.0f)
{
    unsigned s = 4242u + unsigned(t * 1000.0f);
    const double g = std::pow(10.0, double(gainDb) / 20.0);
    const size_t i0 = sec(t);
    double lo = 0.0, lo2 = 0.0;
    const double a7 = std::exp(-2.0 * 3.14159265 * 7000.0 / double(kFs)), a8 = std::exp(-2.0 * 3.14159265 * 800.0 / double(kFs));
    for (size_t i = 0; i < sec(0.25f) && i0 + i < x.size(); ++i) {
        s = s * 1664525u + 1013904223u;
        const double n = double(int(s >> 9) - (1 << 22)) / double(1 << 22);
        lo = (1 - a7) * n + a7 * lo;   // < 7 kHz
        lo2 = (1 - a8) * lo + a8 * lo2; // band: 800 Hz - 7 kHz
        const double tt = double(i) / double(kFs);
        x[i0 + i] += float(g * 0.6 * (0.6 * std::sin(2.0 * 3.14159265 * 185.0 * tt) * std::exp(-tt / 0.03)
                                      + 1.2 * (lo - lo2) * std::exp(-tt / 0.06)));
    }
}

void ducking()
{
    // A held bed (a low chord thrown in at 0.2 s, then frozen: the freeze
    // voicing, so nothing else gets in and the wet is the bed alone), then
    // kicks 4-6 s, or snares + hats 4-6 s. Compared with the bed alone.
    Setup s;
    s.decay   = 1.0f;
    s.voicing = rv::throwhold::kVoicingFreeze;
    const std::vector<Event> fill = {{sec(0.15f), 1}, {sec(0.6f), 0}};
    Buf bed = chord(-6.0f, 0.2f, 0.55f, 9.0f, 110.0);
    Buf kicks = bed, snares = bed;
    for (float a = 4.0f; a < 6.0f; a += 0.5f) addKick(kicks, a);
    for (float a = 4.0f; a < 6.0f; a += 0.5f) addSnare(snares, a + 0.25f);
    for (float a = 4.0f; a < 6.0f; a += 0.125f) addHat(snares, a);
    auto r = make(s), k = make(s), h = make(s);
    const Buf yr = render(*r, bed, fill), yk = render(*k, kicks, fill), ys = render(*h, snares, fill);
    Buf lr, hr, lk, hk;
    split(yr, lr, hr);
    split(yk, lk, hk);
    // Just after each kick (20-120 ms): the dip, full band, lows and highs.
    double dip = 0.0, dipLo = 0.0, dipHi = 0.0;
    for (float a = 4.0f; a < 6.0f; a += 0.5f) {
        dip += (rmsDb(yr, a + 0.02f, a + 0.12f) - rmsDb(yk, a + 0.02f, a + 0.12f)) / 4.0;
        dipLo += (rmsDb(lr, a + 0.02f, a + 0.12f) - rmsDb(lk, a + 0.02f, a + 0.12f)) / 4.0;
        dipHi += (rmsDb(hr, a + 0.02f, a + 0.12f) - rmsDb(hk, a + 0.02f, a + 0.12f)) / 4.0;
    }
    double snareMove = 0.0;
    for (float a = 4.0f; a < 6.0f; a += 0.5f)
        snareMove = std::max(snareMove, std::fabs(rmsDb(yr, a + 0.27f, a + 0.37f) - rmsDb(ys, a + 0.27f, a + 0.37f)));
    const double back = rmsDb(yr, 6.6f, 7.0f) - rmsDb(yk, 6.6f, 7.0f);
    std::snprintf(msg, sizeof msg,
                  "HOLD ducking (whole bed, keyed on the input's lows): kicks dip the bed %.1f dB (lows %.1f, highs %.1f; "
                  ">= 9 of the 12 each); snares + hats move it at most %.2f dB (<= 0.5); back within %.2f dB 0.6 s after "
                  "the last kick (<= 0.5)",
                  dip, dipLo, dipHi, snareMove, back);
    check(dip >= 9.0 && dipLo >= 9.0 && dipHi >= 9.0 && snareMove <= 0.5 && std::fabs(back) <= 0.5, msg);
}

// The hump (owner, round 1: "not just a duck but a swell before each kick,
// an audible hump rather than just a dip"). A chord stab into the Hold
// (layer, the default), then drums. Per beat:
// - pump: the wet just before the kick (60-10 ms before) vs halfway between
//   kicks: how much it is still rising into the kick;
// - overshoot: the wet just before the kick vs the same moment with the
//   same drums and no ducking (duck_voicing 2; in the layer voicing the
//   drums build into the bed either way): a dip comes back to that bed
//   (<= ~0 dB), a hump comes back fuller (> 0). Full band and lows
//   (< 200 Hz).
struct Hump {
    double pump = 0.0, over = 0.0, pumpLo = 0.0, overLo = 0.0;
};
Hump measureHump(float att, bool oneDrop)
{
    Setup s;
    s.decay    = 1.0f;
    s.attitude = att;
    Buf x(sec(16.0f), 0.0f);
    {
        const Buf st = chord(-6.0f, 0.5f, 0.7f, 1.0f, 110.0);
        for (size_t i = 0; i < st.size(); ++i) x[i] += st[i];
    }
    std::vector<float> kickAt;
    if (!oneDrop) { // four on the floor, 120 bpm, hats on the off-beats
        for (float a = 4.0f; a < 14.0f; a += 0.5f) {
            addKick(x, a);
            addHat(x, a + 0.25f);
            kickAt.push_back(a);
        }
    } else { // one drop, 75 bpm: kick on 3, bass on 1 and the "and" of 2, hats on 8ths
        const float beat = 0.8f;
        for (float bar = 4.0f; bar + 4 * beat <= 14.0f; bar += 4 * beat) {
            addKick(x, bar + 2 * beat);
            kickAt.push_back(bar + 2 * beat);
            addBass(x, bar, 0.6f, 55.0);
            addBass(x, bar + 1.5f * beat, 0.4f, 73.4);
            for (int e = 0; e < 8; ++e) addHat(x, bar + 0.5f * beat * float(e), -18.0f);
        }
    }
    auto t = make(s), r = make(s);
    r->setDuckVoicing(2);
    const Buf y = render(*t, x), yr = render(*r, x);
    Buf lo, hi, lor, hir;
    split(y, lo, hi);
    split(yr, lor, hir);
    Hump h;
    int n = 0;
    for (size_t k = 1; k < kickAt.size(); ++k) {
        const float a = kickAt[k - 1], b = kickAt[k], mid = 0.5f * (a + b);
        const float p0 = b - 0.06f, p1 = b - 0.01f;
        h.pump += rmsDb(y, p0, p1) - rmsDb(y, mid - 0.025f, mid + 0.025f);
        h.pumpLo += rmsDb(lo, p0, p1) - rmsDb(lo, mid - 0.025f, mid + 0.025f);
        h.over += rmsDb(y, p0, p1) - rmsDb(yr, p0, p1);
        h.overLo += rmsDb(lo, p0, p1) - rmsDb(lor, p0, p1);
        ++n;
    }
    h.pump /= n, h.pumpLo /= n, h.over /= n, h.overLo /= n;
    return h;
}

void hump()
{
    double worst = -99.0, worstPump = -99.0;
    for (float att : {0.0f, 0.5f})
        for (bool od : {false, true}) {
            const Hump h = measureHump(att, od);
            worst = std::max(worst, std::max(h.over, h.overLo));
            worstPump = std::max(worstPump, std::max(h.pump, h.pumpLo));
            std::printf("INFO    hump %s %s: just before the kick vs halfway %+.2f dB (lows %+.2f); vs no ducking "
                        "%+.2f dB (lows %+.2f)\n",
                        att == 0.0f ? "CLEAN " : "DRIVEN", od ? "one drop 75 bpm      " : "four on the floor 120", h.pump,
                        h.pumpLo, h.over, h.overLo);
        }
    std::snprintf(msg, sizeof msg,
                  "HOLD ducking is a dip, not a hump: just before each kick the held wet is at most %+.2f dB above halfway "
                  "between kicks (<= +0.5: not rising into the kick) and %+.2f dB over the same drums without ducking (<= +1; four on the floor 120 bpm, one drop 75 bpm, CLEAN / DRIVEN, full band and "
                  "lows)",
                  worstPump, worst);
    check(worst <= 1.0 && worstPump <= 0.5, msg);
}

// The Howl flip (owner, 4 Oct: today's long fade, not a held bed): KICKED
// DECAY 1, ATTITUDE -> CLEAN / DRIVEN: bit for bit the Tank without the Hold.
void howlFlip()
{
    Buf x(sec(12.0f), 0.0f);
    for (float a = 0.5f; a < 5.0f; a += 0.8f) addHit(x, a, -6.0f);
    bool same = true;
    for (float to : {0.0f, 0.5f}) {
        Setup s;
        s.decay    = 1.0f;
        s.attitude = 1.0f;
        auto a = make(s), b = make(s);
        b->setHoldEnabled(false);
        Buf ya(x.size()), yb(x.size()), r(x.size());
        for (size_t p = 0; p < x.size(); p += kBlock) {
            if (p == sec(4.0f)) {
                a->setParam(rv::ParamId::Attitude, to);
                b->setParam(rv::ParamId::Attitude, to);
            }
            a->process(&x[p], &x[p], &ya[p], &r[p], kBlock);
            b->process(&x[p], &x[p], &yb[p], &r[p], kBlock);
        }
        for (size_t i = 0; i < x.size(); ++i) same &= ya[i] == yb[i];
        same &= a->holdWeight() == 0.0f;
    }
    // Re-arming: DECAY out of the zone and back in, still in DRIVEN: holds.
    Setup s;
    s.decay    = 1.0f;
    s.attitude = 1.0f;
    auto t = make(s);
    Buf z(sec(1.0f), 0.0f);
    render(*t, z);
    t->setParam(rv::ParamId::Attitude, 0.5f);
    render(*t, z);
    const bool disarmed = t->holdWeight() == 0.0f;
    t->setParam(rv::ParamId::Decay, 0.8f);
    render(*t, z);
    t->setParam(rv::ParamId::Decay, 1.0f);
    render(*t, z);
    const bool rearmed = t->holdWeight() > 0.99f;
    check(same && disarmed && rearmed,
          "Howl flip: KICKED DECAY 1 -> CLEAN / DRIVEN is bit for bit the Tank without the Hold (today's long fade); "
          "the Hold re-arms once DECAY leaves its zone and comes back");
}

void freezeAndThrowIn()
{
    Setup s;
    s.decay   = 1.0f;
    s.voicing = rv::throwhold::kVoicingFreeze;
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
    throwClicks();
    throwParam();
    leaveThrowMode();
    echoModeRoles();
    zone();
    holds();
    ducking();
    hump();
    howlFlip();
    freezeAndThrowIn();
    layerCreep();
    std::printf("%s (%d failure%s)\n", failures ? "FAILED" : "ALL PASSED", failures, failures == 1 ? "" : "s");
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
