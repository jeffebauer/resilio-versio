#include "ParamsJson.h"

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

} // namespace rv::paramsjson
