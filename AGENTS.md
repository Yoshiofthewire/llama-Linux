# Llama Mail — Linux Qt Client

## 1. Project identity

Llama Mail is a **relay-only** Qt/C++/QML email client: there is no IMAP or
SMTP anywhere on-device, and the backend at `mail.urlxl.com` is the sole
transport for mail, contacts, and push. It targets **Qt6/Kirigami KF6
only** across two surfaces from one codebase — **Linux Desktop** (KDE
Plasma primary, packaged as a Flatpak) and **KDE Mobile** (Plasma Mobile,
same Flatpak, mobile UI root). **Ubuntu Touch (Lomiri) support is
deferred** — Qt5 is EOL and this codebase no longer builds against it; see
Section 4 for the re-check trigger. It is the fourth sibling client after
the Android app (`~/git/llama-mobile`) and the SwiftUI macOS/iOS app
(`~/git/llama-Mail-for-Mac`). The authoritative design source for this repo
is `Linux_QT_Client_Plan.md` at the repo root — read it before making any
architectural decision; this file only summarizes and carries forward the
rules most likely to be violated by accident (note: that plan document
predates the Qt5-drop decision and still describes a dual-Qt/Ubuntu Touch
target — this file is the current source of truth on that point).

## 2. Repo layout map

Full detail (including per-subdirectory breakdowns) lives in
`Linux_QT_Client_Plan.md`'s "Repo layout" section — this is a terse pointer
map, not a duplicate:

```
core/       — libllamacore: models/net/db/stores/domain/theme, QtCore+Network+Sql only
app/        — main.cpp, push/ (KUnifiedPush glue, Qt6-only), platform/ (SecureStore backends), qml/ (MobileRoot, DesktopRoot, pages, components)
tests/      — QtTest, stubbed HttpClient; ctest-driven
packaging/  — flatpak/ (Flatpak manifest + metainfo) and click/ (Clickable manifest + apparmor)
po/         — gettext catalogs
docs/       — local tooling/setup notes (see docs/SETUP.md)
```

## 3. Build instructions

A single out-of-tree build directory, Qt6-only:

```sh
cmake -B build -S .
cmake --build build
ctest --test-dir build
```

A change is not verified until this builds cleanly and `ctest` is green.

## 4. Locked decisions (do not relitigate)

Carried forward verbatim (in substance) from `Linux_QT_Client_Plan.md`'s own
"Locked decisions carried over" section:

- **Relay-only.** No IMAP/SMTP anywhere. `mail.urlxl.com` is the sole
  transport. Search is local-cache-only.
- **Wire contracts come from the backend Go source**, plus `llama-mobile`'s
  `RelayModels.kt`/`ContactSyncModels.kt` and `llama-Mail-for-Mac`'s
  `Data/Networking/*` (relay-only, live-verified, test-locked clients).
  **Never guess shapes** — guessed shapes have caused live 400s before.
- **The 13 theme palettes are a binding contract**, sourced from
  `llama-Mail-for-Mac/Style/AppTheme.swift`'s 7-field `ThemePalette` — not
  the web's 16-field `theme.ts`. Copy values, don't approximate.
- **StandardFolder wire names**: `INBOX`, `Drafts`, `Junk`, `Sent`, `Trash`,
  `Archive`; display name splits on both `/` and `.`.
- **90-second foreground refresh cadence.** Full-snapshot refresh
  (`since=0`); delta/cursor mail sync stays v2.
- **License: GPL-2.0** (fine with Qt LGPL / KDE).
- **Qt5/Ubuntu Touch (Clickable) support is dropped, not merely paused.**
  Qt5 is EOL upstream, so this codebase now hard-requires Qt6 and has no
  Qt5 build path at all (previously a dual-Qt CMake toggle selected between
  two separate build trees; that toggle and both trees are gone — see
  Section 3 for the current single build directory). This was a deliberate
  call, not an oversight: Ubuntu Touch is deferred until UBports ships a
  usable Qt6/KF6 track (the 24.04-2.0 beta, slated ~2026-07-20, or the
  further-out 26.04-1.x/Qt 6.9 line) — see Section 1. Re-check when UT
  ships that track; the plan is to target it directly with this same Qt6
  codebase, no separate build path.

## 5. Single-Qt rules

Qt5/KF5 support was dropped (Qt5 is EOL); Ubuntu Touch is deferred until it
ships its own Qt6/KF6 track — see Section 1 and the locked decision in
Section 4. There is no longer a KF5/KF6 divergence to guard against, so
most of what used to live in this section (dual-Qt QML-type restrictions, a
`Compat` singleton escape hatch) is moot and has been removed. One rule
carries forward unchanged, independent of the Qt5 decision:

- `core/`'s **QtCore/QtNetwork/QtSql-only boundary** is the rule most likely
  to be accidentally violated in a single commit: nothing in `core/` may
  pull in QtDBus, QtGui/QtQuick, KUnifiedPush, KNotifications, or any Lomiri
  glue. That code belongs in `app/`. Check `core/`'s own CMakeLists/includes
  before adding a dependency there.

## 6. Ponytail, lazy senior dev mode

Use the smallest correct change.

1. Reuse what already exists.
2. Prefer stdlib and native platform APIs.
3. Add dependencies only when they remove meaningful code.
4. Fix shared root causes, not one caller.
5. If a shortcut has a limit, mark it with `ponytail:` and name the upgrade path.

Non-trivial logic must include one runnable check (unit test or minimal self-check).

## 7. DOX framework

### Core Contract

- AGENTS.md files are binding contracts for their subtree.
- Read from root to nearest AGENTS.md before editing.
- The nearest AGENTS.md controls local details; parent docs keep global rules.

### Update After Editing

- Run a DOX pass for every meaningful change.
- Update nearest owning AGENTS.md when behavior, responsibilities, or verification changes.
- Keep Child DOX Index entries current and delete stale rules.

### User Preferences

- Best-effort 90-second keyword refresh policy (foreground cadence; background catch-up on resume).
- Relay-only: never add IMAP/SMTP client code.
- DOX hierarchy scope is app-only.

### Child DOX Index

(none — no subdirectory AGENTS.md files exist yet)

## 8. Known live-system gotchas

- **Re-registration silently 401s once the pairing token expires.** Token
  rotation after pairing never reaches the server (latent, unfixed
  server-side). Don't re-register on every launch and assume it worked —
  handle the 401 explicitly.
- **`mail.urlxl.com` sits behind Cloudflare** (bare `urlxl.com` → 530) and
  needs a real, non-default User-Agent set on QNAM for every request from
  day one — a bare/default UA gets blocked.
- **Deployment lag.** The backend deploys via a separate Docker pipeline on
  a remote host; a commit landing in the backend repo does not mean it is
  live yet. Verify the relevant backend commits are actually deployed to
  `mail.urlxl.com` before running any live end-to-end test — this has
  previously produced a "committed but 404s live" window.
