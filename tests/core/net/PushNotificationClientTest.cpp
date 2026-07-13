#include "net/PushNotificationClient.h"

#include "net/HttpClient.h"
#include "net/RelayAuth.h"

#include <QHostAddress>
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

// Same real-QTcpServer harness as Task 13's HttpClientTest (FakeRelayServer).
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

class PushNotificationClientTest : public QObject
{
    Q_OBJECT

private slots:
    void successMapsItemsAndSplitsMultiValueKeywords();
    void firstPullOmitsAfterQueryParam();
    void subsequentPullSendsAfterQueryParam();
    void successWithZeroNotificationsIsDistinctFromFailure();
    void unauthorizedFrom401PassesErrorThrough();
};

void PushNotificationClientTest::successMapsItemsAndSplitsMultiValueKeywords()
{
    const QByteArray body = R"({"deliveryMode":"pull","cursor":42,"notifications":[)"
                             R"({"seq":1,"title":"Alice","body":"Hello there","createdAt":"2026-07-01T00:00:00Z",)"
                             R"("data":{"messageId":"msg-1","sender":"a@example.com","subject":"Hello",)"
                             R"("senderName":"Alice","emailSubject":"Hello there",)"
                             R"("Keywords":"work,urgent,follow-up","title":"Alice","body":"Hello there","url":"/read"}},)"
                             R"({"seq":2,"title":"Bob","body":"Reminder","createdAt":"2026-07-02T00:00:00Z",)"
                             R"("data":{"messageId":"msg-2","sender":"b@example.com","subject":"Re: Hello",)"
                             R"("senderName":"Bob","emailSubject":"Re: Hello","Keywords":"personal","title":"Bob",)"
                             R"("body":"Reminder","url":"/read2"}}]})";
    FakeRelayServer fake(httpResponse(200, "OK", body));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    PushNotificationClient client(http);

    const QUrl endpoint(QStringLiteral("http://127.0.0.1:%1/api/notifications/native/pull").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("sub-1"), QStringLiteral("hash-1") };
    const PullResult result = client.pull(endpoint, auth, 0);

    QVERIFY(!result.error.has_value());
    QCOMPARE(result.deliveryMode, QStringLiteral("pull"));
    QCOMPARE(result.cursor, qint64(42));
    QCOMPARE(result.notifications.size(), 2);

    const PullNotificationItem& first = result.notifications.at(0);
    QCOMPARE(first.seq, qint64(1));
    QCOMPARE(first.notification.messageId, QStringLiteral("msg-1"));
    QCOMPARE(first.notification.sender, QStringLiteral("a@example.com"));
    QCOMPARE(first.notification.subject, QStringLiteral("Hello"));
    QCOMPARE(first.notification.senderName, QStringLiteral("Alice"));
    QCOMPARE(first.notification.emailSubject, QStringLiteral("Hello there"));
    QCOMPARE(first.notification.keywords,
             QStringList({ QStringLiteral("work"), QStringLiteral("urgent"), QStringLiteral("follow-up") }));
    QCOMPARE(first.notification.title, QStringLiteral("Alice"));
    QCOMPARE(first.notification.body, QStringLiteral("Hello there"));
    QCOMPARE(first.notification.url, QStringLiteral("/read"));

    const PullNotificationItem& second = result.notifications.at(1);
    QCOMPARE(second.seq, qint64(2));
    QCOMPARE(second.notification.keywords, QStringList({ QStringLiteral("personal") }));
}

void PushNotificationClientTest::firstPullOmitsAfterQueryParam()
{
    FakeRelayServer fake(httpResponse(200, "OK", R"({"deliveryMode":"pull","cursor":0,"notifications":[]})"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    PushNotificationClient client(http);

    const QUrl endpoint(QStringLiteral("http://127.0.0.1:%1/api/notifications/native/pull").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("sub-1"), QStringLiteral("hash-1") };
    client.pull(endpoint, auth, 0);

    const QByteArray request = fake.receivedRequest();
    QVERIFY(request.contains("sub=sub-1"));
    QVERIFY(request.contains("hash=hash-1"));
    QVERIFY(!request.contains("after="));
}

void PushNotificationClientTest::subsequentPullSendsAfterQueryParam()
{
    FakeRelayServer fake(httpResponse(200, "OK", R"({"deliveryMode":"pull","cursor":43,"notifications":[]})"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    PushNotificationClient client(http);

    const QUrl endpoint(QStringLiteral("http://127.0.0.1:%1/api/notifications/native/pull").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("sub-1"), QStringLiteral("hash-1") };
    client.pull(endpoint, auth, 42);

    QVERIFY(fake.receivedRequest().contains("after=42"));
}

void PushNotificationClientTest::successWithZeroNotificationsIsDistinctFromFailure()
{
    FakeRelayServer fake(httpResponse(200, "OK", R"({"deliveryMode":"push","cursor":5,"notifications":[]})"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    PushNotificationClient client(http);

    const QUrl endpoint(QStringLiteral("http://127.0.0.1:%1/api/notifications/native/pull").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("sub-1"), QStringLiteral("hash-1") };
    const PullResult result = client.pull(endpoint, auth, 0);

    QVERIFY(!result.error.has_value());
    QVERIFY(result.notifications.isEmpty());
    QCOMPARE(result.cursor, qint64(5));
}

void PushNotificationClientTest::unauthorizedFrom401PassesErrorThrough()
{
    FakeRelayServer fake(httpResponse(401, "Unauthorized", "{}"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    PushNotificationClient client(http);

    const QUrl endpoint(QStringLiteral("http://127.0.0.1:%1/api/notifications/native/pull").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("sub-1"), QStringLiteral("hash-1") };
    const PullResult result = client.pull(endpoint, auth, 0);

    QVERIFY(result.error.has_value());
    QCOMPARE(*result.error, NetworkError::Unauthorized);
    QVERIFY(result.notifications.isEmpty());
}

QTEST_GUILESS_MAIN(PushNotificationClientTest)
#include "PushNotificationClientTest.moc"
