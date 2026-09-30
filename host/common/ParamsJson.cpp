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

bool applyValue(Tank& tank, ParamId id, const json::Value& val, std::string& error)
{
    const ParamSpec& s = spec(id);
    if (val.isNumber()) {
        tank.setParam(id, float(val.numberValue()));
        return true;
    }
    if (val.isString() && s.kind == ParamKind::Switch3) {
        const std::string label = val.stringValue();
        for (int pos = 0; pos < 3; ++pos) {
            if (label == s.choices[pos]) { tank.setParam(id, switchToNormalised(pos)); return true; }
        }
        error = "unknown label '" + label + "' for " + s.key;
        return false;
    }
    error = std::string("bad value for ") + s.key;
    return false;
}

bool applyPreset(Tank& tank, const json::Value& preset, std::string& error)
{
    if (!preset.isObject()) { error = "preset must be a JSON object"; return false; }
    for (const auto& entry : preset.entries()) {
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
    ParamId id;
    if (!findParamId(key, id)) { error = "unknown param '" + key + "'"; return false; }
    const ParamSpec& s = spec(id);

    if (s.kind == ParamKind::Switch3) {
        for (int pos = 0; pos < 3; ++pos) {
            if (text == s.choices[pos]) { tank.setParam(id, switchToNormalised(pos)); return true; }
        }
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
