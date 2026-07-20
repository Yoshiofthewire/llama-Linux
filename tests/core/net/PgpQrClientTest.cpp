#include "net/PgpQrClient.h"

#include "models/Contact.h"
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
    void fetchTokenSendsAuthAsHeadersAndHitsApiPgpQrToken();
    void fetchToken400NoPgpIdentitySetsStatusCode();
    void fetchToken401MapsToUnauthorized();
    void fetchToken503MapsToServiceUnavailable();
    void fetchKeySuccessParsesNameFingerprintPublicKey();
    void fetchKeySuccessParsesContactCardWhenPresent();
    void fetchKeySuccessOmitsContactCardWhenAbsent();
    void fetchKey403MapsToUnauthorizedWithStatusCode();
    void fetchKey404NoPgpIdentitySetsStatusCode();
    void fetchKey503MapsToServiceUnavailable();
    void fetchKeyPassesRedirectValidatorThroughToHttpClient();
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
    const RelayAuth auth{ QStringLiteral("device-1"), QStringLiteral("secret-1") };
    const PgpQrTokenResult result = client.fetchToken(serverBaseUrl, auth);

    QVERIFY(!result.error.has_value());
    QCOMPARE(result.statusCode, 200);
    QCOMPARE(result.token, QStringLiteral("tok-1"));
    QCOMPARE(result.expiresAt, QStringLiteral("2026-07-17T12:02:00Z"));
    QCOMPARE(result.url, QStringLiteral("https://example.com/api/pgp/qr/key?t=tok-1"));
}

void PgpQrClientTest::fetchTokenSendsAuthAsHeadersAndHitsApiPgpQrToken()
{
    FakeRelayServer fake(httpResponse(200, "OK", R"({"token":"t","expiresAt":"","url":""})"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    PgpQrClient client(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("device-9"), QStringLiteral("secret-9") };
    client.fetchToken(serverBaseUrl, auth);

    const QByteArray request = fake.receivedRequest();
    QVERIFY(request.contains("GET /api/pgp/qr/token HTTP/1.1"));
    QVERIFY(request.contains("X-Kypost-Device-Id: device-9"));
    QVERIFY(request.contains("X-Kypost-Device-Secret: secret-9"));
    QVERIFY(!request.contains("device=device-9"));
    QVERIFY(!request.contains("secret=secret-9"));
}

void PgpQrClientTest::fetchToken400NoPgpIdentitySetsStatusCode()
{
    FakeRelayServer fake(httpResponse(400, "Bad Request", "no pgp identity configured\n", "text/plain"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    PgpQrClient client(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("device-1"), QStringLiteral("secret-1") };
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
    const RelayAuth auth{ QStringLiteral("device-1"), QStringLiteral("secret-1") };
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
    const RelayAuth auth{ QStringLiteral("device-1"), QStringLiteral("secret-1") };
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

void PgpQrClientTest::fetchKeySuccessParsesContactCardWhenPresent()
{
    const QByteArray body = R"({
        "name":"Ada","fingerprint":"ABCD1234","publicKey":"-----BEGIN PGP PUBLIC KEY BLOCK-----",
        "contactCard":{
            "fn":"Ada Lovelace","org":"Analytical Engines Ltd","notes":"Pioneer of computing",
            "emails":[{"label":"work","value":"ada@example.com"}],
            "phones":[{"label":"mobile","value":"+1-555-0100"}],
            "department":"Engineering","pronouns":"she/her"
        }
    })";
    FakeRelayServer fake(httpResponse(200, "OK", body));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    PgpQrClient client(http);

    const QUrl qrUrl(QStringLiteral("http://127.0.0.1:%1/api/pgp/qr/key?t=tok-1").arg(fake.port()));
    const PgpQrKeyResult result = client.fetchKey(qrUrl);

    QVERIFY(!result.error.has_value());
    QVERIFY(result.contactCard.has_value());
    QCOMPARE(result.contactCard->fn, std::optional<QString>(QStringLiteral("Ada Lovelace")));
    QCOMPARE(result.contactCard->org, std::optional<QString>(QStringLiteral("Analytical Engines Ltd")));
    QCOMPARE(result.contactCard->emails.size(), 1);
    QCOMPARE(result.contactCard->emails.first().value, QStringLiteral("ada@example.com"));
    QCOMPARE(result.contactCard->phones.first().value, QStringLiteral("+1-555-0100"));
    QCOMPARE(result.contactCard->department, std::optional<QString>(QStringLiteral("Engineering")));
    QCOMPARE(result.contactCard->pronouns, std::optional<QString>(QStringLiteral("she/her")));
}

void PgpQrClientTest::fetchKeySuccessOmitsContactCardWhenAbsent()
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
    QVERIFY(!result.contactCard.has_value());
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

void PgpQrClientTest::fetchKeyPassesRedirectValidatorThroughToHttpClient()
{
    // VibeSec regression guard: fetchKey must let the caller re-validate
    // any redirect target (see HttpClient::RedirectValidator's doc
    // comment) -- a URL that legitimately passes an initial safety check
    // could otherwise still redirect the actual request somewhere that
    // check would have rejected.
    const QByteArray finalBody =
        R"({"name":"Ada","fingerprint":"ABCD1234","publicKey":"-----BEGIN PGP PUBLIC KEY BLOCK-----"})";
    FakeRelayServer finalServer(httpResponse(200, "OK", finalBody));

    const QByteArray location =
        QStringLiteral("http://127.0.0.1:%1/api/pgp/qr/key?t=tok-1").arg(finalServer.port()).toUtf8();
    FakeRelayServer redirectingServer(
        httpResponse(302, "Found", "", "text/plain", { { "Location", location } }));

    QNetworkAccessManager manager;
    HttpClient http(manager);
    PgpQrClient client(http);

    QStringList validated;
    const HttpClient::RedirectValidator rejectAll = [&validated](const QUrl& target) {
        validated.append(target.toString());
        return false;
    };

    const QUrl qrUrl(QStringLiteral("http://127.0.0.1:%1/api/pgp/qr/key?t=tok-1").arg(redirectingServer.port()));
    const PgpQrKeyResult result = client.fetchKey(qrUrl, rejectAll);

    QVERIFY(result.error.has_value());
    QCOMPARE(validated.size(), 1);
    QVERIFY(validated.first().contains(QStringLiteral("/api/pgp/qr/key?t=tok-1")));
}

QTEST_GUILESS_MAIN(PgpQrClientTest)
#include "PgpQrClientTest.moc"
