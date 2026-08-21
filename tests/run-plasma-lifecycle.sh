#!/usr/bin/env bash

set -euo pipefail

readonly script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
readonly project_root="$(cd -- "$script_dir/.." && pwd)"
ARCHDOCK_LIFECYCLE_STATE_ROOT=''
ARCHDOCK_SESSION_ARCH_DOCK_PID=''
ARCHDOCK_SESSION_PLASMASHELL_PID=''
ARCHDOCK_SESSION_KWIN_PID=''
ARCHDOCK_SIGNAL_MONITOR_PID=''
ARCHDOCK_SESSION_RESULT_FILE="${ARCHDOCK_SESSION_RESULT_FILE:-}"

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
    stop_process "$ARCHDOCK_SIGNAL_MONITOR_PID"
    stop_process "$ARCHDOCK_SESSION_ARCH_DOCK_PID"
    stop_process "$ARCHDOCK_SESSION_PLASMASHELL_PID"
    stop_process "$ARCHDOCK_SESSION_KWIN_PID"
}

record_session_result() {
    local status=$?
    if [[ -n "$ARCHDOCK_SESSION_RESULT_FILE" ]]; then
        printf '%s\n' "$status" >"$ARCHDOCK_SESSION_RESULT_FILE"
    fi
    cleanup_session
}

cleanup_outer() {
    if [[ -n "$ARCHDOCK_LIFECYCLE_STATE_ROOT" ]]; then
        rm -rf "$ARCHDOCK_LIFECYCLE_STATE_ROOT"
    fi
}

panel_call() {
    local method="$1"
    shift
    gdbus call \
        --session \
        --dest org.archdock.ArchDock \
        --object-path /Control \
        --method "local.PanelWindow.$method" \
        "$@"
}

plasma_script() {
    gdbus call \
        --session \
        --timeout=3 \
        --dest org.kde.plasmashell \
        --object-path /PlasmaShell \
        --method org.kde.PlasmaShell.evaluateScript \
        "$1"
}

gvariant_string() {
    sed -n "s/^('\([^']*\)',)$/\1/p"
}

gvariant_integer() {
    sed -n 's/^(\(-\{0,1\}[0-9]\+\),)$/\1/p'
}

panel_ids() {
    plasma_script \
        "var ids = []; var all = panels(); for (var index = 0; index < all.length; ++index) { ids.push(String(all[index].id)); } ids.sort(); print(ids.join(','));"
}

available_screen_count() {
    panel_call availableScreens | grep -o "'index'" | wc -l
}

enabled_output_names() {
    kscreen-doctor --json | jq -r '.outputs[] | select(.enabled) | .name'
}

wait_for_screen_count() {
    local expected_count="$1"
    local attempt
    for ((attempt = 0; attempt < 100; ++attempt)); do
        [[ "$(available_screen_count)" == "$expected_count" ]] && return
        sleep 0.1
    done
    printf 'Timed out waiting for %s Arch Dock screens.\n' "$expected_count" >&2
    return 1
}

wait_for_native_panel_screen() {
    local containment_id="$1"
    local expected_screen="$2"
    local attempt
    local current_screen
    for ((attempt = 0; attempt < 100; ++attempt)); do
        current_screen="$(plasma_script "var panel = panelById($containment_id); print(panel ? panel.screen : -1);" 2>/dev/null | gvariant_string || true)"
        [[ "$current_screen" == "$expected_screen" ]] && return
        sleep 0.1
    done
    printf 'Timed out waiting for native panel %s on virtual output %s; last=%s.\n' \
        "$containment_id" "$expected_screen" "${current_screen:-unavailable}" >&2
    return 1
}

owned_panel_record() {
    local panel_id="$1"
    plasma_script \
        "var target = '$panel_id'; var found = -1; var token = ''; var all = panels(); for (var index = 0; index < all.length; ++index) { var panel = all[index]; panel.currentConfigGroup = ['ArchDock']; if (panel.readConfig('panelId', '') === target) { found = panel.id; token = panel.readConfig('ownerToken', ''); break; } } print(found + '|' + token);" |
        gvariant_string
}

wait_for_owned_panel() {
    local panel_id="$1"
    local attempt
    local current_record=''
    for ((attempt = 0; attempt < 100; ++attempt)); do
        current_record="$(owned_panel_record "$panel_id")"
        if [[ "$current_record" =~ ^[0-9]+\|[0-9a-f-]+$ ]]; then
            return
        fi
        sleep 0.1
    done
    printf 'Timed out waiting for an owned native panel: panel=%s record=%s\n' \
        "$panel_id" "${current_record:-unavailable}" >&2
    return 1
}

owned_dock_id() {
    local containment_id="$1"
    local panel_id="$2"
    plasma_script \
        "var panel = panelById($containment_id); var result = -1; var widgets = panel ? panel.widgets() : []; for (var index = 0; index < widgets.length; ++index) { var dock = panel.widgetById(widgets[index].id); if (dock && dock.type === 'org.archdock.dock') { dock.currentConfigGroup = ['General']; if (dock.readConfig('panelId', '') === '$panel_id') { result = dock.id; break; } } } print(result);" |
        gvariant_string
}

owned_dock_type() {
    local containment_id="$1"
    local panel_id="$2"
    plasma_script \
        "var panel = panelById($containment_id); var result = ''; var widgets = panel ? panel.widgets() : []; for (var index = 0; index < widgets.length; ++index) { var dock = panel.widgetById(widgets[index].id); if (dock && dock.type === 'org.archdock.dock') { dock.currentConfigGroup = ['General']; if (dock.readConfig('panelId', '') === '$panel_id') { result = dock.readConfig('panelType', ''); break; } } } print(result);" |
        gvariant_string
}

native_panel_presentation() {
    local containment_id="$1"
    plasma_script \
        "var panel = panelById($containment_id); if (!panel) { print('missing|-1'); } else { panel.currentConfigGroup = ['ArchDock']; print(panel.hiding + '|' + String(panel.readConfig('temporaryHidden', '0'))); }" |
        gvariant_string
}

native_widget_count() {
    local containment_id="$1"
    plasma_script "var panel = panelById($containment_id); print(panel ? panel.widgets().length : -1);" |
        gvariant_string
}

add_legacy_control_applet() {
    local containment_id="$1"
    local panel_id="$2"
    plasma_script \
        "var panel = panelById($containment_id); if (!panel) { print(-1); } else { var control = panel.addWidget('org.archdock.control'); if (!control) { print(-1); } else { control.currentConfigGroup = ['General']; control.writeConfig('panelId', '$panel_id'); control.reloadConfig(); print(control.id); } }" |
        gvariant_string
}

require_visual_dock() {
    local containment_id="$1"
    local panel_id="$2"
    local panel_type="$3"
    local dock_id
    dock_id="$(owned_dock_id "$containment_id" "$panel_id")"
    [[ "$dock_id" =~ ^[0-9]+$ ]] || {
        printf 'Native visual dock applet was not attached: %s\n' "$dock_id" >&2
        exit 1
    }
    [[ "$(owned_dock_type "$containment_id" "$panel_id")" == "$panel_type" ]] || {
        printf 'Native visual dock applet has the wrong panel type.\n' >&2
        exit 1
    }
    [[ "$(native_widget_count "$containment_id")" == '1' ]] || {
        printf 'Typed native panel should contain exactly one visual dock applet.\n' >&2
        exit 1
    }
}

require_true_reply() {
    [[ "$1" == '(true,)' ]] || {
        printf 'Unexpected D-Bus reply: %s\n' "$1" >&2
        exit 1
    }
}

require_false_reply() {
    [[ "$1" == '(false,)' ]] || {
        printf 'Unexpected D-Bus reply: %s\n' "$1" >&2
        exit 1
    }
}

log_session_phase() {
    printf 'Lifecycle phase: %s\n' "$1" >&2
}

start_plasmashell() {
    local log_name="$1"
    plasmashell --no-respawn >"$ARCHDOCK_TEST_LOG_DIR/$log_name" 2>&1 &
    ARCHDOCK_SESSION_PLASMASHELL_PID=$!
    gdbus wait --session --timeout=20 org.kde.plasmashell
}

start_compositor() {
    kwin_wayland \
        --virtual \
        --width 1280 \
        --height 720 \
        --output-count 2 \
        --no-global-shortcuts \
        --no-lockscreen >"$ARCHDOCK_TEST_LOG_DIR/kwin.log" 2>&1 &
    ARCHDOCK_SESSION_KWIN_PID=$!

    local display_socket="$XDG_RUNTIME_DIR/${WAYLAND_DISPLAY:-wayland-0}"
    local attempt
    for ((attempt = 0; attempt < 200; ++attempt)); do
        [[ -S "$display_socket" ]] && return
        if ! kill -0 "$ARCHDOCK_SESSION_KWIN_PID" 2>/dev/null; then
            printf 'Private KWin exited before creating %s.\n' "$display_socket" >&2
            return 1
        fi
        sleep 0.1
    done

    printf 'Timed out waiting for private KWin socket: %s\n' "$display_socket" >&2
    return 1
}

restart_plasmashell() {
    stop_process "$ARCHDOCK_SESSION_PLASMASHELL_PID"
    ARCHDOCK_SESSION_PLASMASHELL_PID=''
    start_plasmashell plasmashell-restart.log
}

start_signal_monitor() {
    local signal_name="$1"
    set +o pipefail
    timeout 15s stdbuf -oL gdbus monitor \
        --session \
        --dest org.archdock.ArchDock \
        --object-path /Control |
        awk -v signal_name="$signal_name" \
            '$0 ~ signal_name { found = 1; exit } END { exit found ? 0 : 1 }' &
    ARCHDOCK_SIGNAL_MONITOR_PID=$!
    set -o pipefail
}

wait_for_signal_monitor() {
    local signal_name="$1"
    local monitor_status
    set +e
    wait "$ARCHDOCK_SIGNAL_MONITOR_PID"
    monitor_status=$?
    set -e
    ARCHDOCK_SIGNAL_MONITOR_PID=''
    [[ "$monitor_status" == '0' ]] || {
        printf 'Timed out waiting for Arch Dock signal: %s\n' "$signal_name" >&2
        exit 1
    }
}

run_session() {
    trap record_session_result EXIT

    log_session_phase 'starting private KWin'
    start_compositor
    log_session_phase 'starting PlasmaShell'
    start_plasmashell plasmashell.log

    log_session_phase 'starting Arch Dock'
    "$ARCHDOCK_TEST_BINARY" >"$ARCHDOCK_TEST_LOG_DIR/arch-dock.log" 2>&1 &
    ARCHDOCK_SESSION_ARCH_DOCK_PID=$!
    gdbus wait --session --timeout=20 org.archdock.ArchDock

    gdbus introspect \
        --session \
        --dest org.archdock.ArchDock \
        --object-path /Control | grep -Fq 'interface local.PanelWindow'

    local screen_count
    screen_count="$(available_screen_count)"
    [[ "$screen_count" == '2' ]] || {
        printf 'Virtual KWin did not expose two client-visible outputs.\n' >&2
        exit 1
    }

    wait_for_owned_panel bottom
    local recovered_bottom_record
    recovered_bottom_record="$(owned_panel_record bottom)"
    local recovered_bottom_containment_id="${recovered_bottom_record%%|*}"
    local recovered_bottom_token="${recovered_bottom_record#*|}"
    require_visual_dock "$recovered_bottom_containment_id" bottom hybrid
    local recovered_bottom_ids
    recovered_bottom_ids="$(panel_ids)"
    require_true_reply "$(panel_call setPanelVisible bottom true)"
    [[ "$(owned_panel_record bottom)" == "$recovered_bottom_record" &&
        "$(panel_ids)" == "$recovered_bottom_ids" ]] || {
        printf 'Cold missing-id recovery did not persist one stable native association.\n' >&2
        exit 1
    }
    [[ -n "$recovered_bottom_token" ]] || {
        printf 'Cold missing-id recovery did not persist a fresh ownership token.\n' >&2
        exit 1
    }
    log_session_phase 'recovered visible panel from missing ids'

    local baseline_ids
    baseline_ids="$(panel_ids)"

    start_signal_monitor nativePanelRecoveryFinished
    local panel_id
    panel_id="$(panel_call createNativePanel bottom hybrid | gvariant_string)"
    [[ -n "$panel_id" ]] || {
        printf 'Could not create an Arch Dock test panel.\n' >&2
        exit 1
    }
    wait_for_signal_monitor nativePanelRecoveryFinished
    log_session_phase 'created native panel'

    local first_record
    first_record="$(owned_panel_record "$panel_id")"
    local first_containment_id="${first_record%%|*}"
    local first_token="${first_record#*|}"
    [[ "$first_containment_id" =~ ^[0-9]+$ && "$first_token" =~ ^[0-9a-f-]+$ ]] || {
        printf 'Native panel ownership marker was not persisted: %s\n' "$first_record" >&2
        exit 1
    }

    require_visual_dock "$first_containment_id" "$panel_id" hybrid

    local first_dock_id
    first_dock_id="$(owned_dock_id "$first_containment_id" "$panel_id")"
    local ids_before_hide
    ids_before_hide="$(panel_ids)"

    require_true_reply "$(panel_call setPanelVisible "$panel_id" false)"
    log_session_phase 'temporarily hid native panel'
    [[ "$(owned_panel_record "$panel_id")" == "$first_record" ]] || {
        printf 'Temporary hide changed the containment or ownership token.\n' >&2
        exit 1
    }
    [[ "$(owned_dock_id "$first_containment_id" "$panel_id")" == "$first_dock_id" ]] || {
        printf 'Temporary hide changed the native dock applet id.\n' >&2
        exit 1
    }
    [[ "$(native_panel_presentation "$first_containment_id")" == 'autohide|1' ]] || {
        printf 'Temporary hide did not apply the verified Plasma presentation.\n' >&2
        exit 1
    }
    [[ "$(panel_ids)" == "$ids_before_hide" ]] || {
        printf 'Temporary hide changed the Plasma panel id set.\n' >&2
        exit 1
    }

    require_true_reply "$(panel_call setPanelVisible "$panel_id" true)"
    log_session_phase 'showed existing native panel'
    [[ "$(owned_panel_record "$panel_id")" == "$first_record" ]] || {
        printf 'Show changed the containment or ownership token.\n' >&2
        exit 1
    }
    [[ "$(owned_dock_id "$first_containment_id" "$panel_id")" == "$first_dock_id" ]] || {
        printf 'Show changed the native dock applet id.\n' >&2
        exit 1
    }
    [[ "$(native_panel_presentation "$first_containment_id")" == 'none|0' ]] || {
        printf 'Show did not restore the verified Plasma presentation.\n' >&2
        exit 1
    }
    [[ "$(panel_ids)" == "$ids_before_hide" ]] || {
        printf 'Show duplicated or removed a Plasma panel.\n' >&2
        exit 1
    }

    local unsupported_mode
    unsupported_mode="$(plasma_script "var panel = panelById($first_containment_id); if (!panel) { print('missing'); } else { panel.hiding = 'dodgewindows'; print(panel.hiding); }" | gvariant_string)"
    [[ "$unsupported_mode" == 'dodgewindows' ]] || {
        printf 'Could not establish the unsupported-mode test precondition: %s\n' \
            "$unsupported_mode" >&2
        exit 1
    }
    require_false_reply "$(panel_call setPanelVisible "$panel_id" false)"
    [[ "$(native_panel_presentation "$first_containment_id")" == 'dodgewindows|0' ]] || {
        printf 'Unsupported temporary hide changed the native presentation.\n' >&2
        exit 1
    }
    [[ "$(owned_panel_record "$panel_id")" == "$first_record" &&
        "$(owned_dock_id "$first_containment_id" "$panel_id")" == "$first_dock_id" &&
        "$(panel_ids)" == "$ids_before_hide" ]] || {
        printf 'Unsupported temporary hide changed native or unrelated panel identity.\n' >&2
        exit 1
    }
    unsupported_mode="$(plasma_script "var panel = panelById($first_containment_id); if (!panel) { print('missing'); } else { panel.hiding = 'none'; print(panel.hiding); }" | gvariant_string)"
    [[ "$unsupported_mode" == 'none' ]] || {
        printf 'Could not restore the disposable native panel presentation.\n' >&2
        exit 1
    }
    log_session_phase 'rejected unsupported temporary hide safely'

    require_true_reply "$(panel_call setNativePanelType "$panel_id" launcher)"
    require_visual_dock "$first_containment_id" "$panel_id" launcher
    require_true_reply "$(panel_call setNativePanelType "$panel_id" hybrid)"
    require_visual_dock "$first_containment_id" "$panel_id" hybrid

    local legacy_control_id
    legacy_control_id="$(add_legacy_control_applet "$first_containment_id" "$panel_id")"
    [[ "$legacy_control_id" =~ ^[0-9]+$ ]] || {
        printf 'Could not add a disposable legacy control applet: %s\n' "$legacy_control_id" >&2
        exit 1
    }
    [[ "$(native_widget_count "$first_containment_id")" == '2' ]] || {
        printf 'Disposable legacy control applet was not added.\n' >&2
        exit 1
    }
    require_true_reply "$(panel_call createNativeKdePanel "$panel_id")"
    require_visual_dock "$first_containment_id" "$panel_id" hybrid

    local empty_panel_id
    empty_panel_id="$(panel_call createNativePanel top empty | gvariant_string)"
    [[ -n "$empty_panel_id" ]] || {
        printf 'Could not create an empty native Arch Dock panel.\n' >&2
        exit 1
    }
    local empty_record
    empty_record="$(owned_panel_record "$empty_panel_id")"
    local empty_containment_id="${empty_record%%|*}"
    [[ "$empty_containment_id" =~ ^[0-9]+$ ]] || {
        printf 'Empty native panel ownership marker was not persisted: %s\n' "$empty_record" >&2
        exit 1
    }
    [[ "$(native_widget_count "$empty_containment_id")" == '0' ]] || {
        printf 'Empty native panel unexpectedly contains widgets.\n' >&2
        exit 1
    }
    legacy_control_id="$(add_legacy_control_applet "$empty_containment_id" "$empty_panel_id")"
    [[ "$legacy_control_id" =~ ^[0-9]+$ ]] || {
        printf 'Could not add a disposable legacy control applet to an empty panel: %s\n' "$legacy_control_id" >&2
        exit 1
    }
    require_true_reply "$(panel_call createNativeKdePanel "$empty_panel_id")"
    [[ "$(native_widget_count "$empty_containment_id")" == '0' ]] || {
        printf 'Empty native panel retained a legacy control applet.\n' >&2
        exit 1
    }
    require_true_reply "$(panel_call removeNativeKdePanel "$empty_panel_id")"
    panel_call removePanel "$empty_panel_id" >/dev/null

    local typed_edge
    local typed_type
    for typed_edge in left right; do
        if [[ "$typed_edge" == left ]]; then
            typed_type=launcher
        else
            typed_type=tasks
        fi
        local typed_panel_id
        typed_panel_id="$(panel_call createNativePanel "$typed_edge" "$typed_type" | gvariant_string)"
        [[ -n "$typed_panel_id" ]] || {
            printf 'Could not create a %s native Arch Dock panel.\n' "$typed_type" >&2
            exit 1
        }
        local typed_record
        typed_record="$(owned_panel_record "$typed_panel_id")"
        local typed_containment_id="${typed_record%%|*}"
        [[ "$typed_containment_id" =~ ^[0-9]+$ ]] || {
            printf 'Typed native panel ownership marker was not persisted: %s\n' "$typed_record" >&2
            exit 1
        }
        require_visual_dock "$typed_containment_id" "$typed_panel_id" "$typed_type"
        require_true_reply "$(panel_call removeNativeKdePanel "$typed_panel_id")"
        panel_call removePanel "$typed_panel_id" >/dev/null
    done

    panel_call setPanelScreen "$panel_id" 1 >/dev/null
    local reassigned_screen
    reassigned_screen="$(plasma_script "var panel = panelById($first_containment_id); print(panel ? panel.screen : -1);" | gvariant_string)"
    [[ "$reassigned_screen" == '1' ]] || {
        printf 'Native panel did not move to virtual output 1: %s\n' "$reassigned_screen" >&2
        exit 1
    }

    local enabled_outputs
    enabled_outputs="$(enabled_output_names)"
    local secondary_output
    secondary_output="$(printf '%s\n' "$enabled_outputs" | sed -n '2p')"
    [[ -n "$secondary_output" ]] || {
        printf 'Could not identify the secondary virtual output.\n' >&2
        exit 1
    }

    log_session_phase 'disabling secondary virtual output'
    kscreen-doctor "output.$secondary_output.disable" >/dev/null
    wait_for_screen_count 1
    screen_count="$(available_screen_count)"
    [[ "$screen_count" == '1' ]] || {
        printf 'Disabling the secondary virtual output did not remove it.\n' >&2
        exit 1
    }
    wait_for_native_panel_screen "$first_containment_id" 0

    log_session_phase 'restoring secondary virtual output'
    kscreen-doctor "output.$secondary_output.enable" >/dev/null
    wait_for_screen_count 2
    screen_count="$(available_screen_count)"
    [[ "$screen_count" == '2' ]] || {
        printf 'Re-enabling the secondary virtual output did not restore it.\n' >&2
        exit 1
    }
    local restored_expected_screen
    restored_expected_screen="$(panel_call screenIndexForPanel "$panel_id" | gvariant_integer)"
    [[ "$restored_expected_screen" =~ ^[0-9]+$ ]] || {
        printf 'Could not resolve the restored stable virtual output: %s\n' \
            "$restored_expected_screen" >&2
        exit 1
    }
    if ! wait_for_native_panel_screen "$first_containment_id" "$restored_expected_screen"; then
        local restored_screen
        restored_screen="$(plasma_script "var panel = panelById($first_containment_id); print(panel ? panel.screen : -1);" | gvariant_string)"
        printf 'Native panel did not return to its stable virtual output: expected=%s actual=%s\n' \
            "$restored_expected_screen" "$restored_screen" >&2
        printf 'Arch Dock screens after restore: %s\n' "$(panel_call availableScreens)" >&2
        printf 'KScreen outputs after restore: %s\n' "$(enabled_output_names | paste -sd ',' -)" >&2
        exit 1
    fi

    local externally_removed
    externally_removed="$(plasma_script "var panel = panelById($first_containment_id); if (panel === null) { print(0); } else { panel.currentConfigGroup = ['ArchDock']; var owned = panel.readConfig('panelId', '') === '$panel_id' && panel.readConfig('ownerToken', '') === '$first_token'; if (owned) { panel.remove(); print(1); } else { print(0); } }" | gvariant_string)"
    [[ "$externally_removed" == '1' ]] || {
        printf 'Refused to remove the verified disposable containment.\n' >&2
        exit 1
    }

    start_signal_monitor nativePanelRecoveryFinished
    require_true_reply "$(panel_call setPanelVisible "$panel_id" true)"
    wait_for_signal_monitor nativePanelRecoveryFinished
    log_session_phase 'recreated missing native panel through synchronization'
    local replacement_record
    replacement_record="$(owned_panel_record "$panel_id")"
    local replacement_containment_id="${replacement_record%%|*}"
    local replacement_token="${replacement_record#*|}"
    [[ "$replacement_containment_id" =~ ^[0-9]+$ && "$replacement_token" =~ ^[0-9a-f-]+$ ]] || {
        printf 'Replacement native panel ownership marker was not persisted: %s\n' "$replacement_record" >&2
        exit 1
    }
    [[ "$replacement_token" != "$first_token" ]] || {
        printf 'Replacement native panel reused the missing host ownership token.\n' >&2
        exit 1
    }
    require_visual_dock "$replacement_containment_id" "$panel_id" hybrid
    local replacement_dock_id
    replacement_dock_id="$(owned_dock_id "$replacement_containment_id" "$panel_id")"
    local ids_after_replacement
    ids_after_replacement="$(panel_ids)"

    require_true_reply "$(panel_call setPanelVisible "$panel_id" true)"
    require_true_reply "$(panel_call setPanelVisible "$panel_id" true)"
    [[ "$(owned_panel_record "$panel_id")" == "$replacement_record" &&
        "$(owned_dock_id "$replacement_containment_id" "$panel_id")" == "$replacement_dock_id" &&
        "$(panel_ids)" == "$ids_after_replacement" ]] || {
        printf 'Repeated synchronization duplicated or replaced a recovered native panel.\n' >&2
        exit 1
    }
    log_session_phase 'repeated missing-host synchronization remained idempotent'

    start_signal_monitor nativePanelRecoveryFinished
    restart_plasmashell
    wait_for_signal_monitor nativePanelRecoveryFinished
    log_session_phase 'recovered after PlasmaShell restart'
    local restarted_record
    restarted_record="$(owned_panel_record "$panel_id")"
    local restarted_containment_id="${restarted_record%%|*}"
    local restarted_token="${restarted_record#*|}"
    [[ "$restarted_containment_id" =~ ^[0-9]+$ && "$restarted_token" =~ ^[0-9a-f-]+$ ]] || {
        printf 'Restarted native panel ownership marker was not persisted: %s\n' "$restarted_record" >&2
        exit 1
    }
    require_visual_dock "$restarted_containment_id" "$panel_id" hybrid
    local restarted_screen
    restarted_screen="$(plasma_script "var panel = panelById($restarted_containment_id); print(panel ? panel.screen : -1);" | gvariant_string)"
    local restarted_expected_screen
    restarted_expected_screen="$(panel_call screenIndexForPanel "$panel_id" | gvariant_integer)"
    [[ "$restarted_expected_screen" =~ ^[0-9]+$ ]] || {
        printf 'Could not resolve the stable virtual output after PlasmaShell restart: %s\n' \
            "$restarted_expected_screen" >&2
        exit 1
    }
    [[ "$restarted_screen" == "$restarted_expected_screen" ]] || {
        printf 'Restarted native panel did not retain its stable virtual output: expected=%s actual=%s\n' \
            "$restarted_expected_screen" "$restarted_screen" >&2
        exit 1
    }

    require_true_reply "$(panel_call removeNativeKdePanel "$panel_id")"
    local removal_result
    removal_result="$(plasma_script "print(panelById($restarted_containment_id) ? 1 : 0);" | gvariant_string)"
    [[ "$removal_result" == '0' ]] || {
        printf 'Verified native panel was not removed.\n' >&2
        exit 1
    }
    panel_call removePanel "$panel_id" >/dev/null

    local final_ids
    final_ids="$(panel_ids)"
    [[ "$final_ids" == "$baseline_ids" ]] || {
        printf 'The native lifecycle test changed non-Arch-Dock panels: before=%s after=%s\n' \
            "$baseline_ids" "$final_ids" >&2
        exit 1
    }

    log_session_phase 'completed'

}

run_outer() {
    require_command cmake
    require_command dbus-run-session
    require_command gdbus
    require_command jq
    require_command kscreen-doctor
    require_command kwin_wayland
    require_command plasmashell
    require_command stdbuf
    require_command timeout

    local build_dir="${ARCHDOCK_BUILD_DIR:-$project_root/build}"
    local binary_path="$build_dir/arch-dock"
    if [[ ! -x "$binary_path" ]]; then
        printf 'Build Arch Dock first or set ARCHDOCK_BUILD_DIR: %s\n' "$binary_path" >&2
        exit 1
    fi

    ARCHDOCK_LIFECYCLE_STATE_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/archdock-plasma-lifecycle.XXXXXX")"
    trap cleanup_outer EXIT

    local stage_root="$ARCHDOCK_LIFECYCLE_STATE_ROOT/stage"
    local log_dir="$ARCHDOCK_LIFECYCLE_STATE_ROOT/logs"
    local session_result_file="$ARCHDOCK_LIFECYCLE_STATE_ROOT/session-result"
    local session_script="$ARCHDOCK_LIFECYCLE_STATE_ROOT/run-plasma-lifecycle.sh"
    mkdir -p \
        "$ARCHDOCK_LIFECYCLE_STATE_ROOT/cache" \
        "$ARCHDOCK_LIFECYCLE_STATE_ROOT/config" \
        "$ARCHDOCK_LIFECYCLE_STATE_ROOT/config-dirs" \
        "$ARCHDOCK_LIFECYCLE_STATE_ROOT/data" \
        "$ARCHDOCK_LIFECYCLE_STATE_ROOT/home" \
        "$log_dir" \
        "$ARCHDOCK_LIFECYCLE_STATE_ROOT/runtime" \
        "$ARCHDOCK_LIFECYCLE_STATE_ROOT/state"
    chmod 700 "$ARCHDOCK_LIFECYCLE_STATE_ROOT/runtime"
    cp "$0" "$session_script"
    chmod +x "$session_script"

    cmake --install "$build_dir" --prefix "$stage_root"

    local session_runner_status
    set +e
    env \
        ARCHDOCK_PLASMA_LIFECYCLE_SESSION=1 \
        ARCHDOCK_SESSION_RESULT_FILE="$session_result_file" \
        ARCHDOCK_TEST_BINARY="$binary_path" \
        ARCHDOCK_TEST_LOG_DIR="$log_dir" \
        DESKTOP_SESSION=archdock-test \
        HOME="$ARCHDOCK_LIFECYCLE_STATE_ROOT/home" \
        KDE_FULL_SESSION=true \
        PATH="$stage_root/bin:$PATH" \
        QT_QPA_PLATFORM=wayland \
        XDG_CACHE_HOME="$ARCHDOCK_LIFECYCLE_STATE_ROOT/cache" \
        XDG_CONFIG_HOME="$ARCHDOCK_LIFECYCLE_STATE_ROOT/config" \
        XDG_CONFIG_DIRS="$ARCHDOCK_LIFECYCLE_STATE_ROOT/config-dirs" \
        XDG_CURRENT_DESKTOP=archdock-test \
        XDG_DATA_HOME="$ARCHDOCK_LIFECYCLE_STATE_ROOT/data" \
        XDG_DATA_DIRS="$stage_root/share:${XDG_DATA_DIRS:-/usr/local/share:/usr/share}" \
        XDG_RUNTIME_DIR="$ARCHDOCK_LIFECYCLE_STATE_ROOT/runtime" \
        XDG_SESSION_DESKTOP=archdock-test \
        XDG_SESSION_TYPE=wayland \
        XDG_STATE_HOME="$ARCHDOCK_LIFECYCLE_STATE_ROOT/state" \
        dbus-run-session -- \
        timeout --kill-after=10s 120s "$session_script" >"$log_dir/session.log" 2>&1
    session_runner_status=$?
    set -e

    local session_status=''
    if [[ -f "$session_result_file" ]]; then
        session_status="$(<"$session_result_file")"
    fi
    [[ "$session_runner_status" == '0' && "$session_status" == '0' ]] || {
        printf 'Isolated Plasma lifecycle session failed: runner=%s session=%s\n' \
            "$session_runner_status" "${session_status:-missing}" >&2
        rg -n -i 'lifecycle phase|timed out|could not|native panel|unexpected' \
            "$log_dir/session.log" >&2 || true
        tail -n 120 "$log_dir/session.log" >&2 || true
        exit 1
    }

    printf 'Isolated Plasma native lifecycle succeeded.\n'
}

if [[ "${ARCHDOCK_PLASMA_LIFECYCLE_SESSION:-}" == '1' ]]; then
    run_session
else
    run_outer
fi
