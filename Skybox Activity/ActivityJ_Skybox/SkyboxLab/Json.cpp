#include "Json.h"
#include <cstdlib>
#include <cctype>

namespace
{
    struct Parser
    {
        const std::string& s;
        size_t i = 0;
        int line = 1;
        std::string err;

        explicit Parser(const std::string& text) : s(text) {}

        bool Fail(const std::string& m)
        {
            if (err.empty()) err = "line " + std::to_string(line) + ": " + m;
            return false;
        }

        void Skip()
        {
            while (i < s.size())
            {
                char c = s[i];
                if (c == '\n') { ++line; ++i; }
                else if (c == ' ' || c == '\t' || c == '\r') ++i;
                else if (c == '/' && i + 1 < s.size() && s[i + 1] == '/') { while (i < s.size() && s[i] != '\n') ++i; }
                else break;
            }
        }

        bool Lit(const char* w)
        {
            size_t n = 0; while (w[n]) ++n;
            if (s.compare(i, n, w) == 0) { i += n; return true; }
            return false;
        }

        bool ParseString(std::string& out)
        {
            ++i;                                           // opening quote
            out.clear();
            while (i < s.size() && s[i] != '"')
            {
                if (s[i] == '\n') return Fail("string is not closed before the end of the line");
                if (s[i] == '\\' && i + 1 < s.size())
                {
                    char e = s[++i];
                    switch (e) { case 'n': out += '\n'; break; case 't': out += '\t'; break; case 'r': break;
                                 case '"': out += '"'; break; case '\\': out += '\\'; break; case '/': out += '/'; break;
                                 default: return Fail(std::string("unknown escape \\") + e); }
                    ++i;
                }
                else out += s[i++];
            }
            if (i >= s.size()) return Fail("string is not closed");
            ++i;                                           // closing quote
            return true;
        }

        bool ParseValue(JsonValue& v, int depth)
        {
            if (depth > 64) return Fail("nesting is too deep");
            Skip();
            if (i >= s.size()) return Fail("unexpected end of file (a value was expected)");
            v.line = line;
            char c = s[i];
            if (c == '{')
            {
                v.type = JsonValue::Object; ++i;
                Skip();
                if (i < s.size() && s[i] == '}') { ++i; return true; }
                for (;;)
                {
                    Skip();
                    if (i >= s.size() || s[i] != '"') return Fail("a quoted key was expected (a trailing comma, or a missing quote?)");
                    std::string key;
                    if (!ParseString(key)) return false;
                    Skip();
                    if (i >= s.size() || s[i] != ':') return Fail("':' expected after the key \"" + key + "\"");
                    ++i;
                    JsonValue child;
                    if (!ParseValue(child, depth + 1)) return false;
                    v.obj.emplace_back(key, std::move(child));
                    Skip();
                    if (i < s.size() && s[i] == ',') { ++i; continue; }
                    if (i < s.size() && s[i] == '}') { ++i; return true; }
                    return Fail("',' or '}' expected (is a comma missing between two entries?)");
                }
            }
            if (c == '[')
            {
                v.type = JsonValue::Array; ++i;
                Skip();
                if (i < s.size() && s[i] == ']') { ++i; return true; }
                for (;;)
                {
                    JsonValue child;
                    if (!ParseValue(child, depth + 1)) return false;
                    v.arr.push_back(std::move(child));
                    Skip();
                    if (i < s.size() && s[i] == ',') { ++i; continue; }
                    if (i < s.size() && s[i] == ']') { ++i; return true; }
                    return Fail("',' or ']' expected (is a comma missing between two entries?)");
                }
            }
            if (c == '"') { v.type = JsonValue::String; return ParseString(v.str); }
            if (Lit("true"))  { v.type = JsonValue::Bool; v.b = true;  return true; }
            if (Lit("false")) { v.type = JsonValue::Bool; v.b = false; return true; }
            if (Lit("null"))  { v.type = JsonValue::Null; return true; }
            if (c == '-' || std::isdigit((unsigned char)c))
            {
                char* end = nullptr;
                v.num = std::strtod(s.c_str() + i, &end);
                if (end == s.c_str() + i) return Fail("bad number");
                i = (size_t)(end - s.c_str());
                v.type = JsonValue::Number;
                return true;
            }
            return Fail(std::string("unexpected character '") + c + "'");
        }
    };
}

bool ParseJson(const std::string& text, JsonValue& out, std::string& error)
{
    Parser p(text);
    if (!p.ParseValue(out, 0)) { error = p.err; return false; }
    p.Skip();
    if (p.i < text.size()) { error = "line " + std::to_string(p.line) + ": extra text after the end of the file's JSON value"; return false; }
    return true;
}
