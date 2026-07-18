# HTML Compose (rich body + button-styled links)

## Purpose

Compose currently produces plain-text-only email (`app/qml/pages/Compose.qml`
carries an explicit "global constraint 5: no rich-text toolbar" comment from
an earlier phase). This spec reverses that constraint: Compose gains a
WYSIWYG rich-text body with Bold/Italic/Hyperlink formatting, where a
hyperlink can optionally be styled as a button. The backend
(`kypost-server`) already supports `mode: "html"` end-to-end — this is purely
client-side work in this repo.

## Non-goals

- **No `multipart/alternative` plain-text fallback.** The backend sends
  either `text/plain` or `text/html`, never both; adding a fallback would
  require changes to `kypost-server` (`mailmsg.go`), a separate repo, and is
  explicitly out of scope. Compose sends HTML-only.
- **No toolbar active-state sync** (e.g. Bold button lighting up when the
  cursor is inside bold text). Toolbar buttons are stateless triggers; this
  keeps the design free of a JS→QML selection-change channel for a cosmetic
  nicety.
- **No font size/color/lists/headings** — toolbar is Bold, Italic, Hyperlink
  (with button-styling), nothing else.
- **No separate "Insert Button" feature** — button styling is an option on
  the existing Hyperlink action (a checkbox in its dialog), not a distinct
  toolbar entry.

## Architecture

### New component: `app/qml/components/RichBodyEditor.qml`

A self-contained "reusable Item" (same shape as `IconButton`/`TokenField`),
composed of:

- An internal toolbar row built from `IconButton` (Bold, Italic, Hyperlink).
- A `WebEngineView` whose page is a minimal HTML shell with
  `<body contenteditable="true">` as the editing surface.

Unlike `EmailDetail.qml`'s read-only viewer (which sets
`settings.javascriptEnabled: false` because sender HTML is untrusted
content), this editor sets `javascriptEnabled: true`. That's a different
trust boundary: the JS running here is ours (execCommand calls + the
sanitizer below), not attacker-supplied.

**Exposed surface** (everything else — the WebEngineView, the execCommand
plumbing, the sanitizer script — stays private to the component):

- `property bool isEmpty` — kept live via a JS `input` listener reporting
  `document.body.textContent.trim() === ""`; lets `Compose.qml`'s existing
  validation check keep its current shape.
- `function loadInitialHtml(html)` — seeds content once at startup (used for
  reply/forward quoting, see below).
- `function requestSendableHtml(callback)` — runs the sanitizer JS inside
  the page and invokes `callback(cleanHtml)` asynchronously.

### Toolbar behavior

- **Bold / Italic** → `document.execCommand('bold'|'italic')` on the current
  selection.
- **Hyperlink** → opens a dialog with: label text, URL, and a "Style as
  button" checkbox.
  - URL is restricted to `http://`, `https://`, or `mailto:` schemes;
    anything else is rejected in the dialog before it ever reaches the DOM.
  - On confirm, `execCommand('insertHTML', …)` inserts `<a href="...">label</a>`,
    or — if "Style as button" is checked — the same `<a>` with an inline
    `style` attribute mimicking `PrimaryButton`'s look. Colors
    (`Theme.accent` / `Theme.readableOnAccent`) are **snapshotted to static
    hex values at insert time** — the resulting HTML is a static string
    mailed to a third party, not a live binding to this app's theme.
- **Paste** is intercepted and forced to plain text
  (`clipboardData.getData('text/plain')` via `insertText`, with
  `event.preventDefault()` on the default paste), so arbitrary markup from a
  webpage (scripts, tracking styles, iframes) never enters the DOM in the
  first place.

### Sanitization (defense in depth)

Because paste is plain-text-only and every insertion goes through our own
controlled `execCommand` calls, the surviving HTML is naturally narrow:
`p`, `br`, `div`, `b`/`strong`, `i`/`em`, `a`. Before `requestSendableHtml`'s
callback fires, a JS pass over a cloned DOM:

- Unwraps (removes the tag, keeps children/text of) any element not in that
  allowlist.
- Strips every attribute except `href` on `<a>` (re-validated against
  `http(s):`/`mailto:`) and `style` on `<a>` (restricted via regex to
  `color`, `background-color`, `padding`, `border-radius`, `display`,
  `font-weight`, `text-decoration`, `border` — exactly what the button style
  uses).
- Removes everything else outright: `<script>`, `<iframe>`, `<style>`,
  `class`, `id`, event-handler attributes, etc.

This is a backstop, not the primary control — the primary control is that
paste-as-plain-text and controlled `execCommand` calls never produce
disallowed markup to begin with.

## Send pipeline changes

Extracting HTML out of a `WebEngineView` crosses a process boundary and is
inherently asynchronous (`runJavaScript(js, callback)` — there is no
synchronous variant). This reshapes `Compose.qml::trySend()`:

```
toField.commitInputAsToken() / ccField / bccField        // unchanged
if (toField.joinedText === "" || subjectField.text === "" || bodyEditor.isEmpty) {
    root.validationError = i18n("Please fill in all fields")
    return
}
root.validationError = ""
bodyEditor.requestSendableHtml(function(html) {
    const ok = MailApp.sendMail(toField.joinedText, ccField.joinedText, bccField.joinedText,
                                 subjectField.text, html, root.attachmentPaths)
    if (ok)
        root.sendSucceeded()
})
```

- `MailController::sendMail` (`app/mail/MailController.cpp:264`): the
  hardcoded `QStringLiteral("plain")` becomes `QStringLiteral("html")`. This
  is the only call site of `sendMail` in the codebase, so no plain-mode
  branch needs to survive anywhere in the client.
- `currentDraftForPopOut()` (`Compose.qml:116-119`) currently reads
  `bodyArea.text` synchronously; it becomes callback-based the same way, and
  `popOutRequested`'s signature/consumers in `MobileRoot.qml`/`DesktopRoot.qml`
  don't change — the signal just fires slightly later once the async
  extraction resolves.

## Reply/Forward quoting

`EmailDetail.qml`'s reply/reply-all/forward flows build a plain-text quote
block ("On ... wrote:" + quoted lines) and hand it to `Compose.qml` via the
existing `initialBody` property — that construction is untouched.
`Compose.qml`'s `Component.onCompleted` (currently `bodyArea.text =
root.initialBody` at line 89) instead HTML-escapes that string, wraps it in
a `<blockquote>` with `<br>` for line breaks, and calls
`bodyEditor.loadInitialHtml(...)`. The quoted text renders correctly and
stays editable/deletable exactly like today, just inside the rich surface
instead of a plain `TextArea`.

## Testing plan

- Manual: compose a fresh email with Bold/Italic/plain-link/button-link,
  send to a real mailbox, confirm rendering in another client.
- Manual: reply/reply-all/forward, confirm quoted block renders and stays
  editable.
- Manual: paste rich content (e.g. copied from a webpage) into the editor,
  confirm it lands as plain text.
- Manual: attempt a `javascript:` URL in the Hyperlink dialog, confirm it's
  rejected before insertion.
- Manual: pop-out compose mid-draft with formatted content, confirm the
  draft (to/subject/body) survives into the new window.
