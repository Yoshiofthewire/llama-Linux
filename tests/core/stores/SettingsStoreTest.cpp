#include "stores/SettingsStore.h"

#include <QTemporaryDir>
#include <QTest>

class SettingsStoreTest : public QObject
{
    Q_OBJECT

private slots:
    void defaultsAreUnset();
    void themeIdRoundTrips();
    void manualMobileOverrideRoundTrips();
    void pushServerBaseUrlRoundTrips();

private:
    QString tempFilePath(QTemporaryDir& dir, const QString& name) const;
};

QString SettingsStoreTest::tempFilePath(QTemporaryDir& dir, const QString& name) const
{
    return dir.filePath(name);
}

void SettingsStoreTest::defaultsAreUnset()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SettingsStore store(tempFilePath(dir, QStringLiteral("settings.ini")));

    QCOMPARE(store.themeId(), QStringLiteral("Dark Matter"));
    QVERIFY(!store.manualMobileOverride().has_value());
    QCOMPARE(store.pushServerBaseUrl(), QStringLiteral("https://ntfy.sh"));
}

void SettingsStoreTest::themeIdRoundTrips()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SettingsStore store(tempFilePath(dir, QStringLiteral("settings.ini")));

    store.setThemeId(QStringLiteral("Solar Flare"));
    QCOMPARE(store.themeId(), QStringLiteral("Solar Flare"));
}

void SettingsStoreTest::manualMobileOverrideRoundTrips()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SettingsStore store(tempFilePath(dir, QStringLiteral("settings.ini")));

    store.setManualMobileOverride(true);
    QVERIFY(store.manualMobileOverride().has_value());
    QCOMPARE(store.manualMobileOverride().value(), true);

    store.setManualMobileOverride(false);
    QVERIFY(store.manualMobileOverride().has_value());
    QCOMPARE(store.manualMobileOverride().value(), false);

    store.setManualMobileOverride(std::nullopt);
    QVERIFY(!store.manualMobileOverride().has_value());
}

void SettingsStoreTest::pushServerBaseUrlRoundTrips()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SettingsStore store(tempFilePath(dir, QStringLiteral("settings.ini")));

    store.setPushServerBaseUrl(QStringLiteral("https://push.example.com"));
    QCOMPARE(store.pushServerBaseUrl(), QStringLiteral("https://push.example.com"));
}

QTEST_GUILESS_MAIN(SettingsStoreTest)
#include "SettingsStoreTest.moc"
