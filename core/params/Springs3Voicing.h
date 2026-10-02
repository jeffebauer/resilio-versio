#pragma once
// SPRINGS 3 palette (PROTOTYPE, branch proto/springs3-palette; ADR 0037
// proposed). Owner, 1 Oct 2026: "There isn't a very noticeable difference
// between 2 springs and 3 springs ... make the 3 spring option more distinct,
// or even a different approach, for a broader sonic palette." Positions 1 and
// 2 stay as they are.
//
// Why it's subtle today: the three Springs are near copies (SpringModes.h
// kDetune: L, fC and a within ±8 %, a step darker and shorter each), and
// position 3 only adds Spring C in the centre, so a third Spring mostly adds
// density.
//
// A hidden, Renderer-only key picks what position 3 does (Tank::
// setSprings3Voicing, "springs3_voicing" in a sweep or --set). The firmware
// and the plugin never call it: they play kDefaultVoicing (0 = today).
//
//   0  today        reference: positions 1-3 exactly as on main
//   1  long tank    a big "long decay" tank: Springs 1.5x longer (a slower,
//                   deeper drip; the first echo ~15 ms later), a lower Chirp
//                   (fC x0.8: a lower boing), darker (damping x0.7, the fast
//                   high echoes x0.6) and a quarter longer tail
//   2  in series    the dub trick of two tanks in a row: Spring A plays as
//                   today, and its output (not the dry input) feeds the Loops
//                   of Springs B and C, the second tank, left and right.
//                   Thicker, more washed, a doubled boing, a smoother tail.
//                   The send is low-cut at 120 Hz (no boom built twice) and
//                   follows DECAY; the second tank is short (B and C x0.3:
//                   a smear on every echo) and A x0.7 (the pair reads ~25 %
//                   shorter than today at the same DECAY); B and C's fast high echoes still come from the
//                   input (echoed twice they rang at 5-6 kHz).
//   3  wide         three clearly different Springs hard left / centre /
//                   hard right: A short and bright on the left, C medium in
//                   the centre, B long and dark on the right (lengths 0.82 :
//                   0.93 : 1.05, today 0.965 : 0.925 : 1.05), so the drips
//                   bounce across the stereo field
//   4  pan tank     a different tank type: a short, bright, metallic small
//                   "pan" tank: Springs 0.42x as long (never under 20 ms), a
//                   higher Chirp (fC x1.25), bright (damping x1.7, the fast
//                   high echoes x2.2), half the tail, thin (a 100 Hz low cut
//                   on its input: a small tank has little bass). Fewer Chirp
//                   stages (cap 40), so it costs less than today.
//
// Round 2 (2 Oct 2026). Owner, after round 1: a different tank size moves the
// repeat timing, and that muddles TENSION and DECAY. So every round 2
// voicing keeps today's Spring lengths and repeat timing: each Spring's round
// trip at modes::kPickupAlignHz (800 Hz) and its first echo land where
// today's position 3 puts them (keepTiming). A Chirp change moves the time
// the Chirp chain takes by up to ~1 ms (about 1 % of a round trip); the
// Spring's delay is lengthened or shortened by exactly that much, and its
// pickup moved, so the echoes keep today's times. (At the loosest TENSION
// the right Spring can't grow past the delay memory: there a voicing whose
// Chirp is quicker repeats up to ~1 % sooner; the test reports it.)
//
//   5  pan, brighter only    the pan tank's tonal balance (less bass, a
//                   brighter tail: damping x1.7, fast high echoes x2.2, a
//                   100 Hz low cut on the input) at today's lengths, Chirp and
//                   tail length
//   6  pan, higher Chirp only the pan tank's Chirp (fC x1.25, 40 stages: a
//                   higher, quicker, more metallic boing) at today's lengths,
//                   brightness and tail length. 5 and 6 together answer "was it
//                   the brightness or the boing?"
//   7  mixed wire gauges  a real 3-Spring tank with three wire thicknesses:
//                   same lengths, each Spring its own Chirp (left: crisp, a
//                   short boing; right: lower and longer; centre: a thin wire's
//                   high boing), so a hit gives a cluster of boings while the
//                   echoes land on today's times. Left and right equally bright.
//   8  coupled      the three Springs share energy each round trip, as the
//                   shared transducer and frame couple a real tank: a hit's
//                   echoes multiply and bloom over the first ~0.5 s instead of
//                   dripping. Energy-preserving (a rotation of the three
//                   Loops' returns), so it can't run away (couplingAngle).
//                   (The physical coupling is through the shared frame and
//                   transducer; here it is the Loops' returns, every trip.)
//   9  diffuse      more smear in each Spring: a steeper Chirp (a x1.18) and
//                   more stages where today runs fewer (half way to the cap:
//                   the loosest tank, today's worst case for CPU, is
//                   unchanged), and softer fast echoes (x0.6): a smoother,
//                   softer-edged tail at the same timing
//   10 cross-fed wide (owner's idea) bright left, today's centre, dark right,
//                   all at today's lengths, and the left and right Loops feed
//                   each other (a rotation, as 8, between those two only), so
//                   each ear hears both colours: wide, without the lean
//
// Held sounds (ADR 0035): the long, wide and pan tanks build up more on some
// held notes than today's, so their Sustain trim may cut up to 8 dB (5 dB
// today) before the limiter has to; test_springs3 "sustain".
//
// Memory: no voicing needs more delay memory than today. A Spring's delay
// line is sized for the loosest tank (map::kLoopDelayMaxSeconds x the
// longest detune, SpringModes.h): the long tank's L is capped at the loose
// tank's L *before* the per-Spring detune, so the Springs stay detuned
// against each other and fit. It is the full 1.5x up to TENSION about noon
// (69 ms -> 103 ms) and reaches the cap (110 ms) a little looser than noon;
// looser than that it is only darker, lower and longer than position 2.
// The full 1.5x range would need ~12,000 more floats (48 KB) for the three
// Loops: see ADR 0037 for where they could live.
//
// Switching (ADR 0003): the output mix still fades over 20 ms. The Springs'
// own settings (L, fC, damping, T60, stages, the series feed) glide to the
// voicing over kGlideSeconds; L is slew-limited as for DECAY (ADR 0012), so
// switching to and from voicings 1 and 4 bends the live tail's pitch for a
// moment (a tape-like swoop), never a click.
//
// Every number for these voicings lives here; tuning by ear edits this file.

#include "params/Mappings.h"
#include "params/SpringModes.h"

#include <array>
#include <cstddef>

namespace rv::springs3 {

// Built into the desktop hosts (Renderer, tests, plugin) only. The firmware
// builds (firmware/Makefile's RV_MODE_*) play today's position 3 and carry
// none of the palette's code: it is ~3 KB on the Cortex-M7 and the release
// firmware has ~4.5 KB of flash left. The voicing the owner picks ships
// without the gate (and without the other voicings).
// Same switch as the other Renderer-only voicings (core/dsp/Drive.h
// RV_FIXED_VOICINGS, set by firmware/Makefile); the RV_MODE_* check stays as
// a second guard.
#if defined(RV_FIXED_VOICINGS) || defined(RV_MODE_RELEASE) || defined(RV_MODE_PROFILE) || defined(RV_MODE_M0TEST)
constexpr bool kPaletteBuilt = false;
#else
constexpr bool kPaletteBuilt = true;
#endif

constexpr int kToday = 0, kLong = 1, kSeries = 2, kWide = 3, kPan = 4;
constexpr int kPanBright = 5, kPanChirp = 6, kGauges = 7, kCoupled = 8, kDiffuse = 9, kCrossWide = 10;
constexpr int kNumVoicings    = 11;
constexpr int kDefaultVoicing = kToday; // firmware + plugin (owner picks)

// Settings glide this long into and out of a voicing (as DECAY, ADR 0015).
constexpr float kGlideSeconds = 0.08f;

// In series, the second tank is fed the first one's whole tail, so it
// builds up more the more echoes that tail holds (a Loop's energy for a hit
// grows with T60 / L): with a fixed send (and the first, long second tank),
// hits came back -1.3 dB (DECAY 0.1) to +5.9 dB (DECAY 1, tight tank)
// against SPRINGS 2. The send follows (ratio at the reference / (T60 /
// L))^kSeriesExponent instead (T60 = DECAY's, L = TENSION's), reference
// DECAY ~0.4 on the noon tank: within +-1.2 dB of SPRINGS 2 on hits
// (K-weighted), DECAY 0.1-1 x TENSION 0 and 1 (test_springs3).
constexpr float kSeriesRefT60    = 1.5f;
constexpr float kSeriesRefLoop   = map::kTensionMidLoopDelaySeconds;
constexpr float kSeriesExponent  = 0.15f;

// One Spring's shape in a voicing: multipliers on the shared settings, like
// modes::Detune (which they replace in position 3), plus the high path's level.
struct Shape {
    float loopDelay;
    float transition;
    float allpassCoeff;
    float damping;
    float decay;
    float highPath;
    float pickupOffset; // seconds, replaces modes::kPickupOffsetSeconds (glides, like TENSION's)
};

// Today's Spring (modes::kDetune), optionally with every Spring's damping,
// decay and high path scaled alike (the long and pan tanks keep today's
// detune between Springs).
constexpr Shape fromDetune(size_t i, float damping = 1.0f, float decay = 1.0f, float highPath = 1.0f)
{
    const modes::Detune& d = modes::kDetune[i];
    return {d.loopDelay, d.transition, d.allpassCoeff, d.damping * damping, d.decay * decay, highPath,
            modes::kPickupOffsetSeconds[i]};
}
constexpr std::array<Shape, modes::kNumSprings> detuned(float damping = 1.0f, float decay = 1.0f, float highPath = 1.0f)
{
    return {{fromDetune(0, damping, decay, highPath), fromDetune(1, damping, decay, highPath), fromDetune(2, damping, decay, highPath)}};
}

struct Voicing {
    float lengthScale;     // x the shared L (TENSION), capped at map::kLoopDelayMaxSeconds before the detune
    float minLoopSeconds;  // ... and floored here (0 = none; only ever raises a shortened L back toward TENSION's)
    float transitionScale; // x the shared fC (TENSION)
    std::array<Shape, modes::kNumSprings> spring;
    int   stageCap;        // Chirp stages per Spring at TENSION 0 (modes::kStageCap[2] = 52 today)
    float series;          // 1 = Springs B and C's Loops hear Spring A's output instead of the input
    float seriesSend;      // gain on Spring A's output into B and C, at kSeriesRefT60
    float seriesLowCutHz;  // one-pole low cut on that send (0 = none)
    float inputLowCutHz;   // one-pole low cut on every Spring's input (0 = none)
    float sustainMaxDb;    // the Sustain trim's ceiling (ADR 0035; drive::kSusGentleMaxDb = 5 today)
    modes::StereoMix mix;  // output matrix (mid/side/D, SpringModes.h), normalised by the Tank as today
    float trim;            // level, x modes::kModeTrim[2]
    // Round 2:
    bool  keepTiming = false;  // today's round trip (at kPickupAlignHz) and first echo per Spring, whatever the Chirp
    float stageBoost = 0.0f;   // Chirp stages this share of the way from TENSION's count to stageCap (the cap: same worst case)
    float couplingAngle = 0.0f; // the Loops share energy: a rotation by this angle (radians) of their returns, every trip
    int   couplingKind  = 0;    // kCoupleNone, kCoupleAll (A, B, C) or kCoupleLeftRight (A and B)
};
constexpr int kCoupleNone = 0, kCoupleAll = 1, kCoupleLeftRight = 2;

// Today's 3-Spring output matrix (modes::stereoMix(2)), for the voicings that keep it.
constexpr modes::StereoMix kTodayMix{{0.5f, 0.5f, modes::kCentre3}, {modes::kSide3, -modes::kSide3, 0.0f}, modes::kDecorr3};

// In series B and C are the pair you hear (left / right): C's pickup trim
// (-1.2 ms, there to sit between A and B on held chords) put the two near
// copies 1.35 ms apart, a comb notch at ~370 Hz in mono (-10.7 dB on
// stabs). In series C sits where A does (0 ms), 0.15 ms from B, as A and B
// do in position 2.
// A resonance both tanks share comes out twice as strong (the product), so
// at DECAY max a tight tank held one note for 3-4 s and the M6 grid flagged
// 10 cells (Ringing up to 21 dB). The second tank is now short (B, C x0.3 of
// DECAY's tail: a smear on every echo, the doubled boing), detuned further
// from A (B fC x0.88, C fC x1.12; a different C length moved its first echo
// off B's: -9.3 dB mono notch on stabs) and A x0.7; the send's
// DECAY-following is gentler (kSeriesExponent 0.15) and the level trim x1.2
// (+1.6 dB). Ringing then passes the M6 grid; 4 cells still hold a steady
// tone 2-2.5 s on hot bursts at DECAY max (ADR 0037).
constexpr Shape kSeriesB = [] {
    Shape b      = fromDetune(1, 1.0f, 0.3f);
    b.transition = 0.88f;
    return b;
}();
constexpr Shape kSeriesC = [] {
    Shape c        = fromDetune(2, 1.0f, 0.3f);
    c.transition   = 1.12f;
    c.pickupOffset = modes::kPickupOffsetSeconds[0];
    return c;
}();

// The long tank: today's detune, darker, longer, fewer fast echoes. C's
// pickup trim (-1.2 ms, tuned on today's lengths) left the 1.5x longer,
// lower Chirps half-cancelling in mono on stabs at DECAY 0 (-6.1 dB at
// ~400 Hz); at 0 ms, with A, it reads -2.9.
constexpr std::array<Shape, modes::kNumSprings> kLongSprings = [] {
    auto s = detuned(0.7f, 1.25f, 0.6f);
    s[2].pickupOffset = modes::kPickupOffsetSeconds[0];
    return s;
}();

// ---- Round 2 numbers ----
// 5: the pan tank's brightness (round 1 voicing 4) without its size.
constexpr float kPanBrightDamping = 1.7f, kPanBrightHigh = 2.2f;
constexpr float kPanBrightTrim    = 1.0f;
// 6: the pan tank's Chirp without its size.
constexpr float kPanChirpTrim = 1.0f;
// 7: wire gauges. Length, Chirp frequency fC, steepness a, damping, tail,
// high path, pickup trim. Left (A): steepness x0.86, a crisp, short boing.
// Right (B): fC x0.94 and steepness x1.16, a lower, longer boing. Centre
// (C): fC x1.22, the thin wire's high boing. Left and right get the same
// damping (today: the right one darker), so neither ear is brighter.
inline constexpr std::array<Shape, modes::kNumSprings> kGaugeSprings{{
    {modes::kDetune[0].loopDelay, 1.00f, 0.86f, 0.92f, 1.000f, 1.0f, modes::kPickupOffsetSeconds[0]},
    {modes::kDetune[1].loopDelay, 0.94f, 1.16f, 0.92f, 0.930f, 1.0f, modes::kPickupOffsetSeconds[1]},
    {modes::kDetune[2].loopDelay, 1.22f, 1.00f, 0.72f, 0.865f, 1.0f, modes::kPickupOffsetSeconds[2]},
}};
constexpr float kGaugesTrim = 1.0f;
// 8: how much the Loops share per trip: their returns turn by this angle
// (40 degrees) every round trip, about kCoupleAxis (Tank::coupleMatrix).
// 0.15 is barely coupled; about (1, 1, 1) (the sum untouched) a shared
// resonance held a steady tone (test_springs3 "ringing").
constexpr float kCoupledAngle = 0.7f;
inline constexpr std::array<float, 3> kCoupleAxis{{0.26726124f, 0.53452248f, 0.80178373f}}; // (1, 2, 3) / sqrt 14
constexpr float kCoupledTrim  = 1.0f;
// 9: diffuse.
constexpr float kDiffuseSteep = 1.18f, kDiffuseHigh = 0.6f, kDiffuseStageBoost = 0.5f;
inline constexpr std::array<Shape, modes::kNumSprings> kDiffuseSprings = [] {
    auto s = detuned(1.0f, 1.0f, kDiffuseHigh);
    for (auto& x : s) x.allpassCoeff *= kDiffuseSteep;
    return s;
}();
constexpr float kDiffuseTrim = 1.0f;
// 10: cross-fed wide. A (left) bright, C (centre) today's, B (right) dark,
// all at today's lengths; A and B's Loops turn into each other by
// kCrossWideAngle every trip. Today's image (A left, C centre, B right):
// round 1's wide matrix (side 0.42, less D) read L/R correlation 0.56 on
// stabs at DECAY 0 here (limit 0.5): with today's lengths the first echoes
// line up left and right.
inline constexpr std::array<Shape, modes::kNumSprings> kCrossWideSprings{{
    {modes::kDetune[0].loopDelay, 1.07f, 1.03f, 1.20f, 1.0f, 1.25f, modes::kPickupOffsetSeconds[0]}, // bright (left)
    {modes::kDetune[1].loopDelay, 0.93f, 0.96f, 0.72f, 0.965f, 0.75f, modes::kPickupOffsetSeconds[1]}, // dark (right)
    fromDetune(2),                                                                                       // today's (centre)
}};
constexpr float kCrossWideAngle = -0.785f;
constexpr float kCrossWideTrim  = 1.05f;

inline constexpr std::array<Voicing, kNumVoicings> kVoicings{{
    // 0 today: never applied (the Tank plays SpringModes.h as on main).
    {1.0f, 0.0f, 1.0f, detuned(), modes::kStageCap[2], 0.0f, 0.0f, 0.0f, 0.0f, 5.0f, kTodayMix, 1.0f},
    // 1 long tank
    {1.5f, 0.0f, 0.8f, kLongSprings, modes::kStageCap[2], 0.0f, 0.0f, 0.0f, 0.0f, 8.0f, kTodayMix, 1.0f},
    // 2 in series: A centre (the first tank's drip, so a hit keeps its
    // front), B left and C right (the second tank).
    {1.0f, 0.0f, 1.0f,
     {{fromDetune(0, 1.0f, 0.7f), kSeriesB, kSeriesC}},
     modes::kStageCap[2], 1.0f, 0.95f, 120.0f, 0.0f, 5.0f,
     {{0.4f, 0.5f, 0.5f}, {0.0f, 0.33f, -0.33f}, modes::kDecorr3}, 1.2f},
    // 3 wide: lengths and colours well apart, darker = shorter as ADR 0027
    // (so beating pairs still part). Left / centre / right: L = 0.92 A +
    // 0.08 B + c C (side 0.42; at 0.5, hard pan, a single Spring per side
    // peaked higher on held chords), less D needed. A at 0.82 rather than
    // 0.80: at 0.80 an organ chord's attack met its modes (limiter 4.7 dB
    // for a moment vs 1.6; test_springs3 "sustain").
    {1.0f, 0.0f, 1.0f,
     {{
         {0.82f, 1.10f, 1.03f, 1.10f, 1.00f, 1.2f, modes::kPickupOffsetSeconds[0]}, // A: short, bright, longest tail (left)
         {1.05f, 0.90f, 0.96f, 0.70f, 0.85f, 0.8f, modes::kPickupOffsetSeconds[1]}, // B: long, dark, shortest tail   (right)
         {0.93f, 1.00f, 1.07f, 0.85f, 0.92f, 1.0f, modes::kPickupOffsetSeconds[2]}, // C: in between                   (centre)
     }},
     modes::kStageCap[2], 0.0f, 0.0f, 0.0f, 0.0f, 8.0f,
     {{0.5f, 0.5f, 0.5f}, {0.42f, -0.42f, 0.0f}, 0.5f}, 0.95f},
    // 4 pan tank. fC x1.25 and the 20 ms floor (were x1.35, 18 ms): the
    // tightest pan tank rang at 943 Hz with WOBBLE fully left (ringing_db
    // 24.9); now 8.1 at worst. K-weighted it is a little louder than
    // SPRINGS 2 loose (bright) and a little quieter tight (no tight-tank
    // bass bump); test_springs3 "level".
    {0.42f, 0.02f, 1.25f, detuned(1.7f, 0.5f, 2.2f), 40, 0.0f, 0.0f, 0.0f, 100.0f, 8.0f, kTodayMix, 0.97f},
    // ---- Round 2: today's lengths and repeat timing (keepTiming) ----
    // 5 pan, brighter only: the pan tank's damping, high path and low cut.
    {1.0f, 0.0f, 1.0f, detuned(kPanBrightDamping, 1.0f, kPanBrightHigh), modes::kStageCap[2], 0.0f, 0.0f, 0.0f, 100.0f,
     5.0f, kTodayMix, kPanBrightTrim, true},
    // 6 pan, higher Chirp only: the pan tank's fC and stage cap.
    {1.0f, 0.0f, 1.25f, detuned(), 40, 0.0f, 0.0f, 0.0f, 0.0f, 5.0f, kTodayMix, kPanChirpTrim, true},
    // 7 mixed wire gauges
    {1.0f, 0.0f, 1.0f, kGaugeSprings, modes::kStageCap[2], 0.0f, 0.0f, 0.0f, 0.0f, 5.0f, kTodayMix, kGaugesTrim, true},
    // 8 coupled
    {1.0f, 0.0f, 1.0f, detuned(), modes::kStageCap[2], 0.0f, 0.0f, 0.0f, 0.0f, 5.0f, kTodayMix, kCoupledTrim, true, 0.0f,
     kCoupledAngle, kCoupleAll},
    // 9 diffuse
    {1.0f, 0.0f, 1.0f, kDiffuseSprings, modes::kStageCap[2], 0.0f, 0.0f, 0.0f, 0.0f, 5.0f, kTodayMix, kDiffuseTrim, true,
     kDiffuseStageBoost},
    // 10 cross-fed wide
    {1.0f, 0.0f, 1.0f, kCrossWideSprings, modes::kStageCap[2], 0.0f, 0.0f, 0.0f, 0.0f, 5.0f, kTodayMix, kCrossWideTrim,
     true, 0.0f, kCrossWideAngle, kCoupleLeftRight},
}};

// Round 2 must keep today's lengths: every keepTiming voicing plays each
// Spring at today's detuned length (the delay is then nudged by the Chirp
// difference only, in the Tank).
constexpr bool keepsLengths()
{
    for (const auto& v : kVoicings) {
        if (!v.keepTiming) continue;
        if (v.lengthScale != 1.0f || v.minLoopSeconds != 0.0f || v.series != 0.0f) return false;
        for (size_t i = 0; i < v.spring.size(); ++i)
            if (v.spring[i].loopDelay != modes::kDetune[i].loopDelay) return false;
    }
    return true;
}
static_assert(keepsLengths(), "a round 2 voicing changes a Spring's length");

// Memory: no voicing's Spring may be longer than the delay line holds
// (Spring.cpp sizes it for map::kLoopDelayMaxSeconds x the longest detune).
constexpr bool fitsMemory()
{
    for (const auto& v : kVoicings)
        for (const auto& s : v.spring)
            if (s.loopDelay > modes::kMaxLoopDelayDetune) return false;
    return true;
}
static_assert(fitsMemory(), "a voicing's Spring is longer than the delay memory");
// The allpass rings hold K up to ~13 at 48 kHz (Spring.cpp ringSize, clamp):
// the lowest fC a voicing asks for must stay above fs / 26.
constexpr bool fitsRings()
{
    for (const auto& v : kVoicings)
        for (const auto& s : v.spring)
            if (map::kTransitionMinHz * v.transitionScale * s.transition < 1900.0f) return false;
    return true;
}
static_assert(fitsRings(), "a voicing's Chirp is lower than the allpass rings hold");
// CPU: no voicing may cost more Chirp stages than today's 3 Springs.
constexpr bool fitsCpu()
{
    for (const auto& v : kVoicings)
        if (v.stageCap > modes::kStageCap[2]) return false;
    return true;
}
static_assert(fitsCpu(), "a voicing has more Chirp stages than today's 3-Spring worst case");

} // namespace rv::springs3
