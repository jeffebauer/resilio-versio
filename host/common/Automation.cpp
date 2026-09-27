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
    return true;
}

bool loadFile(const std::string& path, Automation& out, std::string& error)
{
    json::Value root;
    if (!json::loadFile(path, root, error)) return false;
    return parse(root, out, error);
}

} // namespace rv::automation
