#include "push/PushPayloadParser.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

namespace PushPayloadParser {

namespace {

// Same split-on-comma convention as core/net/PushNotificationClient.cpp's
// splitKeywords -- data's Keywords wire value is comma-joined (capital K).
// Qt::SkipEmptyParts drops the empties a leading/trailing/doubled comma
// would otherwise produce ("" from "work,,urgent," -> "work", "urgent").
QStringList splitKeywords(const QString& commaJoined)
{
    if (commaJoined.isEmpty())
        return {};

    QStringList result;
    for (const QString& part : commaJoined.split(QLatin1Char(','), Qt::SkipEmptyParts))
        result.append(part.trimmed());
    return result;
}

} // namespace

std::optional<PushNotification> parse(const QByteArray& rawBody)
{
    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(rawBody, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject())
        return std::nullopt;

    // A missing "data" object yields an empty QJsonObject here (every
    // .value() below then yields an empty string, same as an explicitly
    // absent field) -- no separate "data missing" check needed.
    const QJsonObject root = doc.object();
    const QJsonObject data = root.value(QStringLiteral("data")).toObject();

    PushNotification notification;
    notification.messageId = data.value(QStringLiteral("messageId")).toString();
    notification.sender = data.value(QStringLiteral("sender")).toString();
    notification.subject = data.value(QStringLiteral("subject")).toString();
    notification.senderName = data.value(QStringLiteral("senderName")).toString();
    notification.emailSubject = data.value(QStringLiteral("emailSubject")).toString();
    notification.keywords = splitKeywords(data.value(QStringLiteral("Keywords")).toString());
    // data's own title/body copies are authoritative for a real mail push
    // -- see this file's header comment. Fall back to the outer envelope's
    // title/body for the sparser generic-test-notification shape, which
    // never nests a data.title/data.body.
    notification.title = data.value(QStringLiteral("title")).toString();
    if (notification.title.isEmpty())
        notification.title = root.value(QStringLiteral("title")).toString();
    notification.body = data.value(QStringLiteral("body")).toString();
    if (notification.body.isEmpty())
        notification.body = root.value(QStringLiteral("body")).toString();
    notification.url = data.value(QStringLiteral("url")).toString();
    if (notification.url.isEmpty())
        notification.url = root.value(QStringLiteral("url")).toString();

    // Reject only a payload with nothing displayable at all -- neither a
    // real mail identity nor any title to show (mail or generic-test
    // shape). See header comment: an accepted-but-empty-messageId result
    // is expected for the generic /api/notifications/test envelope.
    if (notification.messageId.isEmpty() && notification.title.isEmpty())
        return std::nullopt;

    return notification;
}

} // namespace PushPayloadParser
