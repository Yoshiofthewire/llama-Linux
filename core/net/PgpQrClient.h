#pragma once

#include "models/Contact.h"
#include "net/HttpClient.h"
#include "net/NetworkError.h"

#include <QString>
#include <QUrl>
#include <optional>

struct RelayAuth;

// Response from GET {serverBaseUrl}/api/pgp/qr/token -- the Go backend's
// handlePGPQRToken (internal/api/pgp_qr_handlers.go). statusCode is carried
// alongside error because the shared NetworkError enum only distinguishes
// 401/403/409/429/503 (see NetworkError.cpp); this endpoint's 400 ("no pgp
// identity configured") would otherwise be indistinguishable from any other
// unexpected failure, and PgpQrRepository needs to tell them apart.
struct PgpQrTokenResult
{
    std::optional<NetworkError> error;
    int statusCode = 0;
    QString detail; // human-readable detail on error; empty otherwise
    QString token;
    QString expiresAt; // RFC3339 string, parsed by the caller if needed
    QString url;
};

// Response from GET {qrUrl} (the literal "…/api/pgp/qr/key?t=…" URL decoded
// from a scanned QR) -- handlePGPQRKey. statusCode is carried for the same
// reason as above: this endpoint's 404 ("no pgp identity configured") needs
// to be distinguishable from a generic failure.
struct PgpQrKeyResult
{
    std::optional<NetworkError> error;
    int statusCode = 0;
    QString detail; // human-readable detail on error; empty otherwise
    QString name;
    QString fingerprint;
    QString publicKey;

    // Populated iff the token owner has a contact flagged Contact::isSelf
    // (server's optional "contactCard" key) -- parsed via the existing
    // ContactWire::contactFromJson rather than a second parser, since the
    // card's fields are a strict subset of Contact's own with no
    // field-name translation. Fields the card never carries (uid, rev,
    // photoRef, isSelf, ...) simply stay default-valued and are never read.
    std::optional<Contact> contactCard;
};

// Talks to the two PGP-QR-exchange endpoints. Follows GroupsClient's
// template (constructor takes HttpClient&, one method per endpoint, a small
// *Result struct per call) rather than anything new.
class PgpQrClient
{
public:
    explicit PgpQrClient(HttpClient& httpClient);

    // GET {serverBaseUrl}/api/pgp/qr/token?sub&hash -- withMailAuth (session
    // OR pairing sub/hash), same auth shape as ContactSyncClient::pull.
    PgpQrTokenResult fetchToken(const QUrl& serverBaseUrl, const RelayAuth& auth) const;

    // GET qrUrl as-is -- no RelayAuth. The token in the URL's `t` query
    // param is the sole credential, and the scan target may be a different
    // server than the caller's own paired one, so this deliberately does
    // not go through the caller's own serverBaseUrl/RelayAuth at all.
    // redirectValidator is passed straight through to HttpClient::get() --
    // see its own doc comment. PgpQrController passes isSafeQrTarget here
    // so a redirect can't take the request somewhere the initial URL check
    // would have rejected.
    PgpQrKeyResult fetchKey(const QUrl& qrUrl, const HttpClient::RedirectValidator& redirectValidator = {}) const;

private:
    HttpClient& m_httpClient;
};
