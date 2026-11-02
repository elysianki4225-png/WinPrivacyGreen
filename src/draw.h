// Drawing helpers shared by the view code.
#pragma once
#include <windows.h>
#include <gdiplus.h>

#include <string>

#include "color.h"
#include "items.h"

namespace wpg {

enum class TextAlign { Left, Center, Right };
enum class BtnStyle { Filled, Tonal, Outlined, Text };

inline Gdiplus::Color col(uint32_t v) { return Gdiplus::Color((Gdiplus::ARGB)v); }
inline Gdiplus::Color colA(uint32_t rgb, int alpha) {
    return Gdiplus::Color((Gdiplus::ARGB)(((uint32_t)alpha << 24) | (rgb & 0xFFFFFF)));
}

void addRoundRect(Gdiplus::GraphicsPath& path, const Gdiplus::RectF& r, float radius);
void fillRound(Gdiplus::Graphics& g, const Gdiplus::RectF& r, float radius, uint32_t color);
void strokeRound(Gdiplus::Graphics& g, const Gdiplus::RectF& r, float radius, uint32_t color, float width);
void drawText(Gdiplus::Graphics& g, const std::wstring& text, const Gdiplus::RectF& r, Gdiplus::Font* font,
              uint32_t color, TextAlign align = TextAlign::Left, bool wrap = false, bool ellipsis = true);
float textWidth(Gdiplus::Graphics& g, const std::wstring& text, Gdiplus::Font* font);

void drawIcon(Gdiplus::Graphics& g, IconKind kind, const Gdiplus::RectF& box, uint32_t color,
              float stroke = 1.9f);

void drawButton(Gdiplus::Graphics& g, const Gdiplus::RectF& r, const std::wstring& label, Gdiplus::Font* font,
                BtnStyle style, uint32_t bg, uint32_t fg, uint32_t border, bool hover, bool pressed,
                bool enabled);
void drawSwitch(Gdiplus::Graphics& g, const Gdiplus::RectF& r, bool on, bool hover, uint32_t trackOn,
                uint32_t trackOff, uint32_t thumbOn, uint32_t thumbOff, bool enabled);
void drawChip(Gdiplus::Graphics& g, const Gdiplus::RectF& r, const std::wstring& label, Gdiplus::Font* font,
              bool selected, bool hover, uint32_t bgSelected, uint32_t fgSelected, uint32_t fgIdle,
              uint32_t outline, bool enabled);
void drawIconBadge(Gdiplus::Graphics& g, const Gdiplus::RectF& r, IconKind kind, uint32_t bg, uint32_t fg);

}  // namespace wpg
