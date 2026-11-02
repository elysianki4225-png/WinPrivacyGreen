// WinPrivacyGreen - Material Design 3 settings window.
#pragma once
#include <windows.h>
#include <gdiplus.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "backup.h"
#include "color.h"
#include "items.h"
#include "sys.h"

namespace wpg {

constexpr float kSidebarW = 236.0f;

enum class Page { Overview = 0, Items = 1, Settings = 2, About = 3 };
enum class ThemeMode { Auto = 0, Light = 1, Dark = 2 };
enum class LangMode { Auto = 0, Chinese = 1, English = 2 };

struct Config {
    ThemeMode theme = ThemeMode::Auto;
    bool dynamicColor = true;
    uint32_t seedColor = 0x2E7D32;
    bool backup = true;
    LangMode lang = LangMode::Auto;
};

Config loadConfig();
void saveConfig(const Config& cfg);

enum ActionId {
    A_None = 0,
    A_Nav,
    A_ToggleItem,
    A_Theme,
    A_DynamicToggle,
    A_Seed,
    A_BackupToggle,
    A_BackupNow,
    A_RestoreSnapshot,
    A_RestoreOriginals,
    A_OpenBackupFolder,
    A_BlockAll,
    A_RestoreAll,
    A_OpenUrl,
    A_Lang,
    A_Minimize,
    A_Close,
    A_ConfirmOk,
    A_ConfirmCancel,
    A_OpenRepo,
};

struct Hotspot {
    Gdiplus::RectF rect;
    int action = A_None;
    int a = 0;
    int b = 0;
};

class App {
public:
    App();
    ~App();

    bool create(HINSTANCE instance);
    int run();

private:
    static LRESULT CALLBACK staticProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT proc(HWND, UINT, WPARAM, LPARAM);

    // -- lifecycle ---------------------------------------------------------
    void onPaint(HDC hdc);
    void onResize(int pxW, int pxH);
    void onDpiChanged();
    void onTimer(UINT_PTR id);
    void onMouseMove(float x, float y);
    void onMouseDown(float x, float y);
    void onMouseUp(float x, float y);
    void onMouseLeave();
    void onMouseWheel(int delta);
    void onChar(wchar_t ch);
    void onKeyDown(int vk);

    // -- state -------------------------------------------------------------
    void applyTheme();
    void reloadWallpaper(bool force, bool silent);
    void refreshStates();
    void updateConfig();
    void showToast(const std::wstring& text, int kind);
    void openConfirm(const std::wstring& title, const std::wstring& body, const std::wstring& okLabel, int action);
    void closeConfirm();
    void setPage(Page page);

    // -- actions -----------------------------------------------------------
    void invoke(const Hotspot& spot);
    void toggleItem(int index);
    void setAllItems(bool block);
    void runRestore(bool fromSnapshot);
    void doBackup(bool silent);
    void doRestoreOriginals();
    void promptRestoreAll();

    // -- text --------------------------------------------------------------
    const wchar_t* tr(const wchar_t* zh, const wchar_t* en) const { return chinese_ ? zh : en; }
    const wchar_t* pageTitle(Page p) const;
    const wchar_t* pageSubtitle(Page p) const;

    // -- drawing -----------------------------------------------------------
    void drawSidebar(Gdiplus::Graphics& g, float w, float h);
    void drawHeader(Gdiplus::Graphics& g, float w);
    void drawOverview(Gdiplus::Graphics& g, const Gdiplus::RectF& view);
    void drawItemsPage(Gdiplus::Graphics& g, const Gdiplus::RectF& view);
    void drawSettings(Gdiplus::Graphics& g, const Gdiplus::RectF& view);
    void drawAbout(Gdiplus::Graphics& g, const Gdiplus::RectF& view);
    void drawScrollbar(Gdiplus::Graphics& g, const Gdiplus::RectF& view);
    void drawToast(Gdiplus::Graphics& g, float w, float h);
    void drawConfirm(Gdiplus::Graphics& g, float w, float h);

    Gdiplus::Font* font(float size, bool bold);
    float dip(float value) const { return value; }

    void addHotspot(const Gdiplus::RectF& r, int action, int a = 0, int b = 0);
    bool hovered(int action, int a = -1, int b = -1) const;
    float hoverT(int action, int a = -1) const;

    // -- members -----------------------------------------------------------
    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;
    float scale_ = 1.0f;
    float width_ = 0;   // DIP
    float height_ = 0;  // DIP
    bool chinese_ = true;
    bool maxed_ = false;

    Config config_;
    Scheme scheme_{};
    bool dark_ = false;
    uint32_t seedArgb_ = 0x2E7D32;
    uint32_t wallpaperSeedRgb_ = 0;
    uint32_t wallpaperSeedArgb_ = 0x2E7D32;
    std::wstring wallpaperPath_;
    bool wallpaperOk_ = false;
    std::wstring wallpaperSampleLabel_;

    BackupStore backups_;
    std::vector<ItemState> states_;
    std::wstring lastBackupLabel_;

    Page page_ = Page::Overview;
    float scroll_ = 0;
    float maxScroll_ = 0;

    std::vector<Hotspot> hotspots_;
    std::vector<Hotspot> hitAreas_;
    int hoverAction_ = A_None;
    int hoverA_ = -1;
    int hoverB_ = -1;
    int pressAction_ = A_None;
    int pressA_ = -1;

    // toast
    std::wstring toastText_;
    int toastKind_ = 0;  // 0 info, 1 success, 2 error
    bool toastVisible_ = false;

    // confirm dialog
    bool confirmVisible_ = false;
    std::wstring confirmTitle_, confirmBody_, confirmOk_;
    int confirmAction_ = A_None;

    Gdiplus::FontFamily* family_ = nullptr;
    Gdiplus::FontFamily* iconFamily_ = nullptr;
    std::map<int, std::unique_ptr<Gdiplus::Font>> fonts_;
    std::unique_ptr<Gdiplus::Bitmap> backBuffer_;
    ULONG_PTR gdiplusToken_ = 0;

    HICON iconLarge_ = nullptr;
    HICON iconSmall_ = nullptr;
    bool tracking_ = false;
};

}  // namespace wpg
