#pragma once
#include <QtGlobal>  // quint32 only -- NOT QColor/QtGui, see global constraint #1

struct ThemePalette
{
    quint32 bg;
    quint32 panel;
    quint32 ink;
    quint32 inkStrong;
    quint32 accent;
    quint32 accentSoft;
    quint32 line;

    bool operator==(const ThemePalette&) const = default;

    // True when a hex color (0xRRGGBB) reads as visually "light" by the
    // same formula as the Swift reference: luminance =
    // 0.299*R + 0.587*G + 0.114*B (each channel normalized 0..1),
    // light when luminance > 0.55.
    static bool isPerceptuallyLight(quint32 rgbHex);

    // bg's perceptual lightness -- light themes must present a light
    // system/QML color-scheme hint (Task 6 consumer).
    bool isLight() const; // isPerceptuallyLight(bg)

    enum class ColorScheme { Light, Dark };
    ColorScheme preferredColorScheme() const; // isLight() ? Light : Dark

    // Readable text/icon color to place ON TOP of `accent` fill (web:
    // buttonText). 0x1A1A1E when accent is perceptually light, else
    // 0xFFFFFF -- exact same two constants as AppTheme.swift's
    // readableOnAccent (Color(hex: 0x1A1A1E) / .white).
    quint32 readableOnAccent() const;
};
