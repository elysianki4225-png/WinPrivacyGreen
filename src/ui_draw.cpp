#include "draw.h"

#include <algorithm>
#include <cmath>

#include "ui.h"

using namespace Gdiplus;

namespace wpg {

void addRoundRect(GraphicsPath& path, const RectF& r, float radius) {
    float rad = std::min(radius, std::min(r.Width, r.Height) * 0.5f);
    if (rad <= 0.01f) {
        path.AddRectangle(r);
        return;
    }
    float d = rad * 2;
    path.AddArc(r.X, r.Y, d, d, 180, 90);
    path.AddArc(r.GetRight() - d, r.Y, d, d, 270, 90);
    path.AddArc(r.GetRight() - d, r.GetBottom() - d, d, d, 0, 90);
    path.AddArc(r.X, r.GetBottom() - d, d, d, 90, 90);
    path.CloseFigure();
}

void fillRound(Graphics& g, const RectF& r, float radius, uint32_t color) {
    if (r.Width <= 0 || r.Height <= 0) return;
    GraphicsPath path;
    addRoundRect(path, r, radius);
    SolidBrush brush(col(color));
    g.FillPath(&brush, &path);
}

void strokeRound(Graphics& g, const RectF& r, float radius, uint32_t color, float width) {
    if (r.Width <= 0 || r.Height <= 0) return;
    GraphicsPath path;
    RectF inset(r.X + width * 0.5f, r.Y + width * 0.5f, r.Width - width, r.Height - width);
    addRoundRect(path, inset, std::max(0.0f, radius - width * 0.5f));
    Pen pen(col(color), width);
    g.DrawPath(&pen, &path);
}

void drawText(Graphics& g, const std::wstring& text, const RectF& r, Font* font, uint32_t color, TextAlign align,
              bool wrap, bool ellipsis) {
    if (!font || text.empty() || r.Width <= 0) return;
    StringFormat fmt;
    fmt.SetAlignment(align == TextAlign::Left ? StringAlignmentNear
                                              : (align == TextAlign::Center ? StringAlignmentCenter : StringAlignmentFar));
    fmt.SetLineAlignment(StringAlignmentCenter);
    fmt.SetTrimming(ellipsis ? StringTrimmingEllipsisCharacter : StringTrimmingNone);
    fmt.SetFormatFlags(wrap ? 0 : StringFormatFlagsNoWrap);
    if (wrap) fmt.SetFormatFlags(0);
    SolidBrush brush(col(color));
    g.DrawString(text.c_str(), (INT)text.size(), font, r, &fmt, &brush);
}

float textWidth(Graphics& g, const std::wstring& text, Font* font) {
    if (!font || text.empty()) return 0;
    RectF box;
    StringFormat fmt;
    fmt.SetFormatFlags(StringFormatFlagsNoWrap | StringFormatFlagsMeasureTrailingSpaces);
    g.MeasureString(text.c_str(), (INT)text.size(), font, PointF(0, 0), &fmt, &box);
    return box.Width;
}

// ---------------------------------------------------------------------------
static void iconBody(Graphics& g, IconKind kind, const Pen& pen, const SolidBrush& brush) {
    switch (kind) {
        case IconKind::Megaphone: {
            PointF cone[4] = {PointF(3.5f, 9.5f), PointF(9.5f, 6.0f), PointF(9.5f, 18.0f), PointF(3.5f, 14.5f)};
            GraphicsPath p;
            p.AddPolygon(cone, 4);
            g.DrawPath(&pen, &p);
            g.DrawLine(&pen, 9.5f, 12.0f, 13.0f, 12.0f);
            g.DrawArc(&pen, 11.0f, 7.0f, 9.0f, 10.0f, -55.0f, 110.0f);
            g.DrawLine(&pen, 5.5f, 14.5f, 6.5f, 19.0f);
            break;
        }
        case IconKind::IdCard: {
            GraphicsPath p;
            addRoundRect(p, RectF(3.0f, 5.5f, 18.0f, 13.0f), 2.5f);
            g.DrawPath(&pen, &p);
            g.DrawEllipse(&pen, 6.0f, 9.5f, 4.0f, 4.0f);
            g.DrawLine(&pen, 12.5f, 10.5f, 18.0f, 10.5f);
            g.DrawLine(&pen, 12.5f, 14.0f, 16.5f, 14.0f);
            break;
        }
        case IconKind::Sparkle: {
            PointF star[8] = {PointF(12.0f, 3.0f), PointF(14.0f, 9.8f), PointF(21.0f, 12.0f),
                              PointF(14.0f, 14.2f), PointF(12.0f, 21.0f), PointF(10.0f, 14.2f),
                              PointF(3.0f, 12.0f), PointF(10.0f, 9.8f)};
            GraphicsPath p;
            p.AddPolygon(star, 8);
            g.DrawPath(&pen, &p);
            break;
        }
        case IconKind::Grid: {
            GraphicsPath a, b, c, d;
            addRoundRect(a, RectF(3.5f, 3.5f, 7.0f, 7.0f), 1.8f);
            addRoundRect(b, RectF(13.5f, 3.5f, 7.0f, 7.0f), 1.8f);
            addRoundRect(c, RectF(3.5f, 13.5f, 7.0f, 7.0f), 1.8f);
            addRoundRect(d, RectF(13.5f, 13.5f, 7.0f, 7.0f), 1.8f);
            g.DrawPath(&pen, &a);
            g.DrawPath(&pen, &b);
            g.DrawPath(&pen, &c);
            g.DrawPath(&pen, &d);
            break;
        }
        case IconKind::Search: {
            g.DrawEllipse(&pen, 3.5f, 3.5f, 12.5f, 12.5f);
            g.DrawLine(&pen, 15.5f, 15.5f, 20.5f, 20.5f);
            break;
        }
        case IconKind::Lock: {
            GraphicsPath p;
            addRoundRect(p, RectF(4.5f, 10.0f, 15.0f, 10.5f), 2.5f);
            g.DrawPath(&pen, &p);
            g.DrawArc(&pen, 7.5f, 3.8f, 9.0f, 11.0f, 180.0f, 180.0f);
            g.FillEllipse(&brush, RectF(10.8f, 13.6f, 2.4f, 2.4f));
            break;
        }
        case IconKind::History: {
            g.DrawEllipse(&pen, 3.5f, 3.5f, 17.0f, 17.0f);
            g.DrawLine(&pen, 12.0f, 7.0f, 12.0f, 12.3f);
            g.DrawLine(&pen, 12.0f, 12.3f, 16.0f, 14.2f);
            break;
        }
        case IconKind::Doc: {
            GraphicsPath p;
            p.AddLine(6.0f, 3.0f, 13.5f, 3.0f);
            p.AddLine(13.5f, 3.0f, 18.0f, 7.5f);
            p.AddLine(18.0f, 7.5f, 18.0f, 21.0f);
            p.AddLine(18.0f, 21.0f, 6.0f, 21.0f);
            p.CloseFigure();
            g.DrawPath(&pen, &p);
            g.DrawLine(&pen, 13.5f, 3.0f, 13.5f, 7.5f);
            g.DrawLine(&pen, 13.5f, 7.5f, 18.0f, 7.5f);
            g.DrawLine(&pen, 9.0f, 12.5f, 15.0f, 12.5f);
            g.DrawLine(&pen, 9.0f, 16.0f, 15.0f, 16.0f);
            break;
        }
        case IconKind::Keyboard: {
            GraphicsPath p;
            addRoundRect(p, RectF(2.5f, 6.5f, 19.0f, 11.5f), 2.5f);
            g.DrawPath(&pen, &p);
            for (int i = 0; i < 4; ++i) g.FillEllipse(&brush, RectF(5.6f + i * 3.4f, 9.6f, 1.5f, 1.5f));
            for (int i = 0; i < 3; ++i) g.FillEllipse(&brush, RectF(5.6f + i * 3.4f, 13.2f, 1.5f, 1.5f));
            g.DrawLine(&pen, 16.0f, 13.9f, 18.2f, 13.9f);
            break;
        }
        case IconKind::Location: {
            GraphicsPath p;
            p.StartFigure();
            p.AddLine(12.0f, 21.0f, 5.6f, 13.4f);
            p.AddBezier(5.6f, 13.4f, 4.4f, 11.9f, 4.2f, 10.6f, 4.2f, 9.4f);
            p.AddBezier(4.2f, 9.4f, 4.2f, 5.1f, 7.7f, 2.5f, 12.0f, 2.5f);
            p.AddBezier(12.0f, 2.5f, 16.3f, 2.5f, 19.8f, 5.1f, 19.8f, 9.4f);
            p.AddBezier(19.8f, 9.4f, 19.8f, 10.6f, 19.6f, 11.9f, 18.4f, 13.4f);
            p.CloseFigure();
            g.DrawPath(&pen, &p);
            g.DrawEllipse(&pen, 9.2f, 6.7f, 5.6f, 5.6f);
            break;
        }
        case IconKind::Chart: {
            g.DrawLine(&pen, 3.5f, 20.0f, 20.5f, 20.0f);
            g.DrawLine(&pen, 3.5f, 20.0f, 3.5f, 4.0f);
            g.FillRectangle(&brush, RectF(7.0f, 13.0f, 3.0f, 7.0f));
            g.FillRectangle(&brush, RectF(12.0f, 8.5f, 3.0f, 11.5f));
            g.FillRectangle(&brush, RectF(17.0f, 11.0f, 3.0f, 9.0f));
            break;
        }
        case IconKind::Alert: {
            GraphicsPath p;
            p.StartFigure();
            p.AddLine(12.0f, 3.5f, 21.0f, 20.0f);
            p.AddLine(21.0f, 20.0f, 3.0f, 20.0f);
            p.CloseFigure();
            g.DrawPath(&pen, &p);
            g.DrawLine(&pen, 12.0f, 9.5f, 12.0f, 14.5f);
            g.FillEllipse(&brush, RectF(11.2f, 16.4f, 1.7f, 1.7f));
            break;
        }
        case IconKind::Shield: {
            GraphicsPath p;
            p.StartFigure();
            p.AddLine(12.0f, 2.8f, 19.6f, 5.8f);
            p.AddLine(19.6f, 5.8f, 19.6f, 11.4f);
            p.AddBezier(19.6f, 11.4f, 19.6f, 16.2f, 16.4f, 19.6f, 12.0f, 21.2f);
            p.AddBezier(12.0f, 21.2f, 7.6f, 19.6f, 4.4f, 16.2f, 4.4f, 11.4f);
            p.AddLine(4.4f, 11.4f, 4.4f, 5.8f);
            p.CloseFigure();
            g.DrawPath(&pen, &p);
            g.DrawLine(&pen, 8.6f, 11.6f, 11.0f, 14.2f);
            g.DrawLine(&pen, 11.0f, 14.2f, 15.6f, 8.8f);
            break;
        }
        case IconKind::Speech: {
            GraphicsPath p;
            addRoundRect(p, RectF(2.8f, 3.8f, 18.4f, 12.8f), 3.2f);
            p.AddLine(8.4f, 16.4f, 8.4f, 21.0f);
            p.AddLine(8.4f, 21.0f, 12.8f, 16.4f);
            g.DrawPath(&pen, &p);
            for (int i = 0; i < 3; ++i) g.FillEllipse(&brush, RectF(7.4f + i * 4.3f, 9.6f, 1.7f, 1.7f));
            break;
        }
        case IconKind::Gear: {
            g.DrawEllipse(&pen, 8.4f, 8.4f, 7.2f, 7.2f);
            for (int i = 0; i < 8; ++i) {
                float ang = (float)i * 3.14159265f / 4.0f;
                float c = std::cos(ang), s = std::sin(ang);
                g.DrawLine(&pen, 12.0f + c * 7.0f, 12.0f + s * 7.0f, 12.0f + c * 9.6f, 12.0f + s * 9.6f);
            }
            break;
        }
        case IconKind::Cloud: {
            GraphicsPath p;
            p.AddBezier(6.8f, 18.5f, 3.8f, 18.5f, 2.5f, 16.2f, 2.8f, 14.0f);
            p.AddBezier(2.8f, 14.0f, 3.1f, 11.8f, 5.0f, 10.7f, 6.9f, 11.0f);
            p.AddBezier(6.9f, 11.0f, 7.6f, 6.5f, 12.4f, 4.6f, 15.4f, 7.8f);
            p.AddBezier(15.4f, 7.8f, 15.9f, 7.6f, 16.5f, 7.6f, 17.0f, 7.7f);
            p.AddBezier(17.0f, 7.7f, 21.8f, 8.6f, 21.8f, 13.2f, 21.0f, 14.8f);
            p.AddBezier(21.0f, 14.8f, 20.6f, 17.4f, 18.4f, 18.5f, 16.6f, 18.5f);
            p.AddLine(16.6f, 18.5f, 6.8f, 18.5f);
            g.DrawPath(&pen, &p);
            g.DrawLine(&pen, 12.0f, 10.0f, 12.0f, 16.2f);
            g.DrawLine(&pen, 12.0f, 16.2f, 9.6f, 13.8f);
            g.DrawLine(&pen, 12.0f, 16.2f, 14.4f, 13.8f);
            break;
        }
    }
}

void drawIcon(Graphics& g, IconKind kind, const RectF& box, uint32_t color, float stroke) {
    if (box.Width <= 0 || box.Height <= 0) return;
    GraphicsState state = g.Save();
    g.TranslateTransform(box.X, box.Y);
    g.ScaleTransform(box.Width / 24.0f, box.Height / 24.0f);
    Pen pen(col(color), stroke);
    pen.SetLineCap(LineCapRound, LineCapRound, DashCapRound);
    pen.SetLineJoin(LineJoinRound);
    SolidBrush brush(col(color));
    iconBody(g, kind, pen, brush);
    g.Restore(state);
}

void drawIconBadge(Graphics& g, const RectF& r, IconKind kind, uint32_t bg, uint32_t fg) {
    fillRound(g, r, r.Width * 0.32f, bg);
    float pad = r.Width * 0.27f;
    drawIcon(g, kind, RectF(r.X + pad, r.Y + pad, r.Width - pad * 2, r.Height - pad * 2), fg, 2.0f);
}

void drawButton(Graphics& g, const RectF& r, const std::wstring& label, Font* font, BtnStyle style, uint32_t bg,
                uint32_t fg, uint32_t border, bool hover, bool pressed, bool enabled) {
    uint32_t base = bg;
    if (!enabled) {
        base = blendArgb(bg, 0x888888, 0.55f);
    } else if (pressed) {
        base = blendArgb(bg, fg, 0.16f);
    } else if (hover) {
        base = blendArgb(bg, fg, 0.10f);
    }
    float radius = 20.0f;
    if (style == BtnStyle::Outlined) {
        uint32_t line = enabled ? border : blendArgb(border, 0x9A9A9A, 0.4f);
        strokeRound(g, r, radius, line, 1.0f);
        if (hover) fillRound(g, r, radius, blendArgb(0xFFFFFF & bg, fg, 0.07f));
    } else if (style == BtnStyle::Text) {
        if (hover) fillRound(g, r, radius, argb(fg & 0xFFFFFF, 20));
    } else {
        fillRound(g, r, radius, base);
    }
    uint32_t textColor = enabled ? fg : blendArgb(fg, 0x9A9A9A, 0.55f);
    drawText(g, label, r, font, textColor, TextAlign::Center);
}

void drawSwitch(Graphics& g, const RectF& r, bool on, bool hover, uint32_t trackOn, uint32_t trackOff,
                uint32_t thumbOn, uint32_t thumbOff, bool enabled) {
    uint32_t track = on ? trackOn : trackOff;
    if (!enabled) track = blendArgb(track, 0x9A9A9A, 0.5f);
    if (hover && enabled) track = blendArgb(track, on ? thumbOn : 0x808080, 0.08f);
    fillRound(g, r, r.Height * 0.5f, track);
    float pad = 3.0f;
    float thumb = r.Height - pad * 2;
    float x = on ? r.GetRight() - pad - thumb : r.X + pad;
    uint32_t thumbColor = enabled ? (on ? thumbOn : thumbOff) : blendArgb(thumbOff, 0x9A9A9A, 0.4f);
    SolidBrush brush(col(thumbColor));
    g.FillEllipse(&brush, RectF(x, r.Y + pad, thumb, thumb));
}

void drawChip(Graphics& g, const RectF& r, const std::wstring& label, Font* font, bool selected, bool hover,
              uint32_t bgSelected, uint32_t fgSelected, uint32_t fgIdle, uint32_t outline, bool enabled) {
    if (selected) {
        uint32_t bg = hover && enabled ? blendArgb(bgSelected, fgSelected, 0.08f) : bgSelected;
        fillRound(g, r, r.Height * 0.5f, bg);
        drawText(g, label, r, font, fgSelected, TextAlign::Center);
    } else {
        strokeRound(g, r, r.Height * 0.5f, hover && enabled ? fgIdle : outline, 1.0f);
        drawText(g, label, r, font, enabled ? fgIdle : blendArgb(fgIdle, 0x9A9A9A, 0.5f), TextAlign::Center);
    }
}

}  // namespace wpg
