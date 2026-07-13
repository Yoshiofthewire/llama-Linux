#include "net/PushNotificationClient.h"

#include "net/HttpClient.h"
#include "net/RelayAuth.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

namespace {

// data's values are all strings on the wire (map[string]string in Go); a
// missing key maps to an empty QString, matching PushNotification's
// non-optional QString/QStringList fields.
QStringList splitKeywords(const QString& commaJoined)
{
    if (commaJoined.isEmpty())
        return {};

    QStringList result;
    for (const QString& part : commaJoined.split(QLatin1Char(','), Qt::SkipEmptyParts))
        result.append(part.trimmed());
    return result;
}

PushNotification notificationFromJson(const QJsonObject& item)
{
    const QJsonObject data = item.value(QStringLiteral("data")).toObject();

    PushNotification notification;
    notification.messageId = data.value(QStringLiteral("messageId")).toString();
    notification.sender = data.value(QStringLiteral("sender")).toString();
    notification.subject = data.value(QStringLiteral("subject")).toString();
    notification.senderName = data.value(QStringLiteral("senderName")).toString();
    notification.emailSubject = data.value(QStringLiteral("emailSubject")).toString();
    notification.keywords = splitKeywords(data.value(QStringLiteral("Keywords")).toString());
    notification.url = data.value(QStringLiteral("url")).toString();
    // title/body come from the item's top-level fields, not data — data also
    // carries its own title/body copies (buildNativePushData) but the
    // top-level ones are authoritative for what to display.
    notification.title = item.value(QStringLiteral("title")).toString();
    notification.body = item.value(QStringLiteral("body")).toString();
    return notification;
}

} // namespace

PushNotificationClient::PushNotificationClient(HttpClient& httpClient)
    : m_httpClient(httpClient)
{
}

PullResult PushNotificationClient::pull(const QUrl& pullEndpoint, const RelayAuth& auth, qint64 afterCursor) const
{
    QList<QPair<QString, QString>> query = auth.queryItems();
    if (afterCursor > 0)
        query.append({ QStringLiteral("after"), QString::number(afterCursor) });

    const HttpClient::HttpResult result = m_httpClient.get(pullEndpoint, query);

    PullResult out;

    if (result.error.has_value()) {
        out.error = result.error;
        out.detail = !result.detail.isEmpty()
            ? result.detail
            : QStringLiteral("Pull request failed with status %1").arg(result.statusCode);
        return out;
    }

    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(result.body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        out.error = NetworkError::Decoding;
        out.detail = QStringLiteral("Failed to decode pull response: %1").arg(parseError.errorString());
        return out;
    }

    const QJsonObject json = doc.object();
    out.deliveryMode = json.value(QStringLiteral("deliveryMode")).toString();
    out.cursor = static_cast<qint64>(json.value(QStringLiteral("cursor")).toDouble());

    const QJsonArray items = json.value(QStringLiteral("notifications")).toArray();
    out.notifications.reserve(items.size());
    for (const QJsonValue& value : items) {
        const QJsonObject item = value.toObject();
        PullNotificationItem entry;
        entry.seq = static_cast<qint64>(item.value(QStringLiteral("seq")).toDouble());
        entry.notification = notificationFromJson(item);
        out.notifications.append(entry);
    }

    return out;
}
