#include "net/ContactPhotoClient.h"

#include "net/HttpClient.h"
#include "net/RelayAuth.h"

namespace {

// Builds "api/contacts/<contactUid>/photo" and joins it onto serverBaseUrl's
// path via the shared joinUrlPath() helper (see HttpClient.h). url.setPath()'s
// default DecodedMode percent-encodes contactUid automatically, matching
// this class's header-comment note on why no manual encoding is done here.
QUrl endpointFor(const QUrl& serverBaseUrl, const QString& contactUid)
{
    return joinUrlPath(serverBaseUrl,
                        QStringLiteral("api/contacts/") + contactUid + QStringLiteral("/photo"));
}

} // namespace

ContactPhotoClient::ContactPhotoClient(HttpClient& httpClient)
    : m_httpClient(httpClient)
{
}

ContactPhotoFetchResult ContactPhotoClient::fetch(const QUrl& serverBaseUrl, const QString& contactUid,
                                                    const RelayAuth& auth) const
{
    const HttpClient::HttpResult result =
        m_httpClient.get(endpointFor(serverBaseUrl, contactUid), auth.queryItems());

    ContactPhotoFetchResult out;

    // Covers 401/403/5xx/transport failures alike -- HttpClient::get()
    // already maps the status code to NetworkError, so every non-2xx path
    // lands here and returns empty photoBytes rather than throwing/crashing,
    // same reasoning as GroupsClient::fetch().
    if (result.error.has_value()) {
        out.error = result.error;
        out.detail = !result.detail.isEmpty()
            ? result.detail
            : QStringLiteral("Contact photo fetch failed with status %1").arg(result.statusCode);
        return out;
    }

    // No JSON parsing here, unlike GroupsClient -- the response body is the
    // photo's raw bytes verbatim, per task-3-brief.md ("raw bytes"), so
    // there's no Decoding-error branch to add.
    out.photoBytes = result.body;
    return out;
}
