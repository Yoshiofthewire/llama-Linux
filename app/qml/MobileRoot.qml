import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import org.kde.kirigami 2.20 as Kirigami
import com.urlxl.LlamaMail 1.0
import "components"
import "pages"

// Task 38 -- top-level mobile navigation shell, replacing the Task 1 stub.
// Kirigami.ApplicationWindow + pageStack (Kirigami.PageRow, single-column
// at this window's size) per Phase 6 global constraint 4: Inbox is
// pageStack.initialPage; EmailDetail/Compose/ContactsList/ContactDetail/
// Pairing (Tasks 35-37's plain-Item page components) are pushed wrapped in
// a thin Kirigami.Page shell -- those files are deliberately NOT
// Kirigami.Page themselves so DesktopRoot (Task 39) can embed them
// directly instead. MfaApproval.qml is intentionally left unrouted here:
// its own doc comment says its only real trigger (a notification tap) is a
// later-phase feature, and this task's brief doesn't ask for a dev entry
// point into it (Task 37's own manual-verification harness for it was
// fully reverted before that commit).
Kirigami.ApplicationWindow {
    id: root
    width: 360
    height: 640
    visible: true
    title: "Llama Mail"

    // ---- bottom tab bar selection state --------------------------------
    // "inbox" | "contacts" -- Compose is action-only and deliberately never
    // written here (see footer below / task-38 brief's exact semantics).
    property string activeTab: "inbox"

    function goInbox() {
        // Re-tapping Inbox while its root page is already showing
        // re-triggers a refresh (brief's exact spec); tapping it from
        // anywhere else (EmailDetail/Contacts/ContactDetail/Compose/
        // Pairing) just pops back to the Inbox root first. That "always
        // return to this section's root" half is this file's own
        // extension of the brief's narrower "re-tap while already there"
        // rule to the general pushed-page case -- standard bottom-tab-bar
        // behavior, not contradicted by anything in the brief.
        const alreadyAtInboxRoot = root.activeTab === "inbox" && root.pageStack.depth <= 1
        while (root.pageStack.depth > 1)
            root.pageStack.pop()
        root.activeTab = "inbox"
        if (alreadyAtInboxRoot)
            MailApp.refresh()
    }

    // Guards every pop() triggered by a *pushed page's own signal*
    // (actionCompleted/sendSucceeded/closed below) against firing when
    // depth is already 1. Confirmed by hand during this task's manual
    // verification that calling pageStack.pop() while only the Inbox root
    // page remains doesn't just harmlessly no-op -- it can tear down the
    // root page's own children (observed as the Inbox ListView/its
    // delegates' bindings throwing "Cannot read property of null"
    // immediately afterward, i.e. a real, reproducible corruption, not a
    // hypothetical). None of these 4 call sites *should* ever fire at
    // depth 1 in a single normal interaction, but a double-fired signal
    // (e.g. a fast double-tap on a page's own Delete/Send/Close/Done
    // button racing the pop that's already in flight) is exactly the kind
    // of double-fire this task was asked to scrutinize -- this guard
    // closes that window cheaply, independent of whatever guarding (or
    // lack of it) the hosted page component itself does internally.
    function safePop() {
        if (root.pageStack.depth > 1)
            root.pageStack.pop()
    }

    function goContacts() {
        if (root.activeTab === "contacts") {
            // Already in the Contacts section -- pop back to its root
            // (ContactsList) instead of pushing a second copy.
            while (root.pageStack.depth > 2)
                root.pageStack.pop()
            return
        }
        while (root.pageStack.depth > 1)
            root.pageStack.pop()
        root.activeTab = "contacts"
        root.pageStack.push(contactsListPageComponent)
    }

    // ---- deep-link auto-navigate to Pairing.qml ------------------------
    // main.cpp's routeDeepLink() -> Pairing.pairFromDeepLink() (Task 34)
    // drives Pairing.pairingState away from "idle" whenever a real
    // llamalabels://native-pair link arrives, whether or not this window
    // is already up. autoNavigatedForAttempt guards against
    // double-pushing Pairing.qml: pairingStateChanged can fire more than
    // once per attempt ("idle" -> "working" -> "paired"/"failed" each emit
    // it), so only the *first* transition away from "idle" pushes; the
    // flag resets back to false once pairingState returns to "idle" (a
    // fresh reset()/"Try Again", or removePairing()), arming the guard
    // again for the next attempt. Also checked against the currently
    // active page's objectName so a user who has already manually
    // navigated to Pairing.qml (via the global drawer) doesn't get a
    // second copy pushed on top of the one they're already looking at.
    property bool autoNavigatedForAttempt: false

    Connections {
        target: Pairing
        function onPairingStateChanged() {
            if (Pairing.pairingState === "idle") {
                root.autoNavigatedForAttempt = false
                return
            }
            if (root.autoNavigatedForAttempt)
                return
            root.autoNavigatedForAttempt = true
            if (!root.pageStack.currentItem || root.pageStack.currentItem.objectName !== "pairingPage")
                root.pageStack.push(pairingPageComponent)
        }
    }

    // ---- pushed-page wrappers -------------------------------------------
    // Each Component wraps one of Tasks 35-37's plain-Item page components
    // in a thin Kirigami.Page shell (Phase 6 global constraint 4), pushed
    // via pageStack.push(component, { ...initial property values... }).
    // Those properties apply to the Component's root object -- the
    // Kirigami.Page wrapper -- which forwards them into the hosted Item;
    // this indirection is the only way to seed initial property values on
    // a push without the wrapped Item itself needing to know about
    // push-navigation.

    Component {
        id: emailDetailPageComponent
        Kirigami.Page {
            id: emailDetailPage
            objectName: "emailDetailPage"
            title: "Email"
            property string messageId: ""
            property string folder: ""

            EmailDetail {
                anchors.fill: parent
                messageId: emailDetailPage.messageId
                folder: emailDetailPage.folder

                onComposeRequested: function (to, subject, body) {
                    root.pageStack.push(composePageComponent,
                        { initialTo: to, initialSubject: subject, initialBody: body })
                }
                // Archive/Junk/Delete have already happened by the time
                // this fires -- EmailDetail.qml only emits actionCompleted
                // after its own MailApp.* call reports success (see its
                // Archive/Junk/Delete onClicked handlers) -- so this just
                // returns to whatever pushed this page.
                onActionCompleted: root.safePop()
            }
        }
    }

    Component {
        id: composePageComponent
        Kirigami.Page {
            id: composePage
            objectName: "composePage"
            title: "Compose"
            property string initialTo: ""
            property string initialSubject: ""
            property string initialBody: ""

            Compose {
                anchors.fill: parent
                initialTo: composePage.initialTo
                initialSubject: composePage.initialSubject
                initialBody: composePage.initialBody
                onSendSucceeded: root.safePop()
            }
        }
    }

    Component {
        id: contactsListPageComponent
        Kirigami.Page {
            id: contactsListPage
            objectName: "contactsListPage"
            title: "Contacts"

            ContactsList {
                anchors.fill: parent
                onContactSelected: function (uid) {
                    root.pageStack.push(contactDetailPageComponent, { uid: uid })
                }
            }
        }
    }

    Component {
        id: contactDetailPageComponent
        Kirigami.Page {
            id: contactDetailPage
            objectName: "contactDetailPage"
            title: "Contact"
            property string uid: ""

            ContactDetail {
                anchors.fill: parent
                uid: contactDetailPage.uid
                onClosed: root.safePop()
            }
        }
    }

    Component {
        id: pairingPageComponent
        Kirigami.Page {
            id: pairingPage
            objectName: "pairingPage"
            title: "Pair Device"

            Pairing {
                anchors.fill: parent
                onClosed: root.safePop()
            }
        }
    }

    // ---- global drawer (hamburger menu): Settings + Pair Device --------
    globalDrawer: Kirigami.GlobalDrawer {
        title: "Llama Mail"
        actions: [
            Kirigami.Action {
                text: "Pair Device"
                onTriggered: root.pageStack.push(pairingPageComponent)
            },
            Kirigami.Action {
                text: "Settings"
                // Task 39 (Settings.qml / the Settings dialog) hasn't
                // landed in this branch state yet -- stubbed to do nothing
                // per the task-38 brief's explicit "don't block on it"
                // instruction.
                onTriggered: {}
            }
        ]
    }

    // ---- bottom Inbox / Compose / Contacts bar --------------------------
    // Judgment call: 3 PillTabs in a RowLayout, NOT Kirigami.NavigationTabBar.
    // NavigationTabBar's own delegate (org/kde/kirigami/controls/
    // NavigationTabBar.qml, confirmed installed at Kirigami 6.27.0)
    // unconditionally forces `action.checkable = true` on every action it's
    // given, and clicking a checkable AbstractButton toggles its own
    // `checked` property directly -- which destroys a QML `checked: <expr>`
    // binding the instant it's clicked. This bar's required semantics
    // (Compose is action-only and must NEVER visually become "selected", no
    // matter how many times it's tapped) can't be expressed cleanly against
    // that behavior, so this uses the fallback PillTab bar that the brief
    // and Phase 6 global constraint 4 explicitly allow instead.
    footer: Rectangle {
        width: parent ? parent.width : 0
        height: 64
        color: Theme.panel
        border.width: 1
        border.color: Theme.line

        RowLayout {
            anchors.fill: parent
            anchors.margins: 8
            spacing: 8

            PillTab {
                Layout.fillWidth: true
                text: "Inbox"
                selected: root.activeTab === "inbox"
                onClicked: root.goInbox()
            }
            PillTab {
                Layout.fillWidth: true
                text: "Compose"
                selected: false // action-only -- never participates in tab selection
                onClicked: root.pageStack.push(composePageComponent)
            }
            PillTab {
                Layout.fillWidth: true
                text: "Contacts"
                selected: root.activeTab === "contacts"
                onClicked: root.goContacts()
            }
        }
    }

    // ---- Inbox: pageStack.initialPage -----------------------------------
    pageStack.initialPage: Kirigami.Page {
        id: inboxPage
        objectName: "inboxPage"
        title: "Inbox"

        // Populates MailApp's model from the on-disk cache (and attempts a
        // network refresh -- see MailController::selectFolder/refresh's
        // doc comment) as soon as this page exists, so a cold launch shows
        // whatever's already cached rather than staying empty until the
        // user taps something. currentFolder already defaults to "INBOX"
        // in MailController's constructor, so a plain refresh() (not
        // selectFolder) is enough here.
        Component.onCompleted: MailApp.refresh()

        function currentFolderDisplayName() {
            const folders = MailApp.standardFolders()
            for (let i = 0; i < folders.length; i++) {
                if (folders[i].wireName === MailApp.currentFolder)
                    return folders[i].displayName
            }
            return MailApp.currentFolder
        }

        // Same "reasonable initials logic" shape as EmailDetail.qml's own
        // initialsFor() (Task 35) / ContactsList.qml's initialsFor() (Task
        // 36) -- kept as a local duplicate here rather than promoted to a
        // shared module, matching both of those files' own stated
        // reasoning (a handful of lines, no other coupling between call
        // sites).
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

        function formatTimestamp(atUtc) {
            if (!atUtc)
                return ""
            const d = new Date(atUtc)
            return isNaN(d.getTime()) ? atUtc : Qt.formatDateTime(d, "MMM d, hh:mm")
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 10

            // ---- folder picker + refresh header -----------------------
            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                GhostButton {
                    text: "Folder: " + inboxPage.currentFolderDisplayName()
                    onClicked: folderPopup.open()
                }
                Item { Layout.fillWidth: true }
                BusyIndicator {
                    running: MailApp.isBusy
                    visible: MailApp.isBusy
                    implicitWidth: 28
                    implicitHeight: 28
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

            // ---- keyword pill row ---------------------------------------
            // "All" is hardcoded first and always present (not part of
            // MailApp.keywordTabs -- see MailController::keywordTabs's own
            // doc comment); the rest come straight from keywordTabs.
            // Horizontally-scrolling Flickable rather than a wrapping Flow:
            // this is meant to read as a single scannable filter row, not
            // a multi-line block that pushes the list down as more
            // keywords accumulate.
            Flickable {
                Layout.fillWidth: true
                implicitHeight: keywordRow.implicitHeight
                contentWidth: keywordRow.implicitWidth
                contentHeight: height
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                flickableDirection: Flickable.HorizontalFlick

                Row {
                    id: keywordRow
                    spacing: 8

                    PillTab {
                        text: "All"
                        selected: MailApp.selectedKeyword === ""
                        onClicked: MailApp.selectKeyword("")
                    }
                    Repeater {
                        model: MailApp.keywordTabs
                        delegate: PillTab {
                            text: modelData.name + " (" + modelData.count + ")"
                            selected: MailApp.selectedKeyword === modelData.name
                            onClicked: MailApp.selectKeyword(modelData.name)
                        }
                    }
                }
            }

            // ---- email list -----------------------------------------------
            // No unread/read visual weight: Email.status (core/models/
            // Email.h) has no documented enumeration of values anywhere in
            // core/ and nothing in the seeded/test data conventions implies
            // one either -- per the task-38 brief's own instruction, this
            // skips inventing an "unread" signal rather than guessing at
            // one that doesn't exist in the current schema.
            EmptyState {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: emailListView.count === 0
                // "tap Refresh", not "pull to refresh" -- this screen uses
                // the visible-Refresh-button fallback (see the header
                // above), not an actual pull gesture; see task-38-report.md
                // for why.
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
                // Recycling isn't worth the correctness hazard here: a
                // reused SwipeDelegate would keep its `actionTriggered`
                // guard (see below) across whichever new model row it gets
                // rebound to, which could silently disable that row's
                // swipe actions forever. This app's inbox lists are small
                // (a single relay folder, no infinite scroll), so disabling
                // delegate reuse costs nothing observable.
                reuseItems: false

                delegate: SwipeDelegate {
                    id: emailRow
                    width: emailListView.width

                    // Guards against a double-fire if a user manages to
                    // trigger two click events on the same revealed action
                    // label in quick succession (e.g. a fast double-tap)
                    // before the model updates and this row disappears --
                    // MailApp.archiveEmails/deleteEmails run synchronously
                    // (Phase 6 global constraint 2), so this flag fully
                    // closes that window for a given row instance.
                    property bool actionTriggered: false

                    contentItem: RowLayout {
                        spacing: 12

                        Avatar {
                            initials: inboxPage.initialsForSender(model.sender)
                            size: 40
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Text {
                                    Layout.fillWidth: true
                                    text: model.sender
                                    color: Theme.inkStrong
                                    font.family: Theme.fontUi
                                    font.pixelSize: 14
                                    font.weight: Font.Medium
                                    elide: Text.ElideRight
                                }
                                Text {
                                    text: inboxPage.formatTimestamp(model.atUtc)
                                    color: Theme.ink
                                    font.family: Theme.fontUi
                                    font.pixelSize: 11
                                }
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

                    background: Rectangle {
                        color: emailRow.pressed ? Theme.panel : Theme.bg
                    }

                    // Swipe RIGHT reveals Delete. Per SwipeDelegate's own
                    // documented contract (verified against the installed
                    // Qt 6 docs, not assumed from the property name):
                    // `swipe.left` is the delegate that gets revealed when
                    // the control is swiped to the right.
                    swipe.left: Rectangle {
                        anchors.fill: parent
                        color: Theme.dangerColor

                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: 20
                            anchors.verticalCenter: parent.verticalCenter
                            text: "Delete"
                            // readableOnAccent reuses the same "light text
                            // on a solid saturated background" pairing
                            // already established for PillTab's selected
                            // state / PrimaryButton, rather than inventing
                            // a new semantic-color pairing for this one
                            // spot.
                            color: Theme.readableOnAccent
                            font.family: Theme.fontUi
                            font.pixelSize: 14
                            font.weight: Font.Medium

                            SwipeDelegate.onClicked: {
                                if (emailRow.actionTriggered)
                                    return
                                emailRow.actionTriggered = true
                                MailApp.deleteEmails([model.messageId])
                            }
                        }
                    }

                    // Swipe LEFT reveals Archive. `swipe.right` is revealed
                    // when the control is swiped to the left (same
                    // documented contract as above).
                    swipe.right: Rectangle {
                        anchors.fill: parent
                        color: Theme.warningColor

                        Text {
                            anchors.right: parent.right
                            anchors.rightMargin: 20
                            anchors.verticalCenter: parent.verticalCenter
                            text: "Archive"
                            color: Theme.readableOnAccent
                            font.family: Theme.fontUi
                            font.pixelSize: 14
                            font.weight: Font.Medium

                            SwipeDelegate.onClicked: {
                                if (emailRow.actionTriggered)
                                    return
                                emailRow.actionTriggered = true
                                MailApp.archiveEmails([model.messageId])
                            }
                        }
                    }

                    onClicked: root.pageStack.push(emailDetailPageComponent,
                        { messageId: model.messageId, folder: model.folder })
                }
            }
        }

        // ---- folder picker popup -----------------------------------------
        Popup {
            id: folderPopup
            modal: true
            focus: true
            closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
            padding: 8
            // Centered over the whole window rather than anchored beneath
            // the Folder button -- simpler and just as discoverable given
            // there are only 6 standard folders, and avoids doing
            // mapToItem coordinate math for a dropdown-style anchor.
            x: (root.width - width) / 2
            y: (root.height - height) / 2

            background: Rectangle {
                color: Theme.panel
                radius: Theme.shapePanel
                border.width: 1
                border.color: Theme.line
            }

            contentItem: ColumnLayout {
                spacing: 4

                Repeater {
                    model: MailApp.standardFolders()
                    delegate: Rectangle {
                        Layout.fillWidth: true
                        implicitWidth: 200
                        implicitHeight: folderLabel.implicitHeight + 16
                        radius: Theme.shapeButton
                        color: folderTap.pressed ? Theme.bg : "transparent"

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
                            onTapped: {
                                MailApp.selectFolder(modelData.wireName)
                                folderPopup.close()
                            }
                        }
                    }
                }
            }
        }
    }
}
