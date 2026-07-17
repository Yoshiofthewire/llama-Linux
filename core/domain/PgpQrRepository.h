#pragma once

#include <QString>

class PgpQrClient;
class PairingStore;

enum class PgpQrTokenStatus { Success, NotPaired, NoPgpIdentity, Unauthorized, ServiceUnavailable, Retry };

struct PgpQrTokenOutcome
{
    PgpQrTokenStatus status = PgpQrTokenStatus::Retry;
    QString token;      // meaningful only when status == Success
    QString expiresAt;  // meaningful only when status == Success
    QString url;        // meaningful only when status == Success
    QString detail;     // meaningful when status != Success
};

// Wraps only the "My QR Code" token-fetch side of PGP QR exchange -- the
// only half that needs this device's own pairing (RelayAuth/serverBaseUrl)
// resolved, same as GroupsRepository::refresh(). The key-fetch side
// (scanning someone else's code) needs no pairing at all -- the token in
// the scanned URL is the sole credential, and the scan target may be a
// different server than this device's own paired one -- so
// PgpQrController calls PgpQrClient::fetchKey() directly instead of going
// through a repository method here (mirrors MailController's existing
// "holds both a repository and a raw client" composition).
class PgpQrRepository
{
public:
    PgpQrRepository(PgpQrClient& client, PairingStore& pairingStore);

    PgpQrTokenOutcome fetchMyToken();

private:
    PgpQrClient& m_client;
    PairingStore& m_pairingStore;
};
