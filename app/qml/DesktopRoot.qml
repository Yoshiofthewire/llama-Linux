import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import org.kde.kirigami 2.20 as Kirigami
import com.urlxl.LlamaMail 1.0
import "components"
import "pages"

// Task 39 -- top-level desktop navigation shell, replacing the Task 1 stub.
// Per Phase 6 global constraint 4, this is a persistent 3-column layout
// (sidebar | list | detail) -- NOT pageStack push-navigation like
// MobileRoot.qml. Selection state (currentSection/currentFolder/
// selectedMessageId/selectedContactUid/detailMode) drives which content each
// pane shows; Tasks 35-37's plain-Item page components (EmailDetail/
// Compose/ContactsList/ContactDetail) are embedded directly here, never
// wrapped in a Kirigami.Page (that wrapping is MobileRoot-specific).
//
// Judgment call: a plain RowLayout with a fixed-width sidebar, a
// fill-width list column, and a fixed-width collapsible detail column --
// NOT a resizable SplitView. Global constraint 4 explicitly allows either;
// a RowLayout was chosen to avoid taking on SplitView's less-travelled
// hidden-item-reflow behavior in this codebase (nothing here uses SplitView
// yet) for a nice-to-have (user-resizable columns) the brief doesn't
// require. Documented here per that constraint's "your call, document it."
Kirigami.ApplicationWindow {
    id: root
    width: 1024
    height: 768
    visible: true
    title: "Llama Mail"

    // ---- selection state --------------------------------------------
    property string currentSection: "mail" // "mail" | "contacts"
    property string currentFolder: "INBOX" // wire folder name; meaningful when currentSection === "mail"
    property string selectedMessageId: ""
    property string selectedEmailFolder: ""
    property string selectedContactUid: ""
    // "empty" | "email" | "contact" | "compose" -- which component (if any)
    // the detail pane currently shows. Kept independent of
    // selectedMessageId/selectedContactUid (rather than inferring the mode
    // from which one is non-empty) so an in-progress Compose isn't
    // accidentally clobbered by whatever selection state happens to be
    // sitting in the other two.
    property string detailMode: "empty"
    property var composeSeed: ({ to: "", subject: "", body: "" })

    // Detail-pane collapse: in-memory only, not persisted via SettingsStore
    // -- this is a per-session view preference, not different from
    // countless other transient UI states in this app that don't survive a
    // restart (e.g. MobileRoot's activeTab). Adding a SettingsStore field
    // for one boolean felt like more plumbing than a "your call" judgment
    // call this size warrants; revisit if user feedback wants it to stick.
    property bool detailCollapsed: false

    function folderDisplayName(wireName) {
        const folders = MailApp.standardFolders()
        for (let i = 0; i < folders.length; i++) {
            if (folders[i].wireName === wireName)
                return folders[i].displayName
        }
        return wireName
    }

    function selectFolder(wireName) {
        root.currentSection = "mail"
        root.currentFolder = wireName
        root.selectedMessageId = ""
        root.detailMode = "empty"
        MailApp.selectFolder(wireName)
    }

    function selectContactsSection() {
        root.currentSection = "contacts"
        root.selectedContactUid = ""
        root.detailMode = "empty"
    }

    function selectEmail(messageId, folder) {
        root.selectedMessageId = messageId
        root.selectedEmailFolder = folder
        root.detailMode = "email"
    }

    function selectContact(uid) {
        root.selectedContactUid = uid
        root.detailMode = "contact"
    }

    // Seeds composeSeed BEFORE flipping detailMode, since the compose
    // Loader below only reads composeSeed at the moment it instantiates a
    // fresh Compose (see the Loader's own comment for why it's a Loader,
    // not a persistently-embedded instance like EmailDetail/ContactDetail).
    function openCompose(to, subject, body) {
        root.composeSeed = { to: to || "", subject: subject || "", body: body || "" }
        root.detailMode = "compose"
    }

    function closeDetail() {
        root.detailMode = "empty"
        root.selectedMessageId = ""
        root.selectedContactUid = ""
    }

    Component.onCompleted: MailApp.refresh()

    // ---- keyboard shortcuts ----------------------------------------------
    // Small, non-exhaustive set per the task-39 brief ("don't feel obligated
    // to replicate Mac's full shortcut set") -- refresh-current-section and
    // new-compose only. Ctrl+Shift+C is Mac's own Compose shortcut, added
    // alongside the more standard Ctrl+N rather than instead of it.
    Shortcut {
        sequence: "Ctrl+R"
        onActivated: {
            if (root.currentSection === "mail")
                MailApp.refresh()
            else
                ContactsApp.sync()
        }
    }
    Shortcut {
        sequence: "Ctrl+N"
        onActivated: root.openCompose("", "", "")
    }
    Shortcut {
        sequence: "Ctrl+Shift+C"
        onActivated: root.openCompose("", "", "")
    }

    // ---- Settings modal ----------------------------------------------
    // Kirigami.OverlaySheet, not a second ApplicationWindow -- Global
    // Constraint 4 allows either; OverlaySheet needs no second top-level
    // window/event-loop lifetime to manage, and Kirigami already renders it
    // centered over this window's content, which reads close enough to
    // Mac's separate Preferences window for this task's purposes. Settings
    // itself is a plain Item (same parent-agnostic shape as the other page
    // components) with its own "Done" button wired to close this sheet;
    // MobileRoot.qml's globalDrawer "Settings" action pushes the exact same
    // component wrapped in a Kirigami.Page instead.
    Kirigami.OverlaySheet {
        id: settingsSheet
        onOpened: settingsPane.refreshKeywordSettings()

        Settings {
            id: settingsPane
            implicitWidth: 480
            implicitHeight: 560
            onClosed: settingsSheet.close()
        }
    }

    // ---- compose: Loader, not a persistent embed -----------------------
    // Unlike EmailDetail/ContactDetail below (which rebind messageId/uid as
    // the selection changes and reload() in response -- see those files'
    // own doc comments confirming DesktopRoot is expected to do this),
    // Compose.qml only seeds its fields once, in Component.onCompleted (it
    // has no onInitialToChanged-style rebind hook -- MobileRoot doesn't
    // need one either, since pageStack.push() always creates a fresh
    // instance). A persistently-embedded Compose would only ever read
    // initialTo/initialSubject/initialBody on this file's own startup, so a
    // second Reply/Forward while Compose is already the detail pane's
    // content would silently keep showing the FIRST draft's prefill. A
    // Loader with active bound to detailMode fixes this cheaply: toggling
    // active off then back on destroys and recreates the item (per Qt's
    // documented Loader sizing/lifecycle rules), which re-runs
    // Component.onCompleted every time, matching pageStack.push()'s own
    // fresh-instance semantics.
    Component {
        id: composePaneComponent
        Compose {
            initialTo: root.composeSeed.to
            initialSubject: root.composeSeed.subject
            initialBody: root.composeSeed.body
            onSendSucceeded: root.closeDetail()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ---- top bar -----------------------------------------------------
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 52
            color: Theme.panel
            border.width: 1
            border.color: Theme.line

            RowLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 8

                Text {
                    Layout.fillWidth: true
                    text: "Llama Mail"
                    color: Theme.inkStrong
                    font.family: Theme.fontUi
                    font.pixelSize: 16
                    font.weight: Font.Bold
                }
                GhostButton {
                    text: root.detailCollapsed ? "Show Detail" : "Hide Detail"
                    onClicked: root.detailCollapsed = !root.detailCollapsed
                }
                PrimaryButton {
                    text: "Compose"
                    onClicked: root.openCompose("", "", "")
                }
                GhostButton {
                    text: "Settings"
                    onClicked: settingsSheet.open()
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // ---- sidebar ------------------------------------------------
            // Mac's exact order (task-39 brief): Inbox, then the 4 fixed
            // interior folders (Drafts, Junk, Sent, Trash), then Archive
            // last, under a "Mail" section; Contacts under a "People"
            // section below it. MailApp.standardFolders() already returns
            // exactly this order (see MailController::standardFolders()'s
            // kFolders array), so this just renders it as-is -- no
            // reordering needed here. Keyword tabs / folder-subfolder rows
            // from Mac's sidebar are out of scope this task (no
            // listFolders() call wired anywhere in this phase, see the
            // task-39 brief) -- deferred follow-up.
            Rectangle {
                Layout.preferredWidth: 200
                Layout.fillHeight: true
                color: Theme.panel
                border.width: 1
                border.color: Theme.line

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 4

                    SectionLabel { text: "Mail" }

                    Repeater {
                        model: MailApp.standardFolders()
                        delegate: Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: folderLabel.implicitHeight + 16
                            radius: Theme.shapeButton
                            readonly property bool isCurrent: root.currentSection === "mail"
                                && root.currentFolder === modelData.wireName
                            color: isCurrent ? Theme.accentSoft : (folderTap.pressed ? Theme.bg : "transparent")

                            Text {
                                id: folderLabel
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.left: parent.left
                                anchors.leftMargin: 10
                                text: modelData.displayName
                                color: Theme.inkStrong
                                font.family: Theme.fontUi
                                font.pixelSize: 14
                            }

                            TapHandler {
                                id: folderTap
                                onTapped: root.selectFolder(modelData.wireName)
                            }
                        }
                    }

                    Item { Layout.preferredHeight: 12 }

                    SectionLabel { text: "People" }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: contactsLabel.implicitHeight + 16
                        radius: Theme.shapeButton
                        color: root.currentSection === "contacts" ? Theme.accentSoft
                            : (contactsTap.pressed ? Theme.bg : "transparent")

                        Text {
                            id: contactsLabel
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            anchors.leftMargin: 10
                            text: "Contacts"
                            color: Theme.inkStrong
                            font.family: Theme.fontUi
                            font.pixelSize: 14
                        }

                        TapHandler {
                            id: contactsTap
                            onTapped: root.selectContactsSection()
                        }
                    }

                    Item { Layout.fillHeight: true }
                }
            }

            // ---- list column --------------------------------------------
            Rectangle {
                Layout.fillWidth: root.detailCollapsed
                Layout.preferredWidth: 340
                Layout.fillHeight: true
                color: Theme.bg
                border.width: 1
                border.color: Theme.line

                // ---- mail section: own ListView (no swipe actions -- a
                // right-click context menu with Delete replaces them per
                // the task-39 brief's explicit "reasonable minimum" call).
                // Same delegate content as MobileRoot's Inbox rows (Task
                // 38), minus swipe -- kept as a local duplicate rather than
                // a shared delegate component: the two hosts' interaction
                // models (swipe vs. select+right-click) differ enough that
                // a shared delegate would need its own mode-switching logic,
                // which isn't worth it for one delegate this size.
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 8
                    visible: root.currentSection === "mail"

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            Layout.fillWidth: true
                            text: root.folderDisplayName(root.currentFolder)
                            color: Theme.inkStrong
                            font.family: Theme.fontUi
                            font.pixelSize: 16
                            font.weight: Font.Bold
                            elide: Text.ElideRight
                        }
                        BusyIndicator {
                            running: MailApp.isBusy
                            visible: MailApp.isBusy
                            implicitWidth: 24
                            implicitHeight: 24
                        }
                        GhostButton {
                            text: "Refresh"
                            enabled: !MailApp.isBusy
                            onClicked: MailApp.refresh()
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        visible: MailApp.lastError !== ""
                        text: MailApp.lastError
                        color: Theme.dangerColor
                        font.family: Theme.fontUi
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                    }

                    EmptyState {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        visible: emailListView.count === 0
                        text: "No emails yet — tap Refresh."
                    }

                    ListView {
                        id: emailListView
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        visible: count > 0
                        clip: true
                        spacing: 4
                        model: MailApp.emailModel

                        delegate: Rectangle {
                            id: emailRow
                            width: emailListView.width
                            implicitHeight: emailRowContent.implicitHeight + 16
                            radius: Theme.shapeButton
                            readonly property bool isSelected: root.detailMode === "email"
                                && root.selectedMessageId === model.messageId
                            color: isSelected ? Theme.accentSoft : (emailTap.pressed ? Theme.panel : "transparent")

                            RowLayout {
                                id: emailRowContent
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.margins: 10
                                spacing: 10

                                Avatar {
                                    initials: emailRow.initialsForSender(model.sender)
                                    size: 32
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2

                                    Text {
                                        Layout.fillWidth: true
                                        text: model.sender
                                        color: Theme.inkStrong
                                        font.family: Theme.fontUi
                                        font.pixelSize: 13
                                        font.weight: Font.Medium
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: model.subject
                                        color: Theme.inkStrong
                                        font.family: Theme.fontUi
                                        font.pixelSize: 13
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: model.preview
                                        color: Theme.ink
                                        font.family: Theme.fontUi
                                        font.pixelSize: 12
                                        elide: Text.ElideRight
                                    }
                                }
                            }

                            // Same "reasonable initials logic" shape as
                            // EmailDetail.qml/MobileRoot.qml's own local
                            // copies -- see those files' comments on why
                            // this handful of lines is duplicated rather
                            // than shared.
                            function initialsForSender(sender) {
                                const s = sender || ""
                                const lt = s.indexOf("<")
                                const namePart = (lt !== -1 ? s.substring(0, lt) : s).trim()
                                const parts = namePart.split(/\s+/).filter(function (p) { return p.length > 0 })
                                let initials = ""
                                for (let i = 0; i < parts.length && initials.length < 2; i++)
                                    initials += parts[i].charAt(0).toUpperCase()
                                return initials.length > 0 ? initials : "?"
                            }

                            TapHandler {
                                id: emailTap
                                acceptedButtons: Qt.LeftButton
                                onTapped: root.selectEmail(model.messageId, model.folder)
                            }
                            TapHandler {
                                acceptedButtons: Qt.RightButton
                                onTapped: emailContextMenu.popup()
                            }

                            Menu {
                                id: emailContextMenu
                                MenuItem {
                                    text: "Delete"
                                    onTriggered: {
                                        if (MailApp.deleteEmails([model.messageId]) && root.selectedMessageId === model.messageId)
                                            root.closeDetail()
                                    }
                                }
                            }
                        }
                    }
                }

                // ---- contacts section: ContactsList.qml embedded as-is --
                // Reuses Task 36's component directly (same row content,
                // same Add/Sync header) rather than duplicating its
                // delegate -- its layout works fine anchored into this
                // column, per the task-39 brief's explicit "reuse it
                // directly if its layout works embedded" call.
                ContactsList {
                    anchors.fill: parent
                    visible: root.currentSection === "contacts"
                    onContactSelected: function (uid) { root.selectContact(uid) }
                }
            }

            // ---- detail column (collapsible) -----------------------------
            Rectangle {
                Layout.preferredWidth: 420
                Layout.fillHeight: true
                Layout.maximumWidth: root.detailCollapsed ? 0 : 420
                visible: !root.detailCollapsed
                clip: true
                color: Theme.bg
                border.width: 1
                border.color: Theme.line

                EmptyState {
                    anchors.centerIn: parent
                    visible: root.detailMode === "empty"
                    text: root.currentSection === "mail" ? "Select an email" : "Select a contact"
                }

                // EmailDetail/ContactDetail are embedded directly and kept
                // alive across selection changes -- both already rebind off
                // messageId/uid changes and reload() accordingly (see their
                // own doc comments confirming this is the DesktopRoot
                // pattern they were built for), unlike Compose above.
                EmailDetail {
                    anchors.fill: parent
                    visible: root.detailMode === "email"
                    messageId: root.detailMode === "email" ? root.selectedMessageId : ""
                    folder: root.detailMode === "email" ? root.selectedEmailFolder : ""
                    onComposeRequested: function (to, subject, body) { root.openCompose(to, subject, body) }
                    onActionCompleted: root.closeDetail()
                }

                ContactDetail {
                    anchors.fill: parent
                    visible: root.detailMode === "contact"
                    uid: root.detailMode === "contact" ? root.selectedContactUid : ""
                    onClosed: root.closeDetail()
                    onSaved: function (uid) { ContactsApp.load() }
                }

                Loader {
                    anchors.fill: parent
                    visible: active
                    active: root.detailMode === "compose"
                    sourceComponent: composePaneComponent
                }
            }
        }
    }
}
