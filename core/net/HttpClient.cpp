#include "net/HttpClient.h"

#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>

HttpClient::HttpClient(QNetworkAccessManager& manager, int transferTimeoutMs)
    : m_manager(manager)
{
    // A manager with no transfer timeout configured (the Qt default) leaves
    // waitForReply()'s QEventLoop waiting on ::finished forever if a server
    // accepts the connection but never responds. transferTimeout() == 0
    // means "unset" per Qt docs, so only set it in that case -- a caller
    // that configured its own timeout on the injected manager keeps it.
    if (m_manager.transferTimeout() == 0)
        m_manager.setTransferTimeout(transferTimeoutMs);
}

HttpClient::HttpResult HttpClient::get(const QUrl& url, const QList<QPair<QString, QString>>& query,
                                        const QList<QPair<QString, QString>>& headers,
                                        const RedirectValidator& redirectValidator)
{
    const QUrl requestUrl = urlWithQuery(url, query);
    if (!requestUrl.isValid())
        return HttpResult{ NetworkError::InvalidUrl, 0, {}, QStringLiteral("Invalid URL") };

    QNetworkRequest request(requestUrl);
    for (const auto& header : headers)
        request.setRawHeader(header.first.toUtf8(), header.second.toUtf8());

    // Qt's default (NoLessSafeRedirectPolicy) follows redirects
    // automatically with no way for the caller to inspect the target.
    // UserVerifiedRedirectPolicy (NOT ManualRedirectPolicy, which never
    // emits QNetworkReply::redirected() at all -- it just refuses to
    // follow, full stop) is the one that pauses and waits for
    // redirectAllowed(), letting waitForReply() below re-validate every hop
    // before it's followed. Only switch to this when a validator was
    // actually supplied, so every other existing caller keeps today's
    // automatic-follow behavior unchanged.
    if (redirectValidator)
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::UserVerifiedRedirectPolicy);

    return waitForReply(m_manager.get(request), redirectValidator);
}

HttpClient::HttpResult HttpClient::post(const QUrl& url, const QList<QPair<QString, QString>>& query,
                                         const QJsonObject& jsonBody, const QList<QPair<QString, QString>>& headers)
{
    const QUrl requestUrl = urlWithQuery(url, query);
    if (!requestUrl.isValid())
        return HttpResult{ NetworkError::InvalidUrl, 0, {}, QStringLiteral("Invalid URL") };

    QNetworkRequest request(requestUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    for (const auto& header : headers)
        request.setRawHeader(header.first.toUtf8(), header.second.toUtf8());

    return waitForReply(m_manager.post(request, QJsonDocument(jsonBody).toJson(QJsonDocument::Compact)));
}

HttpClient::HttpResult HttpClient::put(const QUrl& url, const QList<QPair<QString, QString>>& query,
                                        const QJsonObject& jsonBody, const QList<QPair<QString, QString>>& headers)
{
    const QUrl requestUrl = urlWithQuery(url, query);
    if (!requestUrl.isValid())
        return HttpResult{ NetworkError::InvalidUrl, 0, {}, QStringLiteral("Invalid URL") };

    QNetworkRequest request(requestUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    for (const auto& header : headers)
        request.setRawHeader(header.first.toUtf8(), header.second.toUtf8());

    return waitForReply(m_manager.put(request, QJsonDocument(jsonBody).toJson(QJsonDocument::Compact)));
}

HttpClient::HttpResult HttpClient::del(const QUrl& url, const QList<QPair<QString, QString>>& query,
                                        const QList<QPair<QString, QString>>& headers)
{
    const QUrl requestUrl = urlWithQuery(url, query);
    if (!requestUrl.isValid())
        return HttpResult{ NetworkError::InvalidUrl, 0, {}, QStringLiteral("Invalid URL") };

    QNetworkRequest request(requestUrl);
    for (const auto& header : headers)
        request.setRawHeader(header.first.toUtf8(), header.second.toUtf8());

    return waitForReply(m_manager.deleteResource(request));
}

QUrl HttpClient::urlWithQuery(const QUrl& url, const QList<QPair<QString, QString>>& query) const
{
    if (query.isEmpty())
        return url;

    QUrlQuery urlQuery(url);
    for (const auto& item : query)
        urlQuery.addQueryItem(item.first, item.second);

    QUrl result = url;
    result.setQuery(urlQuery);
    return result;
}

HttpClient::HttpResult HttpClient::waitForReply(QNetworkReply* reply, const RedirectValidator& redirectValidator) const
{
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    // ManualRedirectPolicy (set above in get(), only when redirectValidator
    // is non-empty) means Qt pauses and waits for redirectAllowed() before
    // following each hop -- re-run the same safety check against the
    // redirect target here rather than following it blindly. Not calling
    // redirectAllowed() leaves the reply completing with the redirect
    // response itself (e.g. the 302), which is exactly what "don't follow
    // this" should look like to the caller.
    if (redirectValidator) {
        QObject::connect(reply, &QNetworkReply::redirected, reply, [reply, redirectValidator](const QUrl& target) {
            if (redirectValidator(target))
                reply->redirectAllowed();
            else
                // Simply not calling redirectAllowed() leaves Qt waiting
                // indefinitely for a decision under UserVerifiedRedirectPolicy
                // (unlike ManualRedirectPolicy, this isn't "give up and
                // return the 3xx response" -- it just never finishes) --
                // abort() is what actually completes the reply once the
                // target is rejected.
                reply->abort();
        });
    }
    loop.exec();

    HttpResult result;
    result.statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    result.body = reply->readAll();
    const QList<QNetworkReply::RawHeaderPair> rawHeaders = reply->rawHeaderPairs();
    result.headers.reserve(rawHeaders.size());
    for (const auto& header : rawHeaders)
        result.headers.append({ QString::fromLatin1(header.first), QString::fromLatin1(header.second) });

    if (result.statusCode != 0) {
        // Got an HTTP response — map by status code even if QNetworkReply
        // also flagged an error of its own (e.g. 404 sets ContentNotFoundError).
        result.error = networkErrorFromStatusCode(result.statusCode);
    } else if (reply->error() != QNetworkReply::NoError) {
        result.error = NetworkError::Transport;
        result.detail = reply->errorString();
    }

    reply->deleteLater();
    return result;
}

QUrl joinUrlPath(const QUrl& baseUrl, const QString& apiPath)
{
    QUrl url = baseUrl;
    QString path = url.path();
    if (!path.endsWith(QLatin1Char('/')))
        path += QLatin1Char('/');
    path += apiPath;
    url.setPath(path);
    return url;
}

std::optional<QJsonObject> decodeJsonObject(const QByteArray& body, QString* errorString)
{
    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (errorString)
            *errorString = parseError.errorString();
        return std::nullopt;
    }
    return doc.object();
}

std::optional<QJsonArray> decodeJsonArray(const QByteArray& body, QString* errorString)
{
    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isArray()) {
        if (errorString)
            *errorString = parseError.errorString();
        return std::nullopt;
    }
    return doc.array();
}
