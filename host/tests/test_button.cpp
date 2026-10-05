// The button (ADR 0043; params/ThrowHold.h, Tank::button()). Since the Kick
// went, the button throws in SPRINGS 1-2 and taps the echo's tempo in 3.
// Dependency-free: prints PASS/FAIL lines, returns nonzero on any failure.
//
// THROW by hand (SPRINGS 1-2):
//   1. untouched: releases alone (nothing pressed) leave the Tank bit for bit;
//   2. the first press switches throw mode on; the send is open while held
//      (within 3 ms), closed after the release (within 20 ms); the tail
//      rings on and input after the release never reaches the wet;
//   3. with the gate: open while the gate is high OR the button is held;
//   4. clicks: a 0 dBFS low chord thrown by hand 8x, every ATTITUDE: 0.
// Leaving throw mode (double tap, hold the second press 2 s):
//   5. it exits 2 s after the second press, the LEDs' count moves, the send
//      stays open after the release, the next press throws again;
//      no click (0 dBFS chord);
//   6. never by a single press of any length, fast tapping or slow tapping
//      that ends in a long hold, a short second hold, a slow double tap, a
//      long first press; never in SPRINGS 3;
//   7. block-size independent (1, 7, 48, 333): the whole session bit for bit.
// Tap tempo (SPRINGS 3, echo mode):
//   8. tempo within 1 % (steady and slightly uneven taps), TENSION's
//      divisions of it, held after the taps stop, a lone tap = free time;
//   9. with a gate clock: the last to set a tempo wins (the taps over a
//      steady gate clock, a gate clock that changes tempo over the taps, the
//      taps again when the gate clock is lost); the host's tempo wins over all;
//  10. a press in 3 never throws; presses in 1-2 never tap.
//  11. Renderer automation: "buttons" parsed; "kicks" refused.

#include "Automation.h"
#include "Json.h"
#include "Metrics.h"
#include "dsp/Tank.h"
#include "params/EchoVoicing.h"
#include "params/ThrowHold.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
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

size_t sec(double s) { return size_t(s * double(kFs) + 0.5); }

struct Setup {
    float decay = 0.5f, attitude = 0.0f, mix = 1.0f, splash = 0.0f, tension = 0.5f;
    int   springs = 2;
};

std::unique_ptr<rv::Tank> make(const Setup& s)
{
    auto t = std::make_unique<rv::Tank>();
    t->prepare(kFs, kBlock);
    t->setParam(rv::ParamId::Decay, s.decay);
    t->setParam(rv::ParamId::Tension, s.tension);
    t->setParam(rv::ParamId::Tone, 0.5f);
    t->setParam(rv::ParamId::Splash, s.splash);
    t->setParam(rv::ParamId::Wobble, 0.5f);
    t->setParam(rv::ParamId::Mix, s.mix);
    t->setParam(rv::ParamId::Springs, rv::switchToNormalised(s.springs - 1));
    t->setParam(rv::ParamId::Attitude, s.attitude);
    t->setParam(rv::ParamId::Drive, 0.25f);
    return t;
}

// Events at a sample: the gate (as the firmware sends a rising edge: gate
// high + a clock edge), the button.
enum What { kGateLow, kGateHigh, kDown, kUp };
struct Event {
    size_t at;
    What   what;
};

// Press / release pairs (seconds) -> button events.
std::vector<Event> presses(const std::vector<std::pair<double, double>>& p)
{
    std::vector<Event> ev;
    for (auto [a, b] : p) {
        ev.push_back({sec(a), kDown});
        ev.push_back({sec(b), kUp});
    }
    return ev;
}

void addEvents(std::vector<Event>& to, const std::vector<Event>& more)
{
    to.insert(to.end(), more.begin(), more.end());
    std::stable_sort(to.begin(), to.end(), [](const Event& a, const Event& b) { return a.at < b.at; });
}

// Renders `in` (mono, both sides), events on their samples. probe(t, endSample)
// after every block, if given.
template <class Probe>
Buf render(rv::Tank& t, const Buf& in, const std::vector<Event>& ev, int block, Probe probe)
{
    const size_t n = in.size();
    Buf l(n), r(n);
    size_t e = 0;
    for (size_t p = 0; p < n; p += size_t(block)) {
        const int m = int(std::min<size_t>(size_t(block), n - p));
        while (e < ev.size() && ev[e].at < p + size_t(m)) {
            const int off = int(ev[e].at - p);
            switch (ev[e].what) {
                case kGateLow: t.gate(false, off); break;
                case kGateHigh: t.gate(true, off); t.clock(off); break;
                case kDown: t.button(true, off); break;
                case kUp: t.button(false, off); break;
            }
            ++e;
        }
        t.process(&in[p], &in[p], &l[p], &r[p], m);
        probe(t, p + size_t(m));
    }
    return l;
}
Buf render(rv::Tank& t, const Buf& in, const std::vector<Event>& ev = {}, int block = kBlock)
{
    return render(t, in, ev, block, [](const rv::Tank&, size_t) {});
}

Buf chord(float gainDb, double a, double b, double len, double f0 = 110.0)
{
    Buf x(sec(len), 0.0f);
    const double g = std::pow(10.0, double(gainDb) / 20.0);
    const double f[3] = {f0, f0 * 1.1892, f0 * 1.4983};
    const size_t i0 = sec(a), i1 = std::min(sec(b), x.size()), fade = sec(0.01);
    for (size_t i = i0; i < i1; ++i) {
        const double env = std::min({1.0, double(i - i0) / double(fade), double(i1 - 1 - i) / double(fade)});
        double s = 0.0;
        for (double hz : f) s += std::sin(2.0 * double(kPi) * hz * double(i) / double(kFs));
        x[i] = float(g * env * s / 3.0);
    }
    return x;
}

double rmsDb(const Buf& x, double a, double b)
{
    const size_t i0 = sec(a), i1 = std::min(sec(b), x.size());
    double acc = 0.0;
    for (size_t i = i0; i < i1; ++i) acc += double(x[i]) * double(x[i]);
    return 10.0 * std::log10(acc / double(std::max<size_t>(1, i1 - i0)) + 1e-20);
}

long clicks(const Buf& l) { return rv::metrics::compute({l, l}, kFs).clickCount; }

// ---- 1-4. THROW by hand -------------------------------------------------------------

void untouched()
{
    Buf in = chord(-12.0f, 0.1, 1.0, 2.0);
    bool same = true;
    for (float att : {0.0f, 0.5f, 1.0f}) {
        Setup s;
        s.attitude = att;
        s.splash   = 0.5f;
        auto a = make(s), b = make(s);
        const Buf ya = render(*a, in);
        const Buf yb = render(*b, in, {{sec(0.3), kUp}, {sec(1.2), kUp}});
        for (size_t i = 0; i < ya.size(); ++i) same &= ya[i] == yb[i];
        same &= !b->throwOn() && b->sendGain() == 1.0f;
    }
    check(same, "button untouched: releases alone (nothing pressed) are bit for bit the Tank without the button, every ATTITUDE");
}

void handThrow()
{
    bool ok = true;
    float worstOpen = 0.0f, worstClose = 0.0f;
    for (int springs : {1, 2})
        for (float att : {0.0f, 1.0f}) {
            Setup s;
            s.springs  = springs;
            s.attitude = att;
            auto t = make(s);
            const bool before = !t->throwOn() && t->sendGain() == 1.0f;
            float openAt = -1.0f, closedAt = -1.0f;
            bool  heldOpen = true;
            render(*t, Buf(sec(0.4), 0.0f), presses({{0.1, 0.25}}), kBlock, [&](const rv::Tank& u, size_t end) {
                const double now = double(end) / kFs;
                if (now > 0.1 && now <= 0.25) {
                    if (openAt < 0.0f && u.sendGain() >= 0.999f) openAt = float(now - 0.1);
                    if (now > 0.11) heldOpen &= u.sendGain() >= 0.999f;
                }
                if (now > 0.25 && closedAt < 0.0f && u.sendGain() <= 1e-6f) closedAt = float(now - 0.25);
            });
            ok &= before && t->throwOn() && heldOpen && openAt > 0.0f && closedAt > 0.0f;
            worstOpen  = std::max(worstOpen, openAt < 0.0f ? 1.0f : openAt);
            worstClose = std::max(worstClose, closedAt < 0.0f ? 1.0f : closedAt);
        }
    std::snprintf(msg, sizeof msg,
                  "button = throw (SPRINGS 1 and 2, CLEAN and KICKED): the send is open before the first press; the press "
                  "switches throw mode on, opens in %.1f ms (<= 3) and holds open; the release closes it in %.1f ms (<= 20)",
                  1000.0f * worstOpen, 1000.0f * worstClose);
    check(ok && worstOpen <= 0.003f && worstClose <= 0.020f, msg);

    // The tail rings on after the release; input after it never reaches the wet.
    Setup s;
    s.decay = 0.75f;
    const Buf padLong  = chord(-12.0f, 0.0, 4.0, 5.0);
    Buf       padShort = padLong;
    std::fill(padShort.begin() + long(sec(1.0 + rv::throwhold::kThrowCloseSeconds) + 1), padShort.end(), 0.0f);
    const auto ev = presses({{0.5, 1.0}});
    auto a = make(s), b = make(s);
    const Buf ya = render(*a, padLong, ev), yb = render(*b, padShort, ev);
    double err = 0.0, ref = 0.0;
    for (size_t i = sec(1.05); i < ya.size(); ++i) {
        err += double(ya[i] - yb[i]) * double(ya[i] - yb[i]);
        ref += double(yb[i]) * double(yb[i]);
    }
    const double leakDb = 10.0 * std::log10(err / ref + 1e-30);
    const double tailDb = rmsDb(ya, 1.5, 2.0) - rmsDb(ya, 0.7, 1.0);
    std::snprintf(msg, sizeof msg,
                  "button throw: the tail rings on after the release (%.1f dB 0.5-1 s later, > -30), input after the "
                  "release leaks %.1f dB (< -80)",
                  tailDb, leakDb);
    check(tailDb > -30.0 && leakDb < -80.0, msg);
}

void withGate()
{
    // Gate high 0.2-0.6 s, button held 0.4-1.0 s, gate again 1.3-1.5 s.
    Setup s;
    auto t = make(s);
    std::vector<Event> ev = {{sec(0.2), kGateHigh}, {sec(0.6), kGateLow}, {sec(1.3), kGateHigh}, {sec(1.5), kGateLow}};
    addEvents(ev, presses({{0.4, 1.0}}));
    bool ok = true;
    render(*t, Buf(sec(1.8), 0.0f), ev, kBlock, [&](const rv::Tank& u, size_t end) {
        const double now = double(end) / kFs;
        const bool   open = u.sendGain() >= 0.999f, closed = u.sendGain() <= 1e-6f;
        if (std::fabs(now - 0.5) < 0.0005) ok &= open;   // both
        if (std::fabs(now - 0.8) < 0.0005) ok &= open;   // the button alone
        if (std::fabs(now - 1.15) < 0.0005) ok &= closed; // neither
        if (std::fabs(now - 1.4) < 0.0005) ok &= open;   // the gate alone
        if (std::fabs(now - 1.7) < 0.0005) ok &= closed;
    });
    check(ok && t->throwOn(), "button + gate: the send is open while the gate is high OR the button is held (both, button "
                              "alone, gate alone), closed when neither");
}

void handClicks()
{
    const Buf in = chord(0.0f, 0.0, 6.0, 8.0);
    std::vector<std::pair<double, double>> p;
    for (int k = 0; k < 8; ++k) {
        const double a = 0.3 + 0.7 * k + k * 37 / double(kFs);
        p.push_back({a, a + 0.23 + 0.05 * (k % 3) + k * 11 / double(kFs)});
    }
    const auto ev = presses(p);
    long worst = 0;
    for (float att : {0.0f, 0.5f, 1.0f})
        for (float d : {0.5f, 0.95f}) {
            Setup s;
            s.attitude = att;
            s.decay    = d;
            auto t = make(s);
            worst = std::max(worst, clicks(render(*t, in, ev)));
        }
    std::snprintf(msg, sizeof msg,
                  "button throw clicks: 0 dBFS low chord thrown by hand 8x, CLEAN / DRIVEN / KICKED x DECAY 0.5 / 0.95, "
                  "MIX 1: worst click_count %ld (must be 0)",
                  worst);
    check(worst == 0, msg);
}

// ---- 5-7. Leaving throw mode ----------------------------------------------------------

// Plays presses (after a first throw at 0.1 s, so throw mode is on), then
// reports whether and when throw mode went off.
struct ExitRun {
    bool   exited = false;
    double at     = -1.0; // seconds, the first block end where throwExits() moved
    bool   throwOn = false;
    float  send    = 0.0f;
};
ExitRun exitRun(const std::vector<std::pair<double, double>>& p, double len, int springs = 2, const Buf* in = nullptr,
                Buf* out = nullptr, int block = kBlock)
{
    Setup s;
    s.springs = springs;
    auto t = make(s);
    std::vector<std::pair<double, double>> all = {{0.1, 0.2}};
    all.insert(all.end(), p.begin(), p.end());
    ExitRun r;
    const Buf silence(sec(len), 0.0f);
    const Buf y = render(*t, in ? *in : silence, presses(all), block, [&](const rv::Tank& u, size_t end) {
        if (r.at < 0.0 && u.throwExits() > 0) r.at = double(end) / kFs;
    });
    if (out) *out = y;
    r.exited  = t->throwExits() > 0;
    r.throwOn = t->throwOn();
    r.send    = t->sendGain();
    return r;
}

void exitGesture()
{
    // Tap at 1.0-1.1 s, press again at 1.3 s and hold to 3.6 s: off at 3.3 s.
    {
        Setup s;
        auto t = make(s);
        double at = -1.0, sendAfter = -1.0;
        bool   onBefore = false;
        render(*t, Buf(sec(4.5), 0.0f), presses({{0.1, 0.2}, {1.0, 1.1}, {1.3, 3.6}}), kBlock, [&](const rv::Tank& u, size_t end) {
            const double now = double(end) / kFs;
            if (std::fabs(now - 3.2) < 0.0005) onBefore = u.throwOn() && u.sendGain() >= 0.999f;
            if (at < 0.0 && u.throwExits() > 0) at = now;
            if (std::fabs(now - 4.0) < 0.0005) sendAfter = u.sendGain();
        });
        const double late = at - (1.3 + double(rv::throwhold::kThrowExitHoldSeconds));
        // Then a press throws again (closed after its release).
        render(*t, Buf(sec(0.5), 0.0f), presses({{0.1, 0.2}}));
        const bool again = t->throwOn() && t->sendGain() <= 1e-6f;
        std::snprintf(msg, sizeof msg,
                      "exit: tap, then press and hold: throw mode off %.1f ms after 2 s held (0 .. 1: read at the 1 ms block's end), LED count 1, "
                      "the send stays open after the release (%.3f), the next press throws again",
                      1000.0 * late, sendAfter);
        check(onBefore && late >= 0.0 && late <= 0.0011 && t->throwExits() == 1 && sendAfter == 1.0 && again, msg);
    }
    // No click (0 dBFS chord under the whole gesture), every ATTITUDE.
    {
        const Buf in = chord(0.0f, 0.0, 4.5, 5.0);
        long worst = 0;
        bool exited = true;
        for (float att : {0.0f, 0.5f, 1.0f}) {
            Setup s;
            s.attitude = att;
            auto t = make(s);
            worst = std::max(worst, clicks(render(*t, in, presses({{0.1, 0.2}, {1.0, 1.1}, {1.3, 3.6}}))));
            exited &= t->throwExits() == 1 && !t->throwOn();
        }
        std::snprintf(msg, sizeof msg, "exit under a 0 dBFS low chord, every ATTITUDE: worst click_count %ld (must be 0)", worst);
        check(exited && worst == 0, msg);
    }
}

void noAccidentalExit()
{
    struct Case {
        const char* what;
        std::vector<std::pair<double, double>> p;
    };
    std::vector<Case> cases;
    cases.push_back({"one press held 3 s", {{1.0, 4.0}}});
    {
        Case c{"fast tapping (12 taps, 150 ms apart) ending in a 3 s hold", {}};
        for (int k = 0; k < 12; ++k) c.p.push_back({1.0 + 0.15 * k, 1.0 + 0.15 * k + 0.06});
        c.p.push_back({1.0 + 0.15 * 12, 1.0 + 0.15 * 12 + 3.0});
        cases.push_back(c);
    }
    {
        Case c{"fast tapping (8 taps, 100 ms apart, held 30 ms)", {}};
        for (int k = 0; k < 8; ++k) c.p.push_back({1.0 + 0.1 * k, 1.0 + 0.1 * k + 0.03});
        cases.push_back(c);
    }
    {
        Case c{"slow tapping (6 taps, 800 ms apart, held 300 ms) ending in a 3 s hold", {}};
        for (int k = 0; k < 6; ++k) c.p.push_back({1.0 + 0.8 * k, 1.0 + 0.8 * k + 0.3});
        c.p.push_back({1.0 + 0.8 * 6, 1.0 + 0.8 * 6 + 3.0});
        cases.push_back(c);
    }
    cases.push_back({"double tap, second press held 1.9 s", {{1.0, 1.1}, {1.3, 3.2}}});
    cases.push_back({"slow double tap (0.5 s gap), then 3 s", {{1.0, 1.1}, {1.6, 4.6}}});
    cases.push_back({"long first press (0.5 s), quick second held 3 s", {{1.0, 1.5}, {1.6, 4.6}}});
    cases.push_back({"three taps then a 3 s hold", {{1.0, 1.08}, {1.2, 1.28}, {1.4, 4.4}}});
    bool ok = true;
    for (const auto& c : cases) {
        const double len = c.p.back().second + 0.5;
        const ExitRun r = exitRun(c.p, len);
        const bool good = !r.exited && r.throwOn && r.send <= 1e-6f;
        ok &= good;
        if (!good) std::printf("INFO    exited by: %s\n", c.what);
    }
    // SPRINGS 3: the gesture is just taps there.
    const ExitRun r3 = exitRun({{1.0, 1.1}, {1.3, 4.3}}, 5.0, 3);
    ok &= !r3.exited && !r3.throwOn;
    std::snprintf(msg, sizeof msg,
                  "no exit from a single press of any length, fast or slow tapping ending in a long hold, a 1.9 s second "
                  "hold, a slow double tap, a long first press, three taps + hold (%zu cases; throw mode stays on, the "
                  "send closed), nor from the gesture in SPRINGS 3",
                  cases.size());
    check(ok, msg);
}

void blockSizes()
{
    // Two sessions: SPRINGS 2 (hand throws, the exit, throws again) and
    // SPRINGS 3 (taps, then the echo at the tapped time).
    Buf in = chord(-6.0f, 0.0, 6.0, 7.0);
    const auto throws = presses({{0.1, 0.2}, {0.4, 0.6}, {1.0, 1.1}, {1.3, 3.6}, {4.0, 4.3}});
    const auto taps   = presses({{0.5, 0.55}, {1.0, 1.05}, {1.5, 1.55}, {2.0, 2.05}});
    bool same = true, exited = true, tapped = true;
    for (int springs : {2, 3}) {
        std::vector<Buf> outs;
        for (int b : {48, 1, 7, 333}) {
            Setup s;
            s.springs = springs;
            auto t = make(s);
            outs.push_back(render(*t, in, springs == 2 ? throws : taps, b));
            if (springs == 2) exited &= t->throwExits() == 1;
            else tapped &= t->tapClockInUse();
        }
        for (size_t k = 1; k < outs.size(); ++k) {
            if (outs[k] == outs[0]) continue;
            same = false;
            size_t i = 0;
            while (outs[k][i] == outs[0][i]) ++i;
            std::printf("INFO    SPRINGS %d, block set %zu: first difference at %.4f s\n", springs, k, double(i) / kFs);
        }
    }
    check(same && exited && tapped, "button sessions (SPRINGS 2: throws, the exit, throws; SPRINGS 3: taps): bit-identical "
                                    "for blocks 48, 1, 7, 333");
}

// ---- 8-10. Tap tempo ------------------------------------------------------------------

// The beat the echo uses now (seconds), from its time and TENSION's division; 0 = free time.
double beatNow(const rv::Tank& t)
{
    const int d = t.echoDivision();
    return d < 0 ? 0.0 : double(t.echoSeconds()) / double(rv::echo::kDivisionBeats[size_t(d)]);
}

// SPRINGS 3 with taps (seconds, 50 ms presses) and gate clock edges (10 ms
// pulses); beats sampled at `at` (seconds).
std::vector<double> tapRun(const std::vector<double>& taps, const std::vector<double>& gates, const std::vector<double>& at,
                           double len, float tension = 0.5f, float hostBpm = 0.0f, int springs = 3, rv::Tank** keep = nullptr)
{
    Setup s;
    s.springs = springs;
    s.tension = tension;
    static std::unique_ptr<rv::Tank> held;
    held = make(s);
    rv::Tank& t = *held;
    t.setHostTempo(hostBpm);
    std::vector<Event> ev;
    for (double a : taps) {
        ev.push_back({sec(a), kDown});
        ev.push_back({sec(a + 0.05), kUp});
    }
    for (double g : gates) {
        ev.push_back({sec(g), kGateHigh});
        ev.push_back({sec(g + 0.01), kGateLow});
    }
    std::stable_sort(ev.begin(), ev.end(), [](const Event& a, const Event& b) { return a.at < b.at; });
    std::vector<double> beats(at.size(), -1.0);
    render(t, Buf(sec(len), 0.0f), ev, kBlock, [&](const rv::Tank& u, size_t end) {
        for (size_t k = 0; k < at.size(); ++k)
            if (beats[k] < 0.0 && end >= sec(at[k])) beats[k] = beatNow(u);
    });
    if (keep) *keep = held.get();
    return beats;
}

void tapTempo()
{
    // Steady taps at 120 bpm, and slightly uneven ones (+-3 ms).
    {
        const auto steady = tapRun({1.0, 1.5, 2.0, 2.5}, {}, {2.6}, 3.0);
        const auto uneven = tapRun({1.0, 1.502, 1.998, 2.503, 3.0}, {}, {3.1}, 3.5);
        const double e1 = std::fabs(steady[0] / 0.5 - 1.0), e2 = std::fabs(uneven[0] / 0.5 - 1.0);
        std::snprintf(msg, sizeof msg,
                      "tap tempo (SPRINGS 3): 4 taps at 120 bpm -> beat %.4f s (%.2f %% off), uneven taps (+-3 ms) -> %.4f s "
                      "(%.2f %% off; <= 1 %%)",
                      steady[0], 100.0 * e1, uneven[0], 100.0 * e2);
        check(e1 <= 0.01 && e2 <= 0.01, msg);
    }
    // TENSION's divisions of the tapped beat (as a gate clock's).
    {
        bool ok = true;
        for (int z = 0; z < rv::echo::kNumDivisions; ++z) {
            rv::Tank* t = nullptr;
            tapRun({1.0, 1.5, 2.0, 2.5}, {}, {2.6}, 3.0, (float(z) + 0.5f) / float(rv::echo::kNumDivisions), 0.0f, 3, &t);
            const double want = rv::echo::kDivisionBeats[size_t(z)] * 0.5;
            ok &= t->echoDivision() == z && std::fabs(t->echoSeconds() - want) < 1e-3 * want;
        }
        check(ok, "tap tempo: TENSION's seven zones are the same divisions of the tapped beat as of a gate clock (2 .. 1/16 beat)");
    }
    // Held after the taps stop; a lone tap lets it go (free time).
    {
        const auto b = tapRun({1.0, 1.5, 2.0, 2.5, 14.0}, {}, {12.0, 15.0, 17.0}, 17.5);
        std::snprintf(msg, sizeof msg,
                      "tap tempo holds after the taps stop (9.5 s later: beat %.3f s); a lone tap later leaves it for "
                      "now (1 s on: %.3f s), then free time ~2 s on (%.3f, 0 = free)",
                      b[0], b[1], b[2]);
        check(std::fabs(b[0] - 0.5) < 1e-3 && std::fabs(b[1] - 0.5) < 1e-3 && b[2] == 0.0, msg);
    }
}

void tapWithGateClock()
{
    // Gate clock at 100 bpm (0.6 s) from 0 to 8 s, at 90 bpm (0.667 s) from
    // 8 to 14 s, then none. Taps at 120 bpm 4.0 .. 5.5 s.
    std::vector<double> gates;
    for (double g = 0.0; g < 8.0; g += 0.6) gates.push_back(g);
    for (double g = 8.0; g < 14.0; g += 60.0 / 90.0) gates.push_back(g);
    const auto b = tapRun({4.0, 4.5, 5.0, 5.5}, gates, {3.9, 7.5, 12.0, 17.5}, 18.0);
    std::snprintf(msg, sizeof msg,
                  "taps with a gate clock, the last to set a tempo wins: gate 100 bpm -> %.3f s; taps at 120 over the "
                  "steady gate -> %.3f s; the gate changes to 90 bpm -> %.3f s; the gate lost -> the tapped tempo %.3f s",
                  b[0], b[1], b[2], b[3]);
    check(std::fabs(b[0] - 0.6) < 0.006 && std::fabs(b[1] - 0.5) < 0.005 && std::fabs(b[2] - 60.0 / 90.0) < 0.007
              && std::fabs(b[3] - 0.5) < 0.005,
          msg);
    // The host's tempo (the Plugin) wins over taps.
    const auto h = tapRun({1.0, 1.5, 2.0, 2.5}, {}, {2.6}, 3.0, 0.5f, 100.0f);
    std::snprintf(msg, sizeof msg, "the host's tempo (Plugin, 100 bpm) wins over taps at 120: beat %.3f s", h[0]);
    check(std::fabs(h[0] - 0.6) < 1e-3, msg);
}

void rolesBySprings()
{
    // In 3 a press never throws.
    rv::Tank* t = nullptr;
    tapRun({1.0, 1.5, 2.0}, {}, {2.1}, 2.5, 0.5f, 0.0f, 3, &t);
    const bool noThrow = !t->throwOn() && t->sendGain() == 1.0f && t->tapClockInUse();
    // In 2 presses throw and never tap: switch to 3 afterwards, free time.
    Setup s;
    auto u = make(s);
    std::vector<std::pair<double, double>> p;
    for (double a : {1.0, 1.5, 2.0, 2.5}) p.push_back({a, a + 0.05});
    render(*u, Buf(sec(3.0), 0.0f), presses(p));
    const bool threw = u->throwOn();
    u->setParam(rv::ParamId::Springs, rv::switchToNormalised(2));
    render(*u, Buf(sec(0.5), 0.0f));
    const bool free = u->echoDivision() < 0 && !u->tapClockInUse();
    check(noThrow && threw && free, "SPRINGS 3: a press is a tap, never a throw; SPRINGS 1-2: presses throw, never tap");
}

void automation()
{
    rv::json::Value v;
    rv::automation::Automation au;
    std::string err;
    const bool ok = rv::json::parse(R"({"buttons": [[2.0, 2.5], [0.5, 0.75]]})", v, err) && rv::automation::parse(v, au, err);
    const bool order = ok && au.buttonEvents.size() == 4 && au.buttonEvents[0] == std::make_pair(0.5, true)
                       && au.buttonEvents[1] == std::make_pair(0.75, false) && au.buttonEvents[3] == std::make_pair(2.5, false);
    rv::json::Value k;
    rv::automation::Automation ak;
    std::string kerr;
    const bool refused = rv::json::parse(R"({"kicks": [1.0]})", k, kerr) && !rv::automation::parse(k, ak, kerr)
                         && kerr.find("0043") != std::string::npos;
    rv::json::Value bad;
    std::string berr;
    const bool badPair = rv::json::parse(R"({"buttons": [[1.0, 0.5]]})", bad, berr) && !rv::automation::parse(bad, ak, berr);
    check(order && refused && badPair, "Renderer automation: \"buttons\" [press, release] parsed in time order; a release "
                                       "before its press refused; \"kicks\" refused (the Kick was removed, ADR 0043)");
}

} // namespace

int main()
{
    untouched();
    handThrow();
    withGate();
    handClicks();
    exitGesture();
    noAccidentalExit();
    blockSizes();
    tapTempo();
    tapWithGateClock();
    rolesBySprings();
    automation();
    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
