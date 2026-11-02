#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <objidl.h>
#include <gdiplus.h>

#include <algorithm>
#include <cmath>
#include <cwchar>

#include "backup.h"
#include "draw.h"
#include "items.h"
#include "json.h"
#include "sys.h"
#include "ui.h"

using namespace Gdiplus;

namespace wpg {

namespace {
const wchar_t* kClassName = L"WinPrivacyGreenMainWindow";
const wchar_t* kProjectUrl = L"https://github.com/elysianki4225-png/WinPrivacyGreen";

const UINT_PTR kTimerWallpaper = 1;
const UINT_PTR kTimerToast = 2;

typedef DPI_AWARENESS_CONTEXT(WINAPI* SetDpiCtxFn)(DPI_AWARENESS_CONTEXT);
typedef UINT(WINAPI* GetDpiForWindowFn)(HWND);

float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

std::wstring formatInt(int v) { return std::to_wstring(v); }

}  // namespace

App::App() = default;
App::~App() {
    // Release GDI+ objects before the font family and the GDI+ runtime itself.
    fonts_.clear();
    backBuffer_.reset();
    if (family_) delete family_;
    family_ = nullptr;
    if (iconLarge_) DestroyIcon(iconLarge_);
    if (iconSmall_) DestroyIcon(iconSmall_);
    if (gdiplusToken_) GdiplusShutdown(gdiplusToken_);
}

// ---------------------------------------------------------------------------
bool App::create(HINSTANCE instance) {
    instance_ = instance;
    GdiplusStartupInput gdiIn;
    if (GdiplusStartup(&gdiplusToken_, &gdiIn, nullptr) != Ok) return false;

    config_ = loadConfig();
    switch (config_.lang) {
        case LangMode::Chinese: chinese_ = true; break;
        case LangMode::English: chinese_ = false; break;
        default: {
            std::wstring locale = systemLocaleName();
            chinese_ = locale.size() >= 2 && (locale[0] == L'z' && locale[1] == L'h');
            break;
        }
    }

    family_ = new FontFamily(L"Microsoft YaHei UI");
    if (family_->GetLastStatus() != Ok || !family_->IsAvailable()) {
        delete family_;
        family_ = new FontFamily(L"Segoe UI");
        if (family_->GetLastStatus() != Ok || !family_->IsAvailable()) {
            delete family_;
            const FontFamily* generic = FontFamily::GenericSansSerif();
            family_ = generic ? generic->Clone() : new FontFamily(L"Arial");
        }
    }

    backups_.init();
    lastBackupLabel_ = backups_.latestSnapshotLabel();
    refreshStates();

    // Initial theme + wallpaper sample before the first paint.
    reloadWallpaper(true, true);
    applyTheme();

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = &App::staticProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = nullptr;
    wc.hbrBackground = nullptr;
    wc.lpszClassName = kClassName;
    RegisterClassExW(&wc);

    scale_ = 1.0f;
    HDC screen = GetDC(nullptr);
    if (screen) {
        scale_ = GetDeviceCaps(screen, LOGPIXELSX) / 96.0f;
        ReleaseDC(nullptr, screen);
    }

    int winW = (int)std::lround(1080 * scale_);
    int winH = (int)std::lround(720 * scale_);
    int x = (GetSystemMetrics(SM_CXSCREEN) - winW) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - winH) / 2;

    hwnd_ = CreateWindowExW(0, kClassName, L"WinPrivacyGreen", WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_SYSMENU,
                            x, y, winW, winH, nullptr, nullptr, instance, this);
    if (!hwnd_) return false;

    width_ = winW / scale_;
    height_ = winH / scale_;
    onDpiChanged();
    applyTheme();
    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);
    SetTimer(hwnd_, kTimerWallpaper, 2500, nullptr);
    return true;
}

int App::run() {
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}

LRESULT CALLBACK App::staticProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCCREATE) {
        auto* cs = (CREATESTRUCTW*)lp;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
    }
    auto* self = (App*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    if (!self) return DefWindowProcW(hwnd, msg, wp, lp);
    return self->proc(hwnd, msg, wp, lp);
}

void App::onDpiChanged() {
    UINT dpi = 96;
    if (auto fn = (GetDpiForWindowFn)GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"))
        dpi = fn(hwnd_);
    scale_ = dpi / 96.0f;
    fonts_.clear();

    // Window icon rendered from the same palette as the UI.
    HICON icon = (HICON)LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1), IMAGE_ICON, 0, 0,
                                   LR_DEFAULTSIZE);
    if (!icon) {
        wchar_t exe[MAX_PATH * 2] = {};
        GetModuleFileNameW(nullptr, exe, MAX_PATH * 2);
        std::wstring path = exe;
        size_t dot = path.find_last_of(L'.');
        if (dot != std::wstring::npos) path = path.substr(0, dot) + L".ico";
        icon = (HICON)LoadImageW(nullptr, path.c_str(), IMAGE_ICON, 0, 0, LR_LOADFROMFILE);
    }
    if (icon) {
        if (iconLarge_) DestroyIcon(iconLarge_);
        iconLarge_ = icon;
        SendMessageW(hwnd_, WM_SETICON, ICON_BIG, (LPARAM)iconLarge_);
    }
    HICON smallIcon = static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1), IMAGE_ICON,
                                                  GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0));
    if (smallIcon) {
        if (iconSmall_) DestroyIcon(iconSmall_);
        iconSmall_ = smallIcon;
        SendMessageW(hwnd_, WM_SETICON, ICON_SMALL, (LPARAM)iconSmall_);
    }
}

// ---------------------------------------------------------------------------
void App::applyTheme() {
    dark_ = config_.theme == ThemeMode::Dark || (config_.theme == ThemeMode::Auto && systemUsesDarkForApps());
    uint32_t seed = config_.seedColor;
    if (config_.dynamicColor && wallpaperOk_) seed = wallpaperSeedArgb_;
    seedArgb_ = seed;
    scheme_ = buildScheme(oklchFromArgb(seed), dark_);

    if (hwnd_) {
        BOOL darkFrame = dark_ ? TRUE : FALSE;
        // 20 is DWMWA_USE_IMMERSIVE_DARK_MODE on Windows 10 20H1+/11, 19 on 1809/1909.
        if (FAILED(DwmSetWindowAttribute(hwnd_, 20, &darkFrame, sizeof(darkFrame))))
            DwmSetWindowAttribute(hwnd_, 19, &darkFrame, sizeof(darkFrame));
        DWORD border = RGB(chanR(scheme_.outlineVariant), chanG(scheme_.outlineVariant),
                           chanB(scheme_.outlineVariant));  // COLORREF is 0x00BBGGRR
        DwmSetWindowAttribute(hwnd_, 34 /*DWMWA_BORDER_COLOR*/, &border, sizeof(border));
        DWORD corner = 2;  // DWMWCP_ROUND
        DwmSetWindowAttribute(hwnd_, 33, &corner, sizeof(corner));
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void App::reloadWallpaper(bool force, bool silent) {
    std::wstring path = currentWallpaperPath();
    if (!force && path == wallpaperPath_) return;
    wallpaperPath_ = path;
    wallpaperOk_ = false;
    if (path.empty()) {
        if (!silent && hwnd_) showToast(tr(L"未找到壁纸文件，已使用品牌绿", L"No wallpaper found, using brand green"), 0);
        applyTheme();
        return;
    }
    uint32_t rgb = 0;
    Oklch seed;
    if (sampleImageColor(path, rgb, seed)) {
        wallpaperOk_ = true;
        wallpaperSeedRgb_ = rgb;
        wallpaperSeedArgb_ = argbFromOklch(seed.L, seed.C, seed.h);
    }
    if (config_.dynamicColor && !silent) applyTheme();
    if (hwnd_) InvalidateRect(hwnd_, nullptr, FALSE);
}

void App::refreshStates() {
    const auto& catalog = itemCatalog();
    states_.assign(catalog.size(), ItemState{});
    for (size_t i = 0; i < catalog.size(); ++i) states_[i] = readItemState(catalog[i]);
}

void App::updateConfig() { saveConfig(config_); }

void App::showToast(const std::wstring& text, int kind) {
    toastText_ = text;
    toastKind_ = kind;
    toastVisible_ = true;
    if (hwnd_) {
        SetTimer(hwnd_, kTimerToast, 3600, nullptr);
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void App::openConfirm(const std::wstring& title, const std::wstring& body, const std::wstring& okLabel, int action) {
    confirmTitle_ = title;
    confirmBody_ = body;
    confirmOk_ = okLabel;
    confirmAction_ = action;
    confirmVisible_ = true;
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void App::closeConfirm() {
    confirmVisible_ = false;
    confirmAction_ = A_None;
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void App::setPage(Page page) {
    page_ = page;
    scroll_ = 0;
    InvalidateRect(hwnd_, nullptr, FALSE);
}

// ---------------------------------------------------------------------------
void App::addHotspot(const RectF& r, int action, int a, int b) {
    Hotspot spot;
    spot.rect = r;
    spot.action = action;
    spot.a = a;
    spot.b = b;
    hotspots_.push_back(spot);
}

bool App::hovered(int action, int a, int b) const {
    if (hoverAction_ != action) return false;
    if (a >= 0 && hoverA_ != a) return false;
    if (b >= 0 && hoverB_ != b) return false;
    return true;
}

float App::hoverT(int action, int a) const { return (hoverAction_ == action && (a < 0 || hoverA_ == a)) ? 1.0f : 0.0f; }

Font* App::font(float size, bool bold) {
    int key = (int)std::lround(size * 4) * 2 + (bold ? 1 : 0);
    auto it = fonts_.find(key);
    if (it != fonts_.end()) return it->second.get();
    auto f = std::make_unique<Font>(family_, size, bold ? FontStyleBold : FontStyleRegular, UnitPixel);
    Font* raw = f.get();
    fonts_[key] = std::move(f);
    return raw;
}

// ---------------------------------------------------------------------------
void App::onPaint(HDC hdc) {
    int pxW = std::max(1, (int)std::lround(width_ * scale_));
    int pxH = std::max(1, (int)std::lround(height_ * scale_));
    if (!backBuffer_ || (int)backBuffer_->GetWidth() != pxW || (int)backBuffer_->GetHeight() != pxH) {
        backBuffer_ = std::make_unique<Bitmap>(pxW, pxH, PixelFormat32bppPARGB);
    }
    Graphics g(backBuffer_.get());
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(PixelOffsetModeHalf);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
    g.ScaleTransform(scale_, scale_);

    hotspots_.clear();
    SolidBrush bg(col(scheme_.surface));
    g.FillRectangle(&bg, RectF(0, 0, width_, height_));

    drawSidebar(g, width_, height_);
    drawHeader(g, width_);

    if (page_ == Page::Overview) {
        drawOverview(g, RectF(kSidebarW, 96, width_ - kSidebarW - 32, height_ - 120));
    } else if (page_ == Page::Items) {
        drawItemsPage(g, RectF(kSidebarW, 96, width_ - kSidebarW - 32, height_ - 96 - 92));
    } else if (page_ == Page::Settings) {
        drawSettings(g, RectF(kSidebarW, 96, width_ - kSidebarW - 32, height_ - 120));
    } else {
        drawAbout(g, RectF(kSidebarW, 96, width_ - kSidebarW - 32, height_ - 120));
    }

    if (page_ == Page::Items) drawScrollbar(g, RectF(kSidebarW, 96, width_ - kSidebarW - 32, height_ - 96 - 92));
    if (page_ == Page::Settings) drawScrollbar(g, RectF(kSidebarW, 96, width_ - kSidebarW - 32, height_ - 120));

    drawToast(g, width_, height_);
    drawConfirm(g, width_, height_);

    Graphics screen(hdc);
    screen.DrawImage(backBuffer_.get(), 0, 0);
    hitAreas_ = hotspots_;
}

void App::onResize(int pxW, int pxH) {
    if (pxW <= 0 || pxH <= 0) return;
    width_ = pxW / scale_;
    height_ = pxH / scale_;
    backBuffer_.reset();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void App::onMouseMove(float x, float y) {
    int action = A_None, a = -1, b = -1;
    for (auto it = hitAreas_.rbegin(); it != hitAreas_.rend(); ++it) {
        if (it->rect.Contains(x, y)) {
            action = it->action;
            a = it->a;
            b = it->b;
            break;
        }
    }
    if (confirmVisible_) {
        if (action != A_ConfirmOk && action != A_ConfirmCancel) {
            action = A_None;
            a = b = -1;
        }
    }
    if (action != hoverAction_ || a != hoverA_ || b != hoverB_) {
        hoverAction_ = action;
        hoverA_ = a;
        hoverB_ = b;
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
    if (action == A_None) {
        SetCursor(LoadCursorW(nullptr, IDC_ARROW));
    } else {
        SetCursor(LoadCursorW(nullptr, IDC_HAND));
    }
    if (!tracking_) {
        TRACKMOUSEEVENT tme{sizeof(tme), TME_LEAVE, hwnd_, 0};
        TrackMouseEvent(&tme);
        tracking_ = true;
    }
}

void App::onMouseLeave() {
    tracking_ = false;
    hoverAction_ = A_None;
    hoverA_ = hoverB_ = -1;
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void App::onMouseDown(float x, float y) {
    pressAction_ = A_None;
    pressA_ = -1;
    for (auto it = hitAreas_.rbegin(); it != hitAreas_.rend(); ++it) {
        if (!it->rect.Contains(x, y)) continue;
        if (confirmVisible_ && it->action != A_ConfirmOk && it->action != A_ConfirmCancel) break;
        pressAction_ = it->action;
        pressA_ = it->a;
        break;
    }
    if (pressAction_ == A_None && confirmVisible_) return;
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void App::onMouseUp(float x, float y) {
    int action = A_None, a = -1, b = -1;
    for (auto it = hitAreas_.rbegin(); it != hitAreas_.rend(); ++it) {
        if (!it->rect.Contains(x, y)) continue;
        if (confirmVisible_ && it->action != A_ConfirmOk && it->action != A_ConfirmCancel) break;
        action = it->action;
        a = it->a;
        b = it->b;
        break;
    }
    if (action != A_None && action == pressAction_ && a == pressA_) {
        Hotspot spot;
        spot.action = action;
        spot.a = a;
        spot.b = b;
        invoke(spot);
    }
    pressAction_ = A_None;
    pressA_ = -1;
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void App::onMouseWheel(int delta) {
    if (page_ != Page::Items && page_ != Page::Settings) return;
    if (maxScroll_ <= 0) return;
    scroll_ = clampf(scroll_ - delta / 120.0f * 64.0f, 0.0f, maxScroll_);
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void App::onKeyDown(int vk) {
    if (confirmVisible_) {
        if (vk == VK_ESCAPE) closeConfirm();
        return;
    }
    if (page_ == Page::Items) {
        float step = 64.0f;
        if (vk == VK_PRIOR) scroll_ = clampf(scroll_ - step * 5, 0, maxScroll_);
        if (vk == VK_NEXT) scroll_ = clampf(scroll_ + step * 5, 0, maxScroll_);
        if (vk == VK_HOME) scroll_ = 0;
        if (vk == VK_END) scroll_ = maxScroll_;
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void App::onTimer(UINT_PTR id) {
    if (id == kTimerToast) {
        KillTimer(hwnd_, kTimerToast);
        toastVisible_ = false;
        InvalidateRect(hwnd_, nullptr, FALSE);
    } else if (id == kTimerWallpaper) {
        // Follow wallpaper changes (slideshows, rotations) live.
        if (config_.dynamicColor) reloadWallpaper(false, true);
    }
}

// ---------------------------------------------------------------------------
void App::toggleItem(int index) {
    const auto& catalog = itemCatalog();
    if (index < 0 || index >= (int)catalog.size()) return;
    const ItemDef& def = catalog[index];
    if (!states_[index].available) return;
    bool target = !states_[index].blocked;
    if (config_.backup) {
        for (const auto& op : def.ops) backups_.remember(op.addr);
    }
    bool denied = false;
    bool ok = applyItem(def, target, &denied);
    states_[index] = readItemState(def);
    std::wstring name = chinese_ ? def.zhTitle : def.enTitle;
    if (!ok) {
        showToast(denied ? tr(L"需要管理员权限才能修改此项", L"Administrator rights are required")
                         : tr(L"写入注册表失败", L"Failed to write the registry"),
                  2);
    } else if (target) {
        showToast(tr(L"已屏蔽：", L"Blocked: ") + name, 1);
    } else {
        showToast(tr(L"已恢复：", L"Restored: ") + name, 1);
    }
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void App::setAllItems(bool block) {
    const auto& catalog = itemCatalog();
    int changed = 0, failed = 0;
    bool anyDenied = false;
    for (size_t i = 0; i < catalog.size(); ++i) {
        if (!states_[i].available) continue;
        if (states_[i].blocked == block) continue;
        if (config_.backup) {
            for (const auto& op : catalog[i].ops) backups_.remember(op.addr);
        }
        bool denied = false;
        if (applyItem(catalog[i], block, &denied)) {
            ++changed;
        } else {
            ++failed;
            anyDenied = anyDenied || denied;
        }
    }
    refreshStates();
    if (changed == 0 && failed == 0) {
        showToast(block ? tr(L"所有项目都已经屏蔽", L"Everything is already blocked")
                        : tr(L"没有需要恢复的项目", L"Nothing to restore"),
                  0);
    } else if (failed > 0) {
        showToast(std::wstring(block ? tr(L"已屏蔽 ", L"Blocked ") : tr(L"已恢复 ", L"Restored ")) +
                      formatInt(changed) + tr(L" 项，", L" items, ") + formatInt(failed) +
                      tr(L" 项失败（需要管理员权限）", L" failed (administrator rights required)"),
                  failed >= changed ? 2 : 1);
        (void)anyDenied;
    } else {
        showToast((block ? tr(L"已屏蔽 ", L"Blocked ") : tr(L"已恢复 ", L"Restored ")) + formatInt(changed) +
                      tr(L" 项设置", L" settings"),
                  1);
    }
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void App::runRestore(bool fromSnapshot) {
    std::wstring error;
    bool ok = fromSnapshot ? backups_.restoreLatestSnapshot(&error) : backups_.restoreOriginals(&error);
    refreshStates();
    if (ok) {
        showToast(tr(L"已恢复到此前的设置", L"Restored to the previous settings"), 1);
    } else {
        showToast(tr(L"恢复失败，部分项目需要管理员权限", L"Restore failed, some items need administrator rights"), 2);
    }
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void App::doBackup(bool silent) {
    std::wstring error;
    if (backups_.createSnapshot(&error)) {
        lastBackupLabel_ = backups_.latestSnapshotLabel();
        if (!silent) showToast(tr(L"备份已保存", L"Backup saved"), 1);
    } else {
        showToast(tr(L"备份失败，请检查磁盘权限", L"Backup failed, check folder permissions"), 2);
    }
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void App::doRestoreOriginals() {
    if (!backups_.hasOriginals()) {
        showToast(tr(L"还没有记录可恢复的修改", L"No recorded changes to restore"), 0);
        return;
    }
    runRestore(false);
}

void App::promptRestoreAll() {
    if (backups_.hasOriginals()) {
        runRestore(false);
    } else {
        setAllItems(false);
    }
}

void App::invoke(const Hotspot& spot) {
    switch (spot.action) {
        case A_Nav:
            setPage((Page)spot.a);
            break;
        case A_ToggleItem:
            toggleItem(spot.a);
            break;
        case A_Theme: {
            config_.theme = (ThemeMode)spot.a;
            updateConfig();
            applyTheme();
            break;
        }
        case A_DynamicToggle: {
            config_.dynamicColor = !config_.dynamicColor;
            updateConfig();
            applyTheme();
            if (config_.dynamicColor && !wallpaperOk_)
                showToast(tr(L"未找到可用壁纸，将继续使用品牌绿", L"No usable wallpaper, keeping brand green"), 0);
            break;
        }
        case A_Seed: {
            config_.dynamicColor = spot.a == 0;
            if (spot.a != 0) config_.seedColor = (uint32_t)spot.b;
            updateConfig();
            applyTheme();
            break;
        }
        case A_BackupToggle: {
            config_.backup = !config_.backup;
            updateConfig();
            if (!config_.backup)
                showToast(tr(L"已关闭备份，修改后仍可用“全部恢复”写回默认值", L"Backups off: Restore all still writes defaults"), 0);
            else
                showToast(tr(L"已开启备份，修改前会自动记录原值", L"Backups on: original values are recorded"), 1);
            InvalidateRect(hwnd_, nullptr, FALSE);
            break;
        }
        case A_BackupNow:
            doBackup(false);
            break;
        case A_RestoreSnapshot: {
            if (backups_.snapshotCount() == 0) {
                showToast(tr(L"还没有备份文件", L"No backup files yet"), 0);
                break;
            }
            openConfirm(tr(L"恢复最近的备份？", L"Restore the latest backup?"),
                        tr(L"将把下列设置写回备份中的状态：", L"These settings will be written back to the snapshot: ") +
                            backups_.latestSnapshotLabel(),
                        tr(L"恢复", L"Restore"), A_RestoreSnapshot);
            break;
        }
        case A_RestoreOriginals:
            doRestoreOriginals();
            break;
        case A_OpenBackupFolder:
            openFolder(backups_.directory());
            break;
        case A_BlockAll:
            if (!config_.backup) {
                openConfirm(tr(L"备份已关闭", L"Backups are off"),
                            tr(L"本次操作会修改多项注册表设置，并且不会留下备份记录。仍然继续吗？",
                               L"This will change several registry settings without a backup. Continue?"),
                            tr(L"仍然屏蔽", L"Block anyway"), A_BlockAll);
            } else {
                setAllItems(true);
            }
            break;
        case A_RestoreAll:
            promptRestoreAll();
            break;
        case A_OpenUrl:
        case A_OpenRepo:
            openUrl(kProjectUrl);
            break;
        case A_Lang: {
            config_.lang = (LangMode)spot.a;
            chinese_ = config_.lang == LangMode::Chinese ||
                       (config_.lang == LangMode::Auto &&
                        systemLocaleName().size() >= 2 && systemLocaleName()[0] == L'z' && systemLocaleName()[1] == L'h');
            updateConfig();
            InvalidateRect(hwnd_, nullptr, FALSE);
            break;
        }
        case A_Minimize:
            ShowWindow(hwnd_, SW_MINIMIZE);
            break;
        case A_Close:
            PostMessageW(hwnd_, WM_CLOSE, 0, 0);
            break;
        case A_ConfirmOk: {
            int action = confirmAction_;
            closeConfirm();
            if (action == A_BlockAll) setAllItems(true);
            if (action == A_RestoreSnapshot) runRestore(true);
            break;
        }
        case A_ConfirmCancel:
            closeConfirm();
            break;
        default:
            break;
    }
}

// ---------------------------------------------------------------------------
LRESULT App::proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE:
            return 0;
        case WM_NCCALCSIZE:
            if (wp == TRUE) return 0;
            break;
        case WM_GETMINMAXINFO: {
            auto* mmi = (MINMAXINFO*)lp;
            mmi->ptMinTrackSize.x = (LONG)std::lround(900 * scale_);
            mmi->ptMinTrackSize.y = (LONG)std::lround(600 * scale_);
            mmi->ptMaxTrackSize.x = (LONG)std::lround(1800 * scale_);
            mmi->ptMaxTrackSize.y = (LONG)std::lround(1200 * scale_);
            return 0;
        }
        case WM_NCHITTEST: {
            POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            ScreenToClient(hwnd_, &pt);
            float x = pt.x / scale_;
            float y = pt.y / scale_;
            const float border = 6.0f;
            bool left = x < border, right = x > width_ - border;
            bool top = y < border, bottom = y > height_ - border;
            if (top && left) return HTTOPLEFT;
            if (top && right) return HTTOPRIGHT;
            if (bottom && left) return HTBOTTOMLEFT;
            if (bottom && right) return HTBOTTOMRIGHT;
            if (left) return HTLEFT;
            if (right) return HTRIGHT;
            if (top) return HTTOP;
            if (bottom) return HTBOTTOM;
            if (y < 88) {
                if (x < kSidebarW) return HTCAPTION;
                if (x < width_ - 104) return HTCAPTION;
            }
            return HTCLIENT;
        }
        case WM_NCLBUTTONDBLCLK:
            return 0;  // no maximise
        case WM_SYSCOMMAND:
            if ((wp & 0xFFF0) == SC_MAXIMIZE) return 0;
            break;
        case WM_SIZE:
            if (wp != SIZE_MINIMIZED) onResize(LOWORD(lp), HIWORD(lp));
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            onPaint(hdc);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_DPICHANGED: {
            onDpiChanged();
            RECT* r = (RECT*)lp;
            SetWindowPos(hwnd, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }
        case WM_MOUSEMOVE:
            onMouseMove(GET_X_LPARAM(lp) / scale_, GET_Y_LPARAM(lp) / scale_);
            return 0;
        case WM_MOUSELEAVE:
            onMouseLeave();
            return 0;
        case WM_LBUTTONDOWN:
            SetCapture(hwnd);
            onMouseDown(GET_X_LPARAM(lp) / scale_, GET_Y_LPARAM(lp) / scale_);
            return 0;
        case WM_LBUTTONUP:
            ReleaseCapture();
            onMouseUp(GET_X_LPARAM(lp) / scale_, GET_Y_LPARAM(lp) / scale_);
            return 0;
        case WM_SETCURSOR:
            if (LOWORD(lp) == HTCLIENT) return TRUE;  // cursor set in our mouse tracking
            break;
        case WM_MOUSEWHEEL:
            onMouseWheel(GET_WHEEL_DELTA_WPARAM(wp));
            return 0;
        case WM_KEYDOWN:
            onKeyDown((int)wp);
            return 0;
        case WM_TIMER:
            onTimer(wp);
            return 0;
        case WM_SETTINGCHANGE:
            if (config_.theme == ThemeMode::Auto) applyTheme();
            if (config_.dynamicColor) reloadWallpaper(false, true);
            return 0;
        case WM_DWMCOLORIZATIONCOLORCHANGED:
            if (config_.dynamicColor) reloadWallpaper(false, true);
            return 0;
        case WM_KEYUP:
            return 0;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            KillTimer(hwnd, kTimerWallpaper);
            KillTimer(hwnd, kTimerToast);
            PostQuitMessage(0);
            return 0;
        default:
            break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

}  // namespace wpg
