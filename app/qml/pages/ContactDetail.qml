import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import com.urlxl.LlamaMail 1.0
import "../components"

// Task 36 -- plain reusable Item, deliberately NOT a Kirigami.Page (see
// Phase 6 global constraint 4): MobileRoot wraps this in a thin
// Kirigami.Page shell when it pushes it (Task 38); DesktopRoot embeds it
// directly inside its detail-pane Item, rebinding `uid` as the selected
// contact changes rather than re-instantiating this component (Task 39) --
// that's why both Component.onCompleted AND onUidChanged reload below,
// mirroring EmailDetail.qml's onMessageIdChanged/onFolderChanged +
// Component.onCompleted shape (Task 35).
//
// Matches Mac's simpler 3-field edit form over Android's richer one (fn/
// org/notes/single-email/single-phone) -- this is the exact contract
// ContactsController::createContact/updateContact accept (see its header
// doc comment); no multi-email/multi-phone UI here, "extras" beyond index 0
// are preserved server-side by updateContact itself, not managed by this
// form at all.
Item {
    id: root

    // Public API. "" means "Add" (create mode); a non-empty uid means
    // "view/edit an existing contact". Whichever root hosts this component
    // owns the actual uid value (a fresh push with uid: "", a tapped list
    // row's uid, or a persistently-rebound selection) -- this file never
    // assumes push-navigation vs. a pane, per constraint 4.
    property string uid: ""

    // Emitted when a Cancel (from a fresh "Add"), a successful "Add" Save,
    // or a Delete completes -- the signal for "this screen is done, please
    // navigate back / clear the selection". NOT emitted when Cancel/Save
    // happen from edit mode on an *existing* contact -- those return to
    // this same component's own read-only view instead, since there is
    // somewhere sensible to go back to without leaving the screen.
    signal closed()
    // Emitted whenever createContact/updateContact succeeds, carrying the
    // (possibly newly-assigned) uid -- lets a host refresh a list/selection
    // without this component needing to know anything about lists.
    signal saved(string uid)

    implicitWidth: 360
    implicitHeight: 640

    // "view" (read-only card) or "edit" (form). Recomputed by loadContact()
    // whenever `uid` changes; edit-mode Cancel/Save-success on an existing
    // contact flips this directly without going through loadContact() (see
    // cancelEdit()/trySave()) since neither of those changes `uid`.
    property string mode: "view"
    // Raw ContactsApp.contactAt(uid) result -- {} until loadContact() runs.
    // Read-only card binds to this directly; edit mode copies it into the
    // form fields once, on entry (beginEdit()/clearFormFields()), so typing
    // in the form doesn't live-edit the read-only card underneath it.
    property var contact: ({})
    // True iff this component loaded with an empty uid -- i.e. this is an
    // "Add" flow, not "Edit an existing contact". Drives Cancel/Save-success
    // navigation semantics (see cancelEdit()/trySave()): an Add flow has no
    // read-only view to fall back to, so it always ends by emitting
    // closed(); an edit-existing flow falls back to its own view mode
    // instead.
    property bool wasNew: false

    // Best-effort "has this round-tripped through a successful sync"
    // read, per Task 33's ContactListModel::SyncedRole doc comment (`rev !=
    // 0`) -- same field, same one-line formula, not a second independent
    // heuristic. ContactsController's fixed Task 33 surface has no
    // Q_INVOKABLE that returns a single row's `synced` role by uid (only
    // the full contactModel exposes that role, and QAbstractItemModel isn't
    // safely queryable by uid from plain QML/JS outside a delegate without
    // reaching for fragile, undocumented API) -- contactAt()'s `rev` field
    // is the one piece of the same underlying data this component actually
    // has, so the identical formula is applied here directly rather than
    // inventing a different signal for "is this synced".
    readonly property bool synced: root.contact.rev !== undefined && root.contact.rev !== 0

    // label/value pairs for the read-only card, in the brief's specified
    // Email/Phone/Org/Notes order. A Repeater over this avoids writing the
    // same RowLayout{SectionLabel+Text} block out four times by hand.
    readonly property var detailRows: [
        { label: i18nc("contact detail field label, the person's email address", "Email"), value: (root.contact.emails && root.contact.emails.length > 0) ? root.contact.emails[0].value : "" },
        { label: i18n("Phone"), value: (root.contact.phones && root.contact.phones.length > 0) ? root.contact.phones[0].value : "" },
        { label: i18n("Org"), value: root.contact.org || "" },
        { label: i18n("Notes"), value: root.contact.notes || "" },
    ]

    Component.onCompleted: loadContact()
    onUidChanged: loadContact()

    // ---- state transitions ------------------------------------------

    function loadContact() {
        if (root.uid === "") {
            root.wasNew = true
            root.contact = {}
            root.mode = "edit"
            clearFormFields()
        } else {
            root.wasNew = false
            root.contact = ContactsApp.contactAt(root.uid)
            root.mode = "view"
        }
    }

    function beginEdit() {
        loadFormFromContact()
        root.mode = "edit"
    }

    function cancelEdit() {
        // An "Add" flow has no existing contact to fall back to -- discard
        // and tell the host to navigate back. Editing an existing contact
        // just drops the in-progress form and returns to its own read-only
        // card (loadFormFromContact() will re-sync the form from
        // root.contact, unchanged, if the user re-enters edit mode later).
        if (root.wasNew)
            root.closed()
        else
            root.mode = "view"
    }

    function trySave() {
        const fields = {
            fn: nameField.text.trim(),
            org: orgField.text,
            notes: notesArea.text,
            email: emailField.text.trim(),
            phone: phoneField.text.trim(),
        }
        if (root.wasNew) {
            const newUid = ContactsApp.createContact(fields)
            if (newUid !== "") {
                root.saved(newUid)
                root.closed()
            }
            // else: ContactsApp.lastError is now set and displayed below;
            // stay in the form so the user can fix it and retry.
        } else {
            const ok = ContactsApp.updateContact(root.uid, fields)
            if (ok) {
                root.contact = ContactsApp.contactAt(root.uid)
                root.mode = "view"
                root.saved(root.uid)
            }
        }
    }

    function doDelete() {
        // No confirmation dialog -- matches Android's behavior exactly, and
        // Mac's reviewed source doesn't have one either (per the brief).
        ContactsApp.deleteContact(root.uid, root.contact.rev || 0)
        root.closed()
    }

    function loadFormFromContact() {
        nameField.text = root.contact.fn || ""
        orgField.text = root.contact.org || ""
        notesArea.text = root.contact.notes || ""
        const emails = root.contact.emails || []
        emailField.text = emails.length > 0 ? emails[0].value : ""
        const phones = root.contact.phones || []
        phoneField.text = phones.length > 0 ? phones[0].value : ""
    }

    function clearFormFields() {
        nameField.text = ""
        orgField.text = ""
        notesArea.text = ""
        emailField.text = ""
        phoneField.text = ""
    }

    // Same "up to 2 characters from whitespace-split name parts" shape as
    // ContactsList.qml's initialsFor() -- see that file's comment on why
    // this small helper is duplicated rather than shared.
    function initialsFor(fn) {
        const s = (fn || "").trim()
        if (s.length === 0)
            return "?"
        const parts = s.split(/\s+/).filter(function (p) { return p.length > 0 })
        let initials = ""
        for (let i = 0; i < parts.length && initials.length < 2; i++)
            initials += parts[i].charAt(0).toUpperCase()
        return initials
    }

    // ---- layout ----------------------------------------------------

    // Wrapped in a Flickable for the same reason as EmailDetail.qml (Task
    // 35): the read-only card's rows plus its two action buttons, or the
    // edit form's five fields plus its two buttons, can exceed whatever
    // fixed-size pane/Kirigami.Page ends up hosting this component on a
    // short window -- scrolling internally means this component behaves
    // correctly regardless of how MobileRoot/DesktopRoot (Task 38/39) size
    // it.
    Flickable {
        id: flickable
        anchors.fill: parent
        contentWidth: width
        contentHeight: contentColumn.implicitHeight + contentColumn.anchors.margins * 2
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: contentColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 16
            spacing: 16

            // ---- read-only card -------------------------------------

            ColumnLayout {
                Layout.fillWidth: true
                visible: root.mode === "view"
                spacing: 16

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    Avatar {
                        initials: root.initialsFor(root.contact.fn)
                        size: 72
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        Text {
                            Layout.fillWidth: true
                            text: (root.contact.fn && root.contact.fn.length > 0) ? root.contact.fn : i18n("Unnamed")
                            color: Theme.inkStrong
                            font.family: Theme.fontUi
                            font.pixelSize: 22
                            font.weight: Font.Bold
                            wrapMode: Text.WordWrap
                        }
                        StatusBadge {
                            active: root.synced
                            text: root.synced ? i18n("Synced") : i18n("Local")
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Repeater {
                        model: root.detailRows
                        delegate: RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            SectionLabel {
                                Layout.preferredWidth: 70
                                text: modelData.label
                            }
                            Text {
                                Layout.fillWidth: true
                                text: modelData.value !== "" ? modelData.value : "—"
                                color: Theme.inkStrong
                                font.family: Theme.fontMono
                                font.pixelSize: 14
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    GhostButton {
                        text: i18n("Edit")
                        onClicked: root.beginEdit()
                    }
                    DangerButton {
                        text: i18n("Delete")
                        visible: root.uid !== ""
                        onClicked: root.doDelete()
                    }
                    Item { Layout.fillWidth: true }
                }
            }

            // ---- edit form -------------------------------------------

            ColumnLayout {
                Layout.fillWidth: true
                visible: root.mode === "edit"
                spacing: 10

                SectionLabel {
                    text: root.wasNew ? i18n("Add contact") : i18n("Edit contact")
                }

                ThemedTextField {
                    id: nameField
                    Layout.fillWidth: true
                    placeholderText: i18n("Name")
                }
                ThemedTextField {
                    id: orgField
                    Layout.fillWidth: true
                    placeholderText: i18n("Org")
                }
                ThemedTextField {
                    id: emailField
                    Layout.fillWidth: true
                    placeholderText: i18nc("contact detail field label, the person's email address", "Email")
                }
                ThemedTextField {
                    id: phoneField
                    Layout.fillWidth: true
                    placeholderText: i18n("Phone")
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 120
                    radius: Theme.shapeField
                    color: Theme.panel
                    border.width: 1
                    border.color: notesArea.activeFocus ? Theme.accent : Theme.line

                    Behavior on border.color {
                        ColorAnimation { duration: 120 }
                    }

                    Flickable {
                        anchors.fill: parent
                        anchors.margins: 10
                        contentWidth: width
                        contentHeight: notesArea.implicitHeight
                        clip: true

                        TextArea {
                            id: notesArea
                            width: parent.width
                            wrapMode: TextArea.Wrap
                            color: Theme.inkStrong
                            placeholderTextColor: Theme.ink
                            font.family: Theme.fontUi
                            font.pixelSize: 14
                            background: null
                            selectByMouse: true
                            placeholderText: i18n("Notes…")
                        }
                    }
                }

                Text {
                    Layout.fillWidth: true
                    visible: ContactsApp.lastError !== ""
                    text: ContactsApp.lastError
                    color: Theme.dangerColor
                    font.family: Theme.fontUi
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    GhostButton {
                        text: i18n("Cancel")
                        onClicked: root.cancelEdit()
                    }
                    Item { Layout.fillWidth: true }
                    PrimaryButton {
                        text: i18n("Save")
                        enabled: nameField.text.trim() !== ""
                        onClicked: root.trySave()
                    }
                }
            }
        } // contentColumn
    } // flickable
}
