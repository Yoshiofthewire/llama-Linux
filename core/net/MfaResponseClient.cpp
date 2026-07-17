#include "net/MfaResponseClient.h"

#include "net/HttpClient.h"

#include <QJsonDocument>
#include <QJsonObject>

MfaResponseClient::MfaResponseClient(HttpClient& httpClient)
    : m_httpClient(httpClient)
{
}

MfaResponseResult MfaResponseClient::respond(const QUrl& serverBaseUrl, const QString& challengeId,
                                              const QString& subscriberId, const QString& subscriberHash,
                                              const QString& deviceId, bool approve) const
{
    QJsonObject body;
    body[QStringLiteral("challengeId")] = challengeId;
    body[QStringLiteral("subscriberId")] = subscriberId;
    body[QStringLiteral("subscriberHash")] = subscriberHash;
    body[QStringLiteral("deviceId")] = deviceId;
    body[QStringLiteral("approve")] = approve;

    // No query-param auth on this endpoint, unlike every other Relay
    // endpoint in this batch — every credential rides in the JSON body.
    const HttpClient::HttpResult result =
        m_httpClient.post(joinUrlPath(serverBaseUrl, QStringLiteral("api/mfa/push/respond")), {}, body);

    MfaResponseResult out;

    if (result.error.has_value()) {
        switch (*result.error) {
        case NetworkError::Unauthorized:
            out.outcome = MfaResponseOutcome::Rejected;
            return out;
        case NetworkError::Conflict: {
            // 409 — challenge already resolved. status is optional here per
            // the brief; surface it when the server included it.
            out.outcome = MfaResponseOutcome::Rejected;
            const QJsonDocument doc = QJsonDocument::fromJson(result.body);
            if (doc.isObject()) {
                const QString status = doc.object().value(QStringLiteral("status")).toString();
                if (!status.isEmpty())
                    out.status = status;
            }
            return out;
        }
        default:
            out.outcome = MfaResponseOutcome::Failure;
            out.detail = !result.detail.isEmpty()
                ? result.detail
                : QStringLiteral("MFA response request failed with status %1").arg(result.statusCode);
            return out;
        }
    }

    QString errorString;
    const std::optional<QJsonObject> json = decodeJsonObject(result.body, &errorString);
    if (!json.has_value()) {
        out.outcome = MfaResponseOutcome::Failure;
        out.detail = QStringLiteral("Failed to decode MFA response: %1").arg(errorString);
        return out;
    }

    out.outcome = MfaResponseOutcome::Success;
    out.status = json->value(QStringLiteral("status")).toString();
    return out;
}
