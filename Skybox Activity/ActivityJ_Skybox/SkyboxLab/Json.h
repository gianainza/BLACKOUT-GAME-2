#pragma once
// ---------------------------------------------------------------------------
// Json.h / Json.cpp - a small, dependency-free JSON reader                owner: R1
//
// Why not a library: B14 asks that the project builds on a computer that has never seen it,
// and the loader has to say WHICH LINE of the level file is wrong (nlohmann reports a byte
// offset). Every value remembers the line it started on, so the scene loader can say
//     level01.json:57: 'edge' must be a number between 3 and 80
// Extension: // line comments are skipped, so a level file can be annotated.
// ---------------------------------------------------------------------------
#include <string>
#include <vector>
#include <utility>

struct JsonValue
{
    enum Type { Null, Bool, Number, String, Array, Object } type = Null;
    bool b = false;
    double num = 0.0;
    std::string str;
    std::vector<JsonValue> arr;
    std::vector<std::pair<std::string, JsonValue>> obj;
    int line = 1;

    const JsonValue* Find(const std::string& key) const
    {
        for (const auto& kv : obj) if (kv.first == key) return &kv.second;
        return nullptr;
    }
    bool IsNumber() const { return type == Number; }
    bool IsString() const { return type == String; }
    bool IsArray()  const { return type == Array; }
    bool IsObject() const { return type == Object; }
};

// returns true on success; on failure 'error' = "line N: message"
bool ParseJson(const std::string& text, JsonValue& out, std::string& error);
