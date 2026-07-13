#include "net/NativeRegistrationClient.h"

#include "net/HttpClient.h"

#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>

namespace {

QByteArray httpResponse(int statusCode, const QByteArray& statusText, const QByteArray& body)
{
    QByteArray response = "HTTP/1.1 " + QByteArray::number(statusCode) + " " + statusText + "\r\n";
    response += "Content-Type: application/json\r\n";
    response += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
    response += "Connection: close\r\n";
    response += "\r\n";
    response += body;
    return response;
}

// Same real-QTcpServer harness as Task 13's HttpClientTest (FakeRelayServer)
// — accepts one connection, captures the raw request, replies with a canned
// response once the request (headers + any Content-Length body) is fully
// received.
class FakeRelayServer : public QObject
{
public:
    explicit FakeRelayServer(QByteArray response)
        : m_response(std::move(response))
    {
        m_server.listen(QHostAddress::LocalHost);
        connect(&m_server, &QTcpServer::newConnection, this, &FakeRelayServer::onNewConnection);
    }

    quint16 port() const { return m_server.serverPort(); }
    const QByteArray& receivedRequest() const { return m_received; }

    // Parses the JSON body out of the captured raw request.
    QJsonObject receivedJsonBody() const
    {
        const int headerEnd = m_received.indexOf("\r\n\r\n");
        if (headerEnd < 0)
            return {};
        const QByteArray body = m_received.mid(headerEnd + 4);
        return QJsonDocument::fromJson(body).object();
    }

private:
    void onNewConnection()
    {
        QTcpSocket* socket = m_server.nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
            m_received += socket->readAll();
            if (!requestComplete())
                return;
            socket->write(m_response);
            socket->flush();
            socket->disconnectFromHost();
        });
    }

    bool requestComplete() const
    {
        const int headerEnd = m_received.indexOf("\r\n\r\n");
        if (headerEnd < 0)
            return false;
        const QByteArray headers = m_received.left(headerEnd);
        const int idx = headers.indexOf("Content-Length:");
        if (idx < 0)
            return true;
        int lineEnd = headers.indexOf("\r\n", idx);
        if (lineEnd < 0)
            lineEnd = headers.size();
        bool ok = false;
        const int contentLength = headers.mid(idx + 15, lineEnd - idx - 15).trimmed().toInt(&ok);
        if (!ok)
            return true;
        return m_received.size() >= headerEnd + 4 + contentLength;
    }

    QTcpServer m_server;
    QByteArray m_response;
    QByteArray m_received;
};

} // namespace

class NativeRegistrationClientTest : public QObject
{
    Q_OBJECT

private slots:
    void successParsesResponseAndSendsExpectedBody();
    void successOmitsOptionalFieldsWhenNotProvided();
    void successDerivesPullEndpointWhenServerOmitsIt();
    void unauthorizedFrom401();
    void backendMisconfiguredFrom503();
    void noQueryParamsOnRequestUrl();
};

void NativeRegistrationClientTest::successParsesResponseAndSendsExpectedBody()
{
    const QByteArray body = R"({"ok":true,"synced":true,"deviceId":"dev-1","devices":2,)"
                             R"("deliveryMode":"pull","pullEndpoint":"http://relay.example/api/notifications/native/pull",)"
                             R"("transport":"unifiedpush"})";
    FakeRelayServer fake(httpResponse(200, "OK", body));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    NativeRegistrationClient client(http);

    const QUrl endpoint(QStringLiteral("http://127.0.0.1:%1/api/notifications/native/register").arg(fake.port()));
    const NativeRegistrationResult result =
        client.registerDevice(endpoint, QStringLiteral("sub-1"), QStringLiteral("hash-1"), QStringLiteral("pair-tok"),
                               QStringLiteral("https://push.example/endpoint"), QStringLiteral("device-1"),
                               QStringLiteral("My Desktop"));

    QCOMPARE(result.outcome, RegistrationOutcome::Success);
    QVERIFY(result.response.ok);
    QVERIFY(result.response.synced);
    QCOMPARE(result.response.deviceId, QStringLiteral("dev-1"));
    QCOMPARE(result.response.devices, 2);
    QCOMPARE(result.response.deliveryMode, QStringLiteral("pull"));
    QCOMPARE(result.response.pullEndpoint, QStringLiteral("http://relay.example/api/notifications/native/pull"));
    QCOMPARE(result.response.transport, QStringLiteral("unifiedpush"));

    const QJsonObject sent = fake.receivedJsonBody();
    QCOMPARE(sent.value(QStringLiteral("subscriberId")).toString(), QStringLiteral("sub-1"));
    QCOMPARE(sent.value(QStringLiteral("subscriberHash")).toString(), QStringLiteral("hash-1"));
    QCOMPARE(sent.value(QStringLiteral("pairingToken")).toString(), QStringLiteral("pair-tok"));
    QCOMPARE(sent.value(QStringLiteral("deviceToken")).toString(), QStringLiteral("https://push.example/endpoint"));
    QCOMPARE(sent.value(QStringLiteral("deviceId")).toString(), QStringLiteral("device-1"));
    QCOMPARE(sent.value(QStringLiteral("deviceName")).toString(), QStringLiteral("My Desktop"));
    QCOMPARE(sent.value(QStringLiteral("platform")).toString(), QStringLiteral("linux"));
    QCOMPARE(sent.value(QStringLiteral("transport")).toString(), QStringLiteral("unifiedpush"));

    QVERIFY(fake.receivedRequest().contains("Content-Type: application/json"));
}

void NativeRegistrationClientTest::successOmitsOptionalFieldsWhenNotProvided()
{
    const QByteArray body = R"({"ok":true,"synced":false,"deviceId":"dev-2","devices":1,)"
                             R"("deliveryMode":"push","pullEndpoint":"","transport":"unifiedpush"})";
    FakeRelayServer fake(httpResponse(200, "OK", body));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    NativeRegistrationClient client(http);

    const QUrl endpoint(QStringLiteral("http://127.0.0.1:%1/api/notifications/native/register").arg(fake.port()));
    const NativeRegistrationResult result =
        client.registerDevice(endpoint, QStringLiteral("sub-1"), std::nullopt, QStringLiteral("pair-tok"),
                               QStringLiteral("https://push.example/endpoint"), QString(), QString());

    QCOMPARE(result.outcome, RegistrationOutcome::Success);

    const QJsonObject sent = fake.receivedJsonBody();
    QVERIFY(!sent.contains(QStringLiteral("subscriberHash")));
    QVERIFY(!sent.contains(QStringLiteral("deviceId")));
    QVERIFY(!sent.contains(QStringLiteral("deviceName")));
    QVERIFY(!sent.contains(QStringLiteral("appVersion")));
}

void NativeRegistrationClientTest::successDerivesPullEndpointWhenServerOmitsIt()
{
    const QByteArray body = R"({"ok":true,"synced":true,"deviceId":"dev-3","devices":1,)"
                             R"("deliveryMode":"pull","pullEndpoint":"","transport":"unifiedpush"})";
    FakeRelayServer fake(httpResponse(200, "OK", body));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    NativeRegistrationClient client(http);

    const QUrl endpoint(QStringLiteral("http://127.0.0.1:%1/api/notifications/native/register").arg(fake.port()));
    const NativeRegistrationResult result =
        client.registerDevice(endpoint, QStringLiteral("sub-1"), std::nullopt, QStringLiteral("pair-tok"),
                               QStringLiteral("https://push.example/endpoint"), QString(), QString());

    QCOMPARE(result.outcome, RegistrationOutcome::Success);
    QCOMPARE(result.response.pullEndpoint,
             QStringLiteral("http://127.0.0.1:%1/api/notifications/native/pull").arg(fake.port()));
}

void NativeRegistrationClientTest::unauthorizedFrom401()
{
    FakeRelayServer fake(httpResponse(401, "Unauthorized", "{}"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    NativeRegistrationClient client(http);

    const QUrl endpoint(QStringLiteral("http://127.0.0.1:%1/api/notifications/native/register").arg(fake.port()));
    const NativeRegistrationResult result =
        client.registerDevice(endpoint, QStringLiteral("sub-1"), std::nullopt, QStringLiteral("pair-tok"),
                               QStringLiteral("https://push.example/endpoint"), QString(), QString());

    QCOMPARE(result.outcome, RegistrationOutcome::Unauthorized);
}

void NativeRegistrationClientTest::backendMisconfiguredFrom503()
{
    FakeRelayServer fake(httpResponse(503, "Service Unavailable", "{}"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    NativeRegistrationClient client(http);

    const QUrl endpoint(QStringLiteral("http://127.0.0.1:%1/api/notifications/native/register").arg(fake.port()));
    const NativeRegistrationResult result =
        client.registerDevice(endpoint, QStringLiteral("sub-1"), std::nullopt, QStringLiteral("pair-tok"),
                               QStringLiteral("https://push.example/endpoint"), QString(), QString());

    QCOMPARE(result.outcome, RegistrationOutcome::BackendMisconfigured);
}

void NativeRegistrationClientTest::noQueryParamsOnRequestUrl()
{
    FakeRelayServer fake(httpResponse(200, "OK", R"({"ok":true})"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    NativeRegistrationClient client(http);

    const QUrl endpoint(QStringLiteral("http://127.0.0.1:%1/api/notifications/native/register").arg(fake.port()));
    client.registerDevice(endpoint, QStringLiteral("sub-1"), std::nullopt, QStringLiteral("pair-tok"),
                           QStringLiteral("https://push.example/endpoint"), QString(), QString());

    // Unlike every other Relay endpoint in this batch, native/register takes
    // no query-param auth — sub/hash must not be appended to the URL.
    QVERIFY(fake.receivedRequest().contains("POST /api/notifications/native/register HTTP/1.1"));
    QVERIFY(!fake.receivedRequest().contains("?"));
}

QTEST_GUILESS_MAIN(NativeRegistrationClientTest)
#include "NativeRegistrationClientTest.moc"
