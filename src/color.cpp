#include "color.h"

namespace wpg {

static float srgbToLinear(float c) {
    return (c <= 0.04045f) ? (c / 12.92f) : std::pow((c + 0.055f) / 1.055f, 2.4f);
}
static float linearToSrgb(float c) {
    return (c <= 0.0031308f) ? (c * 12.92f) : (1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f);
}

Oklch oklchFromSrgb(float r, float g, float b) {
    float lr = srgbToLinear(r), lg = srgbToLinear(g), lb = srgbToLinear(b);
    float l = 0.4122214708f * lr + 0.5363325363f * lg + 0.0514459929f * lb;
    float m = 0.2119034982f * lr + 0.6806995451f * lg + 0.1073969566f * lb;
    float s = 0.0883024619f * lr + 0.2817188376f * lg + 0.6299787005f * lb;
    l = std::cbrt(l);
    m = std::cbrt(m);
    s = std::cbrt(s);
    float L = 0.2104542553f * l + 0.7936177850f * m - 0.0040720468f * s;
    float A = 1.9779984951f * l - 2.4285922050f * m + 0.4505937099f * s;
    float B = 0.0259040371f * l + 0.7827717662f * m - 0.8086757660f * s;
    Oklch o;
    o.L = L;
    o.C = std::sqrt(A * A + B * B);
    float h = std::atan2(B, A) * 180.0f / 3.14159265358979f;
    if (h < 0) h += 360.0f;
    o.h = h;
    return o;
}

Oklch oklchFromArgb(uint32_t c) {
    return oklchFromSrgb(chanR(c) / 255.0f, chanG(c) / 255.0f, chanB(c) / 255.0f);
}

bool oklchToSrgb(float L, float C, float h, Rgb& out) {
    float rad = h * 3.14159265358979f / 180.0f;
    float A = C * std::cos(rad);
    float B = C * std::sin(rad);
    float l_ = L + 0.3963377774f * A + 0.2158037573f * B;
    float m_ = L - 0.1055613458f * A - 0.0638541728f * B;
    float s_ = L - 0.0894841775f * A - 1.2914855480f * B;
    float l = l_ * l_ * l_, m = m_ * m_ * m_, s = s_ * s_ * s_;
    float r = 4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s;
    float g = -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s;
    float b = -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s;
    const float eps = -0.0005f;  // tiny tolerance keeps rounding stable
    bool inside = r >= eps && r <= 1.0005f && g >= eps && g <= 1.0005f && b >= eps && b <= 1.0005f;
    out.r = linearToSrgb(std::min(1.0f, std::max(0.0f, r)));
    out.g = linearToSrgb(std::min(1.0f, std::max(0.0f, g)));
    out.b = linearToSrgb(std::min(1.0f, std::max(0.0f, b)));
    return inside;
}

static int to255(float v) {
    int i = (int)std::lround(v * 255.0f);
    if (i < 0) i = 0;
    if (i > 255) i = 255;
    return i;
}

uint32_t argbFromOklch(float L, float C, float h, unsigned alpha) {
    Rgb rgb;
    if (!oklchToSrgb(L, C, h, rgb)) {
        float lo = 0.0f, hi = C;
        for (int i = 0; i < 24; ++i) {
            float mid = (lo + hi) * 0.5f;
            Rgb t;
            if (oklchToSrgb(L, mid, h, t)) {
                lo = mid;
                rgb = t;
            } else {
                hi = mid;
            }
        }
    }
    return argb(makeRgb(to255(rgb.r), to255(rgb.g), to255(rgb.b)), alpha);
}

static float toneToLightness(float tone) {
    // CIE L* -> relative luminance -> OKLab L (cube root of Y for neutrals).
    float t = std::min(100.0f, std::max(0.0f, tone));
    float Y = (t > 8.0f) ? std::pow((t + 16.0f) / 116.0f, 3.0f) : (t / 903.3f);
    return std::cbrt(std::max(0.0f, Y));
}

uint32_t TonalPalette::tone(float t) const {
    return argbFromOklch(toneToLightness(t), chroma, hue);
}

Oklch normalizeSeed(const Oklch& in) {
    Oklch s = in;
    if (s.L < 0.30f) s.L = 0.30f;
    if (s.L > 0.80f) s.L = 0.80f;
    if (s.C < 0.075f) s.C = 0.075f;
    if (s.C > 0.155f) s.C = 0.155f;
    if (s.h < 0.0f || s.h >= 360.0f) s.h = 0.0f;
    return s;
}

Scheme buildScheme(const Oklch& seedIn, bool dark) {
    Oklch seed = normalizeSeed(seedIn);
    TonalPalette primary{seed.C, seed.h};
    TonalPalette secondary{std::min(0.055f, std::max(0.030f, seed.C * 0.42f)), seed.h};
    float th = seed.h + 60.0f;
    if (th >= 360.0f) th -= 360.0f;
    TonalPalette tertiary{std::min(0.110f, std::max(0.070f, seed.C * 0.85f)), th};
    TonalPalette neutral{0.008f, seed.h};
    TonalPalette neutralVariant{0.022f, seed.h};
    TonalPalette error{0.145f, 27.0f};

    Scheme s{};
    s.dark = dark;
    if (!dark) {
        s.primary = primary.tone(40);
        s.onPrimary = primary.tone(100);
        s.primaryContainer = primary.tone(90);
        s.onPrimaryContainer = primary.tone(10);
        s.secondary = secondary.tone(40);
        s.onSecondary = secondary.tone(100);
        s.secondaryContainer = secondary.tone(90);
        s.onSecondaryContainer = secondary.tone(10);
        s.tertiary = tertiary.tone(40);
        s.onTertiary = tertiary.tone(100);
        s.tertiaryContainer = tertiary.tone(90);
        s.onTertiaryContainer = tertiary.tone(10);
        s.error = error.tone(40);
        s.onError = error.tone(100);
        s.errorContainer = error.tone(90);
        s.onErrorContainer = error.tone(10);
        s.surface = neutral.tone(98);
        s.onSurface = neutral.tone(10);
        s.surfaceVariant = neutralVariant.tone(90);
        s.onSurfaceVariant = neutralVariant.tone(30);
        s.surfaceContainerLowest = neutral.tone(100);
        s.surfaceContainerLow = neutral.tone(96);
        s.surfaceContainer = neutral.tone(94);
        s.surfaceContainerHigh = neutral.tone(92);
        s.surfaceContainerHighest = neutral.tone(90);
        s.surfaceDim = neutral.tone(87);
        s.surfaceBright = neutral.tone(98);
        s.outline = neutralVariant.tone(50);
        s.outlineVariant = neutralVariant.tone(80);
        s.inverseSurface = neutral.tone(20);
        s.inverseOnSurface = neutral.tone(95);
        s.inversePrimary = primary.tone(80);
        s.scrim = 0x00000000;
        s.shadow = 0xFF000000;
    } else {
        s.primary = primary.tone(80);
        s.onPrimary = primary.tone(20);
        s.primaryContainer = primary.tone(30);
        s.onPrimaryContainer = primary.tone(90);
        s.secondary = secondary.tone(80);
        s.onSecondary = secondary.tone(20);
        s.secondaryContainer = secondary.tone(30);
        s.onSecondaryContainer = secondary.tone(90);
        s.tertiary = tertiary.tone(80);
        s.onTertiary = tertiary.tone(20);
        s.tertiaryContainer = tertiary.tone(30);
        s.onTertiaryContainer = tertiary.tone(90);
        s.error = error.tone(80);
        s.onError = error.tone(20);
        s.errorContainer = error.tone(30);
        s.onErrorContainer = error.tone(90);
        s.surface = neutral.tone(6);
        s.onSurface = neutral.tone(90);
        s.surfaceVariant = neutralVariant.tone(30);
        s.onSurfaceVariant = neutralVariant.tone(80);
        s.surfaceContainerLowest = neutral.tone(4);
        s.surfaceContainerLow = neutral.tone(10);
        s.surfaceContainer = neutral.tone(12);
        s.surfaceContainerHigh = neutral.tone(17);
        s.surfaceContainerHighest = neutral.tone(22);
        s.surfaceDim = neutral.tone(6);
        s.surfaceBright = neutral.tone(24);
        s.outline = neutralVariant.tone(60);
        s.outlineVariant = neutralVariant.tone(30);
        s.inverseSurface = neutral.tone(90);
        s.inverseOnSurface = neutral.tone(20);
        s.inversePrimary = primary.tone(40);
        s.scrim = 0x00000000;
        s.shadow = 0xFF000000;
    }
    return s;
}

uint32_t blendArgb(uint32_t base, uint32_t over, float t) {
    if (t <= 0.0f) return base;
    if (t >= 1.0f) return over;
    float a = t * chanA(over) / 255.0f;
    auto mix = [&](int b, int o) {
        float v = b * (1.0f - a) + o * a;
        int i = (int)std::lround(v);
        return i < 0 ? 0 : (i > 255 ? 255 : i);
    };
    return argb(makeRgb(mix(chanR(base), chanR(over)), mix(chanG(base), chanG(over)),
                        mix(chanB(base), chanB(over))),
                chanA(base));
}

}  // namespace wpg
