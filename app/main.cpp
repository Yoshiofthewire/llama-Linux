#include "push/UnifiedPushConnector.h"

#include <QDebug>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QUrl>

#include <KDBusService>

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);

    // Task 11: applicationName + organizationDomain feed KDBusService's name
    // derivation (reversed domain + app name -- see KDBusService::KDBusService
    // docs), which must produce the same "com.urlxl.LlamaMail" well-known
    // name that UnifiedPushConnector registers below and that the .service
    // file at ~/.local/share/dbus-1/services/com.urlxl.LlamaMail.service
    // advertises to the D-Bus daemon for on-demand activation.
    app.setApplicationName(QStringLiteral("LlamaMail"));
    app.setOrganizationDomain(QStringLiteral("urlxl.com"));

    // KDBusService (KF6DBusAddons, confirmed installed via `pacman -Qs
    // kdbusaddons`) claims the well-known bus name in Unique mode. Verified
    // (Task 11) that this build (KF6DBusAddons 6.27.0) does NOT export
    // org.freedesktop.Application/Activate at /MainApplication -- introspection
    // shows only org.qtproject.Qt.QCoreApplication/QGuiApplication reflected
    // properties there. activateRequested() below fires via the documented
    // Unique-mode collision path instead: launching the binary again while an
    // instance is already running (including one the D-Bus daemon just spawned
    // per the .service file's Exec=) relays the new invocation's argv/cwd to
    // the running instance over D-Bus and emits this signal there; the
    // duplicate process then quits on its own. Constructed before
    // QQmlApplicationEngine per KDBusService's own race-avoidance guidance:
    // export D-Bus objects before the event loop (app.exec()) runs.
    KDBusService dbusService(KDBusService::Unique);
    QObject::connect(&dbusService, &KDBusService::activateRequested, &dbusService,
                      [](const QStringList& arguments, const QString& workingDirectory) {
                          qDebug() << "KDBusService: activateRequested -- arguments:" << arguments
                                    << "workingDirectory:" << workingDirectory;
                      });

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
