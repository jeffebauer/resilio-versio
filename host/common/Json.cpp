#include "Json.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

namespace rv::json {

void Value::set(const std::string& key, Value v)
{
    type = Type::Object;
    for (auto& e : obj_) {
        if (e.first == key) { e.second = std::move(v); return; }
    }
    obj_.emplace_back(key, std::move(v));
}

const Value* Value::find(const std::string& key) const
{
    for (const auto& e : obj_)
        if (e.first == key) return &e.second;
    return nullptr;
}

double Value::get(const std::string& key, double def) const
{
    const Value* v = find(key);
    return v ? v->numberValue(def) : def;
}

std::string Value::get(const std::string& key, const std::string& def) const
{
    const Value* v = find(key);
    return v ? v->stringValue(def) : def;
}

bool Value::get(const std::string& key, bool def) const
{
    const Value* v = find(key);
    return v ? v->boolValue(def) : def;
}

bool Value::operator==(const Value& o) const
{
    if (type != o.type) return false;
    switch (type) {
        case Type::Null:   return true;
        case Type::Bool:   return bool_ == o.bool_;
        case Type::Number: return num_ == o.num_;
        case Type::String: return str_ == o.str_;
        case Type::Array:  return arr_ == o.arr_;
        case Type::Object: return obj_ == o.obj_;
    }
    return false;
}

namespace {

struct Parser {
    const char* p;
    const char* end;
    std::string error;

    void skipWs() { while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) ++p; }

    bool fail(const std::string& msg)
    {
        if (error.empty()) error = msg;
        return false;
    }

    bool parseValue(Value& out)
    {
        skipWs();
        if (p >= end) return fail("unexpected end of input");
        switch (*p) {
            case '{': return parseObject(out);
            case '[': return parseArray(out);
            case '"': { std::string s; if (!parseString(s)) return false; out = Value::makeString(std::move(s)); return true; }
            case 't': if (end - p >= 4 && !std::strncmp(p, "true", 4)) { p += 4; out = Value::makeBool(true); return true; } return fail("bad literal");
            case 'f': if (end - p >= 5 && !std::strncmp(p, "false", 5)) { p += 5; out = Value::makeBool(false); return true; } return fail("bad literal");
            case 'n': if (end - p >= 4 && !std::strncmp(p, "null", 4)) { p += 4; out = Value::makeNull(); return true; } return fail("bad literal");
            default: return parseNumber(out);
        }
    }

    bool parseNumber(Value& out)
    {
        const char* start = p;
        if (p < end && (*p == '-' || *p == '+')) ++p;
        while (p < end && (std::isdigit(static_cast<unsigned char>(*p)) || *p == '.' || *p == 'e' || *p == 'E' || *p == '+' || *p == '-')) ++p;
        if (p == start) return fail("expected number");
        char* numEnd = nullptr;
        double d = std::strtod(start, &numEnd);
        if (numEnd != p) return fail("malformed number");
        out = Value::makeNumber(d);
        return true;
    }

    bool parseString(std::string& out)
    {
        if (p >= end || *p != '"') return fail("expected string");
        ++p;
        out.clear();
        while (p < end && *p != '"') {
            char c = *p++;
            if (c == '\\') {
                if (p >= end) return fail("bad escape");
                char e = *p++;
                switch (e) {
                    case '"': out += '"'; break;
                    case '\\': out += '\\'; break;
                    case '/': out += '/'; break;
                    case 'n': out += '\n'; break;
                    case 't': out += '\t'; break;
                    case 'r': out += '\r'; break;
                    case 'b': out += '\b'; break;
                    case 'f': out += '\f'; break;
                    case 'u': {
                        if (end - p < 4) return fail("bad unicode escape");
                        unsigned code = 0;
                        for (int i = 0; i < 4; ++i) {
                            char h = p[i];
                            code <<= 4;
                            if (h >= '0' && h <= '9') code |= unsigned(h - '0');
                            else if (h >= 'a' && h <= 'f') code |= unsigned(h - 'a' + 10);
                            else if (h >= 'A' && h <= 'F') code |= unsigned(h - 'A' + 10);
                            else return fail("bad unicode escape");
                        }
                        p += 4;
                        // Basic-plane only: encode as UTF-8 (good enough for our JSON, no surrogate pairs expected).
                        if (code < 0x80) out += char(code);
                        else if (code < 0x800) {
                            out += char(0xC0 | (code >> 6));
                            out += char(0x80 | (code & 0x3F));
                        } else {
                            out += char(0xE0 | (code >> 12));
                            out += char(0x80 | ((code >> 6) & 0x3F));
                            out += char(0x80 | (code & 0x3F));
                        }
                        break;
                    }
                    default: return fail("bad escape");
                }
            } else {
                out += c;
            }
        }
        if (p >= end) return fail("unterminated string");
        ++p; // closing quote
        return true;
    }

    bool parseArray(Value& out)
    {
        out = Value::makeArray();
        ++p; // '['
        skipWs();
        if (p < end && *p == ']') { ++p; return true; }
        while (true) {
            Value item;
            if (!parseValue(item)) return false;
            out.push(std::move(item));
            skipWs();
            if (p >= end) return fail("unterminated array");
            if (*p == ',') { ++p; continue; }
            if (*p == ']') { ++p; return true; }
            return fail("expected ',' or ']'");
        }
    }

    bool parseObject(Value& out)
    {
        out = Value::makeObject();
        ++p; // '{'
        skipWs();
        if (p < end && *p == '}') { ++p; return true; }
        while (true) {
            skipWs();
            std::string key;
            if (!parseString(key)) return false;
            skipWs();
            if (p >= end || *p != ':') return fail("expected ':'");
            ++p;
            Value val;
            if (!parseValue(val)) return false;
            out.set(key, std::move(val));
            skipWs();
            if (p >= end) return fail("unterminated object");
            if (*p == ',') { ++p; continue; }
            if (*p == '}') { ++p; return true; }
            return fail("expected ',' or '}'");
        }
    }
};

void writeEscaped(std::string& out, const std::string& s)
{
    out += '"';
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\t': out += "\\t"; break;
            case '\r': out += "\\r"; break;
            default:
                if (c < 0x20) { char buf[8]; std::snprintf(buf, sizeof buf, "\\u%04x", c); out += buf; }
                else out += char(c);
        }
    }
    out += '"';
}

void writeNumber(std::string& out, double d)
{
    if (std::isnan(d) || std::isinf(d)) { out += "null"; return; }
    if (d == std::floor(d) && std::fabs(d) < 1e15) {
        char buf[32];
        std::snprintf(buf, sizeof buf, "%.0f", d);
        out += buf;
        return;
    }
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.17g", d);
    out += buf;
}

void writeValue(std::string& out, const Value& v, int indent, int depth)
{
    const std::string pad(size_t(indent * (depth + 1)), ' ');
    const std::string padEnd(size_t(indent * depth), ' ');
    switch (v.type) {
        case Type::Null: out += "null"; break;
        case Type::Bool: out += v.boolValue() ? "true" : "false"; break;
        case Type::Number: writeNumber(out, v.numberValue()); break;
        case Type::String: writeEscaped(out, v.stringValue()); break;
        case Type::Array: {
            if (v.items().empty()) { out += "[]"; break; }
            out += "[\n";
            for (size_t i = 0; i < v.items().size(); ++i) {
                out += pad;
                writeValue(out, v.items()[i], indent, depth + 1);
                if (i + 1 < v.items().size()) out += ',';
                out += '\n';
            }
            out += padEnd + "]";
            break;
        }
        case Type::Object: {
            if (v.entries().empty()) { out += "{}"; break; }
            out += "{\n";
            for (size_t i = 0; i < v.entries().size(); ++i) {
                out += pad;
                writeEscaped(out, v.entries()[i].first);
                out += ": ";
                writeValue(out, v.entries()[i].second, indent, depth + 1);
                if (i + 1 < v.entries().size()) out += ',';
                out += '\n';
            }
            out += padEnd + "}";
            break;
        }
    }
}

} // namespace

bool parse(const std::string& text, Value& out, std::string& error)
{
    Parser parser{text.data(), text.data() + text.size(), {}};
    if (!parser.parseValue(out)) { error = parser.error; return false; }
    parser.skipWs();
    if (parser.p != parser.end) { error = "trailing data after JSON value"; return false; }
    return true;
}

std::string write(const Value& v, int indent)
{
    std::string out;
    writeValue(out, v, indent, 0);
    return out;
}

bool loadFile(const std::string& path, Value& out, std::string& error)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) { error = "cannot open " + path; return false; }
    std::ostringstream ss;
    ss << f.rdbuf();
    return parse(ss.str(), out, error);
}

bool saveFile(const std::string& path, const Value& v, std::string& error)
{
    std::ofstream f(path, std::ios::binary);
    if (!f) { error = "cannot write " + path; return false; }
    const std::string text = write(v);
    f.write(text.data(), std::streamsize(text.size()));
    return bool(f);
}

} // namespace rv::json
