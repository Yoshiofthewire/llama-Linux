#include "net/PgpQrClient.h"

#include "net/HttpClient.h"
#include "net/RelayAuth.h"

#include "FakeRelayServer.h"

#include <QNetworkAccessManager>
#include <QTest>

class PgpQrClientTest : public QObject
{
    Q_OBJECT

private slots:
    void fetchTokenSuccessParsesTokenExpiresAtAndUrl();
    void fetchTokenSendsAuthAsQueryParamsAndHitsApiPgpQrToken();
    void fetchToken400NoPgpIdentitySetsStatusCode();
    void fetchToken401MapsToUnauthorized();
    void fetchToken503MapsToServiceUnavailable();
    void fetchKeySuccessParsesNameFingerprintPublicKey();
    void fetchKey403MapsToUnauthorizedWithStatusCode();
    void fetchKey404NoPgpIdentitySetsStatusCode();
    void fetchKey503MapsToServiceUnavailable();
};

void PgpQrClientTest::fetchTokenSuccessParsesTokenExpiresAtAndUrl()
{
    const QByteArray body =
        R"({"token":"tok-1","expiresAt":"2026-07-17T12:02:00Z","url":"https://example.com/api/pgp/qr/key?t=tok-1"})";
    FakeRelayServer fake(httpResponse(200, "OK", body));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    PgpQrClient client(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("sub-1"), QStringLiteral("hash-1") };
    const PgpQrTokenResult result = client.fetchToken(serverBaseUrl, auth);

    QVERIFY(!result.error.has_value());
    QCOMPARE(result.statusCode, 200);
    QCOMPARE(result.token, QStringLiteral("tok-1"));
    QCOMPARE(result.expiresAt, QStringLiteral("2026-07-17T12:02:00Z"));
    QCOMPARE(result.url, QStringLiteral("https://example.com/api/pgp/qr/key?t=tok-1"));
}

void PgpQrClientTest::fetchTokenSendsAuthAsQueryParamsAndHitsApiPgpQrToken()
{
    FakeRelayServer fake(httpResponse(200, "OK", R"({"token":"t","expiresAt":"","url":""})"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    PgpQrClient client(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("sub-9"), QStringLiteral("hash-9") };
    client.fetchToken(serverBaseUrl, auth);

    const QByteArray request = fake.receivedRequest();
    QVERIFY(request.contains("GET /api/pgp/qr/token?"));
    QVERIFY(request.contains("sub=sub-9"));
    QVERIFY(request.contains("hash=hash-9"));
}

void PgpQrClientTest::fetchToken400NoPgpIdentitySetsStatusCode()
{
    FakeRelayServer fake(httpResponse(400, "Bad Request", "no pgp identity configured\n", "text/plain"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    PgpQrClient client(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("sub-1"), QStringLiteral("hash-1") };
    const PgpQrTokenResult result = client.fetchToken(serverBaseUrl, auth);

    QVERIFY(result.error.has_value());
    QCOMPARE(result.statusCode, 400);
}

void PgpQrClientTest::fetchToken401MapsToUnauthorized()
{
    FakeRelayServer fake(httpResponse(401, "Unauthorized", R"({"error":"unauthorized"})"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    PgpQrClient client(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("sub-1"), QStringLiteral("hash-1") };
    const PgpQrTokenResult result = client.fetchToken(serverBaseUrl, auth);

    QVERIFY(result.error.has_value());
    QCOMPARE(*result.error, NetworkError::Unauthorized);
    QCOMPARE(result.statusCode, 401);
}

void PgpQrClientTest::fetchToken503MapsToServiceUnavailable()
{
    FakeRelayServer fake(httpResponse(503, "Service Unavailable", "pairing is not configured\n", "text/plain"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    PgpQrClient client(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("sub-1"), QStringLiteral("hash-1") };
    const PgpQrTokenResult result = client.fetchToken(serverBaseUrl, auth);

    QVERIFY(result.error.has_value());
    QCOMPARE(*result.error, NetworkError::ServiceUnavailable);
    QCOMPARE(result.statusCode, 503);
}

void PgpQrClientTest::fetchKeySuccessParsesNameFingerprintPublicKey()
{
    const QByteArray body =
        R"({"name":"Ada","fingerprint":"ABCD1234","publicKey":"-----BEGIN PGP PUBLIC KEY BLOCK-----"})";
    FakeRelayServer fake(httpResponse(200, "OK", body));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    PgpQrClient client(http);

    const QUrl qrUrl(QStringLiteral("http://127.0.0.1:%1/api/pgp/qr/key?t=tok-1").arg(fake.port()));
    const PgpQrKeyResult result = client.fetchKey(qrUrl);

    QVERIFY(!result.error.has_value());
    QCOMPARE(result.statusCode, 200);
    QCOMPARE(result.name, QStringLiteral("Ada"));
    QCOMPARE(result.fingerprint, QStringLiteral("ABCD1234"));
    QCOMPARE(result.publicKey, QStringLiteral("-----BEGIN PGP PUBLIC KEY BLOCK-----"));

    QVERIFY(fake.receivedRequest().contains("GET /api/pgp/qr/key?t=tok-1"));
}

void PgpQrClientTest::fetchKey403MapsToUnauthorizedWithStatusCode()
{
    FakeRelayServer fake(httpResponse(403, "Forbidden", "invalid or expired token\n", "text/plain"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    PgpQrClient client(http);

    const QUrl qrUrl(QStringLiteral("http://127.0.0.1:%1/api/pgp/qr/key?t=expired").arg(fake.port()));
    const PgpQrKeyResult result = client.fetchKey(qrUrl);

    QVERIFY(result.error.has_value());
    QCOMPARE(*result.error, NetworkError::Unauthorized);
    QCOMPARE(result.statusCode, 403);
}

void PgpQrClientTest::fetchKey404NoPgpIdentitySetsStatusCode()
{
    FakeRelayServer fake(httpResponse(404, "Not Found", "no pgp identity configured\n", "text/plain"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    PgpQrClient client(http);

    const QUrl qrUrl(QStringLiteral("http://127.0.0.1:%1/api/pgp/qr/key?t=tok-1").arg(fake.port()));
    const PgpQrKeyResult result = client.fetchKey(qrUrl);

    QVERIFY(result.error.has_value());
    QCOMPARE(result.statusCode, 404);
}

void PgpQrClientTest::fetchKey503MapsToServiceUnavailable()
{
    FakeRelayServer fake(httpResponse(503, "Service Unavailable", "pairing is not configured\n", "text/plain"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    PgpQrClient client(http);

    const QUrl qrUrl(QStringLiteral("http://127.0.0.1:%1/api/pgp/qr/key?t=tok-1").arg(fake.port()));
    const PgpQrKeyResult result = client.fetchKey(qrUrl);

    QVERIFY(result.error.has_value());
    QCOMPARE(*result.error, NetworkError::ServiceUnavailable);
    QCOMPARE(result.statusCode, 503);
}

QTEST_GUILESS_MAIN(PgpQrClientTest)
#include "PgpQrClientTest.moc"
