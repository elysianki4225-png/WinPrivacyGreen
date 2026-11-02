#!/usr/bin/env python3
"""Draws the WinPrivacyGreen application icon.

The mark is a shield with a check on the brand green gradient, kept simple so
it still reads at 16 px in the taskbar. Run: python3 tools/make_icon.py
"""
import math
import os

from PIL import Image, ImageDraw, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ASSETS = os.path.join(ROOT, "assets")
S = 1024
SS = 2  # supersample, then downscale for clean edges


def rounded_mask(size, radius):
    mask = Image.new("L", (size, size), 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, size - 1, size - 1], radius=radius, fill=255)
    return mask


def gradient(size, top, bottom):
    img = Image.new("RGB", (1, size))
    for y in range(size):
        t = y / max(1, size - 1)
        img.putpixel((0, y), tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3)))
    return img.resize((size, size), Image.BILINEAR)


def bezier(p0, p1, p2, p3, steps=140):
    pts = []
    for i in range(steps + 1):
        t = i / steps
        u = 1 - t
        x = u * u * u * p0[0] + 3 * u * u * t * p1[0] + 3 * u * t * t * p2[0] + t * t * t * p3[0]
        y = u * u * u * p0[1] + 3 * u * u * t * p1[1] + 3 * u * t * t * p2[1] + t * t * t * p3[1]
        pts.append((x, y))
    return pts


def stroke_path(draw, points, width, color):
    """Strokes a polyline with round caps and joins by stamping discs."""
    r = width / 2.0
    step = max(1.0, r / 3.0)
    for a, b in zip(points, points[1:]):
        dist = math.hypot(b[0] - a[0], b[1] - a[1])
        n = max(1, int(dist / step))
        for i in range(n + 1):
            t = i / n
            x = a[0] + (b[0] - a[0]) * t
            y = a[1] + (b[1] - a[1]) * t
            draw.ellipse([x - r, y - r, x + r, y + r], fill=color)
    for p in (points[0], points[-1]):
        draw.ellipse([p[0] - r, p[1] - r, p[0] + r, p[1] + r], fill=color)


def shield_outline(cx, top, bottom, half_w):
    """Classic shield: flat top, pointed bottom, straight edges."""
    left = cx - half_w
    right = cx + half_w
    shoulder = top + (bottom - top) * 0.46
    return [
        (left, top),
        (right, top),
        (right, shoulder),
        (cx, bottom),
        (left, shoulder),
        (left, top),
    ]


def build():
    os.makedirs(ASSETS, exist_ok=True)
    size = S * SS
    base = gradient(size, (76, 175, 80), (17, 82, 24)).convert("RGBA")
    base.putalpha(rounded_mask(size, int(size * 0.225)))

    layer = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    white = (255, 255, 255, 255)
    u = size / 1024.0  # design units -> pixels

    stroke_path(d, shield_outline(size / 2, 214 * u, 806 * u, 214 * u), 56 * u, white)
    stroke_path(d, [(404 * u, 486 * u), (476 * u, 558 * u), (612 * u, 410 * u)], 52 * u, white)

    layer = layer.filter(ImageFilter.GaussianBlur(0.7 * SS))
    img = Image.alpha_composite(base, layer).resize((S, S), Image.LANCZOS)

    png_path = os.path.join(ASSETS, "icon.png")
    img.save(png_path)
    ico_path = os.path.join(ASSETS, "icon.ico")
    img.save(ico_path, sizes=[(16, 16), (20, 20), (24, 24), (32, 32), (40, 40), (48, 48), (64, 64),
                              (128, 128), (256, 256)])
    print("wrote", png_path)
    print("wrote", ico_path)


if __name__ == "__main__":
    build()
