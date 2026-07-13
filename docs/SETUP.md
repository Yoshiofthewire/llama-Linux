# Local tooling gaps

Re-verified during Task 8 (Qt5 drop / Qt6-only build). All 5 of the original
items from the Task 1 checklist are now installed on this machine. The one
real gap tracked below — the `org.kde.Sdk` Flatpak runtime — is not one of
those original 5; it's a newly-identified item surfaced by this
re-verification pass.

- **`extra-cmake-modules`** — installed (`extra-cmake-modules 6.27.0-1`,
  verify: `pacman -Q extra-cmake-modules`). The top-level `CMakeLists.txt`
  still does `find_package(ECM QUIET NO_MODULE)` and only extends
  `CMAKE_MODULE_PATH` if found — this stays optional until a task starts
  using ECM's convenience macros (e.g. `ecm_add_test`, `KDECompilerSettings`).

- **`flatpak-builder`** — installed (`/usr/bin/flatpak-builder`, verify:
  `which flatpak-builder`). Unblocks the Flatpak packaging skeleton
  (`packaging/flatpak/`), but see the `org.kde.Sdk` gap below — the runtime
  needed to actually build the Flatpak is still missing.

- **`clickable`** — installed (`/usr/bin/clickable`, verify: `which
  clickable`). Unblocks the Clickable/Ubuntu Touch packaging tooling itself,
  though Ubuntu Touch as a build *target* is deferred per `AGENTS.md`
  Section 4 (Qt5 EOL; UT hasn't shipped its own Qt6 track yet) — clickable
  has nothing Qt6-buildable to package against right now.

- **A QtWebEngine package (Qt6 variant)** — installed as `qt6-webengine
  6.11.1-4` (verify with `pacman -Q qt6-webengine` or `pacman -Qs
  webengine` — note the old checklist's `pacman -Qs qtwebengine` command
  returns nothing because the actual package name is `qt6-webengine`, not
  `qtwebengine`; same package, the old verify command just didn't match the
  hyphenated name). The `io.qt.qtwebengine.BaseApp` Flatpak extension is a
  separate, still-unverified concern for the Flatpak build specifically —
  not needed for this pass. HTML mail rendering (the feature this blocks)
  is still a future phase.

- **`kunifiedpush`** — installed (`kunifiedpush 26.04.3-1.1`, verify:
  `pacman -Qs kunifiedpush`). Unblocks the KUnifiedPush push proofs in a
  future phase (still needs the user's real distributor/account to actually
  exercise it).

## Remaining gap: `org.kde.Sdk` Flatpak runtime

`flatpak list --runtime | grep -i kde` shows `org.kde.Platform` (6.10,
user-scoped) installed, but **not** `org.kde.Sdk`. The `Platform` runtime is
enough to *run* KDE/Qt6 apps but not to *build* a Flatpak against KF6 —
`flatpak-builder` needs the matching `Sdk` runtime. `flathub` is already
configured as a remote (both system and user), so fetching it is a single
`flatpak install` away.

This is the one real gap surfaced by this re-verification pass — not one of
the original 5 checklist items, but a newly-identified prerequisite for
building the Flatpak. Task 9 in this batch resolves it — not addressed
here.
