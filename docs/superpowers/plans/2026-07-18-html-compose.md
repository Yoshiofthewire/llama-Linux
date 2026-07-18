# HTML Compose (rich body + button-styled links) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace Compose's plain-text `TextArea` body with a WYSIWYG HTML editor (Bold/Italic/Hyperlink, with button-styled links) and send the result as `text/html` mail, reversing the earlier "plain text only" constraint documented in `Compose.qml`.

**Architecture:** A new self-contained `RichBodyEditor.qml` component wraps a `WebEngineView` running a `contenteditable` page (execCommand for Bold/Italic/insert-link, paste forced to plain text, a DOM-walking JS sanitizer run before every send). `Compose.qml` swaps its `TextArea` for this component and its `trySend()`/pop-out logic become callback-based to accommodate the inherently async `runJavaScript` extraction. `MailController::sendMail` stops hardcoding `mode: "plain"` and sends `"html"` instead — the backend (a separate repo) already supports this mode end-to-end.

**Tech Stack:** Qt6/QML, QtWebEngine (`WebEngineView`, already a dependency via `EmailDetail.qml`), Qt6::Test/ctest for the one C++ change.

## Global Constraints

- No `multipart/alternative` plain-text fallback — HTML-only send, per the approved spec (`docs/superpowers/specs/2026-07-18-html-compose-design.md`).
- No toolbar active-state sync (Bold/Italic buttons never show a "currently active" indicator).
- Toolbar is exactly Bold, Italic, Hyperlink (with a button-styling option on the Hyperlink dialog) — no lists, headings, font size/color.
- No changes to `kypost-server` (a separate repo/checkout) — this plan is scoped entirely to this repo.
- Sanitize all outgoing HTML to an allowlist (`p`, `br`, `div`, `b`/`strong`, `i`/`em`, `a`; `href` restricted to `http(s):`/`mailto:`, `style` restricted to a fixed property list) before it's ever handed to `MailController::sendMail`.
- Paste is always forced to plain text inside the rich editor.

---

### Task 1: Backend send mode — `"plain"` → `"html"`

**Files:**
- Modify: `app/mail/MailController.h:69-70` (doc comment), `app/mail/MailController.cpp:263-264`
- Test: `tests/app/mail/MailControllerTest.cpp`

**Interfaces:**
- Consumes: `RelayMailSource::sendMail(QUrl, RelayAuth, QString to, QString cc, QString bcc, QString subject, QString body, QString mode, QVector<MailAttachmentUpload>)` — unchanged signature, only the `mode` argument's value changes at the one call site in `MailController::sendMail`.
- Produces: nothing new consumed by later tasks — this task is purely a mode-string flip plus its regression test.

- [ ] **Step 1: Write the failing test**

Add this test method to `tests/app/mail/MailControllerTest.cpp`. Declare it in the `private slots:` list (after `sendMailOverAttachmentCapRejectsBeforeAnyNetworkCall`) and implement it after that test's body:

```cpp
    void sendMailUsesHtmlSendMode();
```

```cpp
void MailControllerTest::sendMailUsesHtmlSendMode()
{
    Database db;
    QVERIFY(db.open(QStringLiteral(":memory:")));
    EmailDao emailDao(db.handle());

    FakeRelayServer fake(httpResponse(200, "OK", R"({"ok":true,"sentSaved":true,"warning":""})"));

    QTemporaryDir secureDir;
    QVERIFY(secureDir.isValid());
    SecureStoreFile secureStore(secureDir.path());
    PairingStore pairingStore(secureStore);
    savePairing(pairingStore, fake.port());

    QTemporaryDir cursorDir;
    QVERIFY(cursorDir.isValid());
    CursorStore cursorStore(cursorDir.filePath(QStringLiteral("cursor.ini")));

    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    SettingsStore settingsStore(settingsDir.filePath(QStringLiteral("settings.ini")));
    KeywordRepository keywordRepository(settingsStore);

    QNetworkAccessManager manager;
    HttpClient http(manager);
    RelayMailSource source(http);
    MailRepository mailRepository(source, emailDao, pairingStore, cursorStore);

    MailController controller(mailRepository, source, keywordRepository, pairingStore);

    const bool ok = controller.sendMail(QStringLiteral("to@example.com"), QString(), QString(),
                                         QStringLiteral("Subject"), QStringLiteral("<b>Body</b>"), {});

    QCOMPARE(ok, true);
    const QJsonObject sent = fake.receivedJsonBody();
    QCOMPARE(sent.value(QStringLiteral("mode")).toString(), QStringLiteral("html"));
    QCOMPARE(sent.value(QStringLiteral("body")).toString(), QStringLiteral("<b>Body</b>"));
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `cmake --build build --target MailControllerTest && ./build/tests/MailControllerTest sendMailUsesHtmlSendMode`
(adjust `build` to this repo's actual build directory if different)

Expected: FAIL — `QCOMPARE(sent.value("mode").toString(), "html")` reports the actual value as `"plain"`.

- [ ] **Step 3: Change the hardcoded mode**

In `app/mail/MailController.cpp`, change:

```cpp
    const SendMailResult result = m_relayMailSource.sendMail(serverBaseUrl, auth, to, cc, bcc, subject, body,
                                                               QStringLiteral("plain"), attachments);
```

to:

```cpp
    const SendMailResult result = m_relayMailSource.sendMail(serverBaseUrl, auth, to, cc, bcc, subject, body,
                                                               QStringLiteral("html"), attachments);
```

Also update the doc comment above `sendMail`'s declaration in `app/mail/MailController.h:69-70` — it currently reads:

```cpp
    // mode is hardcoded "plain" per Phase 6 global constraint 5. Reads each
```

Change to:

```cpp
    // mode is hardcoded "html" -- Compose.qml's RichBodyEditor is the sole
    // caller and always produces sanitized HTML (see
    // docs/superpowers/specs/2026-07-18-html-compose-design.md). Reads each
```

- [ ] **Step 4: Run the test to verify it passes**

Run: `cmake --build build --target MailControllerTest && ./build/tests/MailControllerTest`

Expected: PASS, all `MailControllerTest` cases including the new one.

- [ ] **Step 5: Commit**

```bash
git add app/mail/MailController.h app/mail/MailController.cpp tests/app/mail/MailControllerTest.cpp
git commit -m "Send Compose mail as HTML instead of plain text"
```

---

### Task 2: RichBodyEditor.qml — WYSIWYG surface (Bold/Italic), wired into Compose

**Files:**
- Create: `app/qml/components/RichBodyEditor.qml`
- Modify: `app/qml/pages/Compose.qml` (body section, `Component.onCompleted`, `trySend()`, `currentDraftForPopOut()`, pop-out button handler, header comment)
- Modify: `app/qml/qml.qrc` (register the new file)

**Interfaces:**
- Produces (consumed by Task 3 and by `Compose.qml`):
  - `RichBodyEditor.loadInitialHtml(html: string)` — seeds the editor's content. Call exactly once, right after construction.
  - `RichBodyEditor.requestSendableHtml(callback: function(result))` — `result` is `{ html: string, isEmpty: bool }`. Asynchronous — crosses the WebEngine process boundary via `runJavaScript`.
  - `RichBodyEditor` also owns a `HyperlinkDialog` instance internally (Task 3 adds that dialog's file; this task only wires an `IconButton` placeholder that will open it).

- [ ] **Step 1: Register the new file in `qml.qrc`**

In `app/qml/qml.qrc`, add a line after `<file>components/AddressBookPickerDialog.qml</file>`:

```xml
    <file>components/RichBodyEditor.qml</file>
```

- [ ] **Step 2: Create `app/qml/components/RichBodyEditor.qml`**

```qml
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtWebEngine
import com.urlxl.mail 1.0

// Compose's rich HTML body editor (supersedes the earlier plain-TextArea-
// only constraint). The editing surface is a contenteditable WebEngineView.
// Unlike EmailDetail.qml's read-only viewer -- which sets
// settings.javascriptEnabled: false because sender HTML is untrusted -- this
// view intentionally enables JavaScript: the script running here is ours
// (execCommand calls + the sanitizer below), a different trust boundary than
// rendering someone else's mail.
Item {
    id: root

    implicitWidth: 360
    implicitHeight: 240

    // Seeds the editor's content. Call exactly once, right after
    // construction (Compose.qml's Component.onCompleted) -- there is no
    // "reload" support, callers only ever seed a fresh draft.
    function loadInitialHtml(html) {
        webView.loadHtml(root.shellHtml(html))
    }

    // Runs the sanitizer over the current DOM and invokes
    // callback({html, isEmpty}). Necessarily asynchronous: runJavaScript
    // crosses into WebEngine's separate render process, there is no
    // synchronous variant.
    function requestSendableHtml(callback) {
        webView.runJavaScript(root.sanitizerScript, callback)
    }

    function shellHtml(bodyHtml) {
        return "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><style>"
            + "body { font-family: sans-serif; font-size: 14px; margin: 8px; "
            + "background: " + Theme.panel + "; color: " + Theme.inkStrong + "; }"
            + "</style></head><body contenteditable=\"true\">" + bodyHtml + "</body></html>"
    }

    // Installed after every loadHtml() (see WebEngineView.onLoadingChanged
    // below) -- forces paste to plain text so arbitrary markup/scripts from
    // a webpage never enter the DOM in the first place. The sanitizer below
    // is a backstop, not the primary control.
    readonly property string pasteScript: "
        document.body.addEventListener('paste', function(event) {
            event.preventDefault();
            var text = (event.clipboardData || window.clipboardData).getData('text/plain');
            document.execCommand('insertText', false, text);
        });
    "

    // Allowlist DOM sanitizer: unwraps (keeps children, drops the tag) any
    // element outside the fixed tag list, strips every attribute except
    // href/style on <a> (further restricted below). Runs against a cloned
    // subtree so it never mutates what's on screen.
    readonly property string sanitizerScript: "
        (function() {
            var allowedTags = ['P', 'BR', 'DIV', 'B', 'STRONG', 'I', 'EM', 'A'];
            var allowedStyleProps = ['color', 'background-color', 'padding', 'border-radius',
                                      'display', 'font-weight', 'text-decoration', 'border'];
            function isSafeUrl(url) {
                return /^(https?:|mailto:)/i.test(url);
            }
            function clean(node) {
                Array.from(node.childNodes).forEach(function(child) {
                    if (child.nodeType === Node.ELEMENT_NODE) {
                        clean(child);
                        if (allowedTags.indexOf(child.tagName) === -1) {
                            while (child.firstChild) node.insertBefore(child.firstChild, child);
                            node.removeChild(child);
                            return;
                        }
                        Array.from(child.attributes).forEach(function(attr) {
                            if (child.tagName === 'A' && attr.name === 'href') {
                                if (!isSafeUrl(child.getAttribute('href'))) child.removeAttribute('href');
                            } else if (child.tagName === 'A' && attr.name === 'style') {
                                var kept = attr.value.split(';').map(function(rule) {
                                    var prop = (rule.split(':')[0] || '').trim().toLowerCase();
                                    return allowedStyleProps.indexOf(prop) !== -1 ? rule.trim() : null;
                                }).filter(Boolean).join('; ');
                                if (kept) child.setAttribute('style', kept); else child.removeAttribute('style');
                            } else {
                                child.removeAttribute(attr.name);
                            }
                        });
                    } else if (child.nodeType !== Node.TEXT_NODE) {
                        node.removeChild(child);
                    }
                });
            }
            var clone = document.body.cloneNode(true);
            clean(clone);
            return { html: clone.innerHTML, isEmpty: document.body.textContent.trim() === '' };
        })();
    "

    ColumnLayout {
        anchors.fill: parent
        spacing: 4

        RowLayout {
            Layout.fillWidth: true
            spacing: 4

            IconButton {
                icon: "format-text-bold"
                tooltip: i18n("Bold")
                onClicked: webView.runJavaScript("document.execCommand('bold')")
            }
            IconButton {
                icon: "format-text-italic"
                tooltip: i18n("Italic")
                onClicked: webView.runJavaScript("document.execCommand('italic')")
            }
            Item { Layout.fillWidth: true }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.shapeField
            color: Theme.panel
            border.width: 1
            border.color: Theme.line

            WebEngineView {
                id: webView
                anchors.fill: parent
                anchors.margins: 1
                backgroundColor: Theme.panel
                settings.javascriptEnabled: true

                onLoadingChanged: function(loadRequest) {
                    if (loadRequest.status === WebEngineView.LoadSucceededStatus)
                        webView.runJavaScript(root.pasteScript)
                }
            }
        }
    }
}
```

- [ ] **Step 3: Replace Compose.qml's body `TextArea` with `RichBodyEditor`**

In `app/qml/pages/Compose.qml`, replace the comment block *and* the `Rectangle` it describes (currently lines 200-239 — the comment starting "Body -- plain multi-line TextArea..." through the closing brace of the `Rectangle` wrapping `Flickable`/`TextArea id: bodyArea`) with:

```qml
        // Body -- rich HTML editor (see RichBodyEditor.qml; supersedes the
        // earlier plain-TextArea-only constraint).
        RichBodyEditor {
            id: bodyEditor
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
```

- [ ] **Step 4: Update the file header comment**

Replace the comment block at the top of `Compose.qml` (lines 8-11):

```qml
// Task 35 -- plain reusable Item, deliberately NOT a Kirigami.Page (see
// Phase 6 global constraint 4); MobileRoot/DesktopRoot each host this
// directly (Tasks 38/39). Plain text only, no rich-text toolbar anywhere in
// this file (global constraint 5) -- Body is a bare TextArea.
```

with:

```qml
// Task 35 -- plain reusable Item, deliberately NOT a Kirigami.Page (see
// Phase 6 global constraint 4); MobileRoot/DesktopRoot each host this
// directly (Tasks 38/39). Body is a RichBodyEditor (WYSIWYG HTML,
// see docs/superpowers/specs/2026-07-18-html-compose-design.md) --
// the earlier "plain text only" constraint no longer applies.
```

- [ ] **Step 5: Seed the editor instead of the old `TextArea`**

In `Compose.qml`'s `Component.onCompleted`, replace:

```qml
        bodyArea.text = root.initialBody
```

with:

```qml
        bodyEditor.loadInitialHtml(root.quotedInitialBodyHtml(root.initialBody))
```

And add this helper function near `seedTokensFromString` (same file):

```qml
    function escapeHtml(text) {
        return text.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;")
    }

    // Reply/Forward seed initialBody with a plain-text quote block
    // (EmailDetail.qml's composeRequested() -- unchanged by this feature).
    // HTML-escape it and preserve line breaks so it renders correctly inside
    // the rich editor while staying fully editable/deletable, same as
    // before.
    function quotedInitialBodyHtml(text) {
        if (text === "")
            return ""
        return "<blockquote>" + root.escapeHtml(text).replace(/\n/g, "<br>") + "</blockquote>"
    }
```

- [ ] **Step 6: Make `trySend()` async**

Replace `trySend()`'s body in `Compose.qml`:

```qml
    function trySend() {
        toField.commitInputAsToken()
        ccField.commitInputAsToken()
        bccField.commitInputAsToken()

        if (toField.joinedText.trim() === "" || subjectField.text.trim() === "" || bodyArea.text.trim() === "") {
            root.validationError = i18n("Please fill in all fields")
            return
        }
        root.validationError = ""
        const ok = MailApp.sendMail(toField.joinedText, ccField.joinedText, bccField.joinedText,
                                     subjectField.text, bodyArea.text, root.attachmentPaths)
        if (ok)
            root.sendSucceeded()
    }
```

with:

```qml
    function trySend() {
        toField.commitInputAsToken()
        ccField.commitInputAsToken()
        bccField.commitInputAsToken()

        bodyEditor.requestSendableHtml(function(result) {
            if (toField.joinedText.trim() === "" || subjectField.text.trim() === "" || result.isEmpty) {
                root.validationError = i18n("Please fill in all fields")
                return
            }
            root.validationError = ""
            const ok = MailApp.sendMail(toField.joinedText, ccField.joinedText, bccField.joinedText,
                                         subjectField.text, result.html, root.attachmentPaths)
            if (ok)
                root.sendSucceeded()
        })
    }
```

- [ ] **Step 7: Make `currentDraftForPopOut()` async**

Replace:

```qml
    function currentDraftForPopOut() {
        toField.commitInputAsToken()
        return { to: toField.joinedText, subject: subjectField.text, body: bodyArea.text }
    }
```

with:

```qml
    function currentDraftForPopOut(callback) {
        toField.commitInputAsToken()
        bodyEditor.requestSendableHtml(function(result) {
            callback({ to: toField.joinedText, subject: subjectField.text, body: result.html })
        })
    }
```

And update its one call site (the pop-out `IconButton`'s `onClicked`):

```qml
                onClicked: {
                    const draft = root.currentDraftForPopOut()
                    root.popOutRequested(draft.to, draft.subject, draft.body)
                }
```

becomes:

```qml
                onClicked: {
                    root.currentDraftForPopOut(function(draft) {
                        root.popOutRequested(draft.to, draft.subject, draft.body)
                    })
                }
```

- [ ] **Step 8: Build**

Run: `cmake --build build --target llamamail`

Expected: builds with no errors. (No automated QML test harness exists in this repo — `TESTING.md` is the project's convention for UI-level verification, which Task 4 updates.)

- [ ] **Step 9: Manually verify**

Run the app (`./build/app/llamamail`, or however you normally launch it locally), open Compose, and confirm:
- Typing in the body works, Bold/Italic toolbar buttons visibly change selected text.
- Sending a message with bold/italic text succeeds and, viewed from another mail client (or this app's own `EmailDetail.qml`), renders the formatting.
- Reply/Reply All/Forward still seed the quoted text correctly, and it stays editable.
- Pop-out (Desktop mode) carries the current draft (including any bold/italic content) into the new window.

- [ ] **Step 10: Commit**

```bash
git add app/qml/components/RichBodyEditor.qml app/qml/pages/Compose.qml app/qml/qml.qrc
git commit -m "Add RichBodyEditor: WYSIWYG HTML body for Compose"
```

---

### Task 3: HyperlinkDialog.qml — links and button-styled links

**Files:**
- Create: `app/qml/components/HyperlinkDialog.qml`
- Modify: `app/qml/components/RichBodyEditor.qml` (add the Hyperlink toolbar button + dialog instance + insertion handler)
- Modify: `app/qml/qml.qrc` (register the new file)

**Interfaces:**
- Consumes: `Theme.accent`, `Theme.readableOnAccent`, `Theme.shapeButton` (all already-existing `ThemeController` properties), `ThemedTextField`, `PrimaryButton`, `GhostButton` (existing components, same directory).
- Produces: `HyperlinkDialog.linkConfirmed(label: string, url: string, asButton: bool)` signal, `open()`/`close()` functions — same shape as `AddressBookPickerDialog.qml`.

- [ ] **Step 1: Register the new file in `qml.qrc`**

Add, after the `RichBodyEditor.qml` line added in Task 2:

```xml
    <file>components/HyperlinkDialog.qml</file>
```

- [ ] **Step 2: Create `app/qml/components/HyperlinkDialog.qml`**

```qml
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import com.urlxl.mail 1.0

// Compose's rich-body "Insert Link" toolbar action: label + URL, with an
// optional "Style as button" checkbox that renders the same <a> with an
// inline PrimaryButton-like style (RichBodyEditor.qml's onLinkConfirmed
// handler builds the actual HTML/inline style; this dialog only collects
// and validates the three inputs). Same overlay-Item shape as
// AddressBookPickerDialog.qml -- no QtQuick.Controls.Dialog precedent
// anywhere in this codebase.
Item {
    id: root

    property bool isOpen: false
    property string labelText: ""
    property string urlText: ""
    property bool styleAsButton: false
    property string validationError: ""

    signal linkConfirmed(string label, string url, bool asButton)

    function open() {
        root.labelText = ""
        root.urlText = ""
        root.styleAsButton = false
        root.validationError = ""
        root.isOpen = true
    }

    function close() {
        root.isOpen = false
    }

    function isSafeUrl(url) {
        return /^(https?:|mailto:)/i.test(url)
    }

    function confirm() {
        if (root.labelText.trim() === "" || root.urlText.trim() === "") {
            root.validationError = i18n("Please fill in both fields")
            return
        }
        if (!root.isSafeUrl(root.urlText.trim())) {
            root.validationError = i18n("Link must start with http://, https://, or mailto:")
            return
        }
        root.linkConfirmed(root.labelText, root.urlText.trim(), root.styleAsButton)
        root.close()
    }

    visible: root.isOpen
    anchors.fill: parent

    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.4)

        TapHandler {
            onTapped: root.close()
        }
    }

    Rectangle {
        anchors.centerIn: parent
        width: 320
        implicitHeight: content.implicitHeight + 32
        radius: Theme.shapeSheet
        color: Theme.panel
        border.width: 1
        border.color: Theme.line

        TapHandler {} // swallow taps -- keeps them from reaching the scrim behind

        ColumnLayout {
            id: content
            anchors.fill: parent
            anchors.margins: 16
            spacing: 10

            Text {
                text: i18n("Insert Link")
                color: Theme.inkStrong
                font.family: Theme.fontUi
                font.pixelSize: 16
                font.weight: Font.Medium
            }
            ThemedTextField {
                Layout.fillWidth: true
                placeholderText: i18n("Label")
                text: root.labelText
                onTextChanged: root.labelText = text
            }
            ThemedTextField {
                Layout.fillWidth: true
                placeholderText: i18n("URL")
                text: root.urlText
                onTextChanged: root.urlText = text
            }
            CheckBox {
                Layout.fillWidth: true
                text: i18n("Style as button")
                checked: root.styleAsButton
                contentItem: Text {
                    text: control.text
                    color: Theme.inkStrong
                    font.family: Theme.fontUi
                    font.pixelSize: 14
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: control.indicator.width + control.spacing
                }
                onCheckedChanged: root.styleAsButton = checked
            }
            Text {
                Layout.fillWidth: true
                visible: root.validationError !== ""
                text: root.validationError
                color: Theme.dangerColor
                font.family: Theme.fontUi
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Item { Layout.fillWidth: true }
                GhostButton {
                    text: i18n("Cancel")
                    onClicked: root.close()
                }
                PrimaryButton {
                    text: i18n("Insert")
                    onClicked: root.confirm()
                }
            }
        }
    }
}
```

- [ ] **Step 3: Wire the Hyperlink toolbar button + dialog into `RichBodyEditor.qml`**

Add a third `IconButton` to the toolbar `RowLayout` in `RichBodyEditor.qml` (right after the Italic button, before `Item { Layout.fillWidth: true }`):

```qml
            IconButton {
                icon: "insert-link"
                tooltip: i18n("Insert Link")
                onClicked: linkDialog.open()
            }
```

Add the dialog instance as a top-level child of `RichBodyEditor.qml`'s root `Item` (sibling of the `ColumnLayout`, so it overlays the whole editor):

```qml
    HyperlinkDialog {
        id: linkDialog
        z: 10
        anchors.fill: parent
        onLinkConfirmed: function(label, url, asButton) {
            const style = asButton
                ? " style=\"display:inline-block;padding:10px 20px;border-radius:" + Theme.shapeButton
                    + "px;background-color:" + Theme.accent + ";color:" + Theme.readableOnAccent
                    + ";text-decoration:none;font-weight:600;\""
                : ""
            const escapedLabel = label.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;")
            const html = "<a href=\"" + url + "\"" + style + ">" + escapedLabel + "</a>"
            webView.runJavaScript("document.execCommand('insertHTML', false, " + JSON.stringify(html) + ")")
        }
    }
```

- [ ] **Step 4: Build**

Run: `cmake --build build --target llamamail`

Expected: builds with no errors.

- [ ] **Step 5: Manually verify**

Run the app, open Compose, click the Hyperlink toolbar button and confirm:
- A plain link (checkbox unchecked) inserts a normal `<a>` at the cursor.
- Checking "Style as button" inserts a visually button-styled link (background-filled, rounded, no underline) matching `PrimaryButton`'s look.
- Entering a `javascript:` URL is rejected with the validation message and nothing is inserted.
- Sending a message with both a plain link and a button-styled link, then viewing it from another mail client (or `EmailDetail.qml`), shows both rendered correctly and the button remains clickable.

- [ ] **Step 6: Commit**

```bash
git add app/qml/components/HyperlinkDialog.qml app/qml/components/RichBodyEditor.qml app/qml/qml.qrc
git commit -m "Add Hyperlink toolbar action with button-styled link option"
```

---

### Task 4: Update TESTING.md

**Files:**
- Modify: `TESTING.md`

**Interfaces:** None — documentation only.

- [ ] **Step 1: Replace the existing "Compose sends" bullet**

In `TESTING.md`'s `## Mail` section, replace:

```markdown
- [ ] **Compose sends.** Open Compose, fill recipient/subject/body,
      optionally attach a file (`root.attachmentPaths`), send. Expected:
      `MailApp.sendMail(...)` returns success and the composed message
      shows up in Sent on next refresh.
```

with:

```markdown
- [ ] **Compose sends rich HTML.** Open Compose, fill recipient/subject,
      write a body using Bold/Italic and the Hyperlink toolbar action
      (`app/qml/components/RichBodyEditor.qml`) -- including at least one
      link with "Style as button" checked -- optionally attach a file
      (`root.attachmentPaths`), send. Expected: `MailApp.sendMail(...)`
      passes `mode: "html"` (`MailController::sendMail`,
      `app/mail/MailController.cpp`) and the composed message shows up in
      Sent on next refresh, rendering the formatting/button correctly when
      viewed from another mail client.
- [ ] **Pasting into the body is forced to plain text.** Copy formatted
      content from a webpage and paste it into the Compose body. Expected:
      it lands as plain text (no fonts/colors/scripts carried over) --
      RichBodyEditor's paste handler always calls
      `document.execCommand('insertText', ...)` with the clipboard's
      plain-text data, never the rich HTML variant.
- [ ] **A `javascript:` URL is rejected in the Hyperlink dialog.** Expected:
      a validation message appears and nothing is inserted into the body --
      only `http://`, `https://`, and `mailto:` are accepted.
```

- [ ] **Step 2: Commit**

```bash
git add TESTING.md
git commit -m "Document HTML compose manual verification steps"
```
