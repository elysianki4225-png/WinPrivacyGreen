#include <windows.h>
#include <gdiplus.h>

#include <algorithm>
#include <string>
#include <vector>

#include "draw.h"
#include "items.h"
#include "sys.h"
#include "ui.h"

using namespace Gdiplus;

namespace wpg {

namespace {

const wchar_t* kVersion = L"1.0.0";
const wchar_t* kProjectUrl = L"https://github.com/elysianki4225-png/WinPrivacyGreen";

void cardBg(Graphics& g, const RectF& r, const Scheme& s) {
    fillRound(g, r, 20, s.surfaceContainerLow);
    strokeRound(g, r, 20, blendArgb(s.surfaceContainerLow, s.outlineVariant, 0.6f), 1.0f);
}

uint32_t dim(uint32_t color, uint32_t surface, float t) { return blendArgb(color, surface, t); }

}  // namespace

const wchar_t* App::pageTitle(Page p) const {
    switch (p) {
        case Page::Overview: return tr(L"概览", L"Overview");
        case Page::Items: return tr(L"广告与隐私", L"Ads & privacy");
        case Page::Settings: return tr(L"设置", L"Settings");
        default: return tr(L"关于", L"About");
    }
}

const wchar_t* App::pageSubtitle(Page p) const {
    switch (p) {
        case Page::Overview:
            return tr(L"关闭 Windows 里的广告、推荐与数据收集", L"Turn off ads, suggestions and data collection");
        case Page::Items:
            return tr(L"逐项开关，或者一键批量处理", L"Toggle one by one, or apply in bulk");
        case Page::Settings:
            return tr(L"外观、取色与备份", L"Appearance, color source and backups");
        default:
            return tr(L"开源、离线、只改本机设置", L"Open source, offline, local settings only");
    }
}

// ---------------------------------------------------------------------------
void App::drawSidebar(Graphics& g, float w, float h) {
    SolidBrush side(col(scheme_.surfaceContainerLow));
    g.FillRectangle(&side, RectF(0, 0, kSidebarW, h));
    SolidBrush divider(col(scheme_.outlineVariant));
    g.FillRectangle(&divider, RectF(kSidebarW - 1.0f, 0, 1.0f, h));

    fillRound(g, RectF(22, 22, 36, 36), 12, scheme_.primaryContainer);
    drawIcon(g, IconKind::Shield, RectF(31, 31, 18, 18), scheme_.onPrimaryContainer, 2.0f);
    drawText(g, L"WinPrivacyGreen", RectF(68, 24, 156, 20), font(13, true), scheme_.onSurface);
    drawText(g, tr(L"Windows 隐私清理", L"Windows privacy"), RectF(68, 43, 156, 16), font(11, false),
             scheme_.onSurfaceVariant);

    const wchar_t* labels[4] = {tr(L"概览", L"Overview"), tr(L"广告与隐私", L"Ads & privacy"),
                                tr(L"设置", L"Settings"), tr(L"关于", L"About")};
    IconKind icons[4] = {IconKind::Grid, IconKind::Shield, IconKind::Gear, IconKind::Doc};
    float y = 90;
    for (int i = 0; i < 4; ++i) {
        RectF r(12, y, 212, 46);
        bool selected = (int)page_ == i;
        if (selected) {
            fillRound(g, r, 23, scheme_.secondaryContainer);
        } else if (hovered(A_Nav, i)) {
            fillRound(g, r, 23, blendArgb(scheme_.surfaceContainerLow, scheme_.onSurface, 0.06f));
        }
        uint32_t fg = selected ? scheme_.onSecondaryContainer : scheme_.onSurfaceVariant;
        drawIcon(g, icons[i], RectF(28, y + 14, 18, 18), fg, 1.9f);
        drawText(g, labels[i], RectF(58, y, 150, 46), font(13, selected), fg);
        addHotspot(r, A_Nav, i);
        y += 50;
    }

    // Footer
    SolidBrush footLine(col(scheme_.outlineVariant));
    g.FillRectangle(&footLine, RectF(24, h - 84, kSidebarW - 48, 1.0f));
    drawText(g, tr(L"Windows 10 / 11", L"Windows 10 / 11"), RectF(24, h - 74, 190, 16), font(11, false),
             scheme_.onSurfaceVariant);
    drawText(g, L"MIT License", RectF(24, h - 56, 190, 16), font(11, false), scheme_.onSurfaceVariant);
}

void App::drawHeader(Graphics& g, float w) {
    drawText(g, pageTitle(page_), RectF(kSidebarW + 32, 20, w - kSidebarW - 160, 34), font(21, true), scheme_.onSurface);
    drawText(g, pageSubtitle(page_), RectF(kSidebarW + 32, 54, w - kSidebarW - 160, 18), font(12, false),
             scheme_.onSurfaceVariant);

    RectF minR(w - 88, 14, 34, 34);
    RectF closeR(w - 48, 14, 34, 34);
    if (hovered(A_Minimize)) fillRound(g, minR, 17, scheme_.surfaceContainerHigh);
    if (hovered(A_Close)) fillRound(g, closeR, 17, scheme_.errorContainer);
    Pen minPen(col(hovered(A_Minimize) ? scheme_.onSurface : scheme_.onSurfaceVariant), 1.6f);
    minPen.SetLineCap(LineCapRound, LineCapRound, DashCapRound);
    g.DrawLine(&minPen, minR.X + 11, minR.Y + 17, minR.X + 23, minR.Y + 17);
    Pen closePen(col(hovered(A_Close) ? scheme_.onErrorContainer : scheme_.onSurfaceVariant), 1.6f);
    closePen.SetLineCap(LineCapRound, LineCapRound, DashCapRound);
    g.DrawLine(&closePen, closeR.X + 11, closeR.Y + 11, closeR.X + 23, closeR.Y + 23);
    g.DrawLine(&closePen, closeR.X + 23, closeR.Y + 11, closeR.X + 11, closeR.Y + 23);
    addHotspot(minR, A_Minimize);
    addHotspot(closeR, A_Close);
}

// ---------------------------------------------------------------------------
void App::drawOverview(Graphics& g, const RectF& view) {
    maxScroll_ = 0;
    const auto& catalog = itemCatalog();
    int total = (int)catalog.size();
    int available = 0, blocked = 0;
    for (size_t i = 0; i < states_.size(); ++i) {
        if (!states_[i].available) continue;
        ++available;
        if (states_[i].blocked) ++blocked;
    }

    const float gap = 16;
    const float cw = (view.Width - gap * 2) / 3;
    const float ch = 96;
    struct Stat {
        const wchar_t* label;
        int value;
        uint32_t accent;
    };
    Stat stats[3] = {{tr(L"可关闭项目", L"Available"), available, scheme_.primary},
                     {tr(L"已屏蔽", L"Blocked"), blocked, scheme_.tertiary},
                     {tr(L"项目合计", L"Total"), total, scheme_.onSurface}};
    for (int i = 0; i < 3; ++i) {
        RectF r(view.X + i * (cw + gap), view.Y, cw, ch);
        cardBg(g, r, scheme_);
        drawText(g, stats[i].label, RectF(r.X + 20, r.Y + 16, cw - 40, 18), font(12, false), scheme_.onSurfaceVariant);
        std::wstring value = std::to_wstring(stats[i].value);
        float numW = textWidth(g, value, font(28, true));
        drawText(g, value, RectF(r.X + 20, r.Y + 38, numW + 6, 38), font(28, true), stats[i].accent);
        drawText(g, tr(L"项", L" items"), RectF(r.X + 26 + numW, r.Y + 52, 80, 20), font(12, false),
                 scheme_.onSurfaceVariant);
    }

    // Quick actions
    RectF quick(view.X, view.Y + ch + 16, view.Width, 136);
    cardBg(g, quick, scheme_);
    drawText(g, tr(L"批量处理", L"Bulk actions"), RectF(quick.X + 20, quick.Y + 16, quick.Width - 40, 24),
             font(15, true), scheme_.onSurface);
    drawText(g, tr(L"修改会立即写入当前用户的注册表，并记录原值。",
                   L"Changes are written to your registry right away, original values are recorded."),
             RectF(quick.X + 20, quick.Y + 42, quick.Width - 40, 20), font(12, false), scheme_.onSurfaceVariant);
    RectF blockBtn(quick.X + 20, quick.Y + 74, 124, 42);
    RectF restoreBtn(quick.X + 152, quick.Y + 74, 124, 42);
    drawButton(g, blockBtn, tr(L"全部屏蔽", L"Block all"), font(13, true), BtnStyle::Filled, scheme_.primary,
               scheme_.onPrimary, scheme_.primary, hovered(A_BlockAll), pressAction_ == A_BlockAll, true);
    drawButton(g, restoreBtn, tr(L"全部恢复", L"Restore all"), font(13, false), BtnStyle::Outlined,
               scheme_.surfaceContainerLow, scheme_.primary, scheme_.outline, hovered(A_RestoreAll), false, true);
    addHotspot(blockBtn, A_BlockAll);
    addHotspot(restoreBtn, A_RestoreAll);

    // Backup card
    RectF backup(view.X, quick.GetBottom() + 16, view.Width, 144);
    cardBg(g, backup, scheme_);
    drawText(g, tr(L"备份与恢复", L"Backup & restore"), RectF(backup.X + 20, backup.Y + 16, backup.Width - 40, 24),
             font(15, true), scheme_.onSurface);
    std::wstring info = lastBackupLabel_.empty() ? tr(L"还没有备份文件", L"No backups yet")
                                                 : tr(L"最近备份  ", L"Latest  ") + lastBackupLabel_;
    if (!config_.backup) {
        drawText(g, tr(L"备份已关闭 · 修改不会留下记录", L"Backups off - changes leave no record"),
                 RectF(backup.X + 20, backup.Y + 42, backup.Width - 40, 20), font(12, false), scheme_.error);
        drawText(g, info, RectF(backup.X + 20, backup.Y + 62, backup.Width - 40, 20), font(11, false),
                 scheme_.onSurfaceVariant);
    } else {
        drawText(g, info, RectF(backup.X + 20, backup.Y + 44, backup.Width - 40, 20), font(12, false),
                 scheme_.onSurfaceVariant);
    }
    RectF b1(backup.X + 20, backup.Y + 90, 108, 40);
    RectF b2(backup.X + 140, backup.Y + 90, 128, 40);
    RectF b3(backup.X + 280, backup.Y + 90, 118, 40);
    drawButton(g, b1, tr(L"立即备份", L"Back up now"), font(12, false), BtnStyle::Tonal, scheme_.secondaryContainer,
               scheme_.onSecondaryContainer, scheme_.secondaryContainer, hovered(A_BackupNow), false, true);
    drawButton(g, b2, tr(L"恢复最近备份", L"Restore backup"), font(12, false), BtnStyle::Outlined,
               scheme_.surfaceContainerLow, scheme_.onSurface, scheme_.outline, hovered(A_RestoreSnapshot), false,
               backups_.snapshotCount() > 0);
    drawButton(g, b3, tr(L"打开备份目录", L"Open folder"), font(12, false), BtnStyle::Text, scheme_.surfaceContainerLow,
               scheme_.primary, scheme_.primary, hovered(A_OpenBackupFolder), false, true);
    addHotspot(b1, A_BackupNow);
    addHotspot(b2, A_RestoreSnapshot);
    addHotspot(b3, A_OpenBackupFolder);

    RectF link(view.X + 4, backup.GetBottom() + 18, view.Width - 8, 22);
    drawText(g, tr(L"项目地址：", L"Project: ") + std::wstring(kProjectUrl), link, font(12, false), scheme_.primary);
    addHotspot(link, A_OpenRepo);
}

// ---------------------------------------------------------------------------
void App::drawItemsPage(Graphics& g, const RectF& view) {
    Font* smallFont = font(12, false);

    // ---- toolbar
    int availableCount = 0, blockedCount = 0;
    for (const auto& state : states_) {
        if (!state.available) continue;
        ++availableCount;
        if (state.blocked) ++blockedCount;
    }
    drawText(g, tr(L"共 ", L"") + std::to_wstring(availableCount) + tr(L" 项可用 · 已屏蔽 ", L" available · blocked ") +
                    std::to_wstring(blockedCount) + tr(L" 项", L""),
             RectF(view.X + 4, view.Y, 320, 40), smallFont, scheme_.onSurfaceVariant);

    RectF backupSwitch(view.GetRight() - 46, view.Y + 8, 44, 24);
    drawText(g, tr(L"修改前备份", L"Back up before changes"),
             RectF(view.GetRight() - 46 - 8 - 160, view.Y + 8, 160, 24), smallFont, scheme_.onSurfaceVariant,
             TextAlign::Right);
    drawSwitch(g, backupSwitch, config_.backup, hovered(A_BackupToggle), scheme_.primary, scheme_.surfaceVariant,
               scheme_.onPrimary, scheme_.outline, true);
    addHotspot(backupSwitch, A_BackupToggle);

    // ---- list
    float listTop = view.Y + 56;
    float drawY = listTop - scroll_;
    float contentEnd = listTop;

    GraphicsState clipState = g.Save();
    g.SetClip(RectF(view.X - 8, view.Y + 48, view.Width + 16, view.Height - 48));

    const auto& catalog = itemCatalog();

    for (int group = 0; group < (int)ItemGroup::Count; ++group) {
        std::vector<size_t> groupItems;
        for (size_t idx = 0; idx < catalog.size(); ++idx)
            if ((int)catalog[idx].group == group) groupItems.push_back(idx);
        if (groupItems.empty()) continue;

        int groupBlocked = 0;
        for (size_t idx : groupItems)
            if (states_[idx].blocked) ++groupBlocked;

        if (drawY + 30 > view.Y && drawY < view.GetBottom()) {
            drawText(g, groupTitle((ItemGroup)group, chinese_), RectF(view.X + 4, drawY, 320, 26), font(12, true),
                     scheme_.primary);
            std::wstring counter = std::to_wstring(groupBlocked) + L" / " + std::to_wstring(groupItems.size());
            drawText(g, counter, RectF(view.GetRight() - 120, drawY, 92, 26), font(11, false), scheme_.onSurfaceVariant,
                     TextAlign::Right);
        }
        drawY += 34;

        for (size_t idx : groupItems) {
            if (idx >= states_.size()) continue;
            const ItemDef& def = catalog[idx];
            RectF row(view.X, drawY, view.Width - 8, 72);
            bool available = states_[idx].available;
            bool blocked = states_[idx].blocked;
            bool hov = available && hovered(A_ToggleItem, (int)idx);
            uint32_t base = scheme_.surfaceContainerLow;
            uint32_t rowBg = hov ? blendArgb(base, scheme_.primary, 0.07f) : base;
            if (drawY + 72 > view.Y && drawY < view.GetBottom()) {
                fillRound(g, row, 16, rowBg);
                strokeRound(g, row, 16, blendArgb(rowBg, scheme_.outlineVariant, 0.55f), 1.0f);

                uint32_t badgeBg = available ? scheme_.secondaryContainer : scheme_.surfaceContainerHighest;
                uint32_t badgeFg = available ? scheme_.onSecondaryContainer : scheme_.outline;
                drawIconBadge(g, RectF(row.X + 14, row.Y + 18, 36, 36), def.icon, badgeBg, badgeFg);

                float textX = row.X + 64;
                float textW = row.Width - 64 - 200;
                uint32_t titleColor = available ? scheme_.onSurface : dim(scheme_.onSurface, base, 0.5f);
                uint32_t descColor = available ? scheme_.onSurfaceVariant : dim(scheme_.onSurfaceVariant, base, 0.5f);
                std::wstring title = chinese_ ? def.zhTitle : def.enTitle;
                drawText(g, title, RectF(textX, row.Y + 14, textW, 22), font(14, false), titleColor);
                if (def.needsAdmin && !isAdmin()) {
                    float tw = textWidth(g, title, font(14, false));
                    if (tw < textW - 24)
                        drawIcon(g, IconKind::Lock, RectF(textX + tw + 8, row.Y + 18, 13, 13), scheme_.tertiary, 2.0f);
                }
                drawText(g, chinese_ ? def.zhDesc : def.enDesc, RectF(textX, row.Y + 38, textW, 20), font(12, false),
                         descColor);

                std::wstring state = available ? (blocked ? tr(L"已屏蔽", L"Blocked") : tr(L"已开启", L"Allowed"))
                                               : tr(L"不适用", L"N/A");
                uint32_t stateColor = !available ? scheme_.outline : (blocked ? scheme_.primary : scheme_.onSurfaceVariant);
                drawText(g, state, RectF(row.GetRight() - 58 - 96, row.Y + 24, 96, 24), font(12, false), stateColor,
                         TextAlign::Right);

                RectF sw(row.GetRight() - 58, row.Y + 24, 44, 24);
                drawSwitch(g, sw, blocked, hovered(A_ToggleItem, (int)idx), scheme_.primary, scheme_.surfaceVariant,
                           scheme_.onPrimary, scheme_.outline, available);
            }
            addHotspot(row, available ? A_ToggleItem : A_None, (int)idx);
            drawY += 80;
        }
        drawY += 16;
    }

    g.Restore(clipState);

    contentEnd = drawY + scroll_ - 16;
    maxScroll_ = std::max(0.0f, contentEnd - view.GetBottom());
    if (scroll_ > maxScroll_) scroll_ = maxScroll_;

    // ---- bottom action bar
    const float barH = 84;
    float barY = height_ - barH;
    SolidBrush bar(col(scheme_.surfaceContainerLow));
    g.FillRectangle(&bar, RectF(kSidebarW, barY, width_ - kSidebarW, barH));
    SolidBrush barLine(col(scheme_.outlineVariant));
    g.FillRectangle(&barLine, RectF(kSidebarW, barY, width_ - kSidebarW, 1.0f));

    drawText(g, tr(L"改动立即生效，可随时恢复", L"Changes apply instantly and can be reverted"),
             RectF(kSidebarW + 32, barY + 20, 380, 20), font(12, false), scheme_.onSurface);
    std::wstring backupInfo = config_.backup
                                  ? (tr(L"自动备份已开启", L"Automatic backup on") +
                                     (backups_.originalCount() > 0
                                          ? tr(L" · 已记录 ", L" · recorded ") + std::to_wstring(backups_.originalCount()) +
                                                tr(L" 项原值", L" originals")
                                          : std::wstring()))
                                  : tr(L"自动备份已关闭", L"Automatic backup off");
    drawText(g, backupInfo, RectF(kSidebarW + 32, barY + 44, 380, 18), font(11, false),
             config_.backup ? scheme_.onSurfaceVariant : scheme_.error);

    RectF restoreAll(width_ - 32 - 120, barY + 22, 120, 40);
    RectF blockAll(restoreAll.X - 8 - 124, barY + 22, 124, 40);
    drawButton(g, restoreAll, tr(L"全部恢复", L"Restore all"), font(13, false), BtnStyle::Outlined,
               scheme_.surfaceContainerLow, scheme_.primary, scheme_.outline, hovered(A_RestoreAll), false, true);
    drawButton(g, blockAll, tr(L"全部屏蔽", L"Block all"), font(13, true), BtnStyle::Filled, scheme_.primary,
               scheme_.onPrimary, scheme_.primary, hovered(A_BlockAll), false, true);
    addHotspot(restoreAll, A_RestoreAll);
    addHotspot(blockAll, A_BlockAll);
}

// ---------------------------------------------------------------------------
void App::drawSettings(Graphics& g, const RectF& view) {
    GraphicsState clipState = g.Save();
    g.SetClip(RectF(view.X - 8, view.Y - 8, view.Width + 16, view.Height + 16));

    float y = view.Y - scroll_;

    // ---- appearance
    RectF appearance(view.X, y, view.Width, 250);
    cardBg(g, appearance, scheme_);
    drawText(g, tr(L"外观", L"Appearance"), RectF(appearance.X + 22, appearance.Y + 18, 260, 26), font(15, true),
             scheme_.onSurface);
    drawText(g, tr(L"主题", L"Theme"), RectF(appearance.X + 22, appearance.Y + 56, 200, 24), font(13, false),
             scheme_.onSurface);

    const wchar_t* themeLabels[3] = {tr(L"跟随系统", L"System"), tr(L"亮色", L"Light"), tr(L"暗色", L"Dark")};
    float segW = 92, segGap = 6;
    float segTotal = segW * 3 + segGap * 2;
    float segX = appearance.GetRight() - 22 - segTotal;
    for (int i = 0; i < 3; ++i) {
        RectF chip(segX + i * (segW + segGap), appearance.Y + 50, segW, 34);
        bool selected = (int)config_.theme == i;
        drawChip(g, chip, themeLabels[i], font(12, selected), selected, hovered(A_Theme, i), scheme_.secondaryContainer,
                 scheme_.onSecondaryContainer, scheme_.onSurfaceVariant, scheme_.outline, true);
        addHotspot(chip, A_Theme, i);
    }

    SolidBrush divider1(col(scheme_.outlineVariant));
    g.FillRectangle(&divider1, RectF(appearance.X + 22, appearance.Y + 100, appearance.Width - 44, 1.0f));

    drawText(g, tr(L"动态取色", L"Dynamic color"), RectF(appearance.X + 22, appearance.Y + 112, 260, 24),
             font(13, false), scheme_.onSurface);
    drawText(g, tr(L"从当前壁纸提取主色，生成整套 Material 3 配色",
                   L"Builds the whole Material 3 palette from your wallpaper"),
             RectF(appearance.X + 22, appearance.Y + 134, 420, 18), font(11, false), scheme_.onSurfaceVariant);
    RectF dynSwitch(appearance.GetRight() - 22 - 44, appearance.Y + 112, 44, 24);
    drawSwitch(g, dynSwitch, config_.dynamicColor, hovered(A_DynamicToggle), scheme_.primary, scheme_.surfaceVariant,
               scheme_.onPrimary, scheme_.outline, true);
    addHotspot(dynSwitch, A_DynamicToggle);

    struct SeedOption {
        uint32_t color;
        bool wallpaper;
    };
    const SeedOption seeds[6] = {{0, true},        {0x2E7D32, false}, {0x00796B, false},
                                 {0x3949AB, false}, {0x8E24AA, false}, {0xB3261E, false}};
    float sw = 30, swGap = 12;
    for (int i = 0; i < 6; ++i) {
        RectF dot(appearance.X + 24 + i * (sw + swGap), appearance.Y + 172, sw, sw);
        uint32_t fillColor = seeds[i].wallpaper ? (wallpaperOk_ ? wallpaperSeedRgb_ : scheme_.surfaceVariant)
                                                : seeds[i].color;
        bool selected = seeds[i].wallpaper ? config_.dynamicColor : (!config_.dynamicColor && config_.seedColor == seeds[i].color);
        if (selected) strokeRound(g, RectF(dot.X - 5, dot.Y - 5, sw + 10, sw + 10), (sw + 10) / 2, scheme_.onSurface, 2.0f);
        SolidBrush b(col(fillColor));
        g.FillEllipse(&b, dot);
        if (hovered(A_Seed, i)) strokeRound(g, RectF(dot.X - 3, dot.Y - 3, sw + 6, sw + 6), (sw + 6) / 2,
                                            scheme_.outline, 1.5f);
        if (seeds[i].wallpaper && !wallpaperOk_) {
            drawText(g, L"?", dot, font(13, true), scheme_.onSurfaceVariant, TextAlign::Center);
        }
        addHotspot(RectF(dot.X - 4, dot.Y - 4, sw + 8, sw + 8), A_Seed, i, seeds[i].wallpaper ? 0 : (int)seeds[i].color);
    }
    std::wstring seedLabel;
    if (config_.dynamicColor) {
        seedLabel = wallpaperOk_ ? tr(L"取色来源：当前壁纸 #", L"Color source: wallpaper #") + hex(wallpaperSeedRgb_, 6)
                                 : tr(L"未找到壁纸，使用品牌绿", L"No wallpaper found, using brand green");
    } else {
        seedLabel = tr(L"取色来源：自定义 #", L"Color source: custom #") + hex(config_.seedColor, 6);
    }
    drawText(g, seedLabel, RectF(appearance.X + 24, appearance.Y + 210, appearance.Width - 48, 20), font(11, false),
             scheme_.onSurfaceVariant);

    // ---- backup
    y = appearance.GetBottom() + 16;
    RectF backup(view.X, y, view.Width, 200);
    cardBg(g, backup, scheme_);
    drawText(g, tr(L"备份与恢复", L"Backup & restore"), RectF(backup.X + 22, backup.Y + 18, 300, 26), font(15, true),
             scheme_.onSurface);
    drawText(g, tr(L"修改前自动备份", L"Back up before changes"), RectF(backup.X + 22, backup.Y + 52, 300, 24),
             font(13, false), scheme_.onSurface);
    drawText(g, tr(L"每一项第一次被修改时记录原值", L"Original values are recorded on first change"),
             RectF(backup.X + 22, backup.Y + 78, 400, 18), font(11, false), scheme_.onSurfaceVariant);
    RectF bakSwitch(backup.GetRight() - 22 - 44, backup.Y + 50, 44, 24);
    drawSwitch(g, bakSwitch, config_.backup, hovered(A_BackupToggle), scheme_.primary, scheme_.surfaceVariant,
               scheme_.onPrimary, scheme_.outline, true);
    addHotspot(bakSwitch, A_BackupToggle);

    std::wstring backupInfo = lastBackupLabel_.empty() ? tr(L"还没有备份文件", L"No backup files yet")
                                                       : tr(L"最近备份  ", L"Latest  ") + lastBackupLabel_;
    drawText(g, backupInfo + L"   ·   " + std::to_wstring(backups_.snapshotCount()) + tr(L" 份", L" files"),
             RectF(backup.X + 22, backup.Y + 106, backup.Width - 44, 18), font(11, false), scheme_.onSurfaceVariant);
    if (!config_.backup) {
        drawText(g, tr(L"备份已关闭：修改不会记录，恢复按钮只能写回默认值", L"Backups off: restores fall back to default values"),
                 RectF(backup.X + 22, backup.Y + 124, backup.Width - 44, 18), font(11, false), scheme_.error);
    }
    RectF bs1(backup.X + 22, backup.Y + 146, 108, 38);
    RectF bs2(backup.X + 142, backup.Y + 146, 128, 38);
    RectF bs3(backup.X + 282, backup.Y + 146, 118, 38);
    drawButton(g, bs1, tr(L"立即备份", L"Back up now"), font(12, false), BtnStyle::Tonal, scheme_.secondaryContainer,
               scheme_.onSecondaryContainer, scheme_.secondaryContainer, hovered(A_BackupNow), false, true);
    drawButton(g, bs2, tr(L"恢复最近备份", L"Restore backup"), font(12, false), BtnStyle::Outlined,
               scheme_.surfaceContainerLow, scheme_.onSurface, scheme_.outline, hovered(A_RestoreSnapshot), false,
               backups_.snapshotCount() > 0);
    drawButton(g, bs3, tr(L"打开备份目录", L"Open folder"), font(12, false), BtnStyle::Text, scheme_.surfaceContainerLow,
               scheme_.primary, scheme_.primary, hovered(A_OpenBackupFolder), false, true);
    addHotspot(bs1, A_BackupNow);
    addHotspot(bs2, A_RestoreSnapshot);
    addHotspot(bs3, A_OpenBackupFolder);

    // ---- language
    y = backup.GetBottom() + 16;
    RectF lang(view.X, y, view.Width, 122);
    cardBg(g, lang, scheme_);
    drawText(g, tr(L"语言", L"Language"), RectF(lang.X + 22, lang.Y + 18, 200, 26), font(15, true), scheme_.onSurface);
    const wchar_t* langLabels[3] = {tr(L"跟随系统", L"System"), tr(L"简体中文", L"中文"), L"English"};
    float lw = 104;
    for (int i = 0; i < 3; ++i) {
        RectF chip(lang.X + 22 + i * (lw + 8), lang.Y + 56, lw, 34);
        bool selected = (int)config_.lang == i;
        drawChip(g, chip, langLabels[i], font(12, selected), selected, hovered(A_Lang, i), scheme_.secondaryContainer,
                 scheme_.onSecondaryContainer, scheme_.onSurfaceVariant, scheme_.outline, true);
        addHotspot(chip, A_Lang, i);
    }

    // ---- project
    y = lang.GetBottom() + 16;
    RectF project(view.X, y, view.Width, 110);
    cardBg(g, project, scheme_);
    drawText(g, tr(L"项目", L"Project"), RectF(project.X + 22, project.Y + 18, 200, 26), font(15, true),
             scheme_.onSurface);
    drawText(g, L"WinPrivacyGreen " + std::wstring(kVersion) + tr(L" · MIT 许可证 · 不联网", L" · MIT license · offline"),
             RectF(project.X + 22, project.Y + 48, project.Width - 44, 18), font(11, false), scheme_.onSurfaceVariant);
    RectF link(project.X + 22, project.Y + 70, project.Width - 44, 22);
    drawText(g, kProjectUrl, link, font(12, false), scheme_.primary);
    addHotspot(link, A_OpenRepo);

    g.Restore(clipState);

    float contentEnd = project.GetBottom() - scroll_ + 8;
    maxScroll_ = std::max(0.0f, contentEnd - view.GetBottom());
    if (scroll_ > maxScroll_) scroll_ = maxScroll_;
}

// ---------------------------------------------------------------------------
void App::drawAbout(Graphics& g, const RectF& view) {
    maxScroll_ = 0;
    float y = view.Y;

    RectF logo(view.X, y + 4, 64, 64);
    fillRound(g, logo, 20, scheme_.primaryContainer);
    drawIcon(g, IconKind::Shield, RectF(logo.X + 14, logo.Y + 14, 36, 36), scheme_.onPrimaryContainer, 2.0f);
    drawText(g, L"WinPrivacyGreen", RectF(view.X + 84, y + 4, view.Width - 84, 34), font(24, true), scheme_.onSurface);
    drawText(g, tr(L"版本 ", L"Version ") + std::wstring(kVersion) + tr(L"  ·  MIT 许可证  ·  开源", L"  ·  MIT license  ·  open source"),
             RectF(view.X + 84, y + 40, view.Width - 84, 20), font(12, false), scheme_.onSurfaceVariant);

    drawText(g,
             tr(L"一个只做一件事的小工具：把 Windows 里的广告、推荐、活动记录与诊断数据关掉。全部开关都可以单独调整，修改前自动记录原值，随时可以恢复。",
                L"A small tool that does one job: switching off Windows ads, suggestions, activity history and diagnostics. Every switch is optional, original values are recorded, and everything can be reverted."),
             RectF(view.X, y + 84, view.Width - 40, 48), font(13, false), scheme_.onSurfaceVariant, TextAlign::Left, true);

    RectF info(view.X, y + 146, view.Width, 158);
    cardBg(g, info, scheme_);
    const wchar_t* keys[5] = {tr(L"系统", L"System"), tr(L"主题", L"Theme"), tr(L"取色", L"Color"),
                              tr(L"备份", L"Backup"), tr(L"权限", L"Rights")};
    std::wstring themeValue = config_.theme == ThemeMode::Auto
                                  ? tr(L"跟随系统", L"Follow system")
                                  : (config_.theme == ThemeMode::Light ? tr(L"亮色", L"Light") : tr(L"暗色", L"Dark"));
    themeValue += dark_ ? tr(L"（当前暗色）", L" (dark now)") : tr(L"（当前亮色）", L" (light now)");
    std::wstring colorValue = config_.dynamicColor
                                  ? (wallpaperOk_ ? tr(L"壁纸 #", L"Wallpaper #") + hex(wallpaperSeedRgb_, 6)
                                                  : tr(L"品牌绿（未取到壁纸）", L"Brand green (no wallpaper)"))
                                  : tr(L"自定义 #", L"Custom #") + hex(config_.seedColor, 6);
    std::wstring backupValue = std::wstring(config_.backup ? tr(L"已开启", L"On") : tr(L"已关闭", L"Off")) + L" · " +
                               std::to_wstring(backups_.snapshotCount()) + tr(L" 份备份", L" files");
    std::wstring rightsValue = isAdmin() ? tr(L"管理员", L"Administrator") : tr(L"标准用户", L"Standard user");
    std::wstring values[5] = {windowsVersionName() + L"  (build " + std::to_wstring(windowsBuild()) + L")",
                              themeValue, colorValue, backupValue, rightsValue};
    for (int i = 0; i < 5; ++i) {
        float rowY = info.Y + 16 + i * 28;
        drawText(g, keys[i], RectF(info.X + 22, rowY, 120, 22), font(12, false), scheme_.onSurfaceVariant);
        drawText(g, values[i], RectF(info.X + 140, rowY, info.Width - 162, 22), font(12, false), scheme_.onSurface,
                 TextAlign::Right);
    }

    RectF linkCard(view.X, y + 320, view.Width, 76);
    cardBg(g, linkCard, scheme_);
    drawText(g, tr(L"项目地址", L"Project home"), RectF(linkCard.X + 22, linkCard.Y + 14, 200, 20), font(12, false),
             scheme_.onSurfaceVariant);
    drawText(g, kProjectUrl, RectF(linkCard.X + 22, linkCard.Y + 38, linkCard.Width - 44, 24), font(14, false),
             scheme_.primary);
    addHotspot(linkCard, A_OpenRepo);

    drawText(g,
             tr(L"本程序完全离线运行，不收集、不上传任何数据；只修改当前用户与系统策略下与本机隐私相关的注册表值。",
                L"This program runs fully offline and never collects or uploads data. It only writes the local registry values that control ads and privacy."),
             RectF(view.X, y + 410, view.Width - 40, 40), font(11, false), scheme_.onSurfaceVariant, TextAlign::Left,
             true);
}

// ---------------------------------------------------------------------------
void App::drawScrollbar(Graphics& g, const RectF& view) {
    if (maxScroll_ <= 0) return;
    float trackH = view.Height - 48;
    float thumbH = std::max(40.0f, trackH * (view.Height / (view.Height + maxScroll_)));
    float t = maxScroll_ > 0 ? scroll_ / maxScroll_ : 0;
    float y = view.Y + 48 + (trackH - thumbH) * t;
    RectF thumb(view.GetRight() - 2, y, 4, thumbH);
    fillRound(g, thumb, 2, argb(scheme_.primary, 90));
}

void App::drawToast(Graphics& g, float w, float h) {
    if (!toastVisible_) return;
    Font* f = font(12, false);
    float textWid = textWidth(g, toastText_, f);
    float bw = std::min(w - 120, textWid + 48);
    float bh = 46;
    float bx = (w - bw) / 2;
    float by = h - 118;
    uint32_t bg = toastKind_ == 1 ? scheme_.primaryContainer
                                  : (toastKind_ == 2 ? scheme_.errorContainer : scheme_.inverseSurface);
    uint32_t fg = toastKind_ == 1 ? scheme_.onPrimaryContainer
                                  : (toastKind_ == 2 ? scheme_.onErrorContainer : scheme_.inverseOnSurface);
    fillRound(g, RectF(bx, by, bw, bh), 14, bg);
    drawText(g, toastText_, RectF(bx + 20, by, bw - 40, bh), f, fg, TextAlign::Center);
}

void App::drawConfirm(Graphics& g, float w, float h) {
    if (!confirmVisible_) return;
    SolidBrush scrim(colA(0x000000, 140));
    g.FillRectangle(&scrim, RectF(0, 0, w, h));

    float cw = std::min(460.0f, w - 80);
    float chh = 216;
    RectF card((w - cw) / 2, (h - chh) / 2, cw, chh);
    fillRound(g, card, 26, scheme_.surfaceContainerHigh);
    drawText(g, confirmTitle_, RectF(card.X + 26, card.Y + 24, cw - 52, 30), font(17, true), scheme_.onSurface);
    drawText(g, confirmBody_, RectF(card.X + 26, card.Y + 66, cw - 52, 64), font(12, false), scheme_.onSurfaceVariant,
             TextAlign::Left, true);

    RectF ok(card.GetRight() - 26 - 118, card.GetBottom() - 26 - 42, 118, 42);
    RectF cancel(ok.X - 8 - 100, ok.Y, 100, 42);
    drawButton(g, cancel, tr(L"取消", L"Cancel"), font(13, false), BtnStyle::Text, scheme_.surfaceContainerHigh,
               scheme_.primary, scheme_.primary, hovered(A_ConfirmCancel), false, true);
    drawButton(g, ok, confirmOk_, font(13, true), BtnStyle::Filled, scheme_.primary, scheme_.onPrimary,
               scheme_.primary, hovered(A_ConfirmOk), false, true);
    addHotspot(cancel, A_ConfirmCancel);
    addHotspot(ok, A_ConfirmOk);
}

}  // namespace wpg
