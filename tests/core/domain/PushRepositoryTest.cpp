#include "domain/PushRepository.h"

#include "db/Database.h"
#include "db/PushDao.h"
#include "domain/DevicePairing.h"
#include "domain/PairingStore.h"
#include "models/PushNotification.h"
#include "net/HttpClient.h"
#include "net/PushNotificationClient.h"
#include "stores/CursorStore.h"
#include "stores/SecureStoreFile.h"
#include "stores/SettingsStore.h"

#include "../net/FakeRelayServer.h"

#include <QNetworkAccessManager>
#include <QTemporaryDir>
#include <QTest>

class PushRepositoryTest : public QObject
{
    Q_OBJECT

private slots:
    void differentMessageIdsSameInstantGetDifferentSeqs();
    void sameMessageIdArrivingTwiceReusesOneRow();
    void pullDeduplicatesBySeqAndAdvancesCursorAfterHandoff();
    void storedPullEndpointOverridesDerivedOne();
    void unsetPullEndpointIsDerivedFromServerBaseUrl();
    void pullWithoutPairingReturnsEmptyAndMakesNoRequest();
    void historyReturnsMostRecentFirstUpToLimit();
    void markReadDelegatesToPushDao();

private:
    static void savePairing(PairingStore& pairingStore, const QString& serverBaseUrl);
    static PushNotification samplePayload(const QString& messageId);
};

void PushRepositoryTest::savePairing(PairingStore& pairingStore, const QString& serverBaseUrl)
{
    DevicePairing pairing;
    pairing.subscriberId = QStringLiteral("sub-1");
    pairing.subscriberHash = QStringLiteral("hash-1");
    pairing.serverBaseUrl = serverBaseUrl;
    pairing.registrationUrl = serverBaseUrl + QStringLiteral("/api/notifications/native/register");
    pairing.pairingToken = QStringLiteral("pair-tok");
    pairing.deviceId = QStringLiteral("device-1");
    pairing.deviceName = QStringLiteral("My Linux Desktop");
    QVERIFY(pairingStore.save(pairing));
}

PushNotification PushRepositoryTest::samplePayload(const QString& messageId)
{
    PushNotification payload;
    payload.messageId = messageId;
    payload.sender = QStringLiteral("a@example.com");
    payload.subject = QStringLiteral("Hello");
    payload.title = QStringLiteral("Alice");
    payload.body = QStringLiteral("Hello there");
    return payload;
}

// Proves seq uniqueness is maintained for cursor-ordering correctness even
// when two distinct messages arrive in the same millisecond -- the
// collision-avoidance loop in recordPushArrival() must bump the second one.
void PushRepositoryTest::differentMessageIdsSameInstantGetDifferentSeqs()
{
    Database db;
    QVERIFY(db.open(QStringLiteral(":memory:")));
    PushDao pushDao(db.handle());

    QTemporaryDir cursorDir;
    QVERIFY(cursorDir.isValid());
    CursorStore cursorStore(cursorDir.filePath(QStringLiteral("cursors.ini")));

    QTemporaryDir secureDir;
    QVERIFY(secureDir.isValid());
    SecureStoreFile secureStore(secureDir.path());
    PairingStore pairingStore(secureStore);

    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    SettingsStore settingsStore(settingsDir.filePath(QStringLiteral("settings.ini")));

    QNetworkAccessManager manager;
    HttpClient http(manager);
    PushNotificationClient client(http);

    PushRepository repository(pushDao, cursorStore, client, pairingStore, settingsStore);

    const qint64 instant = 1731000000123;
    const PushRecord first = repository.recordPushArrival(samplePayload(QStringLiteral("msg-1")), instant);
    const PushRecord second = repository.recordPushArrival(samplePayload(QStringLiteral("msg-2")), instant);

    QCOMPARE(first.seq, instant);
    QVERIFY(second.seq != first.seq);
    QCOMPARE(second.seq, instant + 1);

    const QVector<PushRecord> history = repository.history();
    QCOMPARE(history.size(), 2);
}

// Deviation from PushTests.swift's pushArrivalsGetUniqueSynthesizedSeqs
// (see PushRepository::recordPushArrival's doc comment): our
// push_notifications table is keyed by message_id, so the *same* messageId
// arriving twice must reuse one row, not create two.
void PushRepositoryTest::sameMessageIdArrivingTwiceReusesOneRow()
{
    Database db;
    QVERIFY(db.open(QStringLiteral(":memory:")));
    PushDao pushDao(db.handle());

    QTemporaryDir cursorDir;
    QVERIFY(cursorDir.isValid());
    CursorStore cursorStore(cursorDir.filePath(QStringLiteral("cursors.ini")));

    QTemporaryDir secureDir;
    QVERIFY(secureDir.isValid());
    SecureStoreFile secureStore(secureDir.path());
    PairingStore pairingStore(secureStore);

    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    SettingsStore settingsStore(settingsDir.filePath(QStringLiteral("settings.ini")));

    QNetworkAccessManager manager;
    HttpClient http(manager);
    PushNotificationClient client(http);

    PushRepository repository(pushDao, cursorStore, client, pairingStore, settingsStore);

    const qint64 instant = 1731000000123;
    repository.recordPushArrival(samplePayload(QStringLiteral("msg-1")), instant);
    repository.recordPushArrival(samplePayload(QStringLiteral("msg-1")), instant);

    const QVector<PushRecord> history = repository.history();
    QCOMPARE(history.size(), 1);
    QCOMPARE(history.first().messageId, QStringLiteral("msg-1"));
}

void PushRepositoryTest::pullDeduplicatesBySeqAndAdvancesCursorAfterHandoff()
{
    const QByteArray body = R"({"deliveryMode":"pull","cursor":10,"notifications":[)"
                             R"({"seq":3,"title":"Old","body":"Old body","data":{"messageId":"msg-old"}},)"
                             R"({"seq":5,"title":"New","body":"New body","data":{"messageId":"msg-new"}}]})";
    FakeRelayServer fake(httpResponse(200, "OK", body));

    Database db;
    QVERIFY(db.open(QStringLiteral(":memory:")));
    PushDao pushDao(db.handle());

    QTemporaryDir cursorDir;
    QVERIFY(cursorDir.isValid());
    CursorStore cursorStore(cursorDir.filePath(QStringLiteral("cursors.ini")));
    cursorStore.setNotificationCursor(3);

    QTemporaryDir secureDir;
    QVERIFY(secureDir.isValid());
    SecureStoreFile secureStore(secureDir.path());
    PairingStore pairingStore(secureStore);
    savePairing(pairingStore, QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));

    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    SettingsStore settingsStore(settingsDir.filePath(QStringLiteral("settings.ini")));

    QNetworkAccessManager manager;
    HttpClient http(manager);
    PushNotificationClient client(http);

    PushRepository repository(pushDao, cursorStore, client, pairingStore, settingsStore);

    const QVector<PushNotification> delivered = repository.pullOnce();

    QCOMPARE(delivered.size(), 1);
    QCOMPARE(delivered.first().messageId, QStringLiteral("msg-new"));

    QVERIFY(!pushDao.findById(QStringLiteral("msg-old")).has_value());
    const std::optional<PushRecord> persisted = pushDao.findById(QStringLiteral("msg-new"));
    QVERIFY(persisted.has_value());
    QCOMPARE(persisted->seq, qint64(5));

    QCOMPARE(cursorStore.notificationCursor(), qint64(10));
    QVERIFY(fake.receivedRequest().contains("after=3"));
}

void PushRepositoryTest::storedPullEndpointOverridesDerivedOne()
{
    FakeRelayServer fake(httpResponse(200, "OK", R"({"deliveryMode":"pull","cursor":0,"notifications":[]})"));

    Database db;
    QVERIFY(db.open(QStringLiteral(":memory:")));
    PushDao pushDao(db.handle());

    QTemporaryDir cursorDir;
    QVERIFY(cursorDir.isValid());
    CursorStore cursorStore(cursorDir.filePath(QStringLiteral("cursors.ini")));

    QTemporaryDir secureDir;
    QVERIFY(secureDir.isValid());
    SecureStoreFile secureStore(secureDir.path());
    PairingStore pairingStore(secureStore);
    // serverBaseUrl deliberately points nowhere real -- if the derived
    // fallback were used instead of the stored override, this test's
    // FakeRelayServer would never see a connection.
    savePairing(pairingStore, QStringLiteral("http://127.0.0.1:1"));

    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    SettingsStore settingsStore(settingsDir.filePath(QStringLiteral("settings.ini")));
    settingsStore.setPullEndpoint(
        QStringLiteral("http://127.0.0.1:%1/custom/pull/path").arg(fake.port()));

    QNetworkAccessManager manager;
    HttpClient http(manager);
    PushNotificationClient client(http);

    PushRepository repository(pushDao, cursorStore, client, pairingStore, settingsStore);
    repository.pullOnce();

    QVERIFY(fake.receivedRequest().contains("GET /custom/pull/path HTTP/1.1"));
}

void PushRepositoryTest::unsetPullEndpointIsDerivedFromServerBaseUrl()
{
    FakeRelayServer fake(httpResponse(200, "OK", R"({"deliveryMode":"pull","cursor":0,"notifications":[]})"));

    Database db;
    QVERIFY(db.open(QStringLiteral(":memory:")));
    PushDao pushDao(db.handle());

    QTemporaryDir cursorDir;
    QVERIFY(cursorDir.isValid());
    CursorStore cursorStore(cursorDir.filePath(QStringLiteral("cursors.ini")));

    QTemporaryDir secureDir;
    QVERIFY(secureDir.isValid());
    SecureStoreFile secureStore(secureDir.path());
    PairingStore pairingStore(secureStore);
    // Trailing slash on purpose -- resolvePullEndpoint() must trim it before
    // appending the derived path.
    savePairing(pairingStore, QStringLiteral("http://127.0.0.1:%1/").arg(fake.port()));

    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    SettingsStore settingsStore(settingsDir.filePath(QStringLiteral("settings.ini")));
    // pullEndpoint left unset.

    QNetworkAccessManager manager;
    HttpClient http(manager);
    PushNotificationClient client(http);

    PushRepository repository(pushDao, cursorStore, client, pairingStore, settingsStore);
    repository.pullOnce();

    QVERIFY(fake.receivedRequest().contains("GET /api/notifications/native/pull HTTP/1.1"));
}

void PushRepositoryTest::pullWithoutPairingReturnsEmptyAndMakesNoRequest()
{
    Database db;
    QVERIFY(db.open(QStringLiteral(":memory:")));
    PushDao pushDao(db.handle());

    QTemporaryDir cursorDir;
    QVERIFY(cursorDir.isValid());
    CursorStore cursorStore(cursorDir.filePath(QStringLiteral("cursors.ini")));

    QTemporaryDir secureDir;
    QVERIFY(secureDir.isValid());
    SecureStoreFile secureStore(secureDir.path());
    PairingStore pairingStore(secureStore); // never saved -- not paired

    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    SettingsStore settingsStore(settingsDir.filePath(QStringLiteral("settings.ini")));

    QNetworkAccessManager manager;
    HttpClient http(manager);
    PushNotificationClient client(http);

    PushRepository repository(pushDao, cursorStore, client, pairingStore, settingsStore);

    const QVector<PushNotification> delivered = repository.pullOnce();
    QVERIFY(delivered.isEmpty());
    QCOMPARE(cursorStore.notificationCursor(), qint64(0));
}

void PushRepositoryTest::historyReturnsMostRecentFirstUpToLimit()
{
    Database db;
    QVERIFY(db.open(QStringLiteral(":memory:")));
    PushDao pushDao(db.handle());

    QTemporaryDir cursorDir;
    QVERIFY(cursorDir.isValid());
    CursorStore cursorStore(cursorDir.filePath(QStringLiteral("cursors.ini")));

    QTemporaryDir secureDir;
    QVERIFY(secureDir.isValid());
    SecureStoreFile secureStore(secureDir.path());
    PairingStore pairingStore(secureStore);

    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    SettingsStore settingsStore(settingsDir.filePath(QStringLiteral("settings.ini")));

    QNetworkAccessManager manager;
    HttpClient http(manager);
    PushNotificationClient client(http);

    PushRepository repository(pushDao, cursorStore, client, pairingStore, settingsStore);

    repository.recordPushArrival(samplePayload(QStringLiteral("msg-1")), 100);
    repository.recordPushArrival(samplePayload(QStringLiteral("msg-2")), 200);
    repository.recordPushArrival(samplePayload(QStringLiteral("msg-3")), 300);

    const QVector<PushRecord> limited = repository.history(2);
    QCOMPARE(limited.size(), 2);
    QCOMPARE(limited.at(0).messageId, QStringLiteral("msg-3"));
    QCOMPARE(limited.at(1).messageId, QStringLiteral("msg-2"));
}

void PushRepositoryTest::markReadDelegatesToPushDao()
{
    Database db;
    QVERIFY(db.open(QStringLiteral(":memory:")));
    PushDao pushDao(db.handle());

    QTemporaryDir cursorDir;
    QVERIFY(cursorDir.isValid());
    CursorStore cursorStore(cursorDir.filePath(QStringLiteral("cursors.ini")));

    QTemporaryDir secureDir;
    QVERIFY(secureDir.isValid());
    SecureStoreFile secureStore(secureDir.path());
    PairingStore pairingStore(secureStore);

    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    SettingsStore settingsStore(settingsDir.filePath(QStringLiteral("settings.ini")));

    QNetworkAccessManager manager;
    HttpClient http(manager);
    PushNotificationClient client(http);

    PushRepository repository(pushDao, cursorStore, client, pairingStore, settingsStore);

    repository.recordPushArrival(samplePayload(QStringLiteral("msg-1")), 100);
    repository.markRead(QStringLiteral("msg-1"));

    const std::optional<PushRecord> record = pushDao.findById(QStringLiteral("msg-1"));
    QVERIFY(record.has_value());
    QVERIFY(record->consumed);
}

QTEST_GUILESS_MAIN(PushRepositoryTest)
#include "PushRepositoryTest.moc"
