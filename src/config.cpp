#include "json.h"
#include "sys.h"
#include "ui.h"

namespace wpg {

static std::wstring configPath() { return joinPath(appDataDir(), L"config.json"); }

Config loadConfig() {
    Config cfg;
    std::wstring text;
    if (!readTextFile(configPath(), text)) return cfg;
    Json doc = jsonParse(text);
    if (doc.type != Json::Type::Object) return cfg;

    std::wstring theme = doc.getString(L"theme", L"auto");
    cfg.theme = theme == L"light" ? ThemeMode::Light : (theme == L"dark" ? ThemeMode::Dark : ThemeMode::Auto);
    cfg.dynamicColor = doc.getBool(L"dynamicColor", true);
    std::wstring seed = doc.getString(L"seedColor", L"#2E7D32");
    if (seed.size() == 7 && seed[0] == L'#') {
        uint32_t value = (uint32_t)wcstoul(seed.c_str() + 1, nullptr, 16);
        if (value <= 0xFFFFFF) cfg.seedColor = value;
    }
    cfg.backup = doc.getBool(L"backup", true);
    std::wstring lang = doc.getString(L"lang", L"auto");
    cfg.lang = lang == L"zh" ? LangMode::Chinese : (lang == L"en" ? LangMode::English : LangMode::Auto);
    return cfg;
}

void saveConfig(const Config& cfg) {
    Json doc = Json::makeObject();
    const wchar_t* theme = cfg.theme == ThemeMode::Light ? L"light" : (cfg.theme == ThemeMode::Dark ? L"dark" : L"auto");
    doc.set(L"theme", Json::makeString(theme));
    doc.set(L"dynamicColor", Json::makeBool(cfg.dynamicColor));
    doc.set(L"seedColor", Json::makeString(L"#" + hex(cfg.seedColor, 6)));
    doc.set(L"backup", Json::makeBool(cfg.backup));
    const wchar_t* lang = cfg.lang == LangMode::Chinese ? L"zh" : (cfg.lang == LangMode::English ? L"en" : L"auto");
    doc.set(L"lang", Json::makeString(lang));
    ensureDir(appDataDir());
    writeTextFile(configPath(), jsonDump(doc));
}

}  // namespace wpg
