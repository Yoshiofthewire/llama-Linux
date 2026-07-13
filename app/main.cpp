#include "push/UnifiedPushConnector.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QUrl>

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;
    engine.load(QUrl(QStringLiteral("qrc:/qml/MobileRoot.qml")));
    if (engine.rootObjects().isEmpty())
        return -1;

    // Task 10 proof-of-concept: exercise the KUnifiedPush connector so its
    // endpoint/state/message signals can be observed via qDebug(). Not wired
    // into real push handling yet -- see app/push/UnifiedPushConnector.h.
    UnifiedPushConnector pushConnector(QStringLiteral("com.urlxl.LlamaMail"));
    pushConnector.registerClient(QStringLiteral("Llama Mail push notifications"));

    return app.exec();
}
