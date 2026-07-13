#include "push/UnifiedPushConnector.h"

#include <KUnifiedPush/Connector>

#include <QDBusConnection>
#include <QDBusError>
#include <QDebug>

UnifiedPushConnector::UnifiedPushConnector(const QString& serviceName, QObject* parent)
    : QObject(parent)
    , m_connector(new KUnifiedPush::Connector(serviceName, this))
{
    // connector.h documents serviceName as "the application identifier, same
    // as used for registration on D-Bus and for D-Bus activation" -- the
    // distributor delivers NewEndpoint/Message calls to this well-known bus
    // name (observed via `busctl --user monitor`: it calls
    // Destination=<serviceName> Path=/org/unifiedpush/Connector
    // Interface=org.unifiedpush.Connector2). KUnifiedPush::Connector does
    // not claim this name itself, so without this call the distributor's
    // delivery attempt fails with "ServiceUnknown: The name is not
    // activatable" and the endpoint/message never reaches us.
    if (!QDBusConnection::sessionBus().registerService(serviceName)) {
        qDebug() << "UnifiedPushConnector: failed to register D-Bus service name" << serviceName << ":"
                  << QDBusConnection::sessionBus().lastError().message();
    }

    connect(m_connector, &KUnifiedPush::Connector::endpointChanged, this, [](const QString& endpoint) {
        qDebug() << "UnifiedPushConnector: endpoint changed:" << endpoint;
    });

    connect(m_connector, &KUnifiedPush::Connector::messageReceived, this, [](const QByteArray& msg) {
        qDebug() << "UnifiedPushConnector: message received (" << msg.size() << "bytes ):" << msg;
    });

    connect(m_connector, &KUnifiedPush::Connector::stateChanged, this, [](KUnifiedPush::Connector::State state) {
        qDebug() << "UnifiedPushConnector: state changed:" << state;
    });
}

void UnifiedPushConnector::registerClient(const QString& description)
{
    qDebug() << "UnifiedPushConnector: registering client, current state:" << m_connector->state()
              << "current endpoint:" << m_connector->endpoint();
    m_connector->registerClient(description);
}
