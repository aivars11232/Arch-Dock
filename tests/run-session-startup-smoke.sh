#!/usr/bin/env bash
# Runtime mode is sourced by the existing disposable Plasma lifecycle harness.
set -euo pipefail

run_startup_diagnostics() {
    local build_dir="${ARCHDOCK_BUILD_DIR:?ARCHDOCK_BUILD_DIR is required}"
    local root
    root="$(mktemp -d "${TMPDIR:-/tmp}/archdock-startup-diagnostics.XXXXXX")"
    trap "$(printf 'rm -rf -- %q' "$root")" EXIT
    local status=0
    env QT_QPA_PLATFORM=offscreen QT_FORCE_STDERR_LOGGING=1 \
        DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent/archdock-startup-test-bus \
        "$build_dir/arch-dock" >"$root/disconnected.log" 2>&1 || status=$?
    [[ "$status" == 1 ]] && \
        rg -q 'Arch Dock session D-Bus connection failed:' "$root/disconnected.log" || {
        cat "$root/disconnected.log" >&2
        printf 'A disconnected session bus did not produce a failure diagnostic.\n' >&2
        return 1
    }
    cmake --install "$build_dir" --prefix "$root/prefix with spaces" >"$root/install.log" 2>&1 || {
        cat "$root/install.log" >&2; return 1;
    }
    # Remove only this disposable installed executable; exercise native D-Bus
    # activation with the actual installed descriptor and its generated path.
    rm -- "$root/prefix with spaces/bin/arch-dock"
    status=0
    env XDG_DATA_HOME="$root/data" XDG_CONFIG_HOME="$root/config" \
        XDG_DATA_DIRS="$root/prefix with spaces/share:/usr/share" \
        dbus-run-session -- gdbus call --session --timeout=10 \
        --dest org.freedesktop.DBus --object-path /org/freedesktop/DBus \
        --method org.freedesktop.DBus.StartServiceByName org.archdock.ArchDock 0 \
        >"$root/missing.log" 2>&1 || status=$?
    [[ "$status" != 0 ]] && \
        rg -q 'org.freedesktop.DBus.Error.Spawn.ExecFailed' "$root/missing.log" && \
        rg -q 'org.archdock.ArchDock: No such file or directory' "$root/missing.log" || {
        cat "$root/missing.log" >&2
        printf 'Missing executable activation did not identify the service and failure.\n' >&2
        return 1
    }
    local executable
    executable="$(sed -n 's/^Exec=//p' "$root/prefix with spaces/share/dbus-1/services/org.archdock.ArchDock.service")"
    [[ "$executable" == "\"$root/prefix with spaces/bin/arch-dock\" --dbus-activated" ]] || return 1
    printf 'Native activation refused the missing executable; installed Exec=%s\n' "$executable"
    rm -rf -- "$root"
    trap - EXIT
    printf 'Disconnected-bus and missing-installed-executable diagnostics PASS.\n'
}

run_session_startup_smoke() {
    [[ "${ARCHDOCK_PLASMA_LIFECYCLE_SESSION:-}" == 1 && \
       "$XDG_CURRENT_DESKTOP" == archdock-test ]] || return 2
    source "$ARCHDOCK_PRESET_MATRIX_SCRIPT"
    trap record_session_result EXIT
    [[ ! -e "$ARCHDOCK_STARTUP_SOURCE_ROOT/CMakeLists.txt" &&
       ! -e "$ARCHDOCK_STARTUP_SOURCE_ROOT/assets" &&
       ! -e "$ARCHDOCK_STARTUP_SOURCE_ROOT/qml" ]] || {
        printf 'The source checkout is still visible to the installed runtime.\n' >&2; return 1;
    }
    if [[ "${ARCHDOCK_STARTUP_WITHOUT_QUICK3D:-0}" == 1 ]]; then
        [[ ! -e /usr/lib/qt6/qml/QtQuick3D/qmldir ]] || return 1
        log_session_phase 'TASK-0043 optional Qt Quick 3D runtime module absent'
    fi
    log_session_phase 'TASK-0043 installed activation private startup'
    start_compositor
    start_plasmashell plasmashell.log
    # The stage's bin directory is absent from PATH. KDE's native activation
    # environment update is confined to this private bus.
    export PATH="$ARCHDOCK_STARTUP_ORIGINAL_PATH"
    export WAYLAND_DISPLAY="${WAYLAND_DISPLAY:-wayland-0}"
    dbus-update-activation-environment PATH WAYLAND_DISPLAY QT_QPA_PLATFORM \
        QML_IMPORT_PATH XDG_DATA_HOME XDG_DATA_DIRS XDG_CONFIG_HOME XDG_CACHE_HOME
    local reply
    reply="$(gdbus call --session --timeout=30 --dest org.freedesktop.DBus \
        --object-path /org/freedesktop/DBus --method org.freedesktop.DBus.StartServiceByName \
        org.archdock.ArchDock 0)"
    # Plasma's native DBusServiceWatcher can activate the installed backend
    # before this explicit request. Both native routes must converge on it.
    [[ "$reply" == '(uint32 1,)' || "$reply" == '(uint32 2,)' ]] || return 1
    ARCHDOCK_SESSION_ARCH_DOCK_PID="$(arch_dock_service_pid)"
    [[ "$(readlink -f "/proc/$ARCHDOCK_SESSION_ARCH_DOCK_PID/exe")" == \
       "$(readlink -f "$ARCHDOCK_STARTUP_STAGED_BINARY")" ]] || {
        printf 'Activation did not start the installed executable.\n' >&2; return 1;
    }
    [[ "$(sha256sum "/proc/$ARCHDOCK_SESSION_ARCH_DOCK_PID/exe" | cut -d ' ' -f 1)" == \
       "$ARCHDOCK_STARTUP_EXPECTED_SHA256" ]] || {
        printf 'Activated executable bytes differ from the installed payload.\n' >&2; return 1;
    }
    wait_for_kwin_script_state org.archdock.windowwatcher.runtime true 'installed activation'
    PRESET_UNRELATED_ID="$(create_unrelated_panel_fixture)"
    wait_for_native_panel_screen "$PRESET_UNRELATED_ID" 0
    PRESET_UNRELATED_SNAPSHOT="$(unrelated_panel_snapshot "$PRESET_UNRELATED_ID")"
    local fixture
    fixture="$(create_unrelated_free_host_sentinel)"
    PRESET_FREE_DESKTOP="${fixture%%|*}"; PRESET_FREE_APPLET="${fixture#*|}"
    PRESET_FREE_SNAPSHOT="$(wait_for_free_host_snapshot_stable "$PRESET_FREE_DESKTOP" "$PRESET_FREE_APPLET" fixture)"
    wait_for_owned_panel bottom
    local host ids owner
    host="$(owned_panel_record bottom)"; host="${host%%|*}"
    wait_for_panel_registry_value bottom nativePanelId "$host"
    require_visual_dock "$host" bottom hybrid
    local renderer
    renderer="$(preset_renderer bottom)"
    jq -e --arg root "$(dirname -- "$(dirname -- "$ARCHDOCK_STARTUP_STAGED_BINARY")")/share/arch-dock" \
        '.iconStyleDefinition.sourceRoot | startswith($root + "/icon-styles/")' \
        <<<"$renderer" >/dev/null
    ids="$(panel_ids)"
    owner="$ARCHDOCK_SESSION_ARCH_DOCK_PID"
    "$ARCHDOCK_STARTUP_STAGED_BINARY" >"$ARCHDOCK_TEST_LOG_DIR/repeated-launch.log" 2>&1
    "$ARCHDOCK_STARTUP_STAGED_BINARY" --settings >"$ARCHDOCK_TEST_LOG_DIR/forward-settings.log" 2>&1
    reply="$(gdbus call --session --dest org.freedesktop.DBus \
        --object-path /org/freedesktop/DBus --method org.freedesktop.DBus.StartServiceByName \
        org.archdock.ArchDock 0)"
    [[ "$reply" == '(uint32 2,)' && "$(arch_dock_service_pid)" == "$owner" && \
       "$(panel_ids)" == "$ids" ]] || {
        printf 'Repeated launch changed the service owner or managed panel set.\n' >&2; return 1;
    }
    preset_unrelated_unchanged 'installed activation and repeated launch'
    [[ "$(rg -c "Successfully activated service 'org.archdock.ArchDock'" \
        "$ARCHDOCK_TEST_LOG_DIR/session.log")" == 1 ]] || {
        printf 'Expected exactly one successful native D-Bus activation.\n' >&2; return 1;
    }
    if rg -n 'module "ArchDock.Rendering" is not installed|error when loading applet "org.archdock.dock"|ReferenceError:|TypeError:' \
        "$ARCHDOCK_TEST_LOG_DIR"/*.log; then
        printf 'Installed startup produced a runtime resource error.\n' >&2; return 1
    fi
    log_session_phase 'TASK-0043 installed activation, single owner and hidden source PASS'
    run_intentional_quit_smoke "$owner"
}

wait_for_arch_dock_owner() {
    local previous="$1" phase="$2" pid attempt
    for ((attempt = 0; attempt < 300; ++attempt)); do
        if pid="$(arch_dock_service_pid)" && [[ "$pid" != "$previous" ]]; then
            printf '%s\n' "$pid"; return 0
        fi
        sleep 0.1
    done
    printf 'Arch Dock did not start during %s.\n' "$phase" >&2
    return 1
}

wait_for_no_arch_dock_owner() {
    local phase="$1" attempt
    for ((attempt = 0; attempt < 150; ++attempt)); do
        arch_dock_service_pid >/dev/null || [[ $? != 1 ]] || return 0
        sleep 0.1
    done
    printf 'Arch Dock kept its D-Bus name after %s.\n' "$phase" >&2
    return 1
}

# A stopped Arch Dock must not come back while new Plasma windows report to
# the KWin watcher and freshly loaded applets look for their backend.
hold_stopped() {
    local phase="$1" until=$((SECONDS + 6)) reply
    while ((SECONDS < until)); do
        if arch_dock_service_pid >/dev/null; then
            printf 'Arch Dock started again after %s.\n' "$phase" >&2; return 1
        fi
        sleep 0.2
    done
    if reply="$(gdbus call --session --timeout=10 --dest org.freedesktop.DBus \
        --object-path /org/freedesktop/DBus --method org.freedesktop.DBus.StartServiceByName \
        org.archdock.ArchDock 0 2>&1)"; then
        printf 'On-demand activation started a stopped Arch Dock: %s\n' "$reply" >&2; return 1
    fi
    [[ "$reply" == *org.freedesktop.DBus.Error.Spawn.ChildExited*'status 75'* ]] || {
        printf 'Unexpected refusal of on-demand activation: %s\n' "$reply" >&2; return 1;
    }
}

run_intentional_quit_smoke() {
    local owner="$1" stop_file="$XDG_RUNTIME_DIR/arch-dock-intentional-stop.json" pid reply
    log_session_phase 'ADFIX-TASK-001 a killed backend recovers; Quit and TERM stay stopped'
    # KILL runs nothing in the process, so it is a crash: the KWin watcher's
    # next report or an applet's request activates the backend again.
    # The watcher usually reports before a plasmashell restart could: the name
    # may never be seen free, only owned by a new process.
    kill -KILL "$owner"
    restart_plasmashell
    pid="$(wait_for_arch_dock_owner "$owner" 'crash recovery')"
    ARCHDOCK_SESSION_ARCH_DOCK_PID="$pid"
    [[ ! -e "$stop_file" ]] || { printf 'A killed backend recorded a stop.\n' >&2; return 1; }
    wait_for_kwin_script_state org.archdock.windowwatcher.runtime true 'crash recovery'
    log_session_phase "ADFIX-TASK-001 KILL recovered through activation: $owner -> $pid"

    "$ARCHDOCK_STARTUP_STAGED_BINARY" --quit >"$ARCHDOCK_TEST_LOG_DIR/quit.log" 2>&1 || {
        cat "$ARCHDOCK_TEST_LOG_DIR/quit.log" >&2; return 1;
    }
    wait_for_no_arch_dock_owner 'Quit'
    ARCHDOCK_SESSION_ARCH_DOCK_PID=''
    jq -e '.reason == "quit"' "$stop_file" >/dev/null
    wait_for_kwin_script_state org.archdock.windowwatcher.runtime false 'Quit'
    restart_plasmashell
    hold_stopped 'Quit'
    log_session_phase 'ADFIX-TASK-001 Quit stayed stopped through a Plasma restart and activation'

    # Starting it explicitly ends the stop and restores normal activation.
    "$ARCHDOCK_STARTUP_STAGED_BINARY" >"$ARCHDOCK_TEST_LOG_DIR/explicit-start.log" 2>&1 &
    ARCHDOCK_SESSION_ARCH_DOCK_PID=$!
    pid="$(wait_for_arch_dock_owner '' 'explicit start')"
    [[ "$pid" == "$ARCHDOCK_SESSION_ARCH_DOCK_PID" && ! -e "$stop_file" ]] || {
        printf 'An explicit start did not clear the stop.\n' >&2; return 1;
    }
    wait_for_kwin_script_state org.archdock.windowwatcher.runtime true 'explicit start'
    reply="$(gdbus call --session --dest org.freedesktop.DBus --object-path /org/freedesktop/DBus \
        --method org.freedesktop.DBus.StartServiceByName org.archdock.ArchDock 0)"
    [[ "$reply" == '(uint32 2,)' ]]

    # TERM, which System Monitor's "Quit Application" sends, is a Quit.
    kill -TERM "$pid"
    wait "$pid" 2>/dev/null || true
    ARCHDOCK_SESSION_ARCH_DOCK_PID=''
    wait_for_no_arch_dock_owner 'TERM'
    jq -e '.reason == "signal"' "$stop_file" >/dev/null
    wait_for_kwin_script_state org.archdock.windowwatcher.runtime false 'TERM'
    hold_stopped 'TERM'
    if rg -n 'module "ArchDock.Rendering" is not installed|error when loading applet "org.archdock.dock"|ReferenceError:|TypeError:' \
        "$ARCHDOCK_TEST_LOG_DIR"/*.log; then
        printf 'Quit or crash recovery produced a runtime error.\n' >&2; return 1
    fi
    log_session_phase 'ADFIX-TASK-001 intentional Quit versus crash PASS'
}

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
    case "${1:-runtime}" in
        diagnostics) run_startup_diagnostics ;;
        runtime)
            script_root="$(cd -- "$(dirname -- "$0")" && pwd)"
            exec env ARCHDOCK_STARTUP_SMOKE=1 bash "$script_root/run-plasma-lifecycle.sh"
            ;;
        *) printf 'Unsupported startup smoke mode.\n' >&2; exit 2 ;;
    esac
fi
