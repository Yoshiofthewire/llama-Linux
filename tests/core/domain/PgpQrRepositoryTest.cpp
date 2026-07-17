#include "domain/PgpQrRepository.h"

#include "domain/DevicePairing.h"
#include "domain/PairingStore.h"
#include "net/HttpClient.h"
#include "net/PgpQrClient.h"
#include "stores/SecureStoreFile.h"

#include "../net/FakeRelayServer.h"

#include <QNetworkAccessManager>
#include <QTemporaryDir>
#include <QTest>

class PgpQrRepositoryTest : public QObject
{
    Q_OBJECT

private slots:
    void fetchMyTokenWithoutPairingReturnsNotPaired();
    void fetchMyTokenSuccessReturnsTokenExpiresAtAndUrl();
    void fetchMyToken400MapsToNoPgpIdentity();
    void fetchMyToken401MapsToUnauthorized();
    void fetchMyToken503MapsToServiceUnavailable();

private:
    static void savePairing(PairingStore& pairingStore, quint16 port);
};

void PgpQrRepositoryTest::savePairing(PairingStore& pairingStore, quint16 port)
{
    DevicePairing pairing;
    pairing.subscriberId = QStringLiteral("sub-1");
    pairing.subscriberHash = QStringLiteral("hash-1");
    pairing.serverBaseUrl = QStringLiteral("http://127.0.0.1:%1").arg(port);
    pairing.registrationUrl = QStringLiteral("http://127.0.0.1:%1/api/notifications/native/register").arg(port);
    pairing.pairingToken = QStringLiteral("pair-tok");
    pairing.deviceId = QStringLiteral("device-1");
    pairing.deviceName = QStringLiteral("My Linux Desktop");
    QVERIFY(pairingStore.save(pairing));
}

void PgpQrRepositoryTest::fetchMyTokenWithoutPairingReturnsNotPaired()
{
    QTemporaryDir secureDir;
    QVERIFY(secureDir.isValid());
    SecureStoreFile secureStore(secureDir.path());
    PairingStore pairingStore(secureStore); // never saved -- not paired

    QNetworkAccessManager manager;
    HttpClient http(manager);
    PgpQrClient client(http);

    PgpQrRepository repository(client, pairingStore);

    // No FakeRelayServer at all -- must return without attempting any
    // network call when there's no pairing to derive a server URL from.
    const PgpQrTokenOutcome outcome = repository.fetchMyToken();
    QCOMPARE(outcome.status, PgpQrTokenStatus::NotPaired);
}

void PgpQrRepositoryTest::fetchMyTokenSuccessReturnsTokenExpiresAtAndUrl()
{
    const QByteArray body =
        R"({"token":"tok-1","expiresAt":"2026-07-17T12:02:00Z","url":"https://example.com/api/pgp/qr/key?t=tok-1"})";
    FakeRelayServer fake(httpResponse(200, "OK", body));

    QTemporaryDir secureDir;
    QVERIFY(secureDir.isValid());
    SecureStoreFile secureStore(secureDir.path());
    PairingStore pairingStore(secureStore);
    savePairing(pairingStore, fake.port());

    QNetworkAccessManager manager;
    HttpClient http(manager);
    PgpQrClient client(http);

    PgpQrRepository repository(client, pairingStore);
    const PgpQrTokenOutcome outcome = repository.fetchMyToken();

    QCOMPARE(outcome.status, PgpQrTokenStatus::Success);
    QCOMPARE(outcome.token, QStringLiteral("tok-1"));
    QCOMPARE(outcome.expiresAt, QStringLiteral("2026-07-17T12:02:00Z"));
    QCOMPARE(outcome.url, QStringLiteral("https://example.com/api/pgp/qr/key?t=tok-1"));
    QVERIFY(fake.receivedRequest().contains("GET /api/pgp/qr/token?"));
}

void PgpQrRepositoryTest::fetchMyToken400MapsToNoPgpIdentity()
{
    FakeRelayServer fake(httpResponse(400, "Bad Request", "no pgp identity configured\n", "text/plain"));

    QTemporaryDir secureDir;
    QVERIFY(secureDir.isValid());
    SecureStoreFile secureStore(secureDir.path());
    PairingStore pairingStore(secureStore);
    savePairing(pairingStore, fake.port());

    QNetworkAccessManager manager;
    HttpClient http(manager);
    PgpQrClient client(http);

    PgpQrRepository repository(client, pairingStore);
    const PgpQrTokenOutcome outcome = repository.fetchMyToken();

    QCOMPARE(outcome.status, PgpQrTokenStatus::NoPgpIdentity);
}

void PgpQrRepositoryTest::fetchMyToken401MapsToUnauthorized()
{
    FakeRelayServer fake(httpResponse(401, "Unauthorized", R"({"error":"unauthorized"})"));

    QTemporaryDir secureDir;
    QVERIFY(secureDir.isValid());
    SecureStoreFile secureStore(secureDir.path());
    PairingStore pairingStore(secureStore);
    savePairing(pairingStore, fake.port());

    QNetworkAccessManager manager;
    HttpClient http(manager);
    PgpQrClient client(http);

    PgpQrRepository repository(client, pairingStore);
    const PgpQrTokenOutcome outcome = repository.fetchMyToken();

    QCOMPARE(outcome.status, PgpQrTokenStatus::Unauthorized);
}

void PgpQrRepositoryTest::fetchMyToken503MapsToServiceUnavailable()
{
    FakeRelayServer fake(httpResponse(503, "Service Unavailable", "pairing is not configured\n", "text/plain"));

    QTemporaryDir secureDir;
    QVERIFY(secureDir.isValid());
    SecureStoreFile secureStore(secureDir.path());
    PairingStore pairingStore(secureStore);
    savePairing(pairingStore, fake.port());

    QNetworkAccessManager manager;
    HttpClient http(manager);
    PgpQrClient client(http);

    PgpQrRepository repository(client, pairingStore);
    const PgpQrTokenOutcome outcome = repository.fetchMyToken();

    QCOMPARE(outcome.status, PgpQrTokenStatus::ServiceUnavailable);
}

QTEST_GUILESS_MAIN(PgpQrRepositoryTest)
#include "PgpQrRepositoryTest.moc"
