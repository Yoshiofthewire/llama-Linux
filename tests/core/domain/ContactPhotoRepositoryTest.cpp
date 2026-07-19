#include "domain/ContactPhotoRepository.h"

#include "domain/DevicePairing.h"
#include "domain/PairingStore.h"
#include "net/ContactPhotoClient.h"
#include "net/HttpClient.h"
#include "stores/ContactPhotoCache.h"
#include "stores/SecureStoreFile.h"

#include "../net/FakeRelayServer.h"

#include <QFile>
#include <QNetworkAccessManager>
#include <QTemporaryDir>
#include <QTest>

class ContactPhotoRepositoryTest : public QObject
{
    Q_OBJECT

private slots:
    void photoPathForOnEmptyPhotoRefReturnsEmptyWithoutNetworkCall();
    void photoPathForWithoutPairingReturnsEmpty();
    void photoPathForFetchesAndCachesOnCacheMiss();
    void photoPathForOnSecondCallReturnsCachedPathWithoutRefetching();
    void photoPathForOnFetchErrorReturnsEmpty();

private:
    static void savePairing(PairingStore& pairingStore, quint16 port);
};

void ContactPhotoRepositoryTest::savePairing(PairingStore& pairingStore, quint16 port)
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

void ContactPhotoRepositoryTest::photoPathForOnEmptyPhotoRefReturnsEmptyWithoutNetworkCall()
{
    QTemporaryDir secureDir;
    QVERIFY(secureDir.isValid());
    SecureStoreFile secureStore(secureDir.path());
    PairingStore pairingStore(secureStore); // never saved -- not paired either, doesn't matter here

    QTemporaryDir cacheDir;
    QVERIFY(cacheDir.isValid());
    ContactPhotoCache cache(cacheDir.path());

    QNetworkAccessManager manager;
    HttpClient http(manager);
    ContactPhotoClient client(http);

    ContactPhotoRepository repository(client, cache, pairingStore);

    // No FakeRelayServer at all -- an empty photoRef must short-circuit
    // before any network call is attempted.
    QVERIFY(repository.photoPathFor(QStringLiteral("contact-1"), QString()).isEmpty());
}

void ContactPhotoRepositoryTest::photoPathForWithoutPairingReturnsEmpty()
{
    QTemporaryDir secureDir;
    QVERIFY(secureDir.isValid());
    SecureStoreFile secureStore(secureDir.path());
    PairingStore pairingStore(secureStore); // never saved -- not paired

    QTemporaryDir cacheDir;
    QVERIFY(cacheDir.isValid());
    ContactPhotoCache cache(cacheDir.path());

    QNetworkAccessManager manager;
    HttpClient http(manager);
    ContactPhotoClient client(http);

    ContactPhotoRepository repository(client, cache, pairingStore);

    QVERIFY(repository.photoPathFor(QStringLiteral("contact-1"), QStringLiteral("photo-ref-1")).isEmpty());
}

void ContactPhotoRepositoryTest::photoPathForFetchesAndCachesOnCacheMiss()
{
    const QByteArray photoBytes = QByteArrayLiteral("some-jpeg-bytes");
    FakeRelayServer fake(httpResponse(200, "OK", photoBytes, "image/jpeg"));

    QTemporaryDir secureDir;
    QVERIFY(secureDir.isValid());
    SecureStoreFile secureStore(secureDir.path());
    PairingStore pairingStore(secureStore);
    savePairing(pairingStore, fake.port());

    QTemporaryDir cacheDir;
    QVERIFY(cacheDir.isValid());
    ContactPhotoCache cache(cacheDir.path());

    QNetworkAccessManager manager;
    HttpClient http(manager);
    ContactPhotoClient client(http);

    ContactPhotoRepository repository(client, cache, pairingStore);

    const QString path = repository.photoPathFor(QStringLiteral("contact-1"), QStringLiteral("photo-ref-1"));
    QVERIFY(!path.isEmpty());

    QVERIFY(fake.receivedRequest().contains("GET /api/contacts/contact-1/photo HTTP/1.1"));

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), photoBytes);

    // Also actually landed in the cache, independently of the return value.
    QCOMPARE(cache.cachedPathFor(QStringLiteral("photo-ref-1")), path);
}

void ContactPhotoRepositoryTest::photoPathForOnSecondCallReturnsCachedPathWithoutRefetching()
{
    // FakeRelayServer (see tests/core/net/FakeRelayServer.h) only accepts
    // one connection -- a second network call here would fail/hang, so this
    // test doubles as proof that the second photoPathFor() call is served
    // entirely from the disk cache.
    const QByteArray photoBytes = QByteArrayLiteral("some-jpeg-bytes");
    FakeRelayServer fake(httpResponse(200, "OK", photoBytes, "image/jpeg"));

    QTemporaryDir secureDir;
    QVERIFY(secureDir.isValid());
    SecureStoreFile secureStore(secureDir.path());
    PairingStore pairingStore(secureStore);
    savePairing(pairingStore, fake.port());

    QTemporaryDir cacheDir;
    QVERIFY(cacheDir.isValid());
    ContactPhotoCache cache(cacheDir.path());

    QNetworkAccessManager manager;
    HttpClient http(manager);
    ContactPhotoClient client(http);

    ContactPhotoRepository repository(client, cache, pairingStore);

    const QString firstPath = repository.photoPathFor(QStringLiteral("contact-1"), QStringLiteral("photo-ref-1"));
    QVERIFY(!firstPath.isEmpty());

    const QString secondPath = repository.photoPathFor(QStringLiteral("contact-1"), QStringLiteral("photo-ref-1"));
    QCOMPARE(secondPath, firstPath);
}

void ContactPhotoRepositoryTest::photoPathForOnFetchErrorReturnsEmpty()
{
    // 401 -- ContactPhotoClient degrades gracefully to an error result;
    // ContactPhotoRepository::photoPathFor() must then return "" rather
    // than crash or cache a bogus empty file (see this feature's Global
    // Constraints).
    FakeRelayServer fake(httpResponse(401, "Unauthorized", "Unauthorized\n"));

    QTemporaryDir secureDir;
    QVERIFY(secureDir.isValid());
    SecureStoreFile secureStore(secureDir.path());
    PairingStore pairingStore(secureStore);
    savePairing(pairingStore, fake.port());

    QTemporaryDir cacheDir;
    QVERIFY(cacheDir.isValid());
    ContactPhotoCache cache(cacheDir.path());

    QNetworkAccessManager manager;
    HttpClient http(manager);
    ContactPhotoClient client(http);

    ContactPhotoRepository repository(client, cache, pairingStore);

    QVERIFY(repository.photoPathFor(QStringLiteral("contact-1"), QStringLiteral("photo-ref-1")).isEmpty());
    QVERIFY(cache.cachedPathFor(QStringLiteral("photo-ref-1")).isEmpty());
}

QTEST_GUILESS_MAIN(ContactPhotoRepositoryTest)
#include "ContactPhotoRepositoryTest.moc"
