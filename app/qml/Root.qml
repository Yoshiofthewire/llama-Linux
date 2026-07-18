import QtQuick 2.15
import org.kde.kirigami 2.20 as Kirigami

// Convergent root dispatcher -- main.cpp's engine.load() points here
// instead of directly at MobileRoot.qml/DesktopRoot.qml (Tasks 38/39):
// picks between them based on Kirigami.Settings.isMobile, the standard
// KF6 Kirigami form-factor signal. isMobile is true only when
// QT_QUICK_CONTROLS_MOBILE=1 is set or the platform theme plugin reports
// a real mobile shell (e.g. Plasma Mobile); a normal Plasma desktop
// session (Flatpak or not) leaves it false, so DesktopRoot.qml loads --
// this was the missing half of the dispatch Linux_QT_Client_Plan.md's
// "Root selection" section specified but never wired up.
//
// A QML-side Loader is used instead of deciding in main.cpp before
// engine.load() because Kirigami::Platform::Settings has no installed
// public C++ header (QML-singleton-only by design). No manual override
// (e.g. a Settings -> Appearance toggle) is wired here -- separate,
// unbuilt scope, see Linux_QT_Client_Plan.md.
Loader {
    source: Kirigami.Settings.isMobile
        ? "qrc:/qml/MobileRoot.qml"
        : "qrc:/qml/DesktopRoot.qml"
}
