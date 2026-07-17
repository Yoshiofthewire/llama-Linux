#include "net/PgpQrClient.h"

#include "net/HttpClient.h"
#include "net/RelayAuth.h"

#include <QJsonObject>

namespace {

// Appends "api/pgp/qr/token" -- same trailing-slash-safe approach as
// ContactSyncClient.cpp's endpointFor().
QUrl tokenEndpointFor(const QUrl& serverBaseUrl)
{
    QUrl url = serverBaseUrl;
    QString path = url.path();
    if (!path.endsWith(QLatin1Char('/')))
        path += QLatin1Char('/');
    path += QStringLiteral("api/pgp/qr/token");
    url.setPath(path);
    return url;
}

} // namespace

PgpQrClient::PgpQrClient(HttpClient& httpClient)
    : m_httpClient(httpClient)
{
}

PgpQrTokenResult PgpQrClient::fetchToken(const QUrl& serverBaseUrl, const RelayAuth& auth) const
{
    const HttpClient::HttpResult result = m_httpClient.get(tokenEndpointFor(serverBaseUrl), auth.queryItems());

    PgpQrTokenResult out;
    out.statusCode = result.statusCode;

    if (result.error.has_value()) {
        out.error = result.error;
        out.detail = !result.detail.isEmpty()
            ? result.detail
            : QStringLiteral("PGP QR token fetch failed with status %1").arg(result.statusCode);
        return out;
    }

    QString errorString;
    const std::optional<QJsonObject> decoded = decodeJsonObject(result.body, &errorString);
    if (!decoded.has_value()) {
        out.error = NetworkError::Decoding;
        out.detail = QStringLiteral("Failed to decode PGP QR token response: %1").arg(errorString);
        return out;
    }

    const QJsonObject obj = *decoded;
    out.token = obj.value(QStringLiteral("token")).toString();
    out.expiresAt = obj.value(QStringLiteral("expiresAt")).toString();
    out.url = obj.value(QStringLiteral("url")).toString();
    return out;
}

PgpQrKeyResult PgpQrClient::fetchKey(const QUrl& qrUrl) const
{
    const HttpClient::HttpResult result = m_httpClient.get(qrUrl, {});

    PgpQrKeyResult out;
    out.statusCode = result.statusCode;

    if (result.error.has_value()) {
        out.error = result.error;
        out.detail = !result.detail.isEmpty()
            ? result.detail
            : QStringLiteral("PGP QR key fetch failed with status %1").arg(result.statusCode);
        return out;
    }

    QString errorString;
    const std::optional<QJsonObject> decoded = decodeJsonObject(result.body, &errorString);
    if (!decoded.has_value()) {
        out.error = NetworkError::Decoding;
        out.detail = QStringLiteral("Failed to decode PGP QR key response: %1").arg(errorString);
        return out;
    }

    const QJsonObject obj = *decoded;
    out.name = obj.value(QStringLiteral("name")).toString();
    out.fingerprint = obj.value(QStringLiteral("fingerprint")).toString();
    out.publicKey = obj.value(QStringLiteral("publicKey")).toString();
    return out;
}
