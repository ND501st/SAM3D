// simcore/json.hpp — a small JSON reader for simcore's data files.
//
// Standard library only, no exceptions. Strings stay UTF-8 (Cyrillic panel
// labels pass straight through; \uXXXX escapes are decoded to UTF-8). Object
// key order is kept. Nesting is limited to 64 levels. Errors report line and
// column. Numbers use std::strtod, so the C locale's decimal point must be '.'
// (true unless the host program changes it).
#pragma once

#include <cstddef>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

namespace simcore {
namespace json {

enum class Type { Null, Bool, Number, String, Array, Object };

inline const char* TypeName(Type t) {
    switch (t) {
        case Type::Null: return "null";
        case Type::Bool: return "boolean";
        case Type::Number: return "number";
        case Type::String: return "string";
        case Type::Array: return "array";
        case Type::Object: return "object";
    }
    return "?";
}

struct Value {
    Type type = Type::Null;
    bool boolean = false;
    double number = 0.0;
    std::string string;
    std::vector<Value> items;       // Array elements, or Object values (parallel to keys)
    std::vector<std::string> keys;  // Object keys, in file order

    bool Is(Type t) const { return type == t; }

    // Object member, or nullptr if absent or this isn't an object.
    const Value* Find(const std::string& key) const {
        if (type != Type::Object) return nullptr;
        for (std::size_t i = 0; i < keys.size(); ++i) {
            if (keys[i] == key) return &items[i];
        }
        return nullptr;
    }
};

namespace detail {

class Parser {
public:
    explicit Parser(const std::string& text) : s_(text) {}

    bool Run(Value& out, std::string* error) {
        if (s_.compare(0, 3, "\xEF\xBB\xBF") == 0) pos_ = 3;  // UTF-8 byte-order mark
        SkipSpace();
        if (!ParseValue(out, 0)) return Fail(error);
        SkipSpace();
        if (pos_ != s_.size()) {
            message_ = "unexpected text after the end of the document";
            return Fail(error);
        }
        return true;
    }

private:
    static constexpr int kMaxDepth = 64;

    bool Fail(std::string* error) const {
        if (error == nullptr) return false;
        int line = 1;
        int col = 1;
        for (std::size_t i = 0; i < pos_ && i < s_.size(); ++i) {
            if (s_[i] == '\n') {
                ++line;
                col = 1;
            } else {
                ++col;
            }
        }
        *error = "line " + std::to_string(line) + ", column " + std::to_string(col) + ": " + message_;
        return false;
    }

    bool Error(const char* msg) {
        message_ = msg;
        return false;
    }

    bool AtEnd() const { return pos_ >= s_.size(); }
    char Peek() const { return AtEnd() ? '\0' : s_[pos_]; }

    void SkipSpace() {
        while (!AtEnd() && (s_[pos_] == ' ' || s_[pos_] == '\t' || s_[pos_] == '\n' || s_[pos_] == '\r')) ++pos_;
    }

    bool Literal(const char* word) {
        std::size_t n = 0;
        while (word[n] != '\0') ++n;
        if (s_.compare(pos_, n, word) != 0) return Error("unknown literal");
        pos_ += n;
        return true;
    }

    bool ParseValue(Value& v, int depth) {
        if (depth > kMaxDepth) return Error("nesting too deep");
        switch (Peek()) {
            case '{': return ParseObject(v, depth);
            case '[': return ParseArray(v, depth);
            case '"':
                v.type = Type::String;
                return ParseString(v.string);
            case 't':
                v.type = Type::Bool;
                v.boolean = true;
                return Literal("true");
            case 'f':
                v.type = Type::Bool;
                v.boolean = false;
                return Literal("false");
            case 'n':
                v.type = Type::Null;
                return Literal("null");
            default: return ParseNumber(v);
        }
    }

    bool ParseObject(Value& v, int depth) {
        v.type = Type::Object;
        ++pos_;  // '{'
        SkipSpace();
        if (Peek() == '}') {
            ++pos_;
            return true;
        }
        for (;;) {
            SkipSpace();
            if (Peek() != '"') return Error("expected a quoted key");
            std::string key;
            if (!ParseString(key)) return false;
            for (const std::string& k : v.keys) {
                if (k == key) return Error("duplicate key");
            }
            SkipSpace();
            if (Peek() != ':') return Error("expected ':' after a key");
            ++pos_;
            SkipSpace();
            Value item;
            if (!ParseValue(item, depth + 1)) return false;
            v.keys.push_back(std::move(key));
            v.items.push_back(std::move(item));
            SkipSpace();
            if (Peek() == ',') {
                ++pos_;
                continue;
            }
            if (Peek() == '}') {
                ++pos_;
                return true;
            }
            return Error("expected ',' or '}' in an object");
        }
    }

    bool ParseArray(Value& v, int depth) {
        v.type = Type::Array;
        ++pos_;  // '['
        SkipSpace();
        if (Peek() == ']') {
            ++pos_;
            return true;
        }
        for (;;) {
            SkipSpace();
            Value item;
            if (!ParseValue(item, depth + 1)) return false;
            v.items.push_back(std::move(item));
            SkipSpace();
            if (Peek() == ',') {
                ++pos_;
                continue;
            }
            if (Peek() == ']') {
                ++pos_;
                return true;
            }
            return Error("expected ',' or ']' in an array");
        }
    }

    static int HexDigit(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    }

    bool Hex4(unsigned& out) {
        if (pos_ + 4 > s_.size()) return Error("truncated \\u escape");
        out = 0;
        for (int i = 0; i < 4; ++i) {
            const int d = HexDigit(s_[pos_++]);
            if (d < 0) return Error("bad hex digit in \\u escape");
            out = out * 16u + static_cast<unsigned>(d);
        }
        return true;
    }

    static void AppendUtf8(std::string& out, unsigned cp) {
        if (cp < 0x80u) {
            out += static_cast<char>(cp);
        } else if (cp < 0x800u) {
            out += static_cast<char>(0xC0u | (cp >> 6));
            out += static_cast<char>(0x80u | (cp & 0x3Fu));
        } else if (cp < 0x10000u) {
            out += static_cast<char>(0xE0u | (cp >> 12));
            out += static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu));
            out += static_cast<char>(0x80u | (cp & 0x3Fu));
        } else {
            out += static_cast<char>(0xF0u | (cp >> 18));
            out += static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu));
            out += static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu));
            out += static_cast<char>(0x80u | (cp & 0x3Fu));
        }
    }

    bool ParseString(std::string& out) {
        ++pos_;  // opening quote
        out.clear();
        while (!AtEnd()) {
            const char c = s_[pos_++];
            if (c == '"') return true;
            if (static_cast<unsigned char>(c) < 0x20u) return Error("control character in a string");
            if (c != '\\') {
                out += c;
                continue;
            }
            if (AtEnd()) break;
            const char e = s_[pos_++];
            switch (e) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u': {
                    unsigned cp = 0;
                    if (!Hex4(cp)) return false;
                    if (cp >= 0xD800u && cp <= 0xDBFFu) {
                        unsigned lo = 0;
                        if (s_.compare(pos_, 2, "\\u") != 0) return Error("unpaired surrogate");
                        pos_ += 2;
                        if (!Hex4(lo)) return false;
                        if (lo < 0xDC00u || lo > 0xDFFFu) return Error("unpaired surrogate");
                        cp = 0x10000u + ((cp - 0xD800u) << 10) + (lo - 0xDC00u);
                    } else if (cp >= 0xDC00u && cp <= 0xDFFFu) {
                        return Error("unpaired surrogate");
                    }
                    AppendUtf8(out, cp);
                    break;
                }
                default: return Error("unknown escape in a string");
            }
        }
        return Error("unterminated string");
    }

    static bool IsDigit(char c) { return c >= '0' && c <= '9'; }

    bool ParseNumber(Value& v) {
        const std::size_t start = pos_;
        if (Peek() == '-') ++pos_;
        if (Peek() == '0') {
            ++pos_;
        } else if (IsDigit(Peek())) {
            while (IsDigit(Peek())) ++pos_;
        } else {
            return Error("expected a value");
        }
        if (Peek() == '.') {
            ++pos_;
            if (!IsDigit(Peek())) return Error("expected a digit after '.'");
            while (IsDigit(Peek())) ++pos_;
        }
        if (Peek() == 'e' || Peek() == 'E') {
            ++pos_;
            if (Peek() == '+' || Peek() == '-') ++pos_;
            if (!IsDigit(Peek())) return Error("expected a digit in the exponent");
            while (IsDigit(Peek())) ++pos_;
        }
        const std::string text = s_.substr(start, pos_ - start);
        v.type = Type::Number;
        v.number = std::strtod(text.c_str(), nullptr);
        return true;
    }

    const std::string& s_;
    std::size_t pos_ = 0;
    std::string message_;
};

}  // namespace detail

// Parse a whole document. On failure returns false and, if `error` is given, describes where and why.
inline bool Parse(const std::string& text, Value& out, std::string* error = nullptr) {
    out = Value();
    detail::Parser parser(text);
    return parser.Run(out, error);
}

}  // namespace json
}  // namespace simcore
