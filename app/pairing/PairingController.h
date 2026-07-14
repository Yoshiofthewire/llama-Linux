#pragma once

#include <QObject>
#include <QString>

class QUrl;
class DeviceRegistrationService;
class PairingStore;
class SettingsStore;

// QML-facing bridge (Task 34) over core/domain's DeviceRegistrationService/
// PairingStore. Registered as the "Pairing" QML singleton in main.cpp.
// pairFromDeepLink/pairFromPastedLink are the real replacement for the
// Task 12 routeDeepLink stub -- see main.cpp's routeDeepLink for the
// llamalabels://native-pair wiring. pairFromParsedParams (and therefore any
// successful pair) runs deviceRegistrationService.pair() synchronously on
// the calling (GUI) thread -- see Phase 6 global constraint 2, this is a
// known, accepted freeze-the-UI tradeoff for this phase, not a bug.
//
// Known gap (see task-34-report.md): pair()'s deviceToken argument is always
// an empty QString() from this controller. This client's push transport is
// UnifiedPush, and UnifiedPushConnector isn't wired to a stable,
// ready-before-pairing endpoint value yet (it's constructed after
// engine.load() in main.cpp, and threading a real endpoint through to this
// call is Phase 7 territory). A live pairing attempt against a real backend
// will therefore fail (the backend's register endpoint requires a non-empty
// deviceToken) until that wiring lands -- this is expected, not a bug to fix
// in this task.
class PairingController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isPaired READ isPaired NOTIFY pairingChanged)
    // Host-only, never the full URL with token -- matches the existing
    // "never log the full endpoint" precedent from Task 11's post-push
    // security review.
    Q_PROPERTY(QString pairedServerHost READ pairedServerHost NOTIFY pairingChanged)
    Q_PROPERTY(QString deviceId READ deviceId NOTIFY pairingChanged)
    Q_PROPERTY(QString pairingState READ pairingState NOTIFY pairingStateChanged) // "idle" | "working" | "paired" | "failed"
    Q_PROPERTY(QString pairingError READ pairingError NOTIFY pairingStateChanged) // meaningful only when pairingState == "failed"
    // Task 39: read-only display fields for Settings > Notifications.
    // Sourced straight from SettingsStore on every read (no local cache).
    // deliveryMode/transport only ever change together with isPaired/
    // pairedServerHost/deviceId above (DeviceRegistrationService::pair()
    // writes all of them atomically on RegistrationOutcome::Success, per its
    // own class doc comment), so reusing pairingChanged() as NOTIFY here is
    // correct rather than adding a second signal that would always fire in
    // lockstep with it anyway. pushServerBaseUrl is different: nothing in
    // this codebase calls SettingsStore::setPushServerBaseUrl() yet (it's
    // orphaned plumbing -- see NtfySubscriber.h's own comment) other than
    // SettingsStore's own baked-in "https://ntfy.sh" default, so this
    // property always reads that value today, never empty. It's still
    // wired here read-only (never an editable field, per the task-39
    // brief's explicit scope cut) so Settings.qml can display it honestly.
    Q_PROPERTY(QString deliveryMode READ deliveryMode NOTIFY pairingChanged)     // "push" | "pull" | "" (never registered)
    Q_PROPERTY(QString transport READ transport NOTIFY pairingChanged)          // server-normalized transport name, "" if never registered
    Q_PROPERTY(QString pushServerBaseUrl READ pushServerBaseUrl NOTIFY pairingChanged) // "https://ntfy.sh" default; read-only display only, see Settings.qml's Notifications pane

public:
    PairingController(DeviceRegistrationService& service, PairingStore& pairingStore, SettingsStore& settingsStore,
                       QObject* parent = nullptr);

    bool isPaired() const;
    QString pairedServerHost() const;
    QString deviceId() const;
    QString pairingState() const;
    QString pairingError() const;
    QString deliveryMode() const;
    QString transport() const;
    QString pushServerBaseUrl() const;

public slots:
    // Re-reads pairingStore.load(), updates isPaired/pairedServerHost/
    // deviceId. Called once from the constructor, and again by
    // pairFromParsedParams() on a successful pair and by removePairing().
    void refreshFromStore();
    // Parses a llamalabels://native-pair URL per the wire format documented
    // on PairingController.cpp's parseNativePairLink(): sub/hash/srv/pt
    // query params required (hash may be present-but-empty -- matches
    // DevicePairing::subscriberHash's own "may be empty" contract; sub/srv/
    // pt must be present AND non-empty), reg optional (empty/absent derives
    // the registration endpoint from srv). On parse failure sets
    // pairingState="failed"+pairingError and returns false without any
    // network call; on success calls pairFromParsedParams internally.
    bool pairFromDeepLink(const QUrl& url);
    // Same as pairFromDeepLink but the input is a pasted string the user
    // typed/pasted into a TextField -- wrapped in QUrl(text), same
    // validation path.
    bool pairFromPastedLink(const QString& text);
    void reset();         // sets pairingState back to "idle" (for a "Try Again" button after a failure)
    void removePairing(); // pairingStore.clear() + refreshFromStore()
    // Late-bound, same pattern as main.cpp's pairingControllerForDeepLinks
    // pointer (Task 34): UnifiedPushConnector is constructed after this
    // class in main.cpp's dependency order, so main.cpp calls this whenever
    // UnifiedPushConnector::endpointChanged fires (including once with the
    // already-known endpoint right after pushConnector's construction).
    // The backend's deviceToken field is required (POST
    // /api/notifications/native/register) -- sending QString() here made
    // every first-time pairing attempt fail with a 400, discovered during
    // live E2E testing (Task 43). pairFromParsedParams() below now sends
    // whatever this holds, empty or not, rather than always QString().
    void setDeviceToken(const QString& token);

signals:
    void pairingChanged();
    void pairingStateChanged();

private:
    // Builds a PairingParams from already-validated fields, sets
    // pairingState="working", calls
    // deviceRegistrationService.pair(params, m_deviceToken), maps
    // RegistrationOutcome to pairingState/pairingError, calls
    // refreshFromStore() on success.
    bool pairFromParsedParams(const QString& sub, const QString& hash, const QString& srv, const QString& pt,
                               const QString& reg);
    void setPairingState(const QString& state, const QString& error = QString());

    DeviceRegistrationService& m_service;
    PairingStore& m_pairingStore;
    SettingsStore& m_settingsStore;
    QString m_pairingState = QStringLiteral("idle");
    QString m_pairingError;
    bool m_isPaired = false;
    QString m_pairedServerHost;
    QString m_deviceId;
    QString m_deviceToken; // set via setDeviceToken(); empty until UnifiedPushConnector reports a real endpoint
};
