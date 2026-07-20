#include "push/PushPayloadParser.h"

#include <QTest>

class PushPayloadParserTest : public QObject
{
    Q_OBJECT

private slots:
    void fullValidPayloadRoundTripsEveryField();
    void completelyEmptyPayloadFailsGracefully();
    void missingMessageIdStillProducesNotificationWhenTitleIsPresent();
    void genericTestNotificationShapeFallsBackToOuterTitleAndBody();
    void emptyKeywordsStringProducesEmptyStringList();
    void malformedKeywordsWithExtraCommasDropsEmpties();
    void malformedJsonFailsGracefully();
};

void PushPayloadParserTest::fullValidPayloadRoundTripsEveryField()
{
    const QByteArray body = R"({"title":"outer-title","body":"outer-body",)"
                             R"("data":{"messageId":"msg-1","sender":"a@example.com","subject":"Hello",)"
                             R"("senderName":"Alice","emailSubject":"Hello there","Keywords":"work,urgent",)"
                             R"("title":"Alice","body":"Hello there","url":"/read"}})";

    const std::optional<PushNotification> result = PushPayloadParser::parse(body);

    QVERIFY(result.has_value());
    QCOMPARE(result->messageId, QStringLiteral("msg-1"));
    QCOMPARE(result->sender, QStringLiteral("a@example.com"));
    QCOMPARE(result->subject, QStringLiteral("Hello"));
    QCOMPARE(result->senderName, QStringLiteral("Alice"));
    QCOMPARE(result->emailSubject, QStringLiteral("Hello there"));
    QCOMPARE(result->keywords, QStringList({ QStringLiteral("work"), QStringLiteral("urgent") }));
    // title/body must come from data's copies, not the outer envelope's.
    QCOMPARE(result->title, QStringLiteral("Alice"));
    QCOMPARE(result->body, QStringLiteral("Hello there"));
    QCOMPARE(result->url, QStringLiteral("/read"));
}

void PushPayloadParserTest::completelyEmptyPayloadFailsGracefully()
{
    // No data object, no outer title/body -- nothing displayable anywhere,
    // the one case that must still be rejected.
    const QByteArray body = R"({})";

    const std::optional<PushNotification> result = PushPayloadParser::parse(body);

    QVERIFY(!result.has_value());
}

void PushPayloadParserTest::missingMessageIdStillProducesNotificationWhenTitleIsPresent()
{
    // No mail identity, but data.title is present -- a real mail push
    // always carries messageId, so this shape is hypothetical, but there's
    // no reason to drop something displayable just because messageId is
    // absent (see header comment: this parser only rejects payloads with
    // nothing displayable at all).
    const QByteArray body = R"({"title":"outer-title","body":"outer-body",)"
                             R"("data":{"sender":"a@example.com","subject":"Hello",)"
                             R"("senderName":"Alice","emailSubject":"Hello there","Keywords":"work,urgent",)"
                             R"("title":"Alice","body":"Hello there","url":"/read"}})";

    const std::optional<PushNotification> result = PushPayloadParser::parse(body);

    QVERIFY(result.has_value());
    QVERIFY(result->messageId.isEmpty());
    // data's own title/body still win over the outer envelope's when both
    // are present, same rule as the full-payload round-trip test above.
    QCOMPARE(result->title, QStringLiteral("Alice"));
    QCOMPARE(result->body, QStringLiteral("Hello there"));
}

void PushPayloadParserTest::genericTestNotificationShapeFallsBackToOuterTitleAndBody()
{
    // The real shape sent by the backend's POST /api/notifications/test
    // (server.go's handleNotificationTest, "Send Test Notification" on the
    // web app): only an outer title/body and a bare data.url, no
    // data.messageId, no data.title/data.body at all.
    const QByteArray body = R"({"title":"KyPost Test Notification",)"
                             R"("body":"Push delivery is working across all subscribed devices.",)"
                             R"("data":{"url":"/notifications"}})";

    const std::optional<PushNotification> result = PushPayloadParser::parse(body);

    QVERIFY(result.has_value());
    QVERIFY(result->messageId.isEmpty());
    QCOMPARE(result->title, QStringLiteral("KyPost Test Notification"));
    QCOMPARE(result->body, QStringLiteral("Push delivery is working across all subscribed devices."));
    QCOMPARE(result->url, QStringLiteral("/notifications"));
}

void PushPayloadParserTest::emptyKeywordsStringProducesEmptyStringList()
{
    const QByteArray body = R"({"data":{"messageId":"msg-1","Keywords":""}})";

    const std::optional<PushNotification> result = PushPayloadParser::parse(body);

    QVERIFY(result.has_value());
    QVERIFY(result->keywords.isEmpty());
}

void PushPayloadParserTest::malformedKeywordsWithExtraCommasDropsEmpties()
{
    const QByteArray body = R"({"data":{"messageId":"msg-1","Keywords":"work,,urgent,"}})";

    const std::optional<PushNotification> result = PushPayloadParser::parse(body);

    QVERIFY(result.has_value());
    QCOMPARE(result->keywords, QStringList({ QStringLiteral("work"), QStringLiteral("urgent") }));
}

void PushPayloadParserTest::malformedJsonFailsGracefully()
{
    const QByteArray body = "{not valid json";

    const std::optional<PushNotification> result = PushPayloadParser::parse(body);

    QVERIFY(!result.has_value());
}

QTEST_GUILESS_MAIN(PushPayloadParserTest)
#include "PushPayloadParserTest.moc"
