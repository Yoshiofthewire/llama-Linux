#pragma once
#include "ThemePalette.h"
#include <QString>
#include <QStringList>

namespace AppTheme
{
    // Exact list + order from web THEME_OPTIONS -- binding contract, do
    // not reorder or rename.
    QStringList themeNames();

    QString defaultThemeName(); // "Dark Matter"

    // Returns the named palette, or the default palette if `name` is
    // unknown (mirrors AppTheme.swift's `palettes[name] ??
    // palettes[defaultThemeName]!` fallback).
    ThemePalette palette(const QString& name);
}
