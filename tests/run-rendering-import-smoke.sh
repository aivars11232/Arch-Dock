#!/usr/bin/env bash

set -euo pipefail

readonly ARCHDOCK_RENDERING_SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
readonly ARCHDOCK_RENDERING_PROJECT_ROOT="$(cd -- "$ARCHDOCK_RENDERING_SCRIPT_DIR/.." && pwd)"

ARCHDOCK_RENDERING_STATE_ROOT=''
ARCHDOCK_RENDERING_KWIN_PID=''
ARCHDOCK_RENDERING_SERVICE_PID=''
ARCHDOCK_RENDERING_PLASMASHELL_PID=''

require_command() {
    command -v "$1" >/dev/null 2>&1 || {
        printf 'Required command is unavailable: %s\n' "$1" >&2
        exit 1
    }
}

stop_process() {
    local process_id="${1:-}"
    if [[ -n "$process_id" ]] && kill -0 "$process_id" 2>/dev/null; then
        kill "$process_id" 2>/dev/null || true
        local attempt
        for ((attempt = 0; attempt < 50; ++attempt)); do
            if ! kill -0 "$process_id" 2>/dev/null; then
                wait "$process_id" 2>/dev/null || true
                return
            fi
            sleep 0.1
        done
        kill -KILL "$process_id" 2>/dev/null || true
        wait "$process_id" 2>/dev/null || true
    fi
}

cleanup_session() {
    stop_process "$ARCHDOCK_RENDERING_SERVICE_PID"
    stop_process "$ARCHDOCK_RENDERING_PLASMASHELL_PID"
    stop_process "$ARCHDOCK_RENDERING_KWIN_PID"
}

cleanup_outer() {
    if [[ -n "$ARCHDOCK_RENDERING_STATE_ROOT" &&
          "$ARCHDOCK_RENDERING_STATE_ROOT" == /tmp/archdock-rendering-import.* ]]; then
        cmake -E remove_directory "$ARCHDOCK_RENDERING_STATE_ROOT"
    fi
}

gvariant_string() {
    sed -n "s/^('\([^']*\)',)$/\1/p"
}

plasma_script() {
    gdbus call \
        --session \
        --timeout=5 \
        --dest org.kde.plasmashell \
        --object-path /PlasmaShell \
        --method org.kde.PlasmaShell.evaluateScript \
        "$1"
}

panel_call() {
    local method="$1"
    shift
    gdbus call \
        --session \
        --timeout=5 \
        --dest org.archdock.ArchDock \
        --object-path /Control \
        --method "local.PanelWindow.$method" \
        "$@"
}

wait_for_wayland_socket() {
    local socket_path="$XDG_RUNTIME_DIR/$WAYLAND_DISPLAY"
    local attempt
    for ((attempt = 0; attempt < 200; ++attempt)); do
        [[ -S "$socket_path" ]] && return
        if ! kill -0 "$ARCHDOCK_RENDERING_KWIN_PID" 2>/dev/null; then
            printf 'Private KWin exited before creating %s.\n' "$socket_path" >&2
            return 1
        fi
        sleep 0.1
    done
    printf 'Timed out waiting for private KWin socket: %s\n' "$socket_path" >&2
    return 1
}

require_no_import_errors() {
    local log_file
    for log_file in "$ARCHDOCK_RENDERING_LOG_DIR/service.log" \
                    "$ARCHDOCK_RENDERING_LOG_DIR/plasmashell.log"; do
        if rg -n -i \
            'module "ArchDock\.Rendering" is not installed|RenderingModuleProbe[^[:cntrl:]]*(not a type|unavailable)|Panel(Scene|SurfaceLoader|Procedural2D)[^[:cntrl:]]*(not a type|unavailable|not installed)|(IconScene|RunningIndicator|LivePanelPreview)[^[:cntrl:]]*(not a type|unavailable|not installed)|(SettingsPopup|StudioForm)\.qml:[0-9]+:[0-9]+:[^[:cntrl:]]*(error|unavailable|not installed|not a type|typeerror|referenceerror|cannot assign|unable to assign|binding loop)|org\.archdock\.dock/contents/ui/(main|DockEntry|IconVisual|RunningIndicator)\.qml:[0-9]+:[0-9]+:[^[:cntrl:]]*(error|unavailable|not installed|not a type|typeerror|referenceerror|cannot assign|unable to assign|binding loop)|ArchDock/Rendering/(PanelScene|IconScene|RunningIndicator|previews/LivePanelPreview)\.qml:[0-9]+:[0-9]+:[^[:cntrl:]]*(typeerror|referenceerror|cannot assign|unable to assign|binding loop)|Error loading QML file[^[:cntrl:]]*org\.archdock\.dock' \
            "$log_file"; then
            printf 'Staged rendering import failed; relevant QML errors were logged in %s.\n' \
                "$log_file" >&2
            return 1
        fi
    done
}

run_private_session() {
    trap cleanup_session EXIT

    kwin_wayland \
        --virtual \
        --width 1280 \
        --height 720 \
        --output-count 1 \
        --socket "$WAYLAND_DISPLAY" \
        --no-global-shortcuts \
        --no-lockscreen >"$ARCHDOCK_RENDERING_LOG_DIR/kwin.log" 2>&1 &
    ARCHDOCK_RENDERING_KWIN_PID=$!
    wait_for_wayland_socket

    "$ARCHDOCK_RENDERING_STAGED_BINARY" --settings \
        >"$ARCHDOCK_RENDERING_LOG_DIR/service.log" 2>&1 &
    ARCHDOCK_RENDERING_SERVICE_PID=$!
    gdbus wait --session --timeout=20 org.archdock.ArchDock
    kill -0 "$ARCHDOCK_RENDERING_SERVICE_PID"

    plasmashell --no-respawn \
        >"$ARCHDOCK_RENDERING_LOG_DIR/plasmashell.log" 2>&1 &
    ARCHDOCK_RENDERING_PLASMASHELL_PID=$!
    gdbus wait --session --timeout=20 org.kde.plasmashell

    [[ -r "$ARCHDOCK_RENDERING_SMOKE_DESKTOP_FILE" ]] || {
        printf 'The staged smoke-test desktop entry is unavailable: %s\n' \
            "$ARCHDOCK_RENDERING_SMOKE_DESKTOP_FILE" >&2
        return 1
    }
    local smoke_desktop_url="file://$ARCHDOCK_RENDERING_SMOKE_DESKTOP_FILE"
    local pin_reply
    pin_reply="$(panel_call pinDockUrls "['$smoke_desktop_url']")"
    [[ "$pin_reply" == '(true,)' ]] || {
        printf 'Could not seed a deterministic renderer smoke entry: %s\n' \
            "$pin_reply" >&2
        return 1
    }

    local host_ids
    host_ids="$(plasma_script \
        "var result = (function() { var panel = null; var freeWidget = null; try { panel = new Panel; panel.screen = 0; panel.location = 'bottom'; var nativeWidget = panel.addWidget('org.archdock.dock'); if (!nativeWidget) { panel.remove(); return 'missing-native-widget'; } nativeWidget.currentConfigGroup = ['General']; nativeWidget.writeConfig('panelId', 'bottom'); nativeWidget.writeConfig('panelType', 'hybrid'); nativeWidget.reloadConfig(); var desktop = desktopForScreen(0); if (!desktop) { panel.remove(); return 'missing-desktop'; } freeWidget = desktop.addWidget('org.archdock.dock', Math.round(gridUnit * 2), Math.round(gridUnit * 2), Math.round(gridUnit * 16), Math.round(gridUnit * 10)); if (!freeWidget) { panel.remove(); return 'missing-free-widget'; } freeWidget.currentConfigGroup = ['General']; freeWidget.writeConfig('panelId', 'free-render-smoke'); freeWidget.writeConfig('panelType', 'hybrid'); freeWidget.writeConfig('bootstrapFreeDock', false); freeWidget.reloadConfig(); return [String(panel.id), String(nativeWidget.id), String(desktop.id), String(freeWidget.id)].join('|'); } catch (error) { if (freeWidget) { freeWidget.remove(); } if (panel) { panel.remove(); } return 'exception:' + String(error); } })(); print(result);" | gvariant_string)"

    [[ "$host_ids" =~ ^[0-9]+\|[0-9]+\|[0-9]+\|[0-9]+$ ]] || {
        printf 'Private PlasmaShell did not create native and free Arch Dock hosts: %s\n' \
            "${host_ids:-unavailable}" >&2
        return 1
    }

    local panel_id=''
    local native_applet_id=''
    local desktop_id=''
    local free_applet_id=''
    IFS='|' read -r panel_id native_applet_id desktop_id free_applet_id \
        <<<"$host_ids"
    local attempt
    local native_snapshot=''
    local free_snapshot=''
    for ((attempt = 0; attempt < 50; ++attempt)); do
        native_snapshot="$(plasma_script \
            "var panel = panelById($panel_id); var widget = panel ? panel.widgetById($native_applet_id) : null; if (!widget) { print('missing'); } else { widget.currentConfigGroup = ['General']; var geometry = widget.geometry; print([String(widget.type), String(widget.readConfig('panelId', '')), String(widget.readConfig('panelType', '')), String(Number(geometry.width)), String(Number(geometry.height))].join('|')); }" | gvariant_string)"
        free_snapshot="$(plasma_script \
            "var desktop = desktopById($desktop_id); var widget = desktop ? desktop.widgetById($free_applet_id) : null; if (!widget) { print('missing'); } else { widget.currentConfigGroup = ['General']; var geometry = widget.geometry; print([String(widget.type), String(widget.readConfig('panelId', '')), String(widget.readConfig('panelType', '')), String(widget.readConfig('bootstrapFreeDock', true)), String(Number(geometry.width)), String(Number(geometry.height))].join('|')); }" | gvariant_string)"
        if [[ "$native_snapshot" =~ ^org\.archdock\.dock\|bottom\|hybrid\|[1-9][0-9]*([.][0-9]+)?\|[1-9][0-9]*([.][0-9]+)?$ &&
              "$free_snapshot" =~ ^org\.archdock\.dock\|free-render-smoke\|hybrid\|(false|0)\|[1-9][0-9]*([.][0-9]+)?\|[1-9][0-9]*([.][0-9]+)?$ ]]; then
            break
        fi
        sleep 0.1
    done
    [[ "$native_snapshot" =~ ^org\.archdock\.dock\|bottom\|hybrid\|[1-9][0-9]*([.][0-9]+)?\|[1-9][0-9]*([.][0-9]+)?$ ]] || {
        printf 'The staged native PanelScene host was not ready: %s\n' \
            "$native_snapshot" >&2
        return 1
    }
    [[ "$free_snapshot" =~ ^org\.archdock\.dock\|free-render-smoke\|hybrid\|(false|0)\|[1-9][0-9]*([.][0-9]+)?\|[1-9][0-9]*([.][0-9]+)?$ ]] || {
        printf 'The staged free PanelScene host was not ready: %s\n' \
            "$free_snapshot" >&2
        return 1
    }

    sleep 1
    kill -0 "$ARCHDOCK_RENDERING_SERVICE_PID"
    kill -0 "$ARCHDOCK_RENDERING_PLASMASHELL_PID"
    require_no_import_errors

    plasma_script \
        "var desktop = desktopById($desktop_id); var freeWidget = desktop ? desktop.widgetById($free_applet_id) : null; if (freeWidget) { freeWidget.remove(); } var panel = panelById($panel_id); if (panel) { panel.remove(); } print('removed');" \
        >/dev/null

    printf 'Staged service/Studio and native/free PanelScene hosts succeeded.\n'
}

run_outer() {
    require_command cmake
    require_command dbus-run-session
    require_command gdbus
    require_command kwin_wayland
    require_command plasmashell
    require_command qmake6
    require_command rg
    require_command timeout

    local build_dir="${ARCHDOCK_BUILD_DIR:-$ARCHDOCK_RENDERING_PROJECT_ROOT/build}"
    local qml_install_dir="${ARCHDOCK_QML_INSTALL_DIR:-lib/qt6/qml}"
    local qmltestrunner_binary="${ARCHDOCK_QMLTESTRUNNER:-}"
    if [[ -z "$qmltestrunner_binary" ]]; then
        qmltestrunner_binary="$(qmake6 -query QT_INSTALL_BINS)/qmltestrunner"
    fi
    if [[ "$qml_install_dir" == /* || "$qml_install_dir" == *..* ]]; then
        printf 'ARCHDOCK_QML_INSTALL_DIR must be a safe relative install path: %s\n' \
            "$qml_install_dir" >&2
        return 1
    fi
    [[ -x "$build_dir/arch-dock" ]] || {
        printf 'Build Arch Dock first or set ARCHDOCK_BUILD_DIR: %s\n' \
            "$build_dir/arch-dock" >&2
        return 1
    }
    [[ -x "$qmltestrunner_binary" ]] || {
        printf 'The Qt 6 qmltestrunner is unavailable: %s\n' \
            "$qmltestrunner_binary" >&2
        return 1
    }

    ARCHDOCK_RENDERING_STATE_ROOT="$(mktemp -d /tmp/archdock-rendering-import.XXXXXX)"
    trap cleanup_outer EXIT

    local stage_root="$ARCHDOCK_RENDERING_STATE_ROOT/stage"
    local import_root="$stage_root/$qml_install_dir"
    local module_root="$import_root/ArchDock/Rendering"
    local log_dir="$ARCHDOCK_RENDERING_STATE_ROOT/logs"
    mkdir -p \
        "$ARCHDOCK_RENDERING_STATE_ROOT/cache" \
        "$ARCHDOCK_RENDERING_STATE_ROOT/config" \
        "$ARCHDOCK_RENDERING_STATE_ROOT/config-dirs" \
        "$ARCHDOCK_RENDERING_STATE_ROOT/data" \
        "$log_dir" \
        "$ARCHDOCK_RENDERING_STATE_ROOT/runtime" \
        "$ARCHDOCK_RENDERING_STATE_ROOT/state"
    chmod 700 "$ARCHDOCK_RENDERING_STATE_ROOT/runtime"

    cmake --install "$build_dir" --prefix "$stage_root"
    [[ -r "$module_root/qmldir" &&
       -r "$module_root/RenderingModuleProbe.qml" &&
       -r "$module_root/LayoutEngine.js" &&
       -r "$module_root/IconScene.qml" &&
       -r "$module_root/PanelScene.qml" &&
       -r "$module_root/PanelSurfaceLoader.qml" &&
       -r "$module_root/RunningIndicator.qml" &&
       -r "$module_root/previews/LivePanelPreview.qml" &&
       -r "$module_root/renderers/PanelProcedural2D.qml" ]] || {
        printf 'The staged ArchDock.Rendering module is incomplete: %s\n' \
            "$module_root" >&2
        return 1
    }
    [[ -r "$stage_root/share/plasma/plasmoids/org.archdock.dock/metadata.json" ]] || {
        printf 'The staged Arch Dock applet package is unavailable.\n' >&2
        return 1
    }
    [[ ! -e "$stage_root/share/plasma/plasmoids/org.archdock.dock/contents/ui/IconVisual.qml" &&
       ! -e "$stage_root/share/plasma/plasmoids/org.archdock.dock/contents/ui/RunningIndicator.qml" ]] || {
        printf 'The staged applet still contains an obsolete local icon renderer.\n' >&2
        return 1
    }

    QT_QPA_PLATFORM=offscreen \
    QML_IMPORT_PATH="$import_root" \
    QML2_IMPORT_PATH="$import_root" \
        "$qmltestrunner_binary" \
            -import "$import_root" \
            -input "$ARCHDOCK_RENDERING_SCRIPT_DIR/tst_RenderingModuleImport.qml"

    QT_QPA_PLATFORM=offscreen \
    QML_IMPORT_PATH="$import_root" \
    QML2_IMPORT_PATH="$import_root" \
        "$qmltestrunner_binary" \
            -import "$import_root" \
            -input "$ARCHDOCK_RENDERING_SCRIPT_DIR/tst_LivePanelPreview.qml"

    QT_QPA_PLATFORM=offscreen \
    QML_IMPORT_PATH="$import_root" \
    QML2_IMPORT_PATH="$import_root" \
        "$qmltestrunner_binary" \
            -import "$import_root" \
            -input "$ARCHDOCK_RENDERING_SCRIPT_DIR/tst_RendererParity.qml"

    env \
        ARCHDOCK_RENDERING_IMPORT_SESSION=1 \
        ARCHDOCK_RENDERING_LOG_DIR="$log_dir" \
        ARCHDOCK_RENDERING_SMOKE_DESKTOP_FILE="$stage_root/share/applications/org.archdock.ArchDock.desktop" \
        ARCHDOCK_RENDERING_STAGED_BINARY="$stage_root/bin/arch-dock" \
        DESKTOP_SESSION=archdock-rendering-test \
        KDE_FULL_SESSION=true \
        PATH="$stage_root/bin:$PATH" \
        QML_IMPORT_PATH="$import_root" \
        QML2_IMPORT_PATH="$import_root" \
        QT_QPA_PLATFORM=wayland \
        WAYLAND_DISPLAY=archdock-rendering-test-0 \
        XDG_CACHE_HOME="$ARCHDOCK_RENDERING_STATE_ROOT/cache" \
        XDG_CONFIG_HOME="$ARCHDOCK_RENDERING_STATE_ROOT/config" \
        XDG_CONFIG_DIRS="$ARCHDOCK_RENDERING_STATE_ROOT/config-dirs" \
        XDG_CURRENT_DESKTOP=archdock-rendering-test \
        XDG_DATA_HOME="$ARCHDOCK_RENDERING_STATE_ROOT/data" \
        XDG_DATA_DIRS="$stage_root/share:${XDG_DATA_DIRS:-/usr/local/share:/usr/share}" \
        XDG_RUNTIME_DIR="$ARCHDOCK_RENDERING_STATE_ROOT/runtime" \
        XDG_SESSION_DESKTOP=archdock-rendering-test \
        XDG_SESSION_TYPE=wayland \
        XDG_STATE_HOME="$ARCHDOCK_RENDERING_STATE_ROOT/state" \
        dbus-run-session -- \
        timeout --kill-after=10s 90s bash "$0"

    printf 'ArchDock.Rendering staged import smoke succeeded.\n'
}

if [[ "${ARCHDOCK_RENDERING_IMPORT_SESSION:-}" == '1' ]]; then
    run_private_session
else
    run_outer
fi
