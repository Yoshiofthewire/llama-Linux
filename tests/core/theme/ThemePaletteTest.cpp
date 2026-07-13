#include "theme/AppTheme.h"
#include "theme/SemanticColors.h"
#include "theme/Shape.h"
#include "theme/ThemePalette.h"

#include <QTest>

class ThemePaletteTest : public QObject
{
    Q_OBJECT

private slots:
    void readableOnAccentContrast();
    void semanticColorsMatchLiterals();
    void shapeRadiiMatchLiterals();
};

void ThemePaletteTest::readableOnAccentContrast()
{
    QVERIFY(ThemePalette::isPerceptuallyLight(0xFFFFFF));
    QVERIFY(!ThemePalette::isPerceptuallyLight(0x000000));
    // Cyber Punk's accent needs dark text.
    QVERIFY(ThemePalette::isPerceptuallyLight(0x00F5D4));
    // Ocean's accentSoft is dark.
    QVERIFY(!ThemePalette::isPerceptuallyLight(0x214657));

    QCOMPARE(AppTheme::palette(QStringLiteral("Cyber Punk")).readableOnAccent(), 0x1A1A1Eu);

    // NOTE: the task brief guessed Dark Matter's accent (0xC29A72) is NOT
    // perceptually light and expected readableOnAccent() == 0xFFFFFF, but
    // explicitly told us to recompute rather than trust that guess. Worked
    // by hand with the spec'd formula (R=194/255=.7608, G=154/255=.6039,
    // B=114/255=.4471; luminance = .299*.7608 + .587*.6039 + .114*.4471 =
    // .22747 + .35450 + .05096 = .63293), 0xC29A72 IS perceptually light
    // (.63293 > 0.55). So readableOnAccent() correctly returns 0x1A1A1E
    // here, not 0xFFFFFF -- see task-26-report.md for the flagged concern.
    QVERIFY(ThemePalette::isPerceptuallyLight(0xC29A72));
    QCOMPARE(AppTheme::palette(QStringLiteral("Dark Matter")).readableOnAccent(), 0x1A1A1Eu);
}

void ThemePaletteTest::semanticColorsMatchLiterals()
{
    QCOMPARE(SemanticColors::danger.rgb, 0xFF5F5Fu);
    QCOMPARE(SemanticColors::danger.alpha, 1.0);
    QCOMPARE(SemanticColors::dangerBorder.rgb, 0xFFB4ABu);
    QCOMPARE(SemanticColors::dangerBorder.alpha, 0.4);
    QCOMPARE(SemanticColors::dangerFill.rgb, 0xFF5F5Fu);
    QCOMPARE(SemanticColors::dangerFill.alpha, 0.12);
    QCOMPARE(SemanticColors::warning.rgb, 0xFFD64Du);
    QCOMPARE(SemanticColors::warning.alpha, 1.0);
    QCOMPARE(SemanticColors::successBorder.rgb, 0x7BBF7Bu);
    QCOMPARE(SemanticColors::successBorder.alpha, 1.0);
    QCOMPARE(SemanticColors::successText.rgb, 0xA5DCA5u);
    QCOMPARE(SemanticColors::successText.alpha, 1.0);
}

void ThemePaletteTest::shapeRadiiMatchLiterals()
{
    QCOMPARE(Shape::field, 14);
    QCOMPARE(Shape::button, 10);
    QCOMPARE(Shape::panel, 14);
    QCOMPARE(Shape::sheet, 14);
    QCOMPARE(Shape::emptyState, 10);
}

QTEST_APPLESS_MAIN(ThemePaletteTest)
#include "ThemePaletteTest.moc"
