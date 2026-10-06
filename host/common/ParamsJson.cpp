#include "ParamsJson.h"

#include <cerrno>
#include <cmath>
#include <cstdlib>

namespace rv::paramsjson {

bool findParamId(const std::string& key, ParamId& id)
{
    for (const auto& p : kParams) {
        if (key == p.key) { id = p.id; return true; }
    }
    return false;
}

namespace {
// Switch labels from before the v1.0.43 panel names (ADR 0044), still read
// so older presets, sweeps, scripts and reference notes render the same:
// SPRINGS 3 is TANK ECHO; DRIVEN is TAPE, KICKED is VALVE (AMP was the
// name for a few hours on 6 Oct 2026, so it is read too). Same positions.
struct LegacyLabel {
    ParamId     id;
    const char* label;
    int         position;
};
constexpr LegacyLabel kLegacyLabels[] = {
    {ParamId::Springs, "3", 2},
    {ParamId::Attitude, "DRIVEN", 1},
    {ParamId::Attitude, "KICKED", 2},
    {ParamId::Attitude, "AMP", 2},
};
} // namespace

bool switchPosition(ParamId id, const std::string& label, int& position)
{
    const ParamSpec& s = spec(id);
    for (int pos = 0; pos < 3; ++pos) {
        if (s.choices[pos] && label == s.choices[pos]) { position = pos; return true; }
    }
    for (const auto& l : kLegacyLabels) {
        if (l.id == id && label == l.label) { position = l.position; return true; }
    }
    return false;
}

bool applyValue(Tank& tank, ParamId id, const json::Value& val, std::string& error)
{
    const ParamSpec& s = spec(id);
    if (val.isNumber()) {
        tank.setParam(id, float(val.numberValue()));
        return true;
    }
    if (val.isString() && s.kind == ParamKind::Switch3) {
        const std::string label = val.stringValue();
        int pos = 0;
        if (switchPosition(id, label, pos)) { tank.setParam(id, switchToNormalised(pos)); return true; }
        error = "unknown label '" + label + "' for " + s.key;
        return false;
    }
    error = std::string("bad value for ") + s.key;
    return false;
}

bool applyHidden(Tank& tank, const std::string& key, double value)
{
    if (key == kWobbleVoicingKey) {
        tank.setWobbleVoicing(int(std::lround(value)));
        return true;
    }
    if (key == kSustainVoicingKey) {
        tank.setSustainVoicing(int(std::lround(value)));
        return true;
    }
    if (key == kSplashVoicingKey) {
        tank.setSplashVoicing(int(std::lround(value)));
        return true;
    }
    if (key == kToneVoicingKey) {
        tank.setToneVoicing(int(std::lround(value)));
        return true;
    }
    if (key == kTankVoicingKey) {
        tank.setTankVoicing(int(std::lround(value)));
        return true;
    }
    if (key == kFLowCutVoicingKey) {
        tank.setFLowCutVoicing(int(std::lround(value)));
        return true;
    }
    if (key == kOutputBitsKey) {
        tank.setOutputBitsVoicing(int(std::lround(value)));
        return true;
    }
    if (key == kHoldVoicingKey) {
        tank.setHoldVoicing(int(std::lround(value)));
        return true;
    }
    if (key == kDuckVoicingKey) {
        tank.setDuckVoicing(int(std::lround(value)));
        return true;
    }
    if (key == kTonePlaceVoicingKey) {
        tank.setTonePlaceVoicing(int(std::lround(value)));
        return true;
    }
    if (key == kEchoModeKey) {
        tank.setEchoMode(value >= 0.5);
        return true;
    }
    if (key == kEchoSpringsKey) {
        tank.setEchoSpringsVoicing(int(std::lround(value)));
        return true;
    }
    if (key == kEchoBitsKey) {
        tank.setEchoBitsVoicing(int(std::lround(value)));
        return true;
    }
    if (key == kBbdKey) {
        tank.setBbdVoicing(int(std::lround(value)));
        return true;
    }
    if (key == kEchoWearKey) {
        tank.setEchoWearVoicing(int(std::lround(value)));
        return true;
    }
    if (key == kEchoDiffuseKey) {
        tank.setEchoDiffuseVoicing(int(std::lround(value)));
        return true;
    }
    if (key == kHostBpmKey) {
        tank.setHostTempo(float(value));
        return true;
    }
    if (key == kSprings3VoicingKey) {
        tank.setSprings3Voicing(int(std::lround(value)));
        return true;
    }
    return false;
}

std::string wobbleVoicingLabel(const Tank& tank)
{
    return std::string(1, char('A' + tank.wobbleVoicing()));
}

bool applyPreset(Tank& tank, const json::Value& preset, std::string& error)
{
    if (!preset.isObject()) { error = "preset must be a JSON object"; return false; }
    for (const auto& entry : preset.entries()) {
        if (entry.second.isNumber() && applyHidden(tank, entry.first, entry.second.numberValue())) continue;
        ParamId id;
        if (!findParamId(entry.first, id)) { error = "unknown param '" + entry.first + "'"; return false; }
        if (!applyValue(tank, id, entry.second, error)) return false;
    }
    return true;
}

bool applySetArg(Tank& tank, const std::string& arg, std::string& error)
{
    const auto eq = arg.find('=');
    if (eq == std::string::npos) { error = "expected key=value"; return false; }
    const std::string key  = arg.substr(0, eq);
    const std::string text = arg.substr(eq + 1);
    {   // Hidden, Renderer-only keys (wobble_voicing, sustain_voicing, splash_voicing, tone_voicing, springs3_voicing, tank_voicing, f_lowcut_voicing, tone_place_voicing, echo_mode, host_bpm): a plain number.
        char* end = nullptr;
        const double v = std::strtod(text.c_str(), &end);
        if (!text.empty() && *end == '\0' && applyHidden(tank, key, v)) return true;
    }
    ParamId id;
    if (!findParamId(key, id)) { error = "unknown param '" + key + "'"; return false; }
    const ParamSpec& s = spec(id);

    if (s.kind == ParamKind::Switch3) {
        int pos = 0;
        if (switchPosition(id, text, pos)) { tank.setParam(id, switchToNormalised(pos)); return true; }
        error = "'" + text + "' is not a position of " + s.key + " (use " + s.choices[0] + ", "
              + s.choices[1] + " or " + s.choices[2] + ")";
        return false;
    }

    char* end = nullptr;
    errno = 0;
    const float value = std::strtof(text.c_str(), &end);
    if (text.empty() || *end != '\0' || errno != 0 || !std::isfinite(value)) {
        error = std::string(s.key) + " takes a number 0-1, got '" + text + "'";
        return false;
    }
    tank.setParam(id, value);
    return true;
}

} // namespace rv::paramsjson
