#include "sys.h"

#include <objidl.h>
#include <gdiplus.h>
#include <shellapi.h>
#include <shlobj.h>
#include <winternl.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cwchar>
#include <string.h>

#include "json.h"

using namespace Gdiplus;

namespace wpg {

// ---------------------------------------------------------------------------
static std::wstring knownFolder(REFKNOWNFOLDERID id) {
    PWSTR raw = nullptr;
    std::wstring result;
    if (SUCCEEDED(SHGetKnownFolderPath(id, 0, nullptr, &raw)) && raw) {
        result = raw;
        CoTaskMemFree(raw);
    }
    return result;
}

std::wstring joinPath(const std::wstring& a, const std::wstring& b) {
    if (a.empty()) return b;
    std::wstring out = a;
    if (out.back() != L'\\' && out.back() != L'/') out.push_back(L'\\');
    out += b;
    return out;
}

std::wstring appDataDir() {
    std::wstring base = knownFolder(FOLDERID_RoamingAppData);
    if (base.empty()) {
        wchar_t buf[MAX_PATH * 2] = {};
        DWORD n = GetEnvironmentVariableW(L"APPDATA", buf, MAX_PATH * 2);
        if (n > 0 && n < MAX_PATH * 2) base = buf;
    }
    return joinPath(base, L"WinPrivacyGreen");
}

std::wstring backupsDir() { return joinPath(appDataDir(), L"backups"); }

bool ensureDir(const std::wstring& path) {
    if (path.empty()) return false;
    if (CreateDirectoryW(path.c_str(), nullptr)) return true;
    return GetLastError() == ERROR_ALREADY_EXISTS;
}

bool fileExists(const std::wstring& path) {
    DWORD a = GetFileAttributesW(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

bool readTextFile(const std::wstring& path, std::wstring& out) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size{};
    std::string data;
    if (GetFileSizeEx(h, &size) && size.QuadPart >= 0 && size.QuadPart < (64 << 20)) {
        data.resize((size_t)size.QuadPart);
        DWORD read = 0;
        if (!data.empty()) {
            if (!ReadFile(h, &data[0], (DWORD)data.size(), &read, nullptr)) {
                CloseHandle(h);
                return false;
            }
        }
        data.resize(read);
    }
    CloseHandle(h);
    // Strip a UTF-8 BOM if present.
    if (data.size() >= 3 && (unsigned char)data[0] == 0xEF && (unsigned char)data[1] == 0xBB &&
        (unsigned char)data[2] == 0xBF) {
        data.erase(0, 3);
    }
    out = wideFromUtf8(data);
    return true;
}

bool writeTextFile(const std::wstring& path, const std::wstring& text) {
    std::string data = utf8FromWide(text);
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    bool ok = WriteFile(h, data.data(), (DWORD)data.size(), &written, nullptr) && written == data.size();
    CloseHandle(h);
    return ok;
}

std::wstring exeDir() {
    wchar_t buf[MAX_PATH * 2] = {};
    DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH * 2);
    if (n == 0) return std::wstring();
    std::wstring full(buf, n);
    size_t pos = full.find_last_of(L'\\');
    return pos == std::wstring::npos ? full : full.substr(0, pos);
}

std::vector<std::wstring> listFiles(const std::wstring& dir, const std::wstring& extension) {
    std::vector<std::wstring> files;
    WIN32_FIND_DATAW fd{};
    HANDLE h = FindFirstFileW(joinPath(dir, L"*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return files;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        std::wstring name = fd.cFileName;
        if (name.size() >= extension.size()) {
            std::wstring tail = name.substr(name.size() - extension.size());
            std::transform(tail.begin(), tail.end(), tail.begin(), ::towlower);
            if (tail == extension) files.push_back(joinPath(dir, name));
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    std::sort(files.begin(), files.end());
    return files;
}

bool removeFile(const std::wstring& path) { return DeleteFileW(path.c_str()) != FALSE; }

std::wstring wideFromUtf8(const std::string& s) {
    if (s.empty()) return std::wstring();
    int len = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring out(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &out[0], len);
    return out;
}

std::string utf8FromWide(const std::wstring& s) {
    if (s.empty()) return std::string();
    int len = WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0, nullptr, nullptr);
    std::string out(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), &out[0], len, nullptr, nullptr);
    return out;
}

std::wstring hex(uint32_t v, int digits) {
    wchar_t buf[16];
    swprintf(buf, 16, L"%0*X", digits, v);
    return buf;
}

// ---------------------------------------------------------------------------
static HKEY rootKey(Hive hive) { return hive == Hive::CurrentUser ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE; }

bool readRegDword(const RegAddress& addr, DWORD& out) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(rootKey(addr.hive), addr.subkey.c_str(), 0, KEY_READ, &key) != ERROR_SUCCESS) return false;
    DWORD type = 0, size = sizeof(DWORD), value = 0;
    LSTATUS st = RegQueryValueExW(key, addr.value.c_str(), nullptr, &type, (LPBYTE)&value, &size);
    RegCloseKey(key);
    if (st != ERROR_SUCCESS) return false;
    if (type != REG_DWORD) return false;
    out = value;
    return true;
}

bool readRegString(const RegAddress& addr, std::wstring& out) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(rootKey(addr.hive), addr.subkey.c_str(), 0, KEY_READ, &key) != ERROR_SUCCESS) return false;
    DWORD type = 0, size = 0;
    LSTATUS st = RegQueryValueExW(key, addr.value.c_str(), nullptr, &type, nullptr, &size);
    if (st != ERROR_SUCCESS) {
        RegCloseKey(key);
        return false;
    }
    bool ok = false;
    if (type == REG_SZ || type == REG_EXPAND_SZ) {
        std::wstring buf(size / sizeof(wchar_t) + 1, L'\0');
        if (RegQueryValueExW(key, addr.value.c_str(), nullptr, &type, (LPBYTE)&buf[0], &size) == ERROR_SUCCESS) {
            buf.resize(wcslen(buf.c_str()));
            out = buf;
            ok = true;
        }
    }
    RegCloseKey(key);
    return ok;
}

static bool writeRaw(const RegAddress& addr, DWORD type, const void* data, DWORD size) {
    HKEY key = nullptr;
    LSTATUS st = RegCreateKeyExW(rootKey(addr.hive), addr.subkey.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr,
                                 &key, nullptr);
    if (st != ERROR_SUCCESS) return false;
    st = RegSetValueExW(key, addr.value.c_str(), 0, type, (const BYTE*)data, size);
    RegCloseKey(key);
    return st == ERROR_SUCCESS;
}

bool writeRegDword(const RegAddress& addr, DWORD value) {
    return writeRaw(addr, REG_DWORD, &value, sizeof(DWORD));
}

bool writeRegString(const RegAddress& addr, const std::wstring& value) {
    return writeRaw(addr, REG_SZ, value.c_str(), (DWORD)((value.size() + 1) * sizeof(wchar_t)));
}

bool deleteRegValue(const RegAddress& addr) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(rootKey(addr.hive), addr.subkey.c_str(), 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS) return true;
    LSTATUS st = RegDeleteValueW(key, addr.value.c_str());
    RegCloseKey(key);
    return st == ERROR_SUCCESS || st == ERROR_FILE_NOT_FOUND;
}

ValueSnapshot snapshotValue(const RegAddress& addr) {
    ValueSnapshot snap;
    snap.addr = addr;
    HKEY key = nullptr;
    if (RegOpenKeyExW(rootKey(addr.hive), addr.subkey.c_str(), 0, KEY_READ, &key) != ERROR_SUCCESS) return snap;
    DWORD type = 0, size = 0;
    if (RegQueryValueExW(key, addr.value.c_str(), nullptr, &type, nullptr, &size) != ERROR_SUCCESS) {
        RegCloseKey(key);
        return snap;  // value (or key) does not exist yet
    }
    snap.existed = true;
    if (type == REG_DWORD || type == REG_QWORD || type == REG_BINARY) {
        DWORD value = 0;
        size = sizeof(DWORD);
        if (RegQueryValueExW(key, addr.value.c_str(), nullptr, &type, (LPBYTE)&value, &size) == ERROR_SUCCESS)
            snap.dword = value;
    } else {
        snap.isString = true;
        std::wstring buf(size / sizeof(wchar_t) + 2, L'\0');
        if (RegQueryValueExW(key, addr.value.c_str(), nullptr, &type, (LPBYTE)&buf[0], &size) == ERROR_SUCCESS) {
            buf.resize(wcslen(buf.c_str()));
            snap.text = buf;
        }
    }
    RegCloseKey(key);
    return snap;
}

bool restoreValue(const ValueSnapshot& snap) {
    if (!snap.existed) return deleteRegValue(snap.addr);
    if (snap.isString) return writeRegString(snap.addr, snap.text);
    return writeRegDword(snap.addr, snap.dword);
}

// ---------------------------------------------------------------------------
bool isAdmin() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
    TOKEN_ELEVATION elevation{};
    DWORD size = sizeof(elevation);
    bool admin = false;
    if (GetTokenInformation(token, TokenElevation, &elevation, size, &size)) admin = elevation.TokenIsElevated != 0;
    CloseHandle(token);
    return admin;
}

bool systemUsesDarkForApps() {
    DWORD value = 1;
    if (!readRegDword({Hive::CurrentUser, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                       L"AppsUseLightTheme"},
                      value))
        return false;
    return value == 0;
}

bool systemUsesDarkMode() {
    DWORD value = 1;
    if (!readRegDword({Hive::CurrentUser, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                       L"SystemUsesLightTheme"},
                      value))
        return systemUsesDarkForApps();
    return value == 0;
}

int windowsBuild() {
    static int cached = -1;
    if (cached >= 0) return cached;
    cached = 0;
    using RtlGetVersionFn = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);
    HINSTANCE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll) ntdll = LoadLibraryW(L"ntdll.dll");
    if (ntdll) {
        auto fn = (RtlGetVersionFn)GetProcAddress(ntdll, "RtlGetVersion");
        if (fn) {
            RTL_OSVERSIONINFOW info{};
            info.dwOSVersionInfoSize = sizeof(info);
            if (fn(&info) == 0) cached = (int)info.dwBuildNumber;
        }
    }
    return cached;
}

std::wstring windowsVersionName() {
    int build = windowsBuild();
    std::wstring name = build >= 22000 ? L"Windows 11" : L"Windows 10";
    const wchar_t* release = L"";
    switch (build) {
        case 19045: release = L" 22H2"; break;
        case 19044: release = L" 21H2"; break;
        case 22000: release = L" 21H2"; break;
        case 22621: release = L" 22H2"; break;
        case 22631: release = L" 23H2"; break;
        case 26100: release = L" 24H2"; break;
        case 26200: release = L" 25H2"; break;
        default: break;
    }
    return name + release;
}

std::wstring systemLocaleName() {
    wchar_t buf[LOCALE_NAME_MAX_LENGTH] = {};
    if (GetUserDefaultLocaleName(buf, LOCALE_NAME_MAX_LENGTH) > 0) return buf;
    return L"en-US";
}

std::wstring localizedTimestamp(const std::wstring& format) {
    SYSTEMTIME st{};
    GetLocalTime(&st);
    wchar_t buf[128] = {};
    GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, 0, &st, format.c_str(), buf, 128, nullptr);
    return buf;
}

void openUrl(const std::wstring& url) {
    ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void openFolder(const std::wstring& path) {
    std::wstring dir = path;
    if (!fileExists(dir)) ensureDir(dir);
    ShellExecuteW(nullptr, L"open", dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

// ---------------------------------------------------------------------------
std::wstring currentWallpaperPath() {
    wchar_t buf[MAX_PATH * 2] = {};
    if (SystemParametersInfoW(SPI_GETDESKWALLPAPER, MAX_PATH * 2, buf, 0)) {
        std::wstring path = buf;
        if (!path.empty() && fileExists(path)) return path;
    }
    std::wstring fallback = joinPath(knownFolder(FOLDERID_RoamingAppData), L"Microsoft\\Windows\\Themes\\TranscodedWallpaper");
    if (fileExists(fallback)) return fallback;
    std::wstring legacy = joinPath(knownFolder(FOLDERID_RoamingAppData), L"Microsoft\\Windows\\Themes\\CachedFiles");
    auto cached = listFiles(legacy, L".jpg");
    if (!cached.empty()) return cached.front();
    return std::wstring();
}

bool sampleImageColor(const std::wstring& path, uint32_t& argbOut, Oklch& seedOut) {
    if (path.empty() || !fileExists(path)) return false;
    Bitmap source((const WCHAR*)path.c_str(), FALSE);  // parens: avoid the most vexing parse
    if (source.GetLastStatus() != Ok) return false;
    UINT sw = source.GetWidth(), sh = source.GetHeight();
    if (sw < 2 || sh < 2) return false;

    const int kGrid = 56;
    Bitmap thumb(kGrid, kGrid, PixelFormat32bppARGB);
    {
        Graphics g(&thumb);
        g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
        g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
        g.DrawImage(&source, RectF(0, 0, (REAL)kGrid, (REAL)kGrid), 0, 0, (REAL)sw, (REAL)sh, UnitPixel);
    }

    BitmapData data{};
    Rect rect(0, 0, kGrid, kGrid);
    if (thumb.LockBits(&rect, ImageLockModeRead, PixelFormat32bppARGB, &data) != Ok) return false;

    const int kBuckets = 24;
    double weightSum[kBuckets] = {};
    double accA[kBuckets] = {}, accB[kBuckets] = {}, accL[kBuckets] = {};

    for (int y = 0; y < kGrid; ++y) {
        const uint32_t* row = (const uint32_t*)((const BYTE*)data.Scan0 + (size_t)y * data.Stride);
        for (int x = 0; x < kGrid; ++x) {
            uint32_t px = row[x];
            int alpha = (px >> 24) & 255;
            if (alpha < 32) continue;
            Oklch c = oklchFromArgb(px & 0xFFFFFF);
            if (c.L < 0.10f || c.L > 0.94f) continue;
            if (c.C < 0.030f) continue;
            // Favour saturated, mid-lightness pixels; the wallpaper "look".
            double w = (double)c.C * c.C * (1.0 - std::fabs((double)c.L - 0.55) * 1.5);
            if (w <= 0) continue;
            int bucket = (int)(c.h / (360.0f / kBuckets)) % kBuckets;
            float rad = c.h * 3.14159265f / 180.0f;
            weightSum[bucket] += w;
            accA[bucket] += std::cos(rad) * c.C * w;
            accB[bucket] += std::sin(rad) * c.C * w;
            accL[bucket] += c.L * w;
        }
    }
    thumb.UnlockBits(&data);

    int best = -1;
    for (int i = 0; i < kBuckets; ++i)
        if (weightSum[i] > 0 && (best < 0 || weightSum[i] > weightSum[best])) best = i;
    if (best < 0 || weightSum[best] < 0.35) return false;

    float meanA = (float)(accA[best] / weightSum[best]);
    float meanB = (float)(accB[best] / weightSum[best]);
    float meanL = (float)(accL[best] / weightSum[best]);
    Oklch raw{meanL, std::sqrt(meanA * meanA + meanB * meanB), std::atan2(meanB, meanA) * 180.0f / 3.14159265f};
    if (raw.h < 0) raw.h += 360.0f;
    argbOut = argbFromOklch(raw.L, raw.C, raw.h);
    seedOut = normalizeSeed(raw);
    return true;
}

}  // namespace wpg
