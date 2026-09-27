#pragma once
// Minimal JSON reader/writer for desktop Hosts (Renderer): presets,
// automation, sweep configs, sidecars, manifests. Hand-written, no
// dependencies (SPEC §6.2 / docs/m1-contracts.md Stream B).
//
// Supports the JSON subset the Renderer needs: null, bool, number,
// string, array, object. Objects preserve insertion/parse order so
// written files read back predictably and sweeps stay deterministic.

#include <string>
#include <utility>
#include <vector>

namespace rv::json {

enum class Type { Null, Bool, Number, String, Array, Object };

class Value {
public:
    Type type = Type::Null;

    Value() = default;
    static Value makeNull() { return Value(); }
    static Value makeBool(bool b) { Value v; v.type = Type::Bool; v.bool_ = b; return v; }
    static Value makeNumber(double d) { Value v; v.type = Type::Number; v.num_ = d; return v; }
    static Value makeString(std::string s) { Value v; v.type = Type::String; v.str_ = std::move(s); return v; }
    static Value makeArray() { Value v; v.type = Type::Array; return v; }
    static Value makeObject() { Value v; v.type = Type::Object; return v; }

    bool isNull() const { return type == Type::Null; }
    bool isArray() const { return type == Type::Array; }
    bool isObject() const { return type == Type::Object; }
    bool isNumber() const { return type == Type::Number; }
    bool isString() const { return type == Type::String; }

    bool boolValue(bool def = false) const { return type == Type::Bool ? bool_ : def; }
    double numberValue(double def = 0.0) const { return type == Type::Number ? num_ : def; }
    std::string stringValue(const std::string& def = {}) const { return type == Type::String ? str_ : def; }

    // Array access.
    std::vector<Value>& items() { return arr_; }
    const std::vector<Value>& items() const { return arr_; }
    void push(Value v) { type = Type::Array; arr_.push_back(std::move(v)); }

    // Object access; insertion order preserved.
    void set(const std::string& key, Value v);
    const Value* find(const std::string& key) const;
    double get(const std::string& key, double def) const;
    std::string get(const std::string& key, const std::string& def) const;
    bool get(const std::string& key, bool def) const;
    const std::vector<std::pair<std::string, Value>>& entries() const { return obj_; }

    bool operator==(const Value& o) const;
    bool operator!=(const Value& o) const { return !(*this == o); }

private:
    bool bool_ = false;
    double num_ = 0.0;
    std::string str_;
    std::vector<Value> arr_;
    std::vector<std::pair<std::string, Value>> obj_;
};

// Parses `text`. Returns false and fills `error` on malformed JSON.
bool parse(const std::string& text, Value& out, std::string& error);

// Pretty-prints with `indent` spaces per level.
std::string write(const Value& v, int indent = 2);

bool loadFile(const std::string& path, Value& out, std::string& error);
bool saveFile(const std::string& path, const Value& v, std::string& error);

} // namespace rv::json
