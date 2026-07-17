# TESTING.md — Manual verification checklist

This is a manual (human-driven) verification checklist for KyPost's
Linux Qt6/KF6 client (formerly "Llama Mail"), grounded in this repo's own
feature set and its own live-testing history (Phases 6–8, plus the
extended-contact-fields/dedupe/PGP/autocomplete/brand-refresh work that
followed Phase 8). It is written fresh — there is no `TESTING.md` in the
Mac repo to port from, despite older planning-doc phrasing suggesting
otherwise.

Every item below either (a) was live-verified against the real
`mail.urlxl.com` backend at some point during Phase 6/7 development (cited
by task number where useful), (b) is packaging-relevant, locally
verifiable behavior added in Phase 8, or (c) covers the extended contact
fields, contact de-duplication, compose autocomplete, and PGP QR
key-exchange features implemented after Phase 8 (see `Client_Contact_Update.md`,
`Mobile_Contacts_DEDupe.md`, `ContactAutocomplete.md`, and
`Client_PGP_Update.md`) — those items are not yet confirmed against a live
backend and are the main reason this file was extended. Nothing here
describes aspirational behavior — see "Known non-goals / do-not-expect" at
the bottom for the inverse list.

Prerequisites for any live-backend section: a paired account on
`mail.urlxl.com`, KDE Wallet (or another Secret Service provider) unlocked
and running, and network access. The client sends a real User-Agent on
every request — `mail.urlxl.com` sits behind Cloudflare and blocks
default/bare User-Agent strings (see `AGENTS.md` Section 8).

---

## Pairing

- [ ] **Paste a `llamalabels://native-pair` link into Settings → Connection
      → "Pair This Device…".** The popup hosts `Pairing.qml`, whose paste
      field calls `Pairing.pairFromPastedLink(text)`
      (`app/qml/pages/Pairing.qml`). Expected: `pairingState` moves
      `idle → working → paired`; the popup closes and Settings' Connection
      pane shows the `StatusBadge` flip to "Paired" plus the paired
      `Server`/`Device` rows populated (`Pairing.pairedServerHost`,
      `Pairing.deviceId`).
- [ ] **Deep-link the same URL via `llamamail llamalabels://native-pair?sub=...&hash=...&srv=...&pt=...`**
      (or `xdg-open` once the `.desktop` file's
      `MimeType=x-scheme-handler/llamalabels;` is registered). Expected:
      routed through `main.cpp`'s `routeDeepLink()` →
      `PairingController::pairFromDeepLink()`, same success path as the
      pasted-link case above. A second launch while the app is already
      running is expected to relay over D-Bus (`KDBusService::Unique`) to
      the first instance rather than opening a second window.
      `llamalabels://desktop-pair` (or any other host) is deliberately
      **not** handled — `routeDeepLink()` logs it as unrecognized and drops
      it; this client only ever does native sub/hash pairing (see
      `AGENTS.md`/Phase 6 constraint — no separate desktop-session flow).
- [ ] **Confirm the pairing credentials actually land in the OS keychain**,
      not just in the UI: `kwallet-query -f com.urlxl.mail -l kdewallet`
      (or the equivalent for your Secret Service provider) should list the
      `sub`/`hash`/`deviceId`/pairing-token entries `SecureStoreKeychain`
      (`app/platform/SecureStoreKeychain.cpp`, backed by QtKeychain /
      `org.freedesktop.secrets`) wrote. This exact command was used during
      Task 37/38's live verification.
- [ ] **Tap "Remove Pairing"** (`Pairing.removePairing()`). Expected:
      Connection pane reverts to "Not paired", `kwallet-query` shows the
      folder/entries gone (confirmed live during Task 38 via
      `org.kde.KWallet.removeFolder`), and re-pairing with a fresh link
      afterward succeeds again (no leftover state blocks a second pair).

## Mail

- [ ] **Inbox loads on open.** `MailController::refresh()` runs a
      full-snapshot fetch (`since=0`) on `Component.onCompleted` in both
      `MobileRoot.qml` and `DesktopRoot.qml`. Expected: cached emails
      render immediately (from `MailRepository`'s local cache), followed by
      a live refresh once the network call returns.
- [ ] **Manual refresh works.** Tapping the refresh action
      (`MailApp.refresh()`, wired in both roots) re-runs the same
      full-snapshot fetch. Note: there is no periodic auto-refresh timer on
      the Inbox view itself — the "90-second cadence" that exists in this
      codebase lives in `TransportStateMachine`'s **Polling tier**
      (`core/domain/TransportStateMachine.h`/`.cpp`, 90000 ms default),
      which is a push-delivery fallback, not an Inbox-view timer. See the
      Push/notifications section below for that behavior. Don't expect the
      visible Inbox list to silently repaint every 90 seconds by itself
      while sitting on the polling tier — new mail delivered that way
      surfaces as a KNotification and gets persisted, but isn't guaranteed
      to also re-render the currently-open Inbox list without another
      `refresh()`.
- [ ] **Swipe actions apply** (`app/qml/MobileRoot.qml`'s `SwipeDelegate`
      rows). Swipe an inbox row left to reveal Archive, right to reveal
      Delete; both call `MailApp.archiveEmails([messageId])` /
      `MailApp.deleteEmails([messageId])` synchronously and expect a
      `true` return before the row visually resolves the swipe action.
      (Spam is accessible only via the "Junk" button in EmailDetail.qml,
      not as an Inbox swipe action.)
- [ ] **Compose sends.** Open Compose, fill recipient/subject/body,
      optionally attach a file (`root.attachmentPaths`), send. Expected:
      `MailApp.sendMail(...)` returns success and the composed message
      shows up in Sent on next refresh.
- [ ] **EmailDetail renders the HTML body safely.** Open any HTML email.
      Expected: the body renders inside the `WebEngineView`
      (`app/qml/pages/EmailDetail.qml`) with **no JavaScript execution and
      no remote image loading** — `settings.javascriptEnabled: false` and
      `settings.autoLoadImages: false` are both explicitly set (commit
      `f0d48c1`). Tapping a link inside the body opens it externally
      (`Qt.openUrlExternally(request.url)`), not inside the embedded view.
- [ ] **Folder navigation works.** Switching folders calls
      `MailApp.selectFolder(wireName)` with one of the locked
      `StandardFolder` wire names (`INBOX`, `Drafts`, `Junk`, `Sent`,
      `Trash`, `Archive`); expected: the list re-filters from cache
      immediately, then a folder-scoped refresh follows.
- [ ] **Keyword tabs work.** The keyword-pill row above the Inbox list is
      driven by `MailApp.keywordTabs` (derived per-folder); tapping one
      calls `MailApp.selectKeyword(keyword)` and re-filters the
      already-cached folder emails with no network round-trip.

## Contacts

- [ ] **List renders.** `ContactsList.qml` shows `ContactsApp`'s model;
      "No sync yet." is the expected empty state before the first sync.
- [ ] **Detail/edit renders and saves.** Opening a contact from the list
      shows `ContactDetail.qml`; edited fields persist through
      `ContactsController`'s save path.
- [ ] **Sync pulls/pushes changes.** Tap "Sync Now"
      (`ContactsApp.sync()`, disabled while `ContactsApp.isBusy`).
      Expected: `ContactsApp.statusMessage` reports the outcome (or
      `ContactsApp.lastError` on failure, shown in `Theme.dangerColor`), and
      a successful sync also refreshes the group name cache
      (`ContactsController::sync()` chains into `GroupsRepository::refresh()`
      on success — confirm a group renamed on the backend shows its new name
      in the Groups field below without a separate action).

### Extended contact fields (groups, photo, PGP key, IM/websites/relations/events, phonetic names, department, custom fields, pronouns)

This client round-trips the backend's eleven extended `Contact` fields via
its own vCard-shaped mapping (`core/vcard/VCardContact`), not Android's
`ContactsContract` — see `Client_Contact_Update.md` for the field-by-field
backend contract this mirrors.

- [ ] **Pull a contact populated with every extended field** (create/edit it
      via the web UI first — groups, a photo, a PGP key, an IM entry, a
      website, a relation, an extra event, phonetic given/family names,
      department, a custom field, pronouns), then Sync in this client and
      open its `ContactDetail.qml`. Expected: every field shows correctly,
      including a "Groups" row resolving group UUIDs to names via
      `ContactsApp.allGroups()` (`ContactDetail.qml:129`,
      `groupNamesText()`) — if a group id can't be resolved (cache
      stale/empty), expect a `?`-suffixed fallback rather than the row
      silently dropping the assignment.
- [ ] **Contact photo lazy-loads.** `photoRef` arrives with ordinary sync,
      but the photo bytes only fetch on demand when a contact with a
      non-empty `photoRef` is actually displayed —
      `ContactsController::photoPathFor()`
      (`app/contacts/ContactsController.cpp:583`) delegates to
      `ContactPhotoRepository`'s cache/fetch path. Expected: the contact's
      `Avatar` shows the gradient-initials placeholder briefly, then the
      real photo once the fetch completes; a second view of the same
      contact should be instant (disk cache hit, no repeat network call —
      confirm via `journalctl`/no new outbound request).
- [ ] **Edit and save every extended field**, including toggling group
      checkboxes in the "Groups" section
      (`ContactDetail.qml:589` onward — one checkbox per known group, no
      create-new-group affordance, groups are backend-managed). Expected:
      save round-trips correctly and a follow-up pull from another client
      (or the web UI) shows the same values, including group membership.
- [ ] **Scan a PGP key into a contact** via the "Scan PGP Key" button on
      `ContactsList.qml` (see PGP section below) and confirm the scanned
      key lands in that contact's PGP Key field and survives a save + sync
      round-trip.
- [ ] **vCard special-character handling.** Give a contact an IM entry with
      a free-text service label containing `:` or `;` (e.g. `"My:IM"`) and a
      website/relation label with the same characters. Expected: the
      round-trip through the vCard encoder (commit `55f9d23`'s
      escaping fix) preserves the label exactly rather than corrupting the
      vCard structure — this was an open Minor finding until that commit
      landed, worth confirming rather than assuming.

## Contact de-duplication

- [ ] **"Find Duplicates" merges server-side duplicates.** Create two
      contacts sharing a normalized email or phone (easiest via the web
      UI), then tap "Find Duplicates" next to "Sync" in `ContactsList.qml`
      (`ContactsApp.dedupe()`). Expected: `ContactsApp.statusMessage`
      reports a result (mirroring the Sync status banner/timer), and the
      duplicate disappears from this client's list once the follow-up sync
      picks up the tombstone. This client does **not** compute duplicates
      itself — it only calls `POST /api/contacts/dedupe` and reacts to the
      report; the backend is the sole authority on matching/merge policy.
- [ ] **"No duplicates found" path.** Run "Find Duplicates" again
      immediately after (or on an account with no duplicates). Expected: a
      `mergedCount: 0` result surfaces as a clearly-worded "nothing to
      merge" status, not an error state.
- [ ] **Button is disabled while busy**, same as "Sync" — confirm
      `ContactsApp.isBusy` gates both buttons together (e.g. tapping
      "Find Duplicates" mid-sync should be impossible, not merely ignored).

## Compose autocomplete / address book picker

Local contact lookup while composing (`ContactAutocomplete.md`) — three
pieces: an inline dropdown under each To/Cc/Bcc `TokenField`, a full
"address book" picker dialog, and a duplicate-selection toast.

- [ ] **Typing in To/Cc/Bcc shows a debounced dropdown.** Type a partial
      name or email into any of the three `TokenField`s in `Compose.qml`.
      Expected: `AutocompleteDropdown` opens under the active field showing
      up to 5 local matches (case-insensitive, matched against both name
      and email), positioned/sized to that field
      (`Compose.qml:49-61`). With no matches, the dropdown shows a "No
      contacts found" row rather than staying blank or closing.
- [ ] **Keyboard navigation works in the dropdown** — Up/Down to move the
      selection, Enter/Tab to confirm into a token, Escape to close without
      selecting.
- [ ] **Selecting a match (dropdown or picker) creates a token pill** with
      an "X" to remove it, and re-selecting the same contact from either
      the dropdown or the picker while it's already a token shows the
      "%1 is already added" toast (`Compose.qml:132/145/153`,
      `onDuplicateRejected`) instead of adding a second token.
- [ ] **The address-book picker dialog** (opened via its icon next to the
      To/Cc/Bcc fields) lists contacts with Name/Email/Department columns
      and three per-row buttons — To/Cc/Bcc (`AddressBookPickerDialog.qml`,
      150ms debounce on its own search field). Expected: clicking a button
      appends that contact's email as a token into the matching field
      *immediately* and flips that same button to a `✓`-prefixed
      "added" state, and the dialog stays open for multi-selecting more
      contacts rather than closing after one pick.
- [ ] **Manually typed addresses not in the address book** are still
      accepted as long as they pass RFC 5322-shaped validation — confirm
      typing a full email with no matching contact and pressing Enter/Tab
      still creates a token (this is the "not every recipient has to be a
      known contact" fallback the dropown/picker doesn't cover).

## PGP key exchange (QR code)

Backed by `app/pgp/PgpQrController` (wraps `core/domain/PgpQrRepository` +
`core/net/PgpQrClient`); see `Client_PGP_Update.md` for the API contract.
Needs two paired devices/accounts (or one device run twice against two
accounts) to exercise the full handshake.

- [ ] **"My QR Code"** (Settings → Connection, `myPgpQrCodeRequested()` →
      `PgpMyQrCode.qml`, pushed via `pgpMyQrCodePageComponent` on Mobile /
      inline on Desktop). Expected: on open, a QR code renders
      (`PgpQrController::myQrImageDataUrl()`) encoding a
      `https://<host>/api/pgp/qr/key?t=<token>` URL, alongside the token's
      `expiresAt`. If the account has no PGP identity configured, expect
      the `400`-branch message prompting to set one up first, not a raw
      error or a blank QR.
- [ ] **Token expiry.** Leave "My QR Code" open past 2 minutes without
      refreshing, then have another device scan it. Expected: the scan
      fails with the `403`-branch "expired or invalid" message, not a
      silent failure or a stale-but-accepted key.
- [ ] **"Scan PGP Key"** — reachable both from `ContactsList.qml`'s "Scan
      PGP Key" button (unattached scan, `root.scanPgpKeyRequested()`) and
      from within an open contact's edit view (attached scan, pre-targets
      that contact — `ContactDetail.qml:49`'s `scanPgpKeyRequested()`,
      `keyScanned(name, publicKey)` writes straight into `pgpKeyField`,
      `ContactDetail.qml:235`). Scan a valid "My QR Code" from a second
      device with both flows. Expected: the scanner decodes the URL,
      fetches and displays the `fingerprint` (and name) for out-of-band
      confirmation before saving anywhere — never auto-save without that
      confirmation step.
- [ ] **Scanning a user with no PGP identity configured** shows the
      `404`-branch "hasn't set up PGP encryption yet" message.
- [ ] **Save-to-contact round-trips.** After a confirmed scan, save the key
      to a contact (new or existing) and confirm it persists through a full
      Sync pull→push cycle, landing in that contact's `pgpKey` field on the
      backend too.
- [ ] **Non-app QR readers also work.** The QR payload is a plain HTTPS
      URL, not app-proprietary — scanning "My QR Code" with an unrelated
      generic QR scanner (phone camera app, etc.) should produce a working
      URL that, when opened, returns the same JSON contract directly.

### MFA

- [ ] **`Mfa.respond(challengeId, approve)` round-trips against the real
      backend** (`/api/mfa/push/respond`, via `MfaResponseClient`) —
      exercised directly against `MfaApproval.qml`'s controller during
      Task 37's live verification, both Approve and Deny, including the
      "already resolved" branch when a challenge was answered twice.
      **Caveat, read before testing:** as of this phase, `MfaApproval.qml`
      has **no live entry point** in the shipped app — neither
      `MobileRoot.qml` nor `DesktopRoot.qml` routes anything to it (unlike
      `EmailDetail`, which the notification tap-through wiring in
      `main.cpp` — `NotificationDispatcher::openRequested` →
      `MailController::openEmailRequested` — does reach). Task 37's own
      manual-verification harness that made the page reachable was fully
      reverted before that commit landed. To manually verify the page
      itself today you must temporarily push it onto a page stack /
      instantiate it with a real `challengeId` yourself (as Task 37 did),
      then revert the change — it is not something you can reach by
      normal navigation in a built app.
- [ ] **Confirm MFA is polling-only** — this is a locked Phase 7 design
      constraint, worth restating here as something to explicitly **not**
      expect: no push payload ever triggers MFA UI. `main.cpp`'s only
      `NotificationDispatcher::openRequested` connection targets
      `MailController::openEmailRequested`; there is no equivalent
      connection anywhere targeting `Mfa`/`MfaApproval`. A real MFA
      challenge must be discovered by the user through some other means
      (the web app, or a future polling surface) — it will never arrive as
      a tap-through from a KNotification in this client.

## Push/notifications

- [ ] **Real mail push delivers a visible KNotification popup.**
      Live-verified against the real backend in Phase 7: a real inbound
      mail push (Distributor tier, via KUnifiedPush) produces a KDE
      notification popup with sender/subject.
- [ ] **Tapping the notification opens the right email.** The `"View"`
      notification action fires `NotificationDispatcher::openRequested`,
      forwarded to `MailController::openEmailRequested` (Task 42); each
      root's `Connections { target: MailApp }` block hydrates the full
      email via `MailApp.findByMessageId()` and navigates to it, raising
      the window. Confirm the window raises **only** on this genuine user
      tap, never on background arrival of the push itself.
- [ ] **The web app's "Send Test Notification" button also displays.**
      That endpoint (`POST /api/notifications/test`) sends a deliberately
      sparser envelope with no `data.messageId`, only outer `title`/`body`
      plus a bare `data.url` — `PushPayloadParser` accepts this shape too
      (Task 43's generic-envelope fix: any payload with a non-empty
      messageId-or-title after the data/outer fallback is accepted).
      Expected: the popup still renders correctly even without a real mail
      identity behind it.
- [ ] **Transport tier selection behaves per `TransportStateMachine`'s
      rules** (`core/domain/TransportStateMachine.h`): distributor
      available → Distributor tier (KUnifiedPush); no distributor, app
      foregrounded → EmbeddedSubscriber tier (in-process ntfy
      subscription); subscriber unreachable → Polling tier (90s interval,
      `PushRepository::pullOnce()`). Confirm each tier's arrivals reach
      `NotificationDispatcher::notify()` (verify via `journalctl`/`qDebug`
      — `main.cpp` logs `TransportStateMachine tier changed: <n>` on every
      transition) and that foregrounding the app again after a
      polling-tier fallback retries the embedded subscriber rather than
      staying latched on Polling.

## Settings

- [ ] **All 5 panes render**: Connection, Appearance, Keywords, Contacts,
      Notifications (`app/qml/pages/Settings.qml`, selected via a PillTab
      strip, `currentPane` 0–4). On Mobile, Settings opens in a
      `Kirigami.Page` shell pushed from the global drawer; on Desktop it
      opens in a `Kirigami.OverlaySheet`.
  - **Connection**: paired badge, Server/Device rows, Pair/Remove buttons
    (see Pairing section above).
  - **Appearance**: a name-only list of all 15 theme palettes (the 13
    original plus "Patina Ky"/"Polished Ky" appended at the end, per the
    KyPost brand refresh) with a checkmark on the active one; tapping a row
    calls `Theme.setTheme(name)`.
  - **Keywords**: lists every known keyword with a Visible/Hidden PillTab;
    toggling calls `MailApp.setKeywordVisible(keyword, !visible)` and the
    row updates immediately. Known limitation: a keyword only ever seen on
    non-Inbox folders won't appear here.
  - **Contacts**: sync status/error text plus "Sync Now" (no "sync to
    system contacts" toggle — this repo has no Linux system-address-book
    integration to back one).
  - **Notifications**: read-only display of `Pairing.deliveryMode`,
    `Pairing.transport`, `Pairing.pushServerBaseUrl` — "Not yet
    registered" before first pairing. There is deliberately no editable
    push-server-URL field.
- [ ] **Theme switching applies live across all 15 palettes.** Selecting
      each theme in Appearance should repaint every screen without a
      restart — background, panel, ink, accent colors all come from
      `Theme`'s live `QColor` properties (`core/theme/AppTheme.cpp`'s
      7-field `ThemePalette`, sourced from the Mac app's
      `AppTheme.swift`, a binding contract). Palette names themselves are
      intentionally not translated (product/brand-style names).
- [ ] **A fresh install (or cleared `QSettings`/theme storage) launches
      into "Patina Ky", not "Dark Matter".** `AppTheme::defaultThemeName()`
      changed as part of the KyPost brand refresh; "Dark Matter" remains a
      fully valid, selectable preset — this only changed the *default* — so
      also confirm switching to "Dark Matter" (or any other pre-existing
      theme) still works exactly as before.
- [ ] **App name and icon read "KyPost" everywhere it's user-visible**:
      window title (`DesktopRoot.qml`/`MobileRoot.qml`), the About-style
      sidebar text, and the launcher/Dock/taskbar icon and app-switcher
      entry (sourced from `packaging/flatpak/icons/hicolor/*/apps/
      com.urlxl.mail.png` + `.svg`, regenerated from the new `ky.png` mark
      — confirm it's the new mark, not the old llama wordmark). Package ID
      (`com.urlxl.mail`), the `llamalabels://` deep-link scheme, and
      `QSettings`/keychain storage keys are **intentionally unchanged** by
      this rename — don't expect or flag those as needing to say
      "kypost"/"KyPost" anywhere.

## Packaging (new this phase)

- [ ] **`cmake --install` places files at the correct paths.** Run
      `cmake --build <builddir> && cmake --install <builddir>
      --prefix <some-prefix>` and inspect the installed tree:
  - `<prefix>/bin/llamamail` (the binary)
  - `<prefix>/share/icons/hicolor/{16x16,22x22,24x24,32x32,48x48,64x64,128x128,256x256}/apps/com.urlxl.mail.png`
    and `.../scalable/apps/com.urlxl.mail.svg`
  - `<prefix>/share/applications/com.urlxl.mail.desktop`
  - `<prefix>/share/dbus-1/services/com.urlxl.mail.service`
  - `<prefix>/share/knotifications6/LlamaMail.notifyrc`
  - `<prefix>/share/metainfo/com.urlxl.mail.metainfo.xml`
    All of these are hardcoded-path-free `install(FILES ...)` rules in
    `app/CMakeLists.txt` (Tasks 44–46), driven by `GNUInstallDirs`.
- [ ] **`desktop-file-validate` passes** on
      `packaging/flatpak/com.urlxl.mail.desktop`.
- [ ] **`appstreamcli validate --no-net` reports zero errors** on
      `packaging/flatpak/com.urlxl.mail.metainfo.xml` (both the
      source copy and the installed copy). Task 46's own verification
      pass got a non-zero *exit code* (the tool treats warnings as
      failing) but confirmed, via `--pedantic --format=yaml`, that the
      only findings are one `warning` (missing homepage — explicitly
      accepted) and one `info`/`pedantic` each — **no `error`-severity
      issues**, which is the actual bar.
- [ ] **`flatpak-builder` — build the manifest and confirm exactly where it
      stops.** Run:
      `flatpak-builder --user --force-clean --install-deps-from=flathub
      <builddir> packaging/flatpak/com.urlxl.mail.json`. Expected
      *today*: the `org.kde.Sdk//6.10`/`org.kde.Platform//6.10` runtime and
      the from-source `qtkeychain` module (pinned `0.17.0`, no Flatpak/KDE
      equivalent exists) resolve and build cleanly, and the build reaches
      the `llamamail` module's CMake configure step — where it **fails**:
      `Could not find Qt6WebEngineQuick`. This is a real, confirmed,
      currently-open gap (Task 47), not something to "fix" during a
      testing pass — see "Known non-goals" below. Do not expect a
      complete, runnable Flatpak build from this manifest right now.
- [ ] **i18n fallback-to-English behaves correctly with no `.mo` catalogs
      installed.** Build and launch natively with no locale catalog
      installed anywhere on the system (the expected state right now —
      zero `.po` files are populated; `po/llamamail.pot` is a fully
      populated 114-msgid catalog per Task 49's sweep, but nothing has been
      compiled into a `.mo` catalog yet). Expected: every `i18n()`/
      `i18nc()`-wrapped string (114 strings across QML pages/roots and 4
      C++ controllers, per Task 49's sweep) renders as its literal English
      source string, and the launch produces **no** "catalog not found" or
      other KI18n/QML warnings in the terminal/journal — confirmed during
      Task 49's own verification pass.

## Known non-goals / do-not-expect

- **No IMAP/SMTP anywhere.** `mail.urlxl.com` is the sole transport; this
  is a relay-only client (`AGENTS.md` Section 4). Don't go looking for a
  "server settings" IMAP/SMTP screen — it doesn't exist and never will in
  this design.
- **No Clickable/Ubuntu-Touch build.** Qt5/Ubuntu Touch support was
  dropped outright, not paused (`AGENTS.md` Section 4). `packaging/click/`
  is an intentionally-empty `.gitkeep` placeholder; there is no
  `clickable build` job and none should be added until UBports ships a
  usable Qt6/KF6 track.
- **MFA never arrives via push.** Covered above — restated here because
  it's easy to assume otherwise given mail push works. It is a locked
  Phase 7 design decision, not a missing feature.
- **The Flatpak build does not currently produce a runnable app.**
  `app/CMakeLists.txt` unconditionally requires `Qt6WebEngineQuick`
  (`EmailDetail.qml`'s HTML body rendering), and no compatible
  WebEngine-providing Flatpak module or extension exists for
  `org.kde.Platform//6.10` today — Flathub's `io.qt.qtwebengine.BaseApp`
  extension tops out at branch 6.4. `flatpak-builder` reliably fails at
  the `llamamail` module's CMake configure step for this reason (Task 47,
  confirmed, still open). Treat the Flatpak manifest as a
  finish-args/packaging-metadata audit artifact for now, not a working
  distribution channel — do not describe it as building and running end
  to end until a from-source `qt6-webengine` module (or an updated
  WebEngine extension) closes this gap.
- **No live E2E test infrastructure exists in CI.** Task 51's
  `.github/workflows/ci.yml` sets up a stock GitHub Actions Ubuntu runner
  that installs Qt6/KF6 via `apt` and runs `cmake --build` + `ctest` — it
  proves the app builds and its unit tests pass, nothing about pairing,
  push delivery, or MFA against the real backend. All of the live-backend
  items in this checklist require a human running the app locally against
  `mail.urlxl.com`; none of it is automated or CI-gated.
- **No desktop-session pairing flow.** Only native sub/hash pairing via
  `llamalabels://native-pair` exists; `llamalabels://desktop-pair` is
  explicitly recognized-and-dropped, not a second supported flow.
- **No "sync to system contacts" integration.** There is no Linux system
  address-book equivalent wired into this codebase (unlike the Mac/Android
  apps' OS-level Contacts export) — the Settings → Contacts pane shows
  sync status/action only, no such toggle.
- **Re-registration silently 401s once a pairing token expires** (a
  latent, unfixed server-side gap, `AGENTS.md` Section 8). Don't assume
  a stale pairing that starts failing has a client-side bug — check
  whether the token expired first.
- **This client never computes contact duplicates or merge logic itself.**
  "Find Duplicates" only calls `POST /api/contacts/dedupe` and displays
  the server's report; matching is exact-match only (normalized
  email/phone, or name when one side is empty) and on-demand only — no
  fuzzy matching, no scheduled job, no auto-trigger on sync/import. Don't
  go looking for client-side normalization code for this feature.
- **PGP keys are never auto-fetched from a keyserver.** The QR flow only
  stores/displays a key the user explicitly scanned and confirmed
  (fingerprint check) — there is no keyserver lookup or background key
  refresh anywhere in this client.
