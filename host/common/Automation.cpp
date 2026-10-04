#include "Automation.h"
#include "ParamsJson.h"

#include <algorithm>

namespace rv::automation {

double valueAt(const Track& track, double t)
{
    const auto& pts = track.points;
    if (pts.empty()) return 0.0;
    if (t <= pts.front().first) return pts.front().second;
    if (t >= pts.back().first) return pts.back().second;
    for (size_t i = 1; i < pts.size(); ++i) {
        if (t <= pts[i].first) {
            const double t0 = pts[i - 1].first, t1 = pts[i].first;
            const double v0 = pts[i - 1].second, v1 = pts[i].second;
            const double a = (t1 > t0) ? (t - t0) / (t1 - t0) : 0.0;
            return v0 + a * (v1 - v0);
        }
    }
    return pts.back().second;
}

bool parse(const json::Value& root, Automation& out, std::string& error)
{
    out = Automation{};
    if (const json::Value* bp = root.find("breakpoints")) {
        if (!bp->isArray()) { error = "breakpoints must be an array"; return false; }
        for (const auto& item : bp->items()) {
            const std::string key = item.get("key", std::string());
            ParamId id;
            if (!paramsjson::findParamId(key, id)) { error = "unknown automation key '" + key + "'"; return false; }
            const double t = item.get("t", 0.0);
            const double value = item.get("value", 0.0);
            Track* track = nullptr;
            for (auto& tr : out.tracks) if (tr.key == id) { track = &tr; break; }
            if (!track) { out.tracks.push_back(Track{id, {}}); track = &out.tracks.back(); }
            track->points.emplace_back(t, value);
        }
        for (auto& tr : out.tracks)
            std::sort(tr.points.begin(), tr.points.end(), [](auto& a, auto& b) { return a.first < b.first; });
    }
    if (const json::Value* kicks = root.find("kicks")) {
        if (!kicks->isArray()) { error = "kicks must be an array"; return false; }
        for (const auto& item : kicks->items()) out.kicksSeconds.push_back(item.numberValue());
        std::sort(out.kicksSeconds.begin(), out.kicksSeconds.end());
    }
    if (const json::Value* exits = root.find("throw_exits")) {
        if (!exits->isArray()) { error = "throw_exits must be an array of times (seconds)"; return false; }
        for (const auto& item : exits->items()) out.throwExitsSeconds.push_back(item.numberValue());
        std::sort(out.throwExitsSeconds.begin(), out.throwExitsSeconds.end());
    }
    if (const json::Value* gates = root.find("gates")) {
        if (!gates->isArray()) { error = "gates must be an array of [rise, fall] pairs"; return false; }
        for (const auto& item : gates->items()) {
            if (!item.isArray() || item.items().size() != 2) { error = "each gate must be [rise, fall] (seconds)"; return false; }
            const double on = item.items()[0].numberValue(), off = item.items()[1].numberValue();
            if (!(off > on)) { error = "a gate's fall must come after its rise"; return false; }
            out.gateEvents.emplace_back(on, true);
            out.gateEvents.emplace_back(off, false);
        }
        std::stable_sort(out.gateEvents.begin(), out.gateEvents.end(), [](auto& a, auto& b) { return a.first < b.first; });
    }
    if (const json::Value* clocks = root.find("clocks")) {
        if (!clocks->isArray()) { error = "clocks must be an array"; return false; }
        for (const auto& item : clocks->items()) out.clocksSeconds.push_back(item.numberValue());
        std::sort(out.clocksSeconds.begin(), out.clocksSeconds.end());
    }
    if (const json::Value* cb = root.find("clock_bpm")) {
        if (cb->isNumber()) out.clockBpm = cb->numberValue();
        else if (cb->isObject()) {
            out.clockBpm   = cb->get("bpm", 0.0);
            out.clockStart = cb->get("start", 0.0);
            out.clockEnd   = cb->get("end", -1.0);
        } else { error = "clock_bpm must be a number or {bpm, start, end}"; return false; }
        if (!(out.clockBpm > 0.0)) { error = "clock_bpm needs a bpm above 0"; return false; }
    }
    return true;
}

bool loadFile(const std::string& path, Automation& out, std::string& error)
{
    json::Value root;
    if (!json::loadFile(path, root, error)) return false;
    return parse(root, out, error);
}

} // namespace rv::automation
