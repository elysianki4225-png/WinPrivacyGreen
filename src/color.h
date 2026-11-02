// WinPrivacyGreen - Material Design 3 color engine
// Pure C++ (no Windows dependency) so it can be unit tested on any platform.
#pragma once
#include <cstdint>
#include <cmath>
#include <algorithm>

namespace wpg {

struct Oklch {
    float L = 0.5f;  // 0..1 perceptual lightness
    float C = 0.0f;  // chroma
    float h = 0.0f;  // hue degrees 0..360
};

struct Rgb {
    float r = 0, g = 0, b = 0;
};

// helpers ------------------------------------------------------------------
inline uint32_t argb(uint32_t rgb, unsigned a = 255) {
    return ((uint32_t)a << 24) | (rgb & 0xFFFFFFu);
}
inline uint32_t makeRgb(int r, int g, int b) {
    return ((uint32_t)(r & 255) << 16) | ((uint32_t)(g & 255) << 8) | (uint32_t)(b & 255);
}
inline int chanR(uint32_t c) { return (int)((c >> 16) & 255); }
inline int chanG(uint32_t c) { return (int)((c >> 8) & 255); }
inline int chanB(uint32_t c) { return (int)(c & 255); }
inline int chanA(uint32_t c) { return (int)((c >> 24) & 255); }

// Perceptual (OKLab) conversions -------------------------------------------
Oklch oklchFromSrgb(float r, float g, float b);
Oklch oklchFromArgb(uint32_t c);
// Maps L/C/h to sRGB, reducing chroma until the color fits the sRGB gamut.
uint32_t argbFromOklch(float L, float C, float h, unsigned alpha = 255);
bool oklchToSrgb(float L, float C, float h, Rgb& out);

// A Material 3 style tonal palette: constant hue/chroma, tone = CIE L* 0..100.
struct TonalPalette {
    float chroma = 0.0f;
    float hue = 0.0f;
    uint32_t tone(float t) const;
};

struct Scheme {
    uint32_t primary, onPrimary, primaryContainer, onPrimaryContainer;
    uint32_t secondary, onSecondary, secondaryContainer, onSecondaryContainer;
    uint32_t tertiary, onTertiary, tertiaryContainer, onTertiaryContainer;
    uint32_t error, onError, errorContainer, onErrorContainer;
    uint32_t surface, onSurface, surfaceVariant, onSurfaceVariant;
    uint32_t surfaceContainerLowest, surfaceContainerLow, surfaceContainer;
    uint32_t surfaceContainerHigh, surfaceContainerHighest;
    uint32_t surfaceDim, surfaceBright;
    uint32_t outline, outlineVariant;
    uint32_t inverseSurface, inverseOnSurface, inversePrimary;
    uint32_t scrim, shadow;
    bool dark;
};

// Builds a full Material 3 role scheme from a seed color.
Scheme buildScheme(const Oklch& seed, bool dark);

// The application's own "green" brand seed, used when wallpaper sampling is
// unavailable or disabled.
inline Oklch brandSeed() { return oklchFromArgb(0x2E7D32); }

// Clamp a sampled color into a range that produces good, readable palettes.
Oklch normalizeSeed(const Oklch& in);

// Blend helper for hover/state overlays.
uint32_t blendArgb(uint32_t base, uint32_t over, float t);

}  // namespace wpg
