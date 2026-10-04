// Plugin Host's panel (docs/briefs/plugin-panel-ui.md): not a custom design,
// just JUCE's standard knobs, buttons and text placed where the Versio's
// parts sit, plus the four LED meters running the release firmware's own
// meter maths (firmware/LedMeter.h, ADR 0031) so a clipping input or a
// limiting output shows red at the desk the way it does on the module.

#include "PluginEditor.h"

#include "../firmware/LedMeter.h"
#include "params/ParamSpec.h"
#include "params/ThrowHold.h"
#include "params/WobbleVoicing.h"

#include <cmath>

namespace rv::plugin {

namespace {

// ---- Panel geometry, in mm from the panel's top-left corner ----------------
// Copied from tools/make_panel_svg.py (Noise Engineering's official Versio
// template); the pot -> function order is ADR 0028's, as drawn by
// tools/make_panel_mapping_svg.py.
constexpr float kPanelW   = 50.5f;  // 10 HP as cut
constexpr float kPanelH   = 128.5f; // 3U
constexpr float kPxPerMm  = 5.0f;   // at 1x: 253 x 643 px
constexpr int   kWidthPx  = 253;
constexpr int   kHeightPx = 643;

struct Part {
    float x, y; // centre, mm
};

struct Knob {
    ParamId id;
    Part    at;
    bool    labelAbove; // P6: the toggles sit below it
};

constexpr Knob kKnobs[] = {
    {ParamId::Mix,     {7.770f, 18.530f}, false}, // P1
    {ParamId::Decay,   {43.330f, 18.530f}, false}, // P2
    {ParamId::Tone,    {25.169f, 28.690f}, false}, // P3
    {ParamId::Splash,  {7.770f, 39.485f}, false}, // P4
    {ParamId::Tension, {43.330f, 39.485f}, false}, // P5
    {ParamId::Wobble,  {25.169f, 49.328f}, true},  // P6
    {ParamId::Drive,   {43.330f, 60.440f}, false}, // P7
};

// SW1 = top toggle = SPRINGS, SW2 = ATTITUDE (ADR 0028, measured at M0).
// The hardware toggles throw left / centre / right: choices[0..2].
struct Toggle {
    ParamId id;
    Part    at;
};
constexpr Toggle kToggles[] = {
    {ParamId::Springs,  {8.405f, 57.900f}}, // SW1
    {ParamId::Attitude, {8.405f, 67.425f}}, // SW2
};

constexpr Part kButton{26.185f, 69.330f};
// THROW (ADR 0039): the gate jack's stand-in, not on the printed panel. Right
// of the KICK button, below DRIVE.
constexpr Part kThrow{37.5f, 69.330f};

// LED1..LED4, left to right: In L, In R, Out L, Out R (PanelLink::Meter order).
constexpr Part kLeds[PanelLink::kNumMeters] = {
    {17.295f, 19.800f}, {22.375f, 19.800f}, {29.360f, 19.800f}, {34.440f, 19.800f}};

constexpr float kKnobSizeMm    = 12.5f; // slider bounds; the cap on the module is ~11.6 mm
constexpr float kLedRadiusMm   = 1.8f;  // the hole is 3 mm
constexpr float kSegmentWMm    = 6.8f;  // one toggle position (three side by side)
constexpr float kSegmentHMm    = 4.5f;
constexpr float kSegmentLeftMm = 1.5f;  // the three end left of the KICK button
constexpr float kButtonMm      = 7.0f;
constexpr float kLabelHMm      = 2.6f;

constexpr float px(float mm) { return mm * kPxPerMm; }

juce::Rectangle<int> centred(Part at, float wMm, float hMm)
{
    return juce::Rectangle<float>(px(wMm), px(hMm)).withCentre({px(at.x), px(at.y)}).toNearestInt();
}

// ---- Colours ----------------------------------------------------------------
const juce::Colour kPanelColour{0xff26282b};
const juce::Colour kOutlineColour{0xff6b6f75};
const juce::Colour kTextColour{0xffe6e6e6};
const juce::Colour kDimTextColour{0xff9a9ea4};

// An unlit LED's body (linear light, before the sRGB encode below).
constexpr float kLedBodyLight = 0.012f;

float srgbEncode(float linear)
{
    const float v = std::clamp(linear, 0.0f, 1.0f);
    return v <= 0.0031308f ? 12.92f * v : 1.055f * std::pow(v, 1.0f / 2.4f) - 0.055f;
}

// LedMeter.h gives drive values; on the module the PWM cubes each channel
// (rvled::pwmCount) and the LED's light follows that duty. A screen pixel's
// value is gamma-encoded, so the same light needs sRGB-encoding the duty:
// a dim LED is as dim on screen as on the panel (kMinGlow -> 2.7 % light).
juce::Colour ledColour(const rvled::Rgb& drive)
{
    auto channel = [](float d) {
        const float duty = float(rvled::pwmCount(d)) / float(rvled::kPwmSteps);
        return srgbEncode(kLedBodyLight + duty);
    };
    return juce::Colour::fromFloatRGBA(channel(drive.r), channel(drive.g), channel(drive.b), 1.0f);
}

// Knob value text, shown while dragging or hovering: percent of travel, and
// WOBBLE as its side and amount (ADR 0034; noon is still, ±3 % dead zone).
juce::String knobText(ParamId id, double value)
{
    const float v = float(value);
    if (id == ParamId::Wobble) {
        if (wobble::randomAmount(v) > 0.0f) return "Drift " + juce::String(juce::roundToInt(100.0f * wobble::randomAmount(v))) + " %";
        if (wobble::lfoAmount(v) > 0.0f) return "Warble " + juce::String(juce::roundToInt(100.0f * wobble::lfoAmount(v))) + " %";
        return "Still";
    }
    return juce::String(juce::roundToInt(100.0f * v)) + " %";
}

// JUCE's standard look with three tweaks for a dark panel: the value arc
// and the chosen toggle position in the accent colour (the dark scheme's
// defaults are nearly invisible here), knobs drawn about the size of the
// module's caps, and smaller button text so CLEAN / DRIVEN / KICKED fit a
// toggle position about 7 mm wide. A knob marked "bipolar" (WOBBLE) fills
// its arc from noon, not from the left end.
const juce::Identifier kBipolar{"bipolar"};

struct PanelLook final : juce::LookAndFeel_V4 {
    PanelLook()
    {
        const auto accent = findColour(juce::Slider::thumbColourId);
        setColour(juce::Slider::rotarySliderFillColourId, accent);
        setColour(juce::TextButton::buttonOnColourId, accent);
        setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    }

    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override
    {
        return juce::FontOptions(std::min(11.0f, float(buttonHeight) * 0.45f), juce::Font::bold);
    }

    // LookAndFeel_V4::drawRotarySlider, with a smaller inset and the bipolar fill.
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos, float startAngle,
                          float endAngle, juce::Slider& slider) override
    {
        const auto bounds    = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(4.0f);
        const auto centre    = bounds.getCentre();
        const float radius   = std::min(bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const float toAngle  = startAngle + sliderPos * (endAngle - startAngle);
        const float lineW    = std::min(7.0f, radius * 0.3f);
        const float arcR     = radius - lineW * 0.5f;
        const bool  bipolar  = bool(slider.getProperties().getWithDefault(kBipolar, false));
        const float fromAngle = bipolar ? 0.5f * (startAngle + endAngle) : startAngle;
        const juce::PathStrokeType stroke(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

        juce::Path track;
        track.addCentredArc(centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
        g.setColour(slider.findColour(juce::Slider::rotarySliderOutlineColourId));
        g.strokePath(track, stroke);

        if (slider.isEnabled() && std::abs(toAngle - fromAngle) > 0.01f) {
            juce::Path value;
            value.addCentredArc(centre.x, centre.y, arcR, arcR, 0.0f, std::min(fromAngle, toAngle),
                                std::max(fromAngle, toAngle), true);
            g.setColour(slider.findColour(juce::Slider::rotarySliderFillColourId));
            g.strokePath(value, stroke);
        }

        const float thumbW = lineW * 2.0f;
        const juce::Point<float> thumb(centre.x + arcR * std::cos(toAngle - juce::MathConstants<float>::halfPi),
                                       centre.y + arcR * std::sin(toAngle - juce::MathConstants<float>::halfPi));
        g.setColour(slider.findColour(juce::Slider::thumbColourId));
        g.fillEllipse(juce::Rectangle<float>(thumbW, thumbW).withCentre(thumb));
    }
};

} // namespace

// ---- The panel ----------------------------------------------------------------
// Laid out at 1x (5 px per mm); the editor scales it as a whole.
class PanelEditor::Panel final : public juce::Component, private juce::Timer {
public:
    Panel(juce::AudioProcessorValueTreeState& state, PanelLink& link, std::function<void(float)> onScale,
          float scale)
        : link_(link)
    {
        setLookAndFeel(&lookAndFeel_);

        for (size_t i = 0; i < knobs_.size(); ++i) {
            const auto id = kKnobs[i].id;
            auto& s       = knobs_[i];
            s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
            s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            s.setPopupDisplayEnabled(true, true, this);
            s.getProperties().set(kBipolar, id == ParamId::Wobble);
            addAndMakeVisible(s);
            // The attachment also sets double-click -> the ParamSpec default.
            knobAttach_[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, spec(id).key, s);
            s.textFromValueFunction = [id](double v) { return knobText(id, v); };
        }

        for (size_t t = 0; t < toggles_.size(); ++t) {
            const auto& ps = spec(kToggles[t].id);
            auto*       p  = state.getParameter(ps.key);
            for (int pos = 0; pos < 3; ++pos) {
                auto& b = toggles_[t][static_cast<size_t>(pos)];
                b.setButtonText(ps.choices[pos]);
                b.setClickingTogglesState(true);
                b.setRadioGroupId(int(t) + 1, juce::dontSendNotification);
                b.setConnectedEdges((pos > 0 ? juce::Button::ConnectedOnLeft : 0)
                                    | (pos < 2 ? juce::Button::ConnectedOnRight : 0));
                b.onClick = [this, t, pos] {
                    if (toggles_[t][static_cast<size_t>(pos)].getToggleState())
                        toggleAttach_[t]->setValueAsCompleteGesture(float(pos));
                };
                addAndMakeVisible(b);
            }
            toggleAttach_[t] = std::make_unique<juce::ParameterAttachment>(*p, [this, t](float v) {
                const int pos = std::clamp(juce::roundToInt(v), 0, 2);
                toggles_[t][static_cast<size_t>(pos)].setToggleState(true, juce::dontSendNotification);
            });
            toggleAttach_[t]->sendInitialUpdate();
        }

        // One Kick per press, fired on mouse down like the hardware button.
        // The audio thread picks it up at the start of its next block.
        kick_.setButtonText("KICK");
        kick_.setTriggeredOnMouseDown(true);
        kick_.onClick = [this] { link_.requestKick(); };
        addAndMakeVisible(kick_);

        // THROW: the gate (on = high, the send open; ADR 0039). A latching
        // button on the automatable throw_gate param.
        throw_.setButtonText("THROW");
        throw_.setClickingTogglesState(true);
        addAndMakeVisible(throw_);
        throwAttach_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(state, spec(ParamId::Throw).key, throw_);

        const float scales[3] = {1.0f, 1.5f, 2.0f};
        const char* names[3]  = {"1x", "1.5x", "2x"};
        for (size_t i = 0; i < sizes_.size(); ++i) {
            auto& b = sizes_[i];
            b.setButtonText(names[i]);
            b.setClickingTogglesState(true);
            b.setRadioGroupId(10, juce::dontSendNotification);
            b.setConnectedEdges((i > 0 ? juce::Button::ConnectedOnLeft : 0) | (i < 2 ? juce::Button::ConnectedOnRight : 0));
            b.setToggleState(juce::approximatelyEqual(scale, scales[i]), juce::dontSendNotification);
            b.onClick = [this, onScale, s = scales[i], i] {
                if (sizes_[i].getToggleState()) onScale(s);
            };
            addAndMakeVisible(b);
        }

        setSize(kWidthPx, kHeightPx);
        lastTickMs_ = juce::Time::getMillisecondCounterHiRes();
        startTimerHz(30);
    }

    ~Panel() override
    {
        stopTimer();
        setLookAndFeel(nullptr);
    }

    void resized() override
    {
        for (size_t i = 0; i < knobs_.size(); ++i)
            knobs_[i].setBounds(centred(kKnobs[i].at, kKnobSizeMm, kKnobSizeMm));

        for (size_t t = 0; t < toggles_.size(); ++t)
            for (size_t pos = 0; pos < 3; ++pos) {
                const Part c{kSegmentLeftMm + kSegmentWMm * (float(pos) + 0.5f), kToggles[t].at.y};
                toggles_[t][pos].setBounds(centred(c, kSegmentWMm, kSegmentHMm));
            }

        kick_.setBounds(centred(kButton, kButtonMm, kButtonMm));
        throw_.setBounds(centred(kThrow, 9.0f, kSegmentHMm));

        constexpr float sizeW = 7.0f;
        for (size_t i = 0; i < sizes_.size(); ++i)
            sizes_[i].setBounds(centred({kPanelW * 0.5f + sizeW * (float(i) - 1.0f), 121.0f}, sizeW, 4.0f));
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(kPanelColour);
        g.setColour(kOutlineColour);
        g.drawRect(getLocalBounds(), 1);

        auto text = [&g](const juce::String& s, Part at, float hMm, float sizePx, juce::Colour c,
                         juce::Justification j = juce::Justification::centred, float wMm = 16.0f) {
            g.setColour(c);
            g.setFont(juce::FontOptions(sizePx, juce::Font::bold));
            g.drawFittedText(s, centred(at, wMm, hMm), j, 1, 0.8f);
        };

        text("RESILIO VERSIO", {kPanelW * 0.5f, 9.0f}, 3.0f, 12.0f, kTextColour, juce::Justification::centred, 30.0f);

        // Meter labels: left pair is the input, right pair the output.
        text("IN", {(kLeds[0].x + kLeds[1].x) * 0.5f, 15.6f}, kLabelHMm, 9.0f, kDimTextColour);
        text("OUT", {(kLeds[2].x + kLeds[3].x) * 0.5f, 15.6f}, kLabelHMm, 9.0f, kDimTextColour);

        for (const auto& k : kKnobs) {
            const float half = kKnobSizeMm * 0.5f;
            const float y    = k.labelAbove ? k.at.y - half - 2.6f : k.at.y + half + 1.6f;
            text(spec(k.id).name, {k.at.x, y}, kLabelHMm, 11.0f, kTextColour);
        }

        // WOBBLE is bipolar (ADR 0034): a tick at noon, where it is still.
        {
            const auto& w   = kKnobs[5];
            const float top = w.at.y - kKnobSizeMm * 0.5f;
            g.setColour(kTextColour);
            g.drawLine(px(w.at.x), px(top - 1.0f), px(w.at.x), px(top + 0.3f), 1.5f); // ends above the arc
        }

        for (const auto& t : kToggles)
            text(spec(t.id).name, {kSegmentLeftMm + 7.0f, t.at.y - kSegmentHMm * 0.5f - 1.6f}, kLabelHMm, 11.0f,
                 kTextColour, juce::Justification::centredLeft, 14.0f);

        text("SIZE", {kPanelW * 0.5f, 117.0f}, kLabelHMm, 9.0f, kDimTextColour);

        for (int m = 0; m < PanelLink::kNumMeters; ++m) {
            const auto  c = kLeds[m];
            const float r = px(kLedRadiusMm);
            const auto  e = juce::Rectangle<float>(2.0f * r, 2.0f * r).withCentre({px(c.x), px(c.y)});
            const bool blink = juce::Time::getMillisecondCounterHiRes() < blinkUntilMs_;
            g.setColour(blink ? juce::Colours::white : ledColour(meters_[static_cast<size_t>(m)].colour()));
            g.fillEllipse(e);
            g.setColour(kOutlineColour);
            g.drawEllipse(e, 1.0f);
        }
    }

private:
    // Same steps as firmware/main.cpp's main loop, at ~30 Hz with the real
    // elapsed time, so ballistics and red hold match the module.
    void timerCallback() override
    {
        const double now = juce::Time::getMillisecondCounterHiRes();
        const float  dt  = float(std::max(0.0, now - lastTickMs_) * 0.001);
        lastTickMs_      = now;

        float peak[PanelLink::kNumMeters];
        for (int m = 0; m < PanelLink::kNumMeters; ++m) peak[m] = link_.takePeak(m);

        // Red: input near full scale; output while the Tank's safety limiter
        // pulls the wet down (stereo-linked: both output LEDs together).
        // KICK held (ADR 0039): after kThrowExitHoldSeconds, throw mode off
        // once per press; the LEDs blink white if it was on (as the module).
        if (kick_.isDown()) {
            if (kickDownMs_ < 0.0) kickDownMs_ = now;
            if (!exitSent_ && now - kickDownMs_ >= 1000.0 * double(rv::throwhold::kThrowExitHoldSeconds)) {
                link_.requestThrowExit();
                exitSent_ = true;
            }
        } else {
            kickDownMs_ = -1.0;
            exitSent_   = false;
        }
        if (link_.takeThrowExited()) blinkUntilMs_ = now + 1000.0 * double(rv::throwhold::kThrowExitBlinkSeconds);

        const bool limiting = rvled::limiterReducing(link_.takeLimiterGain());
        meters_[PanelLink::kInL].update(peak[PanelLink::kInL], rvled::inputNearClip(peak[PanelLink::kInL]), dt);
        meters_[PanelLink::kInR].update(peak[PanelLink::kInR], rvled::inputNearClip(peak[PanelLink::kInR]), dt);
        meters_[PanelLink::kOutL].update(peak[PanelLink::kOutL], limiting, dt);
        meters_[PanelLink::kOutR].update(peak[PanelLink::kOutR], limiting, dt);

        repaint(juce::Rectangle<float>(px(kLeds[0].x - 2.5f), px(kLeds[0].y - 2.5f),
                                       px(kLeds[3].x - kLeds[0].x + 5.0f), px(5.0f))
                    .toNearestInt());
    }

    PanelLink&      link_;
    PanelLook       lookAndFeel_;

    std::array<juce::Slider, std::size(kKnobs)> knobs_;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, std::size(kKnobs)> knobAttach_;
    std::array<std::array<juce::TextButton, 3>, std::size(kToggles)> toggles_;
    std::array<std::unique_ptr<juce::ParameterAttachment>, std::size(kToggles)> toggleAttach_;
    juce::TextButton                kick_;
    double                          kickDownMs_ = -1.0, blinkUntilMs_ = 0.0; // KICK held -> throw mode off
    bool                            exitSent_   = false;
    juce::TextButton                throw_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> throwAttach_;
    std::array<juce::TextButton, 3> sizes_;

    std::array<rvled::LevelMeter, PanelLink::kNumMeters> meters_;
    double                                               lastTickMs_ = 0.0;
};

// ---- The editor: the panel at the chosen size --------------------------------
namespace {
// The chosen size is kept in the plugin's saved state (not a parameter, so no
// automation lane and nothing for the DSP to see).
const juce::Identifier kScaleProperty{"panelScale"};
} // namespace

PanelEditor::PanelEditor(juce::AudioProcessor& owner, juce::AudioProcessorValueTreeState& state, PanelLink& link)
    : AudioProcessorEditor(owner), state_(state)
{
    float scale = float(double(state_.state.getProperty(kScaleProperty, 1.0)));
    if (!(juce::approximatelyEqual(scale, 1.5f) || juce::approximatelyEqual(scale, 2.0f))) scale = 1.0f;

    panel_ = std::make_unique<Panel>(state, link, [this](float s) {
        state_.state.setProperty(kScaleProperty, s, nullptr);
        setPanelScale(s);
    }, scale);
    addAndMakeVisible(*panel_);
    setResizable(false, false);
    setPanelScale(scale);
}

PanelEditor::~PanelEditor() = default;

void PanelEditor::setPanelScale(float scale)
{
    panel_->setTransform(juce::AffineTransform::scale(scale));
    setSize(juce::roundToInt(float(kWidthPx) * scale), juce::roundToInt(float(kHeightPx) * scale));
}

void PanelEditor::resized() { panel_->setTopLeftPosition(0, 0); }

} // namespace rv::plugin
