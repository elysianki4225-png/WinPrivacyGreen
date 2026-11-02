#include "backup.h"

#include <algorithm>
#include <cwchar>
#include <string.h>

#include "items.h"
#include "json.h"

namespace wpg {

namespace {
const wchar_t* kSnapshotPrefix = L"snapshot-";
const size_t kMaxSnapshots = 20;

std::wstring fileStem(const std::wstring& path) {
    size_t slash = path.find_last_of(L"\\/");
    std::wstring name = slash == std::wstring::npos ? path : path.substr(slash + 1);
    size_t dot = name.find_last_of(L'.');
    return dot == std::wstring::npos ? name : name.substr(0, dot);
}
}  // namespace

std::wstring snapshotLabelFromPath(const std::wstring& path) {
    std::wstring stem = fileStem(path);
    // snapshot-20261102-131500 -> 2026-11-02 13:15
    if (stem.size() >= 24 && stem.compare(0, 9, kSnapshotPrefix) == 0) {
        std::wstring d = stem.substr(9);
        if (d.size() >= 15) {
            return d.substr(0, 4) + L"-" + d.substr(4, 2) + L"-" + d.substr(6, 2) + L" " + d.substr(9, 2) + L":" +
                   d.substr(11, 2);
        }
    }
    return stem;
}

void BackupStore::init() {
    ensureDir(appDataDir());
    ensureDir(backupsDir());
    loadOriginals();
    refreshSnapshots();
}

std::wstring BackupStore::originalsPath() const { return joinPath(backupsDir(), L"originals.json"); }

void BackupStore::refreshSnapshots() {
    snapshots_ = listFiles(backupsDir(), L".json");
    snapshots_.erase(std::remove_if(snapshots_.begin(), snapshots_.end(),
                                    [](const std::wstring& p) {
                                        std::wstring stem = fileStem(p);
                                        return stem.compare(0, wcslen(kSnapshotPrefix), kSnapshotPrefix) != 0;
                                    }),
                     snapshots_.end());
    std::sort(snapshots_.begin(), snapshots_.end());
}

void BackupStore::loadOriginals() {
    originals_ = readValuesFile(originalsPath());
}

bool BackupStore::saveOriginals() {
    return writeValuesFile(originalsPath(), originals_);
}

std::vector<ValueSnapshot> BackupStore::readValuesFile(const std::wstring& path) {
    std::vector<ValueSnapshot> values;
    std::wstring text;
    if (!readTextFile(path, text)) return values;
    Json doc = jsonParse(text);
    const Json* arr = doc.find(L"values");
    if (!arr || arr->type != Json::Type::Array) return values;
    for (const auto& v : arr->items) {
        ValueSnapshot snap;
        std::wstring hive = v.getString(L"hive", L"HKCU");
        snap.addr.hive = (hive == L"HKLM") ? Hive::LocalMachine : Hive::CurrentUser;
        snap.addr.subkey = v.getString(L"key");
        snap.addr.value = v.getString(L"name");
        snap.existed = v.getBool(L"existed", false);
        snap.isString = v.getString(L"type", L"dword") == L"string";
        snap.dword = (DWORD)v.getInt(L"dword", 0);
        snap.text = v.getString(L"text");
        if (!snap.addr.subkey.empty()) values.push_back(snap);
    }
    return values;
}

bool BackupStore::writeValuesFile(const std::wstring& path, const std::vector<ValueSnapshot>& values) {
    Json doc = Json::makeObject();
    doc.set(L"app", Json::makeString(L"WinPrivacyGreen"));
    doc.set(L"version", Json::makeNumber(1));
    doc.set(L"created", Json::makeString(localizedTimestamp(L"yyyy-MM-dd HH:mm:ss")));
    Json arr = Json::makeArray();
    for (const auto& snap : values) {
        Json v = Json::makeObject();
        v.set(L"hive", Json::makeString(snap.addr.hive == Hive::LocalMachine ? L"HKLM" : L"HKCU"));
        v.set(L"key", Json::makeString(snap.addr.subkey));
        v.set(L"name", Json::makeString(snap.addr.value));
        v.set(L"existed", Json::makeBool(snap.existed));
        v.set(L"type", Json::makeString(snap.isString ? L"string" : L"dword"));
        if (snap.isString)
            v.set(L"text", Json::makeString(snap.text));
        else
            v.set(L"dword", Json::makeNumber((double)snap.dword));
        arr.items.push_back(std::move(v));
    }
    doc.set(L"values", std::move(arr));
    ensureDir(appDataDir());
    ensureDir(backupsDir());
    return writeTextFile(path, jsonDump(doc));
}

bool BackupStore::remember(const RegAddress& addr) {
    for (const auto& snap : originals_)
        if (snap.addr.hive == addr.hive && _wcsicmp(snap.addr.subkey.c_str(), addr.subkey.c_str()) == 0 &&
            _wcsicmp(snap.addr.value.c_str(), addr.value.c_str()) == 0)
            return true;
    originals_.push_back(snapshotValue(addr));
    return saveOriginals();
}

bool BackupStore::restoreOriginals(std::wstring* error) {
    if (originals_.empty()) {
        if (error) *error = L"empty";
        return false;
    }
    int failed = 0;
    for (const auto& snap : originals_)
        if (!restoreValue(snap)) ++failed;
    if (failed == 0) {
        originals_.clear();
        saveOriginals();
        return true;
    }
    if (error) *error = std::to_wstring(failed);
    return false;
}

bool BackupStore::createSnapshot(std::wstring* error) {
    std::vector<ValueSnapshot> values;
    auto rememberUnique = [&](const RegAddress& addr) {
        for (const auto& existing : values)
            if (existing.addr.hive == addr.hive && _wcsicmp(existing.addr.subkey.c_str(), addr.subkey.c_str()) == 0 &&
                _wcsicmp(existing.addr.value.c_str(), addr.value.c_str()) == 0)
                return;
        values.push_back(snapshotValue(addr));
    };
    for (const auto& item : itemCatalog())
        for (const auto& op : item.ops) rememberUnique(op.addr);

    wchar_t stamp[32] = {};
    SYSTEMTIME st{};
    GetLocalTime(&st);
    swprintf(stamp, 32, L"%04d%02d%02d-%02d%02d%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    std::wstring path = joinPath(backupsDir(), std::wstring(kSnapshotPrefix) + stamp + L".json");
    if (!writeValuesFile(path, values)) {
        if (error) *error = path;
        return false;
    }
    refreshSnapshots();
    while (snapshots_.size() > kMaxSnapshots) {
        removeFile(snapshots_.front());
        snapshots_.erase(snapshots_.begin());
    }
    return true;
}

bool BackupStore::restoreLatestSnapshot(std::wstring* error) {
    refreshSnapshots();
    if (snapshots_.empty()) {
        if (error) *error = L"empty";
        return false;
    }
    std::vector<ValueSnapshot> values = readValuesFile(snapshots_.back());
    if (values.empty()) {
        if (error) *error = L"empty";
        return false;
    }
    int failed = 0;
    for (const auto& snap : values)
        if (!restoreValue(snap)) ++failed;
    if (failed > 0) {
        if (error) *error = std::to_wstring(failed);
        return false;
    }
    return true;
}

std::wstring BackupStore::latestSnapshotLabel() const {
    if (snapshots_.empty()) return std::wstring();
    return snapshotLabelFromPath(snapshots_.back());
}

}  // namespace wpg
