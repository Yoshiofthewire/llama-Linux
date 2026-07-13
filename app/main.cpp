#include "push/UnifiedPushConnector.h"

#include <QDebug>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QUrl>

#include <KDBusService>

// Task 12 stub router: scans a set of command-line-style arguments for a
// llamalabels:// deep link and logs it. Real pairing-URL parsing (token
// extraction, routing to a QML page, etc.) is a later phase -- this proof
// only needs to demonstrate that the URL reaches application code, whether
// via a fresh launch's argv or via KDBusService::activateRequested relaying
// a second launch's argv to the already-running instance.
static void routeDeepLink(const QStringList& arguments)
{
    for (const QString& argument : arguments) {
        const QUrl url(argument);
        if (url.scheme() == QStringLiteral("llamalabels")) {
            qDebug() << "StubRouter: received deep link:" << url;
            return;
        }
    }
}

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
                          // Task 12: a second `llamamail llamalabels://...` invocation gets
                          // redirected here instead of spawning a duplicate process -- route
                          // its argv through the same stub deep-link handler used at startup.
                          routeDeepLink(arguments);
                      });

    // Task 12: this process is the one that "won" KDBusService's Unique-mode
    // registration, so also check its own argv for a llamalabels:// URL --
    // covers the case where xdg-open launches llamamail fresh (nothing was
    // running yet to redirect to).
    routeDeepLink(app.arguments());

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
