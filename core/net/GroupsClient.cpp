#include "net/GroupsClient.h"

#include "net/HttpClient.h"
#include "net/RelayAuth.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

namespace {

Group groupFromJson(const QJsonObject& obj)
{
    Group group;
    group.id = obj.value(QStringLiteral("id")).toString();
    group.name = obj.value(QStringLiteral("name")).toString();
    group.rev = static_cast<qint64>(obj.value(QStringLiteral("rev")).toDouble());
    return group;
}

} // namespace

GroupsClient::GroupsClient(HttpClient& httpClient)
    : m_httpClient(httpClient)
{
}

GroupsFetchResult GroupsClient::fetch(const QUrl& serverBaseUrl, const RelayAuth& auth) const
{
    const HttpClient::HttpResult result =
        m_httpClient.get(joinUrlPath(serverBaseUrl, QStringLiteral("api/groups")), auth.queryItems());

    GroupsFetchResult out;

    // Covers 401/403/5xx/transport failures alike -- HttpClient::get()
    // already maps the status code to NetworkError, so no separate
    // Unauthorized-only branch is needed here; every non-2xx path lands here
    // and returns an empty groups list rather than throwing/crashing.
    if (result.error.has_value()) {
        out.error = result.error;
        out.detail = !result.detail.isEmpty()
            ? result.detail
            : QStringLiteral("Groups fetch failed with status %1").arg(result.statusCode);
        return out;
    }

    QString errorString;
    const std::optional<QJsonArray> array = decodeJsonArray(result.body, &errorString);
    if (!array.has_value()) {
        out.error = NetworkError::Decoding;
        out.detail = QStringLiteral("Failed to decode groups response: %1").arg(errorString);
        return out;
    }

    out.groups.reserve(array->size());
    for (const QJsonValue& value : *array)
        out.groups.append(groupFromJson(value.toObject()));

    return out;
}
