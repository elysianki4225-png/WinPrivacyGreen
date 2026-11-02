// Tiny JSON reader/writer used for config and backup files.
#pragma once
#include <string>
#include <vector>
#include <utility>

namespace wpg {

class Json {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Type type = Type::Null;
    bool boolean = false;
    double number = 0;
    std::wstring text;
    std::vector<Json> items;
    std::vector<std::pair<std::wstring, Json>> members;

    static Json makeObject();
    static Json makeArray();
    static Json makeString(const std::wstring& s);
    static Json makeNumber(double d);
    static Json makeBool(bool b);

    void set(const std::wstring& key, Json value);
    const Json* find(const std::wstring& key) const;

    std::wstring getString(const std::wstring& key, const std::wstring& fallback = L"") const;
    bool getBool(const std::wstring& key, bool fallback = false) const;
    int getInt(const std::wstring& key, int fallback = 0) const;
    std::wstring asString(const std::wstring& fallback = L"") const;
    bool asBool(bool fallback = false) const;
    int asInt(int fallback = 0) const;
    size_t size() const { return type == Type::Array ? items.size() : members.size(); }
};

// Returns a Null value when the document is malformed.
Json jsonParse(const std::wstring& source);
std::wstring jsonDump(const Json& value);

}  // namespace wpg
