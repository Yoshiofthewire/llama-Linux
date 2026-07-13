#include "theme/AppTheme.h"

#include <QSet>
#include <QTest>

class AppThemeTest : public QObject
{
    Q_OBJECT

private slots:
    void thirteenThemesMatchWebList();
    void defaultThemeIsDarkMatter();
    void spotCheckHexValuesAgainstThemeTs();
    void lightThemesGetLightColorScheme();
};

void AppThemeTest::thirteenThemesMatchWebList()
{
    const QStringList expected = {
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

    const QStringList actual = AppTheme::themeNames();
    QCOMPARE(actual.size(), 13);
    QCOMPARE(actual, expected);

    for (const QString& name : actual) {
        // Should not throw / crash; just exercise the lookup.
        const ThemePalette p = AppTheme::palette(name);
        Q_UNUSED(p);
    }
}

void AppThemeTest::defaultThemeIsDarkMatter()
{
    QCOMPARE(AppTheme::defaultThemeName(), QStringLiteral("Dark Matter"));
    QCOMPARE(AppTheme::palette(QStringLiteral("Nope")), AppTheme::palette(QStringLiteral("Dark Matter")));
}

void AppThemeTest::spotCheckHexValuesAgainstThemeTs()
{
    const ThemePalette darkMatter = AppTheme::palette(QStringLiteral("Dark Matter"));
    QCOMPARE(darkMatter.bg, 0x1A1A1Eu);
    QCOMPARE(darkMatter.accent, 0xC29A72u);

    QCOMPARE(AppTheme::palette(QStringLiteral("Cyber Punk")).accent, 0x00F5D4u);
    QCOMPARE(AppTheme::palette(QStringLiteral("White Cliffs")).panel, 0xFFFFFFu);
    QCOMPARE(AppTheme::palette(QStringLiteral("Sun")).line, 0xD4B27Au);
    QCOMPARE(AppTheme::palette(QStringLiteral("Ocean")).accentSoft, 0x214657u);
}

void AppThemeTest::lightThemesGetLightColorScheme()
{
    const QSet<QString> lightThemes = {
        QStringLiteral("Light Matter"),
        QStringLiteral("Tropics"),
        QStringLiteral("White Cliffs"),
        QStringLiteral("Sky"),
        QStringLiteral("Sun"),
    };

    for (const QString& name : AppTheme::themeNames()) {
        const ThemePalette p = AppTheme::palette(name);
        const bool expectedLight = lightThemes.contains(name);
        QVERIFY2(p.isLight() == expectedLight, qPrintable(name));
        QVERIFY2(p.preferredColorScheme() ==
                     (expectedLight ? ThemePalette::ColorScheme::Light : ThemePalette::ColorScheme::Dark),
                 qPrintable(name));
    }
}

QTEST_APPLESS_MAIN(AppThemeTest)
#include "AppThemeTest.moc"
