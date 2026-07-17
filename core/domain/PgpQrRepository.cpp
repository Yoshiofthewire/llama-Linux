#include "domain/PgpQrRepository.h"

#include "domain/DevicePairing.h"
#include "domain/PairingStore.h"
#include "net/NetworkError.h"
#include "net/PgpQrClient.h"
#include "net/RelayAuth.h"

#include <QUrl>

namespace {

// 400 ("no pgp identity configured") isn't in NetworkError's own switch (see
// core/net/NetworkError.cpp), so it collapses to NetworkError::Server --
// distinguish it here via the raw statusCode PgpQrClient carries alongside
// the generic error, rather than adding a case to the shared enum every
// other client's exhaustive switch would then have to account for.
PgpQrTokenStatus tokenStatusFrom(const PgpQrTokenResult& result)
{
    if (result.statusCode == 400)
        return PgpQrTokenStatus::NoPgpIdentity;
    if (!result.error.has_value())
        return PgpQrTokenStatus::Success;
    switch (*result.error) {
    case NetworkError::Unauthorized:
        return PgpQrTokenStatus::Unauthorized;
    case NetworkError::ServiceUnavailable:
        return PgpQrTokenStatus::ServiceUnavailable;
    default:
        return PgpQrTokenStatus::Retry;
    }
}

} // namespace

PgpQrRepository::PgpQrRepository(PgpQrClient& client, PairingStore& pairingStore)
    : m_client(client)
    , m_pairingStore(pairingStore)
{
}

PgpQrTokenOutcome PgpQrRepository::fetchMyToken()
{
    const std::optional<DevicePairing> pairing = m_pairingStore.load();
    if (!pairing.has_value())
        return { PgpQrTokenStatus::NotPaired, {}, {}, {}, QStringLiteral("Not paired") };

    const RelayAuth auth{ pairing->subscriberId, pairing->subscriberHash };
    const QUrl serverUrl(pairing->serverBaseUrl);

    const PgpQrTokenResult result = m_client.fetchToken(serverUrl, auth);
    const PgpQrTokenStatus status = tokenStatusFrom(result);

    if (status != PgpQrTokenStatus::Success)
        return { status, {}, {}, {}, result.detail };

    return { PgpQrTokenStatus::Success, result.token, result.expiresAt, result.url, QString() };
}
