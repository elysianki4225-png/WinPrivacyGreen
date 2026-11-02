#include "json.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace wpg {

Json Json::makeObject() {
    Json j;
    j.type = Type::Object;
    return j;
}
Json Json::makeArray() {
    Json j;
    j.type = Type::Array;
    return j;
}
Json Json::makeString(const std::wstring& s) {
    Json j;
    j.type = Type::String;
    j.text = s;
    return j;
}
Json Json::makeNumber(double d) {
    Json j;
    j.type = Type::Number;
    j.number = d;
    return j;
}
Json Json::makeBool(bool b) {
    Json j;
    j.type = Type::Bool;
    j.boolean = b;
    return j;
}

void Json::set(const std::wstring& key, Json value) {
    if (type != Type::Object) {
        type = Type::Object;
        members.clear();
    }
    for (auto& kv : members) {
        if (kv.first == key) {
            kv.second = std::move(value);
            return;
        }
    }
    members.emplace_back(key, std::move(value));
}

const Json* Json::find(const std::wstring& key) const {
    if (type != Type::Object) return nullptr;
    for (const auto& kv : members)
        if (kv.first == key) return &kv.second;
    return nullptr;
}

std::wstring Json::getString(const std::wstring& key, const std::wstring& fallback) const {
    const Json* v = find(key);
    return v ? v->asString(fallback) : fallback;
}
bool Json::getBool(const std::wstring& key, bool fallback) const {
    const Json* v = find(key);
    return v ? v->asBool(fallback) : fallback;
}
int Json::getInt(const std::wstring& key, int fallback) const {
    const Json* v = find(key);
    return v ? v->asInt(fallback) : fallback;
}
std::wstring Json::asString(const std::wstring& fallback) const {
    return type == Type::String ? text : fallback;
}
bool Json::asBool(bool fallback) const {
    if (type == Type::Bool) return boolean;
    if (type == Type::Number) return number != 0;
    return fallback;
}
int Json::asInt(int fallback) const {
    if (type == Type::Number) return (int)std::lround(number);
    if (type == Type::Bool) return boolean ? 1 : 0;
    return fallback;
}

// ---------------------------------------------------------------------------
namespace {

struct Parser {
    const std::wstring& s;
    size_t i = 0;
    bool failed = false;

    explicit Parser(const std::wstring& src) : s(src) {}

    void skip() {
        while (i < s.size()) {
            wchar_t c = s[i];
            if (c == L' ' || c == L'\t' || c == L'\r' || c == L'\n') {
                ++i;
            } else if (c == L'/' && i + 1 < s.size() && s[i + 1] == L'/') {
                while (i < s.size() && s[i] != L'\n') ++i;
            } else {
                break;
            }
        }
    }

    bool eat(wchar_t c) {
        skip();
        if (i < s.size() && s[i] == c) {
            ++i;
            return true;
        }
        return false;
    }

    void appendUtf8(std::wstring& out, unsigned cp) {
        if (cp <= 0xFFFF) {
            out.push_back((wchar_t)cp);
        } else {
            cp -= 0x10000;
            out.push_back((wchar_t)(0xD800 + (cp >> 10)));
            out.push_back((wchar_t)(0xDC00 + (cp & 0x3FF)));
        }
    }

    std::wstring parseString() {
        std::wstring out;
        if (!eat(L'"')) {
            failed = true;
            return out;
        }
        while (i < s.size()) {
            wchar_t c = s[i++];
            if (c == L'"') return out;
            if (c != L'\\') {
                out.push_back(c);
                continue;
            }
            if (i >= s.size()) break;
            wchar_t e = s[i++];
            switch (e) {
                case L'"': out.push_back(L'"'); break;
                case L'\\': out.push_back(L'\\'); break;
                case L'/': out.push_back(L'/'); break;
                case L'b': out.push_back(L'\b'); break;
                case L'f': out.push_back(L'\f'); break;
                case L'n': out.push_back(L'\n'); break;
                case L'r': out.push_back(L'\r'); break;
                case L't': out.push_back(L'\t'); break;
                case L'u': {
                    unsigned cp = 0;
                    for (int k = 0; k < 4 && i < s.size(); ++k) {
                        wchar_t h = s[i++];
                        cp <<= 4;
                        if (h >= L'0' && h <= L'9') cp |= (unsigned)(h - L'0');
                        else if (h >= L'a' && h <= L'f') cp |= (unsigned)(h - L'a' + 10);
                        else if (h >= L'A' && h <= L'F') cp |= (unsigned)(h - L'A' + 10);
                        else { failed = true; return out; }
                    }
                    // Re-join surrogate pairs written as two escapes.
                    if (cp >= 0xD800 && cp <= 0xDBFF && i + 6 <= s.size() && s[i] == L'\\' && s[i + 1] == L'u') {
                        unsigned lo = 0;
                        size_t save = i;
                        i += 2;
                        bool ok = true;
                        for (int k = 0; k < 4; ++k) {
                            wchar_t h = s[i++];
                            lo <<= 4;
                            if (h >= L'0' && h <= L'9') lo |= (unsigned)(h - L'0');
                            else if (h >= L'a' && h <= L'f') lo |= (unsigned)(h - L'a' + 10);
                            else if (h >= L'A' && h <= L'F') lo |= (unsigned)(h - L'A' + 10);
                            else { ok = false; break; }
                        }
                        if (ok && lo >= 0xDC00 && lo <= 0xDFFF) {
                            appendUtf8(out, 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00));
                            break;
                        }
                        i = save;
                    }
                    appendUtf8(out, cp);
                    break;
                }
                default:
                    out.push_back(e);
                    break;
            }
        }
        failed = true;
        return out;
    }

    Json parseValue(int depth) {
        if (depth > 64) {
            failed = true;
            return Json();
        }
        skip();
        if (i >= s.size()) {
            failed = true;
            return Json();
        }
        wchar_t c = s[i];
        if (c == L'{') {
            ++i;
            Json obj = Json::makeObject();
            skip();
            if (eat(L'}')) return obj;
            while (i < s.size() && !failed) {
                std::wstring key = parseString();
                if (failed || !eat(L':')) {
                    failed = true;
                    break;
                }
                Json value = parseValue(depth + 1);
                obj.set(key, std::move(value));
                if (eat(L',')) continue;
                if (eat(L'}')) return obj;
                failed = true;
            }
            failed = true;
            return obj;
        }
        if (c == L'[') {
            ++i;
            Json arr = Json::makeArray();
            skip();
            if (eat(L']')) return arr;
            while (i < s.size() && !failed) {
                arr.items.push_back(parseValue(depth + 1));
                if (eat(L',')) continue;
                if (eat(L']')) return arr;
                failed = true;
            }
            failed = true;
            return arr;
        }
        if (c == L'"') return Json::makeString(parseString());
        if (s.compare(i, 4, L"true") == 0) {
            i += 4;
            return Json::makeBool(true);
        }
        if (s.compare(i, 5, L"false") == 0) {
            i += 5;
            return Json::makeBool(false);
        }
        if (s.compare(i, 4, L"null") == 0) {
            i += 4;
            return Json();
        }
        // number
        size_t start = i;
        if (i < s.size() && (s[i] == L'-' || s[i] == L'+')) ++i;
        bool any = false;
        while (i < s.size() && ((s[i] >= L'0' && s[i] <= L'9') || s[i] == L'.' || s[i] == L'e' ||
                                s[i] == L'E' || s[i] == L'-' || s[i] == L'+')) {
            any = true;
            ++i;
        }
        if (!any) {
            failed = true;
            return Json();
        }
        std::wstring num = s.substr(start, i - start);
        return Json::makeNumber(wcstod(num.c_str(), nullptr));
    }
};

void escapeTo(std::wstring& out, const std::wstring& in) {
    out.push_back(L'"');
    for (wchar_t c : in) {
        switch (c) {
            case L'"': out += L"\\\""; break;
            case L'\\': out += L"\\\\"; break;
            case L'\n': out += L"\\n"; break;
            case L'\r': out += L"\\r"; break;
            case L'\t': out += L"\\t"; break;
            default:
                if (c < 0x20) {
                    wchar_t buf[8];
                    swprintf(buf, 8, L"\\u%04x", (unsigned)c);
                    out += buf;
                } else {
                    out.push_back(c);
                }
        }
    }
    out.push_back(L'"');
}

void dumpTo(std::wstring& out, const Json& j, int indent) {
    auto pad = [&](int n) {
        for (int k = 0; k < n; ++k) out += L"  ";
    };
    switch (j.type) {
        case Json::Type::Null: out += L"null"; break;
        case Json::Type::Bool: out += j.boolean ? L"true" : L"false"; break;
        case Json::Type::Number: {
            wchar_t buf[64];
            if (j.number == std::floor(j.number) && std::fabs(j.number) < 1e15)
                swprintf(buf, 64, L"%.0f", j.number);
            else
                swprintf(buf, 64, L"%.10g", j.number);
            out += buf;
            break;
        }
        case Json::Type::String: escapeTo(out, j.text); break;
        case Json::Type::Array: {
            if (j.items.empty()) {
                out += L"[]";
                break;
            }
            out += L"[\n";
            for (size_t k = 0; k < j.items.size(); ++k) {
                pad(indent + 1);
                dumpTo(out, j.items[k], indent + 1);
                if (k + 1 < j.items.size()) out += L",";
                out += L"\n";
            }
            pad(indent);
            out += L"]";
            break;
        }
        case Json::Type::Object: {
            if (j.members.empty()) {
                out += L"{}";
                break;
            }
            out += L"{\n";
            for (size_t k = 0; k < j.members.size(); ++k) {
                pad(indent + 1);
                escapeTo(out, j.members[k].first);
                out += L": ";
                dumpTo(out, j.members[k].second, indent + 1);
                if (k + 1 < j.members.size()) out += L",";
                out += L"\n";
            }
            pad(indent);
            out += L"}";
            break;
        }
    }
}

}  // namespace

Json jsonParse(const std::wstring& source) {
    Parser p(source);
    Json v = p.parseValue(0);
    if (p.failed) return Json();
    return v;
}

std::wstring jsonDump(const Json& value) {
    std::wstring out;
    dumpTo(out, value, 0);
    out += L"\n";
    return out;
}

}  // namespace wpg
