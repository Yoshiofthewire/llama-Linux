#include "theme/AppTheme.h"

#include <QHash>

namespace {

const QHash<QString, ThemePalette>& allPalettes()
{
    static const QHash<QString, ThemePalette> palettes = {
        {QStringLiteral("Dark Matter"),
         ThemePalette{0x1A1A1E, 0x252530, 0xD4C5E2, 0xE8DDF5, 0xC29A72, 0x5A3F31, 0x404050}},
        {QStringLiteral("Light Matter"),
         ThemePalette{0xF5EFE5, 0xFFF8EE, 0x4C3D32, 0x2D1F15, 0xC29A72, 0xE6D2BE, 0xC5B29D}},
        {QStringLiteral("Tropics"),
         ThemePalette{0xF4F1EB, 0xFFFAF0, 0x43362D, 0x241A14, 0x9BC400, 0xD4E3A0, 0xC4B7A3}},
        {QStringLiteral("Tropic Night"),
         ThemePalette{0x15131A, 0x221F2B, 0xCDBDE0, 0xE8DDF5, 0x9BC400, 0x6B4A42, 0x3C3650}},
        {QStringLiteral("Ocean"),
         ThemePalette{0x0F1B24, 0x152A36, 0xB8D8E8, 0xE0F2FB, 0x5EA9BE, 0x214657, 0x2F5567}},
        {QStringLiteral("Coffee"),
         ThemePalette{0x1D1714, 0x2A211D, 0xD6C0B3, 0xF0DED2, 0xB47F5C, 0x5F3F2F, 0x4A3830}},
        {QStringLiteral("White Cliffs"),
         ThemePalette{0xF7F9FB, 0xFFFFFF, 0x2E4C63, 0x163246, 0x5EA8D8, 0xDFF1FB, 0x8FC3DF}},
        {QStringLiteral("Cyber Punk"),
         ThemePalette{0x120918, 0x1E1028, 0xF5D0FF, 0xFFE9FF, 0x00F5D4, 0x3B1760, 0x5C2D84}},
        {QStringLiteral("Neon Purple"),
         ThemePalette{0x130B1D, 0x231233, 0xE4CCFF, 0xF2E6FF, 0xC86CFF, 0x47206C, 0x63358A}},
        {QStringLiteral("Space"),
         ThemePalette{0x0B0F1A, 0x151C2D, 0xC8D5F0, 0xE7EFFF, 0x86A8FF, 0x263E74, 0x34496F}},
        {QStringLiteral("Sky"),
         ThemePalette{0xDFF1FF, 0xF4FBFF, 0x2F4F64, 0x183142, 0x6DB3D6, 0xB6DCED, 0x93BDD2}},
        {QStringLiteral("Forest"),
         ThemePalette{0x142018, 0x1F2F24, 0xC7DBC7, 0xE3F0DF, 0x8FAA74, 0x3A5837, 0x4F694F}},
        {QStringLiteral("Sun"),
         ThemePalette{0xFFF3DC, 0xFFF9EC, 0x5A4024, 0x392611, 0xE0AB4F, 0xF1D9A2, 0xD4B27A}},
    };
    return palettes;
}

} // namespace

namespace AppTheme {

QStringList themeNames()
{
    return {
        QStringLiteral("Dark Matter"),
        QStringLiteral("Light Matter"),
        QStringLiteral("Tropics"),
        QStringLiteral("Tropic Night"),
        QStringLiteral("Ocean"),
        QStringLiteral("Coffee"),
        QStringLiteral("White Cliffs"),
        QStringLiteral("Cyber Punk"),
        QStringLiteral("Neon Purple"),
        QStringLiteral("Space"),
        QStringLiteral("Sky"),
        QStringLiteral("Forest"),
        QStringLiteral("Sun"),
    };
}

QString defaultThemeName()
{
    return QStringLiteral("Dark Matter");
}

ThemePalette palette(const QString& name)
{
    const QHash<QString, ThemePalette>& palettes = allPalettes();
    const auto it = palettes.constFind(name);
    if (it != palettes.constEnd())
        return it.value();
    return palettes.constFind(defaultThemeName()).value();
}

} // namespace AppTheme
