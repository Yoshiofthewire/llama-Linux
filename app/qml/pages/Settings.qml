import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import com.urlxl.mail 1.0
import "../components"
// Qualified import of this same directory, aliased to PairingPage -- the
// bare name "Pairing" is ambiguous in this file: the automatic IMPLICIT
// same-directory import (every .qml file in pages/ is implicitly visible by
// filename to every other file in pages/, no import statement needed) would
// resolve "Pairing" to Pairing.qml, but the "com.urlxl.mail 1.0"
// import above explicitly registers a QML singleton ALSO named "Pairing"
// (the PairingController instance, see main.cpp) -- and per QML's import
// precedence rules, an explicit import always wins over the implicit
// per-directory one. The bare name resolved to the singleton at runtime
// ("Pairing: Element is not creatable" -- a singleton can't be instantiated
// with curly-brace syntax), confirmed during this task's manual
// verification. A qualified EXPLICIT import of "." (this same directory,
// re-imported under an alias) outranks the implicit import the same way
// MobileRoot.qml's own explicit `import "pages"` does from outside this
// directory, sidestepping the collision entirely.
import "." as PagesDir

// Task 39 -- plain reusable Item, deliberately NOT a Kirigami.Page (same
// "parent-agnostic" shape as Tasks 35-37's page components, per Phase 6
// global constraint 4): MobileRoot wraps this in a thin Kirigami.Page shell
// when it pushes it from the globalDrawer's "Settings" action (previously a
// stub, see MobileRoot.qml); DesktopRoot hosts it inside a
// Kirigami.OverlaySheet instead of a second ApplicationWindow -- Global
// Constraint 4 leaves that choice open, and OverlaySheet was picked because
// it needs no second top-level window/event-loop lifetime to manage, unlike
// a real separate ApplicationWindow.
//
// 5 panes selected via a PillTab strip, not QtQuick.Controls
// TabBar/TabButton -- keeps this screen themed via the same "PillTab as a
// segmented selector" convention MobileRoot.qml's keyword pill row already
// established, rather than introducing a second, unthemed tab-chrome
// component into the app.
Item {
    id: root

    // Emitted from the header's "Done" button -- same "don't assume
    // push-navigation vs. a pane/sheet" shape as every other page component
    // this phase (EmailDetail/Compose/ContactDetail/Pairing): whichever
    // root hosts this decides what "close" means (pageStack.pop() for
    // Mobile, sheet.close() for Desktop).
    signal closed()
    // PGP QR key exchange: same "let the host decide how to navigate" shape
    // as closed() -- MobileRoot pushes PgpMyQrCode.qml via pageStack,
    // DesktopRoot opens it in a Kirigami.OverlaySheet (same choice it
    // already made for this Settings screen itself).
    signal myPgpQrCodeRequested()

    implicitWidth: 480
    implicitHeight: 560

    property int currentPane: 0 // 0 Connection, 1 Appearance, 2 Keywords, 3 Contacts, 4 Notifications
    readonly property var paneNames: [i18n("Connection"), i18n("Appearance"), i18n("Keywords"), i18n("Contacts"), i18n("Notifications")]

    // MailApp.allKeywordSettings() is a Q_INVOKABLE snapshot, not a
    // NOTIFY-bound property (see MailController.h's doc comment on why) --
    // cached here as a plain JS array and re-pulled on load and after every
    // toggle so the Keywords pane's row list stays in sync with whatever
    // setKeywordVisible() just wrote.
    property var keywordSettings: []

    function refreshKeywordSettings() {
        root.keywordSettings = MailApp.allKeywordSettings()
    }

    Component.onCompleted: refreshKeywordSettings()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        // ---- header ------------------------------------------------------
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                Layout.fillWidth: true
                text: i18n("Settings")
                color: Theme.inkStrong
                font.family: Theme.fontUi
                font.pixelSize: 20
                font.weight: Font.Bold
            }
            GhostButton {
                text: i18n("Done")
                onClicked: root.closed()
            }
        }

        // ---- pane selector -------------------------------------------------
        Flickable {
            Layout.fillWidth: true
            implicitHeight: paneTabRow.implicitHeight
            contentWidth: paneTabRow.implicitWidth
            contentHeight: height
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            flickableDirection: Flickable.HorizontalFlick

            Row {
                id: paneTabRow
                spacing: 8

                Repeater {
                    model: root.paneNames
                    delegate: PillTab {
                        text: modelData
                        selected: root.currentPane === index
                        onClicked: root.currentPane = index
                    }
                }
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: root.currentPane

            // ---- 1. Connection ----------------------------------------
            ColumnLayout {
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    StatusBadge {
                        active: Pairing.isPaired
                        text: Pairing.isPaired ? i18n("Paired") : i18n("Not paired")
                    }
                    Item { Layout.fillWidth: true }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    visible: Pairing.isPaired
                    spacing: 6

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        SectionLabel { Layout.preferredWidth: 70; text: i18n("Server") }
                        Text {
                            Layout.fillWidth: true
                            text: Pairing.pairedServerHost
                            color: Theme.inkStrong
                            font.family: Theme.fontMono
                            font.pixelSize: 14
                            wrapMode: Text.WordWrap
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        SectionLabel { Layout.preferredWidth: 70; text: i18n("Device") }
                        Text {
                            Layout.fillWidth: true
                            text: Pairing.deviceId
                            color: Theme.inkStrong
                            font.family: Theme.fontMono
                            font.pixelSize: 14
                            wrapMode: Text.WordWrap
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    // No "Test Connection" / desktop-session pairing here --
                    // both explicitly out of scope, see Phase 6 global
                    // constraint 6 (this client family only ever does
                    // sub/hash native pairing, no separate desktop-session
                    // flow) and the task-39 brief's Connection pane spec.
                    PrimaryButton {
                        text: i18n("Pair This Device…")
                        visible: !Pairing.isPaired
                        onClicked: pairingPopup.open()
                    }
                    DangerButton {
                        text: i18n("Remove Pairing")
                        visible: Pairing.isPaired
                        onClicked: Pairing.removePairing()
                    }
                }

                Item { Layout.fillHeight: true }
            }

            // ---- 2. Appearance ------------------------------------------
            // Name-only list + checkmark, not per-theme color swatches:
            // ThemeController only ever exposes the CURRENTLY ACTIVE
            // theme's palette as live QColor properties (see its own doc
            // comment) -- there is no QML-reachable way to peek another
            // theme's bg/panel/accent today without adding new core-to-QML
            // plumbing (an AppTheme::palette(name) bridge) purely for this
            // one swatch row. The task-39 brief explicitly sanctions this
            // exact fallback rather than blocking on that plumbing.
            ListView {
                id: themeListView
                clip: true
                spacing: 2
                model: Theme.themeNames

                delegate: Rectangle {
                    width: themeListView.width
                    height: themeRow.implicitHeight + 16
                    radius: Theme.shapeButton
                    color: modelData === Theme.themeName ? Theme.panel : "transparent"

                    RowLayout {
                        id: themeRow
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 8

                        Text {
                            Layout.fillWidth: true
                            // modelData is a theme's proper name (e.g. "Dark
                            // Matter", "Cyber Punk" -- see core/theme/
                            // AppTheme.cpp), NOT wrapped in i18n(): these are
                            // brand-style palette names (same "don't
                            // translate the product name" reasoning as
                            // "Llama Mail" itself), AND they live in core/,
                            // which the Phase 8 global-constraints boundary
                            // (item 3) forbids linking KI18n into -- they're
                            // also the literal identifier Theme.setTheme()
                            // stores/compares, not just display text.
                            text: modelData
                            color: Theme.inkStrong
                            font.family: Theme.fontUi
                            font.pixelSize: 14
                        }
                        Text {
                            visible: modelData === Theme.themeName
                            text: "✓"
                            color: Theme.accent
                            font.family: Theme.fontUi
                            font.pixelSize: 14
                            font.weight: Font.Bold
                        }
                    }

                    TapHandler {
                        onTapped: Theme.setTheme(modelData)
                    }
                }
            }

            // ---- 3. Keywords ---------------------------------------------
            Item {
                EmptyState {
                    anchors.fill: parent
                    visible: keywordListView.count === 0
                    text: i18n("No keywords yet.")
                }

                ListView {
                    id: keywordListView
                    anchors.fill: parent
                    visible: count > 0
                    clip: true
                    spacing: 4
                    model: root.keywordSettings

                    delegate: RowLayout {
                        width: keywordListView.width
                        spacing: 8

                        Text {
                            Layout.fillWidth: true
                            text: modelData.keyword
                            color: Theme.inkStrong
                            font.family: Theme.fontUi
                            font.pixelSize: 14
                        }
                        PillTab {
                            text: modelData.visible ? i18n("Visible") : i18n("Hidden")
                            selected: modelData.visible
                            onClicked: {
                                MailApp.setKeywordVisible(modelData.keyword, !modelData.visible)
                                root.refreshKeywordSettings()
                            }
                        }
                    }
                }
            }

            // ---- 4. Contacts ------------------------------------------------
            // No "Sync to system contacts" toggle: that's the Mac/Android
            // apps' OS-level Contacts-app export integration, and this repo
            // has no Linux equivalent anywhere in core/ or app/ (nothing
            // this plan has built through Phase 6 talks to a system address
            // book) -- a toggle here would do nothing when flipped, which
            // this task's brief explicitly forbids. This pane shows real
            // sync status/action instead (reusing ContactsApp exactly as
            // ContactsList.qml's own header does).
            ColumnLayout {
                spacing: 12

                Text {
                    Layout.fillWidth: true
                    text: ContactsApp.lastError !== "" ? ContactsApp.lastError
                        : (ContactsApp.statusMessage !== "" ? ContactsApp.statusMessage : i18n("No sync yet."))
                    color: ContactsApp.lastError !== "" ? Theme.dangerColor : Theme.ink
                    font.family: Theme.fontUi
                    font.pixelSize: 13
                    wrapMode: Text.WordWrap
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    PrimaryButton {
                        text: i18n("Sync Now")
                        enabled: !ContactsApp.isBusy
                        onClicked: ContactsApp.sync()
                    }
                    GhostButton {
                        text: i18n("My PGP QR Code")
                        onClicked: root.myPgpQrCodeRequested()
                    }
                    Item { Layout.fillWidth: true }
                }

                Item { Layout.fillHeight: true }
            }

            // ---- 5. Notifications -------------------------------------------
            // Read-only display only -- no editable push-server-URL field.
            // Global constraint 6's deviceToken gap means live re-
            // registration isn't wired this phase anyway (see
            // PairingController.h's known-gap comment), so an editable
            // field here would look functional while doing nothing; a
            // Phase 7 follow-up once real registration lands end-to-end.
            ColumnLayout {
                spacing: 10

                Text {
                    Layout.fillWidth: true
                    visible: Pairing.deliveryMode === "" && Pairing.transport === ""
                    text: i18n("Not yet registered")
                    color: Theme.ink
                    font.family: Theme.fontUi
                    font.pixelSize: 13
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    visible: Pairing.deliveryMode !== "" || Pairing.transport !== ""
                    spacing: 6

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        SectionLabel { Layout.preferredWidth: 100; text: i18n("Delivery Mode") }
                        Text {
                            Layout.fillWidth: true
                            text: Pairing.deliveryMode
                            color: Theme.inkStrong
                            font.family: Theme.fontMono
                            font.pixelSize: 14
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        SectionLabel { Layout.preferredWidth: 100; text: i18n("Transport") }
                        Text {
                            Layout.fillWidth: true
                            text: Pairing.transport
                            color: Theme.inkStrong
                            font.family: Theme.fontMono
                            font.pixelSize: 14
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    SectionLabel { Layout.preferredWidth: 100; text: i18n("Push Server") }
                    Text {
                        Layout.fillWidth: true
                        text: Pairing.pushServerBaseUrl
                        color: Theme.inkStrong
                        font.family: Theme.fontMono
                        font.pixelSize: 14
                        wrapMode: Text.WordWrap
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }
    }

    // ---- "Pair This Device…" overlay -------------------------------------
    // Settings.qml manages its own nested popup rather than asking the host
    // to navigate anywhere -- keeps this file host-agnostic (same reasoning
    // as the signal-based navigation on EmailDetail/ContactsList/etc.)
    // instead of assuming a pageStack that DesktopRoot doesn't have.
    Popup {
        id: pairingPopup
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        x: (root.width - width) / 2
        y: (root.height - height) / 2
        width: Math.min(380, root.width - 32)
        height: Math.min(560, root.height - 32)
        padding: 0

        background: Rectangle {
            color: Theme.panel
            radius: Theme.shapeSheet
            border.width: 1
            border.color: Theme.line
        }

        contentItem: PagesDir.Pairing {
            anchors.fill: parent
            onClosed: pairingPopup.close()
        }
    }
}
