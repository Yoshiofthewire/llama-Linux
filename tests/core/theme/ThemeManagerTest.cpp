#include "theme/ThemeManager.h"

#include "stores/SettingsStore.h"

#include <QTemporaryDir>
#include <QTest>

class ThemeManagerTest : public QObject
{
    Q_OBJECT

private slots:
    void freshStoreDefaultsToPatinaKy();
    void setThemeUpdatesNameAndPalette();
    void setThemeIgnoresUnknownName();
    void persistenceRoundTripsAcrossManagers();
    void invalidPersistedThemeIdFallsBackToDefault();

private:
    QString tempFilePath(QTemporaryDir& dir, const QString& name) const;
};

QString ThemeManagerTest::tempFilePath(QTemporaryDir& dir, const QString& name) const
{
    return dir.filePath(name);
}

void ThemeManagerTest::freshStoreDefaultsToPatinaKy()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SettingsStore store(tempFilePath(dir, QStringLiteral("settings.ini")));

    ThemeManager manager(store);
    QCOMPARE(manager.themeName(), QStringLiteral("Patina Ky"));
    QCOMPARE(manager.palette(), AppTheme::palette(QStringLiteral("Patina Ky")));
}

void ThemeManagerTest::setThemeUpdatesNameAndPalette()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SettingsStore store(tempFilePath(dir, QStringLiteral("settings.ini")));

    ThemeManager manager(store);
    manager.setTheme(QStringLiteral("Ocean"));

    QCOMPARE(manager.themeName(), QStringLiteral("Ocean"));
    QCOMPARE(manager.palette(), AppTheme::palette(QStringLiteral("Ocean")));
}

void ThemeManagerTest::setThemeIgnoresUnknownName()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SettingsStore store(tempFilePath(dir, QStringLiteral("settings.ini")));

    ThemeManager manager(store);
    manager.setTheme(QStringLiteral("Ocean"));
    manager.setTheme(QStringLiteral("Not A Theme"));

    QCOMPARE(manager.themeName(), QStringLiteral("Ocean"));
    QCOMPARE(manager.palette(), AppTheme::palette(QStringLiteral("Ocean")));
}

void ThemeManagerTest::persistenceRoundTripsAcrossManagers()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = tempFilePath(dir, QStringLiteral("settings.ini"));
    SettingsStore store(path);

    ThemeManager first(store);
    first.setTheme(QStringLiteral("Ocean"));

    SettingsStore secondStore(path);
    ThemeManager second(secondStore);
    QCOMPARE(second.themeName(), QStringLiteral("Ocean"));
}

void ThemeManagerTest::invalidPersistedThemeIdFallsBackToDefault()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SettingsStore store(tempFilePath(dir, QStringLiteral("settings.ini")));
    store.setThemeId(QStringLiteral("Bogus"));

    ThemeManager manager(store);
    QCOMPARE(manager.themeName(), QStringLiteral("Patina Ky"));
    QCOMPARE(manager.palette(), AppTheme::palette(QStringLiteral("Patina Ky")));
}

QTEST_GUILESS_MAIN(ThemeManagerTest)
#include "ThemeManagerTest.moc"
