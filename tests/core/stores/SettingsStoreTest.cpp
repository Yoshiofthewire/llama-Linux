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
    void pushDeliveryFieldsRoundTrip();
    void keywordVisibleDefaultsTrueUntilExplicitlyToggled();

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

void SettingsStoreTest::pushDeliveryFieldsRoundTrip()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SettingsStore store(tempFilePath(dir, QStringLiteral("settings.ini")));

    QVERIFY(store.deliveryMode().isEmpty());
    QVERIFY(store.pullEndpoint().isEmpty());
    QVERIFY(store.transport().isEmpty());

    store.setDeliveryMode(QStringLiteral("pull"));
    store.setPullEndpoint(QStringLiteral("http://relay.example/api/notifications/native/pull"));
    store.setTransport(QStringLiteral("unifiedpush"));

    QCOMPARE(store.deliveryMode(), QStringLiteral("pull"));
    QCOMPARE(store.pullEndpoint(), QStringLiteral("http://relay.example/api/notifications/native/pull"));
    QCOMPARE(store.transport(), QStringLiteral("unifiedpush"));
}

void SettingsStoreTest::keywordVisibleDefaultsTrueUntilExplicitlyToggled()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SettingsStore store(tempFilePath(dir, QStringLiteral("settings.ini")));

    QCOMPARE(store.keywordVisible(QStringLiteral("Work")), true);

    store.setKeywordVisible(QStringLiteral("Work"), false);
    QCOMPARE(store.keywordVisible(QStringLiteral("Work")), false);
    // An unrelated keyword that was never toggled stays visible.
    QCOMPARE(store.keywordVisible(QStringLiteral("Personal")), true);

    store.setKeywordVisible(QStringLiteral("Work"), true);
    QCOMPARE(store.keywordVisible(QStringLiteral("Work")), true);
}

QTEST_GUILESS_MAIN(SettingsStoreTest)
#include "SettingsStoreTest.moc"
