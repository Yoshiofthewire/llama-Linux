import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import com.urlxl.mail 1.0
import "../components"

// Task 37 -- plain reusable Item, deliberately NOT a Kirigami.Page (see
// Phase 6 global constraint 4). No live trigger exists yet for this screen
// (opening it from a notification tap is a later phase, per the task
// brief) -- MobileRoot/DesktopRoot (Tasks 38/39) decide how/whether to
// route to it; this file only needs `challengeId` set and reacts to
// Mfa.respondState from there.
Item {
    id: root

    // Public API -- which challenge this approval screen is responding to.
    // Set by whichever host routes here (a notification tap in a later
    // phase, or a dev entry point for now).
    property string challengeId: ""

    // Emitted from the "done" state's Close button -- same "don't assume
    // push-navigation vs. a pane" shape as the rest of Task 35-37's pages.
    signal closed()

    implicitWidth: 360
    implicitHeight: 640

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 16

        Item { Layout.fillHeight: true; Layout.preferredHeight: 1 }

        // ---- idle --------------------------------------------------
        ColumnLayout {
            Layout.fillWidth: true
            visible: Mfa.respondState === "idle"
            spacing: 16

            Text {
                Layout.fillWidth: true
                text: i18n("Sign-in request")
                color: Theme.inkStrong
                font.family: Theme.fontUi
                font.pixelSize: 22
                font.weight: Font.Bold
                horizontalAlignment: Text.AlignHCenter
            }
            Text {
                Layout.fillWidth: true
                text: i18n("A sign-in is waiting for approval from this device.")
                color: Theme.ink
                font.family: Theme.fontUi
                font.pixelSize: 14
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            // challengeId "in a mono chip" per the brief -- reuses the same
            // panel/line/radius chip shape EmailDetail.qml's attachment
            // chips and Compose.qml's attachment-name chips already use,
            // rather than inventing a new chip look for a single value.
            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                radius: Theme.shapeButton
                color: Theme.panel
                border.width: 1
                border.color: Theme.line
                implicitWidth: chipLabel.implicitWidth + 24
                implicitHeight: chipLabel.implicitHeight + 16

                Text {
                    id: chipLabel
                    anchors.centerIn: parent
                    text: root.challengeId
                    color: Theme.inkStrong
                    font.family: Theme.fontMono
                    font.pixelSize: 13
                }
            }

            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 12

                DangerButton {
                    text: i18n("Deny")
                    onClicked: Mfa.respond(root.challengeId, false)
                }
                PrimaryButton {
                    text: i18n("Approve")
                    onClicked: Mfa.respond(root.challengeId, true)
                }
            }
        }

        // ---- sending -----------------------------------------------
        ColumnLayout {
            Layout.fillWidth: true
            visible: Mfa.respondState === "sending"
            spacing: 16

            BusyIndicator {
                Layout.alignment: Qt.AlignHCenter
                running: Mfa.respondState === "sending"
            }
            Text {
                Layout.fillWidth: true
                text: i18n("Sending…")
                color: Theme.ink
                font.family: Theme.fontUi
                font.pixelSize: 15
                horizontalAlignment: Text.AlignHCenter
            }
            // Approve/Deny disabled+hidden while sending per the brief --
            // this ColumnLayout replaces the idle block entirely (visible:
            // false above), so there is nothing further needed here to
            // "hide" them.
        }

        // ---- done --------------------------------------------------
        ColumnLayout {
            Layout.fillWidth: true
            visible: Mfa.respondState === "done"
            spacing: 16

            Text {
                Layout.fillWidth: true
                text: "✓"
                color: Theme.successBorderColor
                font.pixelSize: 40
                horizontalAlignment: Text.AlignHCenter
            }
            Text {
                Layout.fillWidth: true
                text: Mfa.resultMessage
                color: Theme.successTextColor
                font.family: Theme.fontUi
                font.pixelSize: 16
                font.weight: Font.Medium
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }
            PrimaryButton {
                Layout.alignment: Qt.AlignHCenter
                text: i18n("Close")
                onClicked: root.closed()
            }
        }

        // ---- failed --------------------------------------------------
        ColumnLayout {
            Layout.fillWidth: true
            visible: Mfa.respondState === "failed"
            spacing: 16

            Text {
                Layout.fillWidth: true
                text: "⚠"
                color: Theme.dangerColor
                font.pixelSize: 40
                horizontalAlignment: Text.AlignHCenter
            }
            Text {
                Layout.fillWidth: true
                text: Mfa.resultMessage
                color: Theme.dangerColor
                font.family: Theme.fontUi
                font.pixelSize: 16
                font.weight: Font.Medium
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }
            GhostButton {
                Layout.alignment: Qt.AlignHCenter
                text: i18n("Try Again")
                onClicked: Mfa.reset()
            }
        }

        Item { Layout.fillHeight: true; Layout.preferredHeight: 1 }
    }
}
