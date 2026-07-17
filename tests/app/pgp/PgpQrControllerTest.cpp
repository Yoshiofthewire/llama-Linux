#include "pgp/PgpQrController.h"

#include "domain/DevicePairing.h"
#include "domain/PairingStore.h"
#include "domain/PgpQrRepository.h"
#include "net/HttpClient.h"
#include "net/PgpQrClient.h"
#include "stores/SecureStoreFile.h"

#include "../../core/net/FakeRelayServer.h"

#include <QNetworkAccessManager>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

class PgpQrControllerTest : public QObject
{
    Q_OBJECT

private slots:
    void refreshMyQrCodeWithoutPairingSetsNotPairedError();
    void refreshMyQrCodeSuccessPopulatesUrlAndExpiresAt();
    void refreshMyQrCodeNoPgpIdentitySetsFriendlyMessage();
    void myQrImageDataUrlIsEmptyBeforeRefreshAndPopulatedAfter();
    void scanQrPayloadRejectsNonPgpQrUrl();
    void scanQrPayloadRejectsNonHttpScheme();
    void scanQrPayloadRejectsLinkLocalMetadataHost();
    void scanQrPayloadSuccessPopulatesScanResult();
    void scanQrPayload404SetsFriendlyMessage();
    void clearScanResultResetsFields();

private:
    static void savePairing(PairingStore& pairingStore, quint16 port);
};

void PgpQrControllerTest::savePairing(PairingStore& pairingStore, quint16 port)
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

void PgpQrControllerTest::refreshMyQrCodeWithoutPairingSetsNotPairedError()
{
    QTemporaryDir secureDir;
    QVERIFY(secureDir.isValid());
    SecureStoreFile secureStore(secureDir.path());
    PairingStore pairingStore(secureStore); // never saved -- not paired

    QNetworkAccessManager manager;
    HttpClient http(manager);
    PgpQrClient client(http);
    PgpQrRepository repository(client, pairingStore);
    PgpQrController controller(repository, client);

    controller.refreshMyQrCode();

    QCOMPARE(controller.lastError(), QStringLiteral("Not paired"));
    QCOMPARE(controller.myQrUrl(), QString());
}

void PgpQrControllerTest::refreshMyQrCodeSuccessPopulatesUrlAndExpiresAt()
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
    PgpQrController controller(repository, client);

    QSignalSpy dataSpy(&controller, &PgpQrController::myQrDataChanged);
    controller.refreshMyQrCode();

    QCOMPARE(controller.lastError(), QString());
    QCOMPARE(controller.myQrUrl(), QStringLiteral("https://example.com/api/pgp/qr/key?t=tok-1"));
    QCOMPARE(controller.myQrExpiresAt(), QStringLiteral("2026-07-17T12:02:00Z"));
    QVERIFY(dataSpy.count() >= 1);
}

void PgpQrControllerTest::refreshMyQrCodeNoPgpIdentitySetsFriendlyMessage()
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
    PgpQrController controller(repository, client);

    controller.refreshMyQrCode();

    QCOMPARE(controller.lastError(), QStringLiteral("You haven't set up PGP encryption yet"));
}

void PgpQrControllerTest::myQrImageDataUrlIsEmptyBeforeRefreshAndPopulatedAfter()
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
    PgpQrController controller(repository, client);

    // Nothing fetched yet -- no URL to encode.
    QCOMPARE(controller.myQrImageDataUrl(), QString());

    controller.refreshMyQrCode();

    const QString dataUrl = controller.myQrImageDataUrl();
    QVERIFY(dataUrl.startsWith(QStringLiteral("data:image/png;base64,")));
    // A real, non-trivial PNG payload was actually encoded, not a stub.
    QVERIFY(dataUrl.length() > 100);
}

void PgpQrControllerTest::scanQrPayloadRejectsNonPgpQrUrl()
{
    QTemporaryDir secureDir;
    QVERIFY(secureDir.isValid());
    SecureStoreFile secureStore(secureDir.path());
    PairingStore pairingStore(secureStore);

    QNetworkAccessManager manager;
    HttpClient http(manager);
    PgpQrClient client(http);
    PgpQrRepository repository(client, pairingStore);
    PgpQrController controller(repository, client);

    // No FakeRelayServer at all -- an invalid payload must be rejected
    // before any network call is attempted.
    controller.scanQrPayload(QStringLiteral("https://example.com/totally/unrelated"));

    QCOMPARE(controller.lastError(), QStringLiteral("That QR code isn't a PGP key-exchange code"));
    QCOMPARE(controller.scannedFingerprint(), QString());
}

void PgpQrControllerTest::scanQrPayloadRejectsNonHttpScheme()
{
    QTemporaryDir secureDir;
    QVERIFY(secureDir.isValid());
    SecureStoreFile secureStore(secureDir.path());
    PairingStore pairingStore(secureStore);

    QNetworkAccessManager manager;
    HttpClient http(manager);
    PgpQrClient client(http);
    PgpQrRepository repository(client, pairingStore);
    PgpQrController controller(repository, client);

    // A file:// QR payload must never reach HttpClient/QNetworkAccessManager
    // -- doing so would let a scanned QR code read local files back as if
    // they were key material.
    controller.scanQrPayload(QStringLiteral("file:///etc/passwd#/api/pgp/qr/key"));

    QCOMPARE(controller.lastError(), QStringLiteral("That QR code isn't a PGP key-exchange code"));
    QCOMPARE(controller.scannedFingerprint(), QString());
}

void PgpQrControllerTest::scanQrPayloadRejectsLinkLocalMetadataHost()
{
    QTemporaryDir secureDir;
    QVERIFY(secureDir.isValid());
    SecureStoreFile secureStore(secureDir.path());
    PairingStore pairingStore(secureStore);

    QNetworkAccessManager manager;
    HttpClient http(manager);
    PgpQrClient client(http);
    PgpQrRepository repository(client, pairingStore);
    PgpQrController controller(repository, client);

    // 169.254.169.254 is the cloud-metadata address on AWS/Azure/DigitalOcean
    // -- must be rejected before any request is attempted, same as the
    // file:// case above.
    controller.scanQrPayload(QStringLiteral("http://169.254.169.254/api/pgp/qr/key"));

    QCOMPARE(controller.lastError(), QStringLiteral("That QR code isn't a PGP key-exchange code"));
    QCOMPARE(controller.scannedFingerprint(), QString());
}

void PgpQrControllerTest::scanQrPayloadSuccessPopulatesScanResult()
{
    const QByteArray body =
        R"({"name":"Ada","fingerprint":"ABCD1234","publicKey":"-----BEGIN PGP PUBLIC KEY BLOCK-----"})";
    FakeRelayServer fake(httpResponse(200, "OK", body));

    QTemporaryDir secureDir;
    QVERIFY(secureDir.isValid());
    SecureStoreFile secureStore(secureDir.path());
    PairingStore pairingStore(secureStore);

    QNetworkAccessManager manager;
    HttpClient http(manager);
    PgpQrClient client(http);
    PgpQrRepository repository(client, pairingStore);
    PgpQrController controller(repository, client);

    QSignalSpy scanSpy(&controller, &PgpQrController::scanResultChanged);
    const QString qrUrl = QStringLiteral("http://127.0.0.1:%1/api/pgp/qr/key?t=tok-1").arg(fake.port());
    controller.scanQrPayload(qrUrl);

    QCOMPARE(controller.lastError(), QString());
    QCOMPARE(controller.scannedName(), QStringLiteral("Ada"));
    QCOMPARE(controller.scannedFingerprint(), QStringLiteral("ABCD1234"));
    QCOMPARE(controller.scannedPublicKey(), QStringLiteral("-----BEGIN PGP PUBLIC KEY BLOCK-----"));
    QVERIFY(scanSpy.count() >= 1);
}

void PgpQrControllerTest::scanQrPayload404SetsFriendlyMessage()
{
    FakeRelayServer fake(httpResponse(404, "Not Found", "no pgp identity configured\n", "text/plain"));

    QTemporaryDir secureDir;
    QVERIFY(secureDir.isValid());
    SecureStoreFile secureStore(secureDir.path());
    PairingStore pairingStore(secureStore);

    QNetworkAccessManager manager;
    HttpClient http(manager);
    PgpQrClient client(http);
    PgpQrRepository repository(client, pairingStore);
    PgpQrController controller(repository, client);

    const QString qrUrl = QStringLiteral("http://127.0.0.1:%1/api/pgp/qr/key?t=tok-1").arg(fake.port());
    controller.scanQrPayload(qrUrl);

    QCOMPARE(controller.lastError(), QStringLiteral("This person hasn't set up PGP encryption yet"));
}

void PgpQrControllerTest::clearScanResultResetsFields()
{
    const QByteArray body =
        R"({"name":"Ada","fingerprint":"ABCD1234","publicKey":"-----BEGIN PGP PUBLIC KEY BLOCK-----"})";
    FakeRelayServer fake(httpResponse(200, "OK", body));

    QTemporaryDir secureDir;
    QVERIFY(secureDir.isValid());
    SecureStoreFile secureStore(secureDir.path());
    PairingStore pairingStore(secureStore);

    QNetworkAccessManager manager;
    HttpClient http(manager);
    PgpQrClient client(http);
    PgpQrRepository repository(client, pairingStore);
    PgpQrController controller(repository, client);

    const QString qrUrl = QStringLiteral("http://127.0.0.1:%1/api/pgp/qr/key?t=tok-1").arg(fake.port());
    controller.scanQrPayload(qrUrl);
    QVERIFY(!controller.scannedFingerprint().isEmpty());

    controller.clearScanResult();

    QCOMPARE(controller.scannedName(), QString());
    QCOMPARE(controller.scannedFingerprint(), QString());
    QCOMPARE(controller.scannedPublicKey(), QString());
    QCOMPARE(controller.lastError(), QString());
}

QTEST_GUILESS_MAIN(PgpQrControllerTest)
#include "PgpQrControllerTest.moc"
