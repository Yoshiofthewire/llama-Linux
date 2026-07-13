#pragma once

#include <QObject>
#include <QString>

namespace KUnifiedPush {
class Connector;
}

// Thin wrapper around KUnifiedPush::Connector (the system UnifiedPush client
// library, /usr/include/KUnifiedPush/kunifiedpush/connector.h). Lives in
// app/, never core/, because it links against a QtDBus-adjacent system
// library, consistent with the existing core/ boundary rule (see
// SecureStoreKeychain).
//
// This is throwaway proof-of-concept code for Task 10: it logs the acquired
// endpoint URL and incoming push message bytes to stdout via qDebug(). A
// later phase wires the received messages into real push/sync handling.
class UnifiedPushConnector : public QObject
{
    Q_OBJECT
public:
    // serviceName is the app-unique identifier registered with the local
    // UnifiedPush distributor over D-Bus, e.g. "com.urlxl.LlamaMail".
    explicit UnifiedPushConnector(const QString& serviceName, QObject* parent = nullptr);

    // Subscribes with the local distributor. Persisted until explicitly
    // unregistered; safe to call on every startup.
    void registerClient(const QString& description);

private:
    KUnifiedPush::Connector* m_connector;
};
