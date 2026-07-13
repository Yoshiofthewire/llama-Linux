#include "theme/ThemePalette.h"

bool ThemePalette::isPerceptuallyLight(quint32 rgbHex)
{
    const double r = ((rgbHex >> 16) & 0xFF) / 255.0;
    const double g = ((rgbHex >> 8) & 0xFF) / 255.0;
    const double b = (rgbHex & 0xFF) / 255.0;
    const double luminance = 0.299 * r + 0.587 * g + 0.114 * b;
    return luminance > 0.55;
}

bool ThemePalette::isLight() const
{
    return isPerceptuallyLight(bg);
}

ThemePalette::ColorScheme ThemePalette::preferredColorScheme() const
{
    return isLight() ? ColorScheme::Light : ColorScheme::Dark;
}

quint32 ThemePalette::readableOnAccent() const
{
    return isPerceptuallyLight(accent) ? 0x1A1A1E : 0xFFFFFF;
}
