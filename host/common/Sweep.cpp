#include "Sweep.h"

#include <cstdio>

namespace rv::sweep {

bool parse(const json::Value& root, Config& out, std::string& error)
{
    out = Config{};
    out.name = root.get("name", std::string("sweep"));
    out.input = root.get("input", std::string());
    if (out.input.empty()) { error = "sweep: missing 'input'"; return false; }
    out.tailSeconds = root.get("tail_seconds", 0.0);
    out.autoPath = root.get("auto", std::string());
    if (const json::Value* f = root.find("ignore_flags"); f && f->isArray()) out.ignoreFlags = *f;
    if (const json::Value* base = root.find("base")) out.base = *base;

    const json::Value* grid = root.find("grid");
    if (!grid || !grid->isObject()) { error = "sweep: missing 'grid' object"; return false; }
    for (const auto& entry : grid->entries()) {
        if (!entry.second.isArray()) { error = "sweep: grid values for '" + entry.first + "' must be an array"; return false; }
        Axis axis;
        axis.key = entry.first;
        for (const auto& v : entry.second.items()) axis.values.push_back(v.numberValue());
        if (axis.values.empty()) { error = "sweep: grid axis '" + entry.first + "' is empty"; return false; }
        out.grid.push_back(std::move(axis));
    }
    if (out.grid.empty()) { error = "sweep: grid has no axes"; return false; }
    return true;
}

bool loadFile(const std::string& path, Config& out, std::string& error)
{
    json::Value root;
    if (!json::loadFile(path, root, error)) return false;
    return parse(root, out, error);
}

std::vector<Combo> cartesian(const std::vector<Axis>& grid)
{
    std::vector<Combo> out;
    out.emplace_back(); // seed with one empty combo
    for (const auto& axis : grid) {
        std::vector<Combo> next;
        next.reserve(out.size() * axis.values.size());
        for (const auto& combo : out) {
            for (double v : axis.values) {
                Combo c = combo;
                c.emplace_back(axis.key, v);
                next.push_back(std::move(c));
            }
        }
        out = std::move(next);
    }
    return out;
}

std::string fileBaseName(const std::string& name, const Combo& combo)
{
    std::string out = name + "__";
    for (size_t i = 0; i < combo.size(); ++i) {
        char buf[64];
        std::snprintf(buf, sizeof buf, "%s%.2f", combo[i].first.c_str(), combo[i].second);
        out += buf;
        if (i + 1 < combo.size()) out += "_";
    }
    return out;
}

} // namespace rv::sweep
