#include "pairing/PairingController.h"

#include "domain/DeviceRegistrationService.h"
#include "domain/DevicePairing.h"
#include "domain/PairingStore.h"
#include "stores/SettingsStore.h"

#include <QUrl>
#include <QUrlQuery>

namespace {

// Task 34 deep-link wire format, confirmed against both this project's
// Android and Swift sibling clients' real parsers:
// llamalabels://native-pair?sub=<id>&hash=<hash>&srv=<serverBaseUrl>&pt=<pairingToken>&reg=<optional>
//
// sub/srv/pt must be present in the query AND non-empty. hash must be
// present but its value may legitimately be empty -- mirrors
// DevicePairing::subscriberHash's own "may be empty" doc comment, i.e. a
// server can pair a subscriber with no hash at all, but the deep link must
// still carry the key to say so explicitly rather than silently omitting
// it. reg is optional; empty/absent means "derive from srv".
struct ParsedPairingLink
{
    QString subscriberId;
    QString subscriberHash;
    QString serverBaseUrl;
    QString pairingToken;
    QString registrationUrl; // empty if reg was absent/empty in the link
};

// Mirrors Android's NativeRegistrationEndpointResolver.resolve: strips any
// trailing slashes off srv, then appends the well-known native-register
// path.
QString deriveRegistrationUrl(const QString& serverBaseUrl)
{
    QString base = serverBaseUrl.trimmed();
    while (base.endsWith(QLatin1Char('/')))
        base.chop(1);
    return base + QStringLiteral("/api/notifications/native/register");
}

std::optional<ParsedPairingLink> parseNativePairLink(const QUrl& url)
{
    if (url.scheme() != QStringLiteral("llamalabels") || url.host() != QStringLiteral("native-pair"))
        return std::nullopt;

    const QUrlQuery query(url);
    if (!query.hasQueryItem(QStringLiteral("sub")) || !query.hasQueryItem(QStringLiteral("hash"))
        || !query.hasQueryItem(QStringLiteral("srv")) || !query.hasQueryItem(QStringLiteral("pt")))
        return std::nullopt;

    ParsedPairingLink parsed;
    parsed.subscriberId = query.queryItemValue(QStringLiteral("sub"), QUrl::FullyDecoded);
    parsed.subscriberHash = query.queryItemValue(QStringLiteral("hash"), QUrl::FullyDecoded);
    parsed.serverBaseUrl = query.queryItemValue(QStringLiteral("srv"), QUrl::FullyDecoded);
    parsed.pairingToken = query.queryItemValue(QStringLiteral("pt"), QUrl::FullyDecoded);
    parsed.registrationUrl = query.queryItemValue(QStringLiteral("reg"), QUrl::FullyDecoded);

    if (parsed.subscriberId.isEmpty() || parsed.serverBaseUrl.isEmpty() || parsed.pairingToken.isEmpty())
        return std::nullopt;

    return parsed;
}

} // namespace

PairingController::PairingController(DeviceRegistrationService& service, PairingStore& pairingStore,
                                       SettingsStore& settingsStore, QObject* parent)
    : QObject(parent)
    , m_service(service)
    , m_pairingStore(pairingStore)
    , m_settingsStore(settingsStore)
{
    // Unlike MailController/ContactsController (which deliberately start
    // empty until QML calls a load slot), the pairing badge/menu entries
    // that read isPaired/pairedServerHost need a correct answer from the
    // very first frame -- there is no reasonable "unknown" state to show
    // meanwhile -- so this one refreshes eagerly on construction.
    refreshFromStore();
}

bool PairingController::isPaired() const
{
    return m_isPaired;
}

QString PairingController::pairedServerHost() const
{
    return m_pairedServerHost;
}

QString PairingController::deviceId() const
{
    return m_deviceId;
}

QString PairingController::pairingState() const
{
    return m_pairingState;
}

QString PairingController::pairingError() const
{
    return m_pairingError;
}

QString PairingController::deliveryMode() const
{
    return m_settingsStore.deliveryMode();
}

QString PairingController::transport() const
{
    return m_settingsStore.transport();
}

QString PairingController::pushServerBaseUrl() const
{
    return m_settingsStore.pushServerBaseUrl();
}

void PairingController::setPairingState(const QString& state, const QString& error)
{
    if (m_pairingState == state && m_pairingError == error)
        return;
    m_pairingState = state;
    m_pairingError = error;
    emit pairingStateChanged();
}

void PairingController::refreshFromStore()
{
    const std::optional<DevicePairing> pairing = m_pairingStore.load();
    const bool nowPaired = pairing.has_value();
    const QString host = nowPaired ? QUrl(pairing->serverBaseUrl).host() : QString();
    const QString deviceId = nowPaired ? pairing->deviceId : QString();

    if (nowPaired == m_isPaired && host == m_pairedServerHost && deviceId == m_deviceId)
        return;

    m_isPaired = nowPaired;
    m_pairedServerHost = host;
    m_deviceId = deviceId;
    emit pairingChanged();
}

bool PairingController::pairFromDeepLink(const QUrl& url)
{
    const std::optional<ParsedPairingLink> parsed = parseNativePairLink(url);
    if (!parsed.has_value()) {
        setPairingState(QStringLiteral("failed"), QStringLiteral("This pairing link is invalid or incomplete."));
        return false;
    }

    return pairFromParsedParams(parsed->subscriberId, parsed->subscriberHash, parsed->serverBaseUrl,
                                 parsed->pairingToken, parsed->registrationUrl);
}

bool PairingController::pairFromPastedLink(const QString& text)
{
    return pairFromDeepLink(QUrl(text));
}

void PairingController::reset()
{
    setPairingState(QStringLiteral("idle"));
}

void PairingController::removePairing()
{
    m_pairingStore.clear();
    refreshFromStore();
}

void PairingController::setDeviceToken(const QString& token)
{
    m_deviceToken = token;
}

bool PairingController::pairFromParsedParams(const QString& sub, const QString& hash, const QString& srv,
                                              const QString& pt, const QString& reg)
{
    setPairingState(QStringLiteral("working"));

    PairingParams params;
    params.subscriberId = sub;
    params.subscriberHash = hash;
    params.serverBaseUrl = srv;
    params.registrationUrl = reg.isEmpty() ? deriveRegistrationUrl(srv) : reg;
    params.pairingToken = pt;
    // deviceName: not part of the deep-link wire format and not otherwise
    // sourced by this task -- left empty. A later task can add a "name this
    // device" field to the pairing UI if the plan wants one; nothing here
    // depends on it being non-empty.

    // m_deviceToken is set via setDeviceToken(), called from main.cpp
    // whenever UnifiedPushConnector reports its current endpoint (Task 43
    // fix -- the backend rejects a first-time pairing request with a 400
    // when deviceToken is empty, since the field is required). Still empty
    // if the distributor hasn't reported an endpoint yet by the time the
    // user completes pairing; the existing endpointChanged ->
    // reregisterIfPaired wiring in main.cpp corrects a stale/empty token
    // once one becomes available.
    const NativeRegistrationResult result = m_service.pair(params, m_deviceToken);

    switch (result.outcome) {
    case RegistrationOutcome::Success:
        refreshFromStore();
        setPairingState(QStringLiteral("paired"));
        return true;
    case RegistrationOutcome::Unauthorized:
        setPairingState(QStringLiteral("failed"),
                         QStringLiteral("This pairing link was rejected. Check the link and try again."));
        return false;
    case RegistrationOutcome::BackendMisconfigured:
        setPairingState(QStringLiteral("failed"), QStringLiteral("The server is not configured for pairing yet."));
        return false;
    case RegistrationOutcome::Failure:
        setPairingState(QStringLiteral("failed"),
                         result.detail.isEmpty() ? QStringLiteral("Pairing failed, please try again.")
                                                  : result.detail);
        return false;
    }
    return false;
}
