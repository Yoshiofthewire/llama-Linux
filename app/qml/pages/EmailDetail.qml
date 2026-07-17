import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtWebEngine
import com.urlxl.mail 1.0
import "../components"
import "../utils/format.js" as Format

// Task 35 -- plain reusable Item, deliberately NOT a Kirigami.Page (see
// Phase 6 global constraint 4): MobileRoot wraps this in a thin
// Kirigami.Page shell when it pushes it (Task 38); DesktopRoot embeds it
// directly inside its detail-pane Item (Task 39). This file itself must not
// assume which root is hosting it -- Reply/Reply All/Forward surface their
// pre-filled fields via composeRequested() instead of pushing Compose.qml
// themselves, and Archive/Junk/Delete surface completion via
// actionCompleted() instead of popping pageStack/clearing a selection
// directly.
Item {
    id: root

    // Public API.
    property string messageId: ""
    property string folder: "" // wire folder name, e.g. "INBOX"

    signal composeRequested(string to, string subject, string body)
    signal actionCompleted(string action) // action: "archive" | "junk" | "delete"

    implicitWidth: 360
    implicitHeight: 640

    // MailApp.findByMessageId() result -- a QVariantMap keyed the same way
    // as EmailListModel's roles (see MailController::findByMessageId's doc
    // comment). {} (empty map) means "nothing loaded yet" or "not cached".
    property var email: ({})
    property var attachments: [] // [{index, name, mimeType, size}, ...]
    property string attachmentStatus: ""

    onMessageIdChanged: reload()
    onFolderChanged: reload()
    Component.onCompleted: reload()

    // ---- data loading -------------------------------------------------

    function reload() {
        if (root.messageId === "") {
            root.email = {}
            root.attachments = []
            webView.loadHtml(renderedHtml(""))
            return
        }
        root.email = MailApp.findByMessageId(root.messageId)
        root.attachments = (root.email && root.email.hasAttachments)
            ? MailApp.listAttachments(root.folder, root.messageId)
            : []
        const source = (root.email && root.email.body) ? root.email.body
                                                         : (root.email ? root.email.preview : "")
        webView.loadHtml(renderedHtml(source || ""))
    }

    // ---- string-transform helpers (ported from Android's
    // EmailDetailActivity, per Task 35's brief -- kept here as plain JS
    // helpers rather than promoted to MailController, since none of them
    // touch anything beyond string manipulation of already-loaded QML data)
    // ---------------------------------------------------------------

    // If `raw` contains "<...>", returns the content between the first '<'
    // and the matching '>'; otherwise returns `raw` trimmed as-is.
    function extractAddress(raw) {
        const s = raw || ""
        const lt = s.indexOf("<")
        if (lt !== -1) {
            const gt = s.indexOf(">", lt)
            if (gt !== -1)
                return s.substring(lt + 1, gt).trim()
        }
        return s.trim()
    }

    // Splits a comma/semicolon-joined address-list string (Email.sentTo/.cc
    // wire format, see RelayMailSource.h) into trimmed, non-empty parts.
    function splitAddresses(raw) {
        if (!raw)
            return []
        return raw.split(/[,;]/).map(function (s) { return s.trim() })
                   .filter(function (s) { return s.length > 0 })
    }

    // Prepends "<prefix> " unless `subject` already starts with `prefix`
    // case-insensitively (no double "Re: Re:"/"Fwd: Fwd:" prefixing).
    function withPrefix(subject, prefix) {
        const s = subject || ""
        if (s.toLowerCase().indexOf(prefix.toLowerCase()) === 0)
            return s
        return prefix + " " + s
    }

    // First letter(s) of the sender's display name (or, absent a display
    // name, the local-part of the address), split on whitespace, up to 2
    // characters -- "reasonable initials logic" per Task 35's brief, same
    // shape as Avatar's other call sites. The actual whitespace-split-to-
    // initials core is shared (Format.initialsFromNamePart()); the
    // "Name <email>" parsing and email-local-part fallback stay here since
    // MobileRoot.qml's own sender-initials wrapper doesn't need the latter.
    function initialsFor(sender) {
        const s = sender || ""
        const lt = s.indexOf("<")
        let namePart = (lt !== -1 ? s.substring(0, lt) : s).trim()
        if (namePart.length === 0) {
            const addr = extractAddress(s)
            const at = addr.indexOf("@")
            namePart = at !== -1 ? addr.substring(0, at) : addr
        }
        return Format.initialsFromNamePart(namePart)
    }

    function formatSize(size) {
        if (size < 1024)
            return i18n("%1 B", size)
        if (size < 1048576)
            return i18n("%1 KB", (size / 1024).toFixed(1))
        return i18n("%1 MB", (size / 1048576).toFixed(1))
    }

    // ---- body HTML scaffold --------------------------------------------

    function colorToHex(c) {
        function pad(v) {
            const h = Math.round(v * 255).toString(16)
            return h.length === 1 ? "0" + h : h
        }
        return "#" + pad(c.r) + pad(c.g) + pad(c.b)
    }

    function escapeHtml(s) {
        return (s || "").replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;")
    }

    // HTML-vs-plain-text sniff per Task 35's brief: a handful of common tags
    // followed by whitespace/'>'/'/' is treated as "this is already HTML";
    // anything else is escaped and wrapped in <pre> so it renders literally.
    readonly property var htmlSniffRegex: /<(html|head|body|div|p|br|table|tr|td|a|img|span|ul|ol|li|h[1-6])[\s>/]/i

    function renderedHtml(body) {
        const inner = htmlSniffRegex.test(body) ? body : ("<pre>" + escapeHtml(body) + "</pre>")
        return "<html><head>"
            + "<meta charset=\"utf-8\">"
            + "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\" />"
            + "<style>"
            + "body { font-family: monospace; font-size: 14px; line-height: 1.5;"
            + " color: " + colorToHex(Theme.inkStrong) + "; background-color: " + colorToHex(Theme.bg) + ";"
            + " margin: 0; padding: 12px; word-break: break-word; }"
            + "a { color: " + colorToHex(Theme.accent) + "; }"
            + "img { max-width: 100%; height: auto; }"
            + "pre { white-space: pre-wrap; }"
            + "</style>"
            + "</head><body>" + inner + "</body></html>"
    }

    // ---- layout ----------------------------------------------------

    // Wrapped in a Flickable rather than anchored straight to root: the sum
    // of the header/actions/WebEngineView/attachments' minimum heights can
    // exceed whatever fixed-size pane/Kirigami.Page ends up hosting this
    // component (a long email plus a full attachment row, on a short
    // window), and plain ColumnLayout doesn't clip or scroll its own
    // overflow. Scrolling here, internally, means this component behaves
    // correctly regardless of how MobileRoot/DesktopRoot (Task 38/39) end
    // up sizing it, rather than relying on every future host to remember to
    // wrap it in a scrollable container itself.
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
            spacing: 12

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Avatar {
                initials: root.initialsFor(root.email.sender)
                size: 52
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Text {
                    Layout.fillWidth: true
                    text: root.email.subject || ""
                    color: Theme.inkStrong
                    font.family: Theme.fontUi
                    font.pixelSize: 20
                    font.weight: Font.Bold
                    wrapMode: Text.WordWrap
                }
                Text {
                    Layout.fillWidth: true
                    text: root.email.sender || ""
                    color: Theme.ink
                    font.family: Theme.fontUi
                    font.pixelSize: 13
                    elide: Text.ElideRight
                }
                Text {
                    // Raw ISO string as-is for v1 -- no date-formatting
                    // library decision needed this task (see Task 35 brief);
                    // follow-up: format this for display in a later task.
                    text: root.email.atUtc || ""
                    color: Theme.ink
                    opacity: 0.6
                    font.family: Theme.fontUi
                    font.pixelSize: 11
                }
            }
        }

        Flow {
            Layout.fillWidth: true
            spacing: 6
            visible: (root.email.keywords || []).length > 0

            Repeater {
                model: root.email.keywords || []
                delegate: PillTab {
                    // Non-interactive display mode: always selected:true,
                    // no onClicked wired up -- a stray tap just plays
                    // PillTab's own press-opacity animation with no effect,
                    // which reads fine for a read-only chip.
                    text: modelData
                    selected: true
                }
            }
        }

        // Six-action row (exact order/rules per Task 35 brief, ported from
        // Android's EmailDetailActivity). Flow (not RowLayout) so the six
        // buttons wrap onto a second line on narrow/mobile widths instead
        // of overflowing. Reply/Reply All/Forward are non-destructive and
        // use Primary/GhostButton (judgment call: Reply is the single most
        // likely next action, so it gets PrimaryButton; the rest of the
        // non-destructive five use GhostButton); Delete alone uses
        // DangerButton per the brief.
        Flow {
            Layout.fillWidth: true
            spacing: 8

            PrimaryButton {
                text: i18n("Reply")
                enabled: !MailApp.isBusy
                onClicked: {
                    const to = root.extractAddress(root.email.sender)
                    const subject = root.withPrefix(root.email.subject, "Re:")
                    const body = "\n\n" + i18n("%1 wrote:", root.email.sender) + "\n" + root.email.preview
                    root.composeRequested(to, subject, body)
                }
            }
            GhostButton {
                text: i18n("Reply All")
                enabled: !MailApp.isBusy
                onClicked: {
                    const addrs = [root.extractAddress(root.email.sender)]
                        .concat(root.splitAddresses(root.email.sentTo).map(root.extractAddress))
                        .concat(root.splitAddresses(root.email.cc).map(root.extractAddress))
                    const seen = {}
                    const deduped = []
                    for (let i = 0; i < addrs.length; i++) {
                        const a = addrs[i]
                        if (a.length > 0 && !seen[a]) {
                            seen[a] = true
                            deduped.push(a)
                        }
                    }
                    const subject = root.withPrefix(root.email.subject, "Re:")
                    const body = "\n\n" + i18n("%1 wrote:", root.email.sender) + "\n" + root.email.preview
                    root.composeRequested(deduped.join(", "), subject, body)
                }
            }
            GhostButton {
                text: i18n("Forward")
                enabled: !MailApp.isBusy
                onClicked: {
                    const subject = root.withPrefix(root.email.subject, "Fwd:")
                    const body = "\n\n" + i18n("---------- Forwarded message ----------")
                        + "\n" + i18n("From: %1", root.email.sender)
                        + "\n" + i18n("Subject: %1", root.email.subject)
                        + "\n\n" + root.email.preview
                    root.composeRequested("", subject, body)
                }
            }
            GhostButton {
                text: i18n("Archive")
                enabled: !MailApp.isBusy
                onClicked: {
                    if (MailApp.archiveEmails([root.messageId]))
                        root.actionCompleted("archive")
                }
            }
            GhostButton {
                text: i18n("Junk")
                enabled: !MailApp.isBusy
                onClicked: {
                    if (MailApp.markSpam([root.messageId]))
                        root.actionCompleted("junk")
                }
            }
            DangerButton {
                text: i18n("Delete")
                enabled: !MailApp.isBusy
                onClicked: {
                    if (MailApp.deleteEmails([root.messageId]))
                        root.actionCompleted("delete")
                }
            }
        }

        WebEngineView {
            id: webView
            Layout.fillWidth: true
            // A fixed preferredHeight, not fillHeight -- this now sits
            // inside contentColumn, which is sized by its own
            // implicitHeight (Flickable's contentHeight above), not by a
            // bounded parent height there'd be free space to "fill".
            Layout.preferredHeight: 360
            backgroundColor: Theme.bg

            // Email body HTML is untrusted content -- it comes from whatever
            // sender wrote the message, not from this app or the paired
            // relay server. Two settings changed from WebEngineView's
            // defaults to close the two classic mail-client HTML risks:
            // running the sender's JavaScript, and auto-fetching remote
            // <img> sources (a standard tracking-pixel/read-receipt leak --
            // it would fire on every open even though the HTML itself is
            // rendered via loadHtml(), since <img src="https://..."> is
            // still a real network fetch regardless of the base content
            // being local). Neither is needed for anything this view does
            // (link clicks are already intercepted below via
            // navigationRequested/openUrlExternally, not JS).
            settings.javascriptEnabled: false
            settings.autoLoadImages: false

            // Only a real user link click should escape to the system
            // browser -- the initial loadHtml() call above also produces a
            // navigationRequested event, but its navigationType is not
            // LinkClickedNavigation, so it's left alone here and proceeds
            // in-place as normal.
            onNavigationRequested: function (request) {
                if (request.navigationType === WebEngineNavigationRequest.LinkClickedNavigation) {
                    request.reject()
                    Qt.openUrlExternally(request.url)
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8
            visible: root.email.hasAttachments === true

            SectionLabel { text: i18n("Attachments") }

            Flow {
                Layout.fillWidth: true
                spacing: 8

                Repeater {
                    model: root.attachments
                    delegate: Rectangle {
                        radius: Theme.shapeButton
                        color: Theme.panel
                        border.width: 1
                        border.color: Theme.line
                        implicitWidth: chipRow.implicitWidth + 24
                        implicitHeight: chipRow.implicitHeight + 16

                        Row {
                            id: chipRow
                            anchors.centerIn: parent
                            spacing: 8

                            Text {
                                text: i18n("%1 (%2)", modelData.name, root.formatSize(modelData.size))
                                color: Theme.inkStrong
                                font.family: Theme.fontUi
                                font.pixelSize: 12
                            }
                        }

                        TapHandler {
                            onTapped: {
                                const ok = MailApp.downloadAttachment(
                                    root.folder, root.messageId, modelData.index, modelData.name)
                                root.attachmentStatus = ok ? i18n("Saved to Downloads") : MailApp.lastError
                                attachmentStatusTimer.restart()
                            }
                        }
                    }
                }
            }

            Text {
                text: root.attachmentStatus
                visible: root.attachmentStatus !== ""
                color: Theme.ink
                font.family: Theme.fontUi
                font.pixelSize: 12
            }
        }
        } // contentColumn
    } // flickable

    // No dedicated Toast component exists yet (Task 35 brief allows either
    // choice) -- a plain Text that self-clears via this Timer is enough for
    // a one-off "Saved to Downloads"/error line.
    Timer {
        id: attachmentStatusTimer
        interval: 3000
        onTriggered: root.attachmentStatus = ""
    }
}
