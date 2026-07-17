import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import com.urlxl.mail 1.0

// New component (extended-contact-fields Task 5): ContactDetail.qml's edit
// form gained five list-typed fields (ims/websites/relations/events/
// customFields) that need genuine add/remove list-editing UI -- nothing
// existing covers this (ThemedTextField is single-value; Compose.qml's
// attachment chips are display-only, no per-field editing). One generic,
// data-driven component covers all five call sites rather than five
// near-duplicate blocks, per the task brief's explicit recommendation.
//
// Public API: `columns` is an ordered array of {key, placeholder} describing
// each entry's string sub-fields -- 2 columns for websites/relations/events/
// customFields, 3 for ims' service/label/value -- so this one component
// handles every list field without a per-type subclass. `entries` is a
// plain JS array of {key: value, ...} objects matching `columns`' keys;
// ContactDetail.qml reads `.entries` back out directly in trySave(), and
// QML's JS-array<->QVariantList-of-QVariantMap bridging does the rest (same
// bridging contactAt() already relies on for the read side).
//
// Judgment call on mutation strategy: adding/removing a row reassigns
// `entries` (a fresh array), which is the only time this component asks the
// Repeater to rebuild its delegates. Editing text within a row mutates the
// row's object *in place* (no reassignment) instead of rebinding `entries`
// per keystroke -- reassigning per keystroke would make the Repeater treat
// the plain-JS-array model as entirely new on every character typed,
// destroying and recreating every TextField and dropping keyboard focus
// mid-word. In-place mutation keeps `entries` correct for trySave() to read
// later without fighting the Repeater over who owns focus while the user is
// typing.
ColumnLayout {
    id: root

    // [{key: "value", placeholder: "..."}, ...] -- static per call site, set
    // once by ContactDetail.qml.
    property var columns: []
    // [{<column.key>: <string>, ...}, ...] -- the live list being edited.
    property var entries: []

    Layout.fillWidth: true
    spacing: 6

    function add() {
        const blank = {}
        for (let i = 0; i < root.columns.length; i++)
            blank[root.columns[i].key] = ""
        root.entries = root.entries.concat([blank])
    }

    function removeAt(index) {
        const updated = root.entries.slice()
        updated.splice(index, 1)
        root.entries = updated
    }

    Repeater {
        model: root.entries
        delegate: RowLayout {
            id: rowDelegate
            // Captures this row's outer index/modelData under distinct
            // names before the nested Repeater below shadows the bare
            // `index`/`modelData` identifiers with its own.
            property int entryIndex: index
            property var entryData: modelData

            Layout.fillWidth: true
            spacing: 6

            Repeater {
                model: root.columns
                delegate: ThemedTextField {
                    Layout.fillWidth: true
                    placeholderText: modelData.placeholder
                    text: rowDelegate.entryData[modelData.key] || ""
                    // In-place mutation -- see the type-level comment above
                    // for why this doesn't reassign root.entries.
                    onTextChanged: root.entries[rowDelegate.entryIndex][modelData.key] = text
                }
            }

            Text {
                text: "✕"
                color: Theme.ink
                font.family: Theme.fontUi
                font.pixelSize: 16

                TapHandler {
                    onTapped: root.removeAt(rowDelegate.entryIndex)
                }
            }
        }
    }

    GhostButton {
        text: i18n("+ Add")
        onClicked: root.add()
    }
}
