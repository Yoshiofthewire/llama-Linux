#!/bin/sh
# Task 48: extract i18n()/i18nc()/... call sites into po/llamamail.pot.
#
# No KDE-source-tree Messages.sh driver (the kde-dev-scripts family:
# extractrc/extractattr/xml2pot, normally invoked from a framework-provided
# Messages.sh) is installed on this machine -- confirmed via
# `which extractrc extractattr xml2pot`, all miss. Those tools exist to pull
# strings out of .ui/.rc/.kcfg/.desktop-style files this project doesn't
# have; app/'s only translatable sources are plain .cpp and .qml, so a direct
# xgettext invocation is both sufficient and matches the task brief's
# "otherwise write a plain xgettext invocation" fallback.
#
# Two xgettext passes merged into one .pot, mirroring the real-world KDE/
# Plasma Mobile convention (Kirigami-based apps such as Angelfish/Index/
# Calindori do the same in their own po/Messages.sh): C++ sources use
# xgettext's native C++ parser (--kde additionally flags KDE-style %1/%2
# positional format strings, matching KLocalizedString::subs()); QML sources
# use --language=JavaScript, since xgettext has no dedicated QML grammar but
# QML's script-expression/function-call syntax is JS-compatible enough for
# its JS lexer to reliably find i18n()/i18nc() call sites (this is the actual
# convention those real Kirigami apps use for their own .qml extraction).
#
# The -k keyword list matches KI18n's C++ and QML-context API
# (klocalizedstring.h, klocalizedqmlcontext.h): i18n/i18nc/i18np/i18ncp plus
# their KUIT xi18n* markup-aware counterparts, with argument positions set to
# each function's context/singular/plural parameter order.
#
# Expected right now: zero call sites. Task 49 (not this task) is the sweep
# that wraps app/qml/ and app/*.cpp's user-facing strings in i18n()/i18nc();
# this task only proves the extraction pipeline itself runs end to end.
#
# --force-po is required for that on every step (both xgettext calls AND the
# msgcat merge below): the default behavior of both tools (discovered
# running this against the current, not-yet-swept source) is to silently
# skip writing *any* output file when the result would contain zero
# translatable strings, exit 0 regardless -- without --force-po this script
# would appear to "succeed" while producing no .pot at all, rather than the
# near-empty one (header only) the task brief expects as proof the pipeline
# runs.
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
po_dir="$script_dir"
app_dir="$po_dir/../app"
pot_file="$po_dir/llamamail.pot"

cpp_files=$(find "$app_dir" -name '*.cpp' | sort)
qml_files=$(find "$app_dir/qml" -name '*.qml' | sort)

keywords="-ki18n:1 -ki18nc:1c,2 -ki18np:1,2 -ki18ncp:1c,2,3 -kxi18n:1 -kxi18nc:1c,2 -kxi18np:1,2 -kxi18ncp:1c,2,3"

work_dir=$(mktemp -d)
trap 'rm -rf "$work_dir"' EXIT

# shellcheck disable=SC2086
xgettext --c++ --kde --force-po --from-code=UTF-8 \
    --package-name=llamamail \
    --copyright-holder="Llama Mail" \
    $keywords \
    -o "$work_dir/cpp.pot" \
    $cpp_files

# shellcheck disable=SC2086
xgettext --language=JavaScript --force-po --from-code=UTF-8 \
    --package-name=llamamail \
    --copyright-holder="Llama Mail" \
    $keywords \
    -o "$work_dir/qml.pot" \
    $qml_files

msgcat --use-first --force-po -o "$pot_file" "$work_dir/cpp.pot" "$work_dir/qml.pot"

count=$(grep -c '^msgid "' "$pot_file" || true)
# msgid "" (the header entry) is always present, so a "near-empty" catalog
# reports as 1 here until Task 49 wraps real strings.
echo "extract-messages.sh: wrote $pot_file ($count msgid entries, including the header)"
