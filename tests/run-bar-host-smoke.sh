#!/usr/bin/env bash
# Arch Dock's own bars in a private KWin: the bar host (arch-dock-bars) shows
# the bars of a list - two side by side on the bottom edge, one holding Arch
# Dock's dock, and one on the left edge - each where the list puts it, with
# one strip per edge keeping a maximised window clear of them. When the list
# changes, the bars follow: a bar taken off the list goes with what it held,
# and a middle bar moves aside of a corner bar instead of covering it.
set -euo pipefail

build_dir="${ARCHDOCK_BUILD_DIR:?ARCHDOCK_BUILD_DIR is required}"
source_dir="${ARCHDOCK_SOURCE_DIR:?ARCHDOCK_SOURCE_DIR is required}"

if [[ "${ARCHDOCK_BAR_SMOKE_INNER:-}" != 1 ]]; then
    for command in kwin_wayland dbus-run-session gdbus; do
        command -v "$command" >/dev/null || { printf 'bar-host-smoke needs %s.\n' "$command" >&2; exit 1; }
    done
    root="$(mktemp -d "${TMPDIR:-/tmp}/archdock-bars.XXXXXX")"
    trap "$(printf 'rm -rf -- %q' "$root")" EXIT
    mkdir -p "$root"/{runtime,config,data,cache,logs}
    chmod 700 "$root/runtime"
    # Arch Dock's own packages, from the source tree.
    mkdir -p "$root/data/plasma/plasmoids" "$root/data/plasma/shells"
    cp -a "$source_dir/plasma-dock-widget" "$root/data/plasma/plasmoids/org.archdock.dock"
    cp -a "$source_dir/plasma-bar-containment" "$root/data/plasma/plasmoids/org.archdock.bar"
    cp -a "$source_dir/plasma-bar-shell" "$root/data/plasma/shells/org.archdock.barshell"
    status=0
    env -i HOME="$root" USER="${USER:-tester}" PATH="/usr/local/bin:/usr/bin:/bin" LANG=C.UTF-8 \
        ARCHDOCK_BAR_SMOKE_INNER=1 ARCHDOCK_BUILD_DIR="$build_dir" ARCHDOCK_SOURCE_DIR="$source_dir" \
        ARCHDOCK_QML_TOOL="${ARCHDOCK_QML_TOOL:-qml}" ARCHDOCK_TEST_LOG_DIR="$root/logs" ROOT="$root" \
        XDG_RUNTIME_DIR="$root/runtime" XDG_CONFIG_HOME="$root/config" XDG_DATA_HOME="$root/data" \
        XDG_CACHE_HOME="$root/cache" XDG_DATA_DIRS="/usr/local/share:/usr/share" \
        WAYLAND_DISPLAY="archdock-bars-$$" QT_QPA_PLATFORM=wayland QT_FORCE_STDERR_LOGGING=1 \
        QML_IMPORT_PATH="${ARCHDOCK_QML_IMPORT_ROOT:-$build_dir/qml-imports}" \
        dbus-run-session -- bash "$0" || status=$?
    if [[ "$status" != 0 ]]; then
        for log in "$root"/logs/*.log; do
            [[ -f "$log" ]] || continue
            printf -- '--- %s\n' "$(basename "$log")" >&2
            tail -n 40 "$log" >&2
        done
    fi
    exit "$status"
fi

log_dir="$ARCHDOCK_TEST_LOG_DIR"
records="$ROOT/bars.json"
# run_kwin_snippet: JavaScript run once in the private KWin.
source "$source_dir/tests/run-session-startup-smoke.sh"

kwin_pid=''
host_pid=''
window_pid=''
stop_all() {
    for pid in "$window_pid" "$host_pid" "$kwin_pid"; do
        [[ -n "$pid" ]] && kill -TERM "$pid" 2>/dev/null || true
    done
}
trap stop_all EXIT

fail() { printf 'bar-host-smoke: %s\n' "$*" >&2; exit 1; }

env QT_LOGGING_RULES='js.debug=true' kwin_wayland --virtual --width 1280 --height 720 --output-count 1 \
    --socket "$WAYLAND_DISPLAY" --no-global-shortcuts --no-lockscreen >"$log_dir/kwin.log" 2>&1 &
kwin_pid=$!
for ((attempt = 0; attempt < 200; ++attempt)); do
    [[ -S "$XDG_RUNTIME_DIR/$WAYLAND_DISPLAY" ]] && break
    kill -0 "$kwin_pid" 2>/dev/null || fail 'the private KWin exited before it was ready.'
    sleep 0.1
done
[[ -S "$XDG_RUNTIME_DIR/$WAYLAND_DISPLAY" ]] || fail 'the private KWin did not start.'

# The bar host's windows as KWin has them, one "layer|x|y|width|height" line
# each, in order; and a window of the test's own by its title.
bar_windows() {
    run_kwin_snippet 'const windows = workspace.windowList();
for (let index = 0; index < windows.length; ++index) {
    const w = windows[index];
    if (w.resourceClass === "arch-dock-bars")
        report([w.layer, w.frameGeometry.x, w.frameGeometry.y, w.frameGeometry.width, w.frameGeometry.height].join("|"));
}' | sort
}
test_window() {
    run_kwin_snippet 'const windows = workspace.windowList();
for (let index = 0; index < windows.length; ++index) {
    const w = windows[index];
    if (w.caption === "bar-smoke-window")
        report([w.frameGeometry.x, w.frameGeometry.y, w.frameGeometry.width, w.frameGeometry.height].join("|"));
}'
}
# Waits until a listing is what is expected, or says what it was.
expect() {
    local what="$1" wanted="$2" lister="$3" seen='' attempt
    for ((attempt = 0; attempt < 60; ++attempt)); do
        seen="$("$lister")" || true
        [[ "$seen" == "$wanted" ]] && { printf 'PASS: %s\n' "$what"; return 0; }
        sleep 0.25
    done
    fail "$what: expected [$(tr '\n' ' ' <<<"$wanted")] but KWin has [$(tr '\n' ' ' <<<"$seen")]"
}
# Writes the list whole, so the bar host never reads half of it.
write_records() {
    printf '%s\n' "$1" >"$records.new"
    mv -f "$records.new" "$records"
}

write_records '[
  {"id": "bar-a", "type": "empty", "edge": "bottom", "alignment": "start", "thickness": 60,
   "lengthMode": "fixed", "fixedLength": 500},
  {"id": "bar-b", "type": "launcher", "edge": "bottom", "alignment": "end", "thickness": 76,
   "lengthMode": "fixed", "fixedLength": 400, "offset": 40},
  {"id": "bar-c", "type": "empty", "edge": "left", "alignment": "start", "thickness": 50,
   "lengthMode": "fixed", "fixedLength": 300, "offset": 20}
]'
ARCHDOCK_BARS_RECORDS_FILE="$records" "$ARCHDOCK_BUILD_DIR/arch-dock-bars" >"$log_dir/bars.log" 2>&1 &
host_pid=$!

# Layer 1 is the bottom layer (the strips), 3 the top one (the bars). The
# bottom edge's bars stand side by side at its two ends, the second 40 px in
# from its corner; the left bar 20 px down from the top. KWin gives the
# strips their room in turn, so the left one stands in what the bottom one
# leaves.
expect 'three bars where the list puts them, a strip along each of their edges' "$(printf '%s\n' \
    '1|0|0|1|644' '1|0|719|1280|1' '3|0|20|50|300' '3|0|660|500|60' '3|840|644|400|76' | sort)" bar_windows

# A maximised window keeps clear of every bar: of the left one's 50 px and
# of the deepest bottom bar's 76 px.
printf 'import QtQuick\nimport QtQuick.Window\nWindow { title: "bar-smoke-window"; width: 300; height: 200; visible: true; visibility: Window.Maximized }\n' \
    >"$ROOT/window.qml"
"$ARCHDOCK_QML_TOOL" "$ROOT/window.qml" >"$log_dir/window.log" 2>&1 &
window_pid=$!
expect 'a maximised window stops at the bars' '50|0|1230|644' test_window

# The bar holding Arch Dock's dock tells it which panel it is; the empty
# bars hold nothing.
layout="$XDG_CONFIG_HOME/arch-dock-bars-appletsrc"
[[ "$(grep -c '^plugin=org.archdock.dock$' "$layout")" == 1 ]] \
    && grep -qx 'panelId=bar-b' "$layout" && grep -qx 'panelType=launcher' "$layout" \
    || fail "the bars' layout does not hold the one dock it should: $(tr '\n' ' ' <"$layout")"
printf 'PASS: the dock widget is in its bar, told its panel\n'

# The list changes: the left bar goes, and the first bar moves to the middle,
# where it would cover the end bar; it stands aside of it, a gap away.
write_records '[
  {"id": "bar-a", "type": "empty", "edge": "bottom", "alignment": "center", "thickness": 60,
   "lengthMode": "fixed", "fixedLength": 500},
  {"id": "bar-b", "type": "launcher", "edge": "bottom", "alignment": "end", "thickness": 76,
   "lengthMode": "fixed", "fixedLength": 400, "offset": 40}
]'
expect 'the bars follow their list' "$(printf '%s\n' '1|0|719|1280|1' '3|332|660|500|60' '3|840|644|400|76' | sort)" bar_windows
expect 'the maximised window takes the room the left bar left' '0|0|1280|644' test_window
removed_bar() { grep -c '^archDockBar=bar-c$' "$layout" || true; }
expect 'a bar taken off the list leaves the layout' '0' removed_bar

# A deeper bar: its edge's strip reserves its depth, and the maximised window
# makes room for it.
write_records '[
  {"id": "bar-a", "type": "empty", "edge": "bottom", "alignment": "center", "thickness": 60,
   "lengthMode": "fixed", "fixedLength": 500},
  {"id": "bar-b", "type": "launcher", "edge": "bottom", "alignment": "end", "thickness": 90,
   "lengthMode": "fixed", "fixedLength": 400, "offset": 40}
]'
expect 'a deeper bar' "$(printf '%s\n' '1|0|719|1280|1' '3|332|660|500|60' '3|840|630|400|90' | sort)" bar_windows
expect 'the maximised window makes room for the deeper bar' '0|0|1280|630' test_window

# Asked to stop, the bar host stops at once.
kill -TERM "$host_pid"
for ((attempt = 0; attempt < 50; ++attempt)); do
    kill -0 "$host_pid" 2>/dev/null || break
    sleep 0.1
done
kill -0 "$host_pid" 2>/dev/null && fail 'the bar host did not stop when asked.'
wait "$host_pid" || fail 'the bar host stopped with a failure.'
host_pid=''
printf 'PASS: the bar host stops when asked\n'
