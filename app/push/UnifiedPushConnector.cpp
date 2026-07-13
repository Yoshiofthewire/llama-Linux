#include "push/UnifiedPushConnector.h"

#include <KUnifiedPush/Connector>

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
    // not claim this name itself -- but this process's KDBusService
    // (constructed in main.cpp before this class) already has, and it's the
    // same D-Bus connection, so no registration call belongs here. See the
    // comment at the KDBusService construction site in main.cpp.

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
