#!/usr/bin/env bash
# Sourced by the existing lifecycle harness; all mutations use its private bus.
set -euo pipefail

hardening_group_valid() {
    case "$1" in scale100|scale125|scale150|scale200|hotplug-recovery|resources) return 0 ;; *) return 2 ;; esac
}

hardening_wait_revision() {
    local previous="$1" attempt current
    for ((attempt=0; attempt<100; ++attempt)); do
        current="$(preset_variant_json "$(panel_property screenRevision)")"
        (( current > previous )) && return
        sleep 0.1
    done
    printf 'Display geometry/DPI change was not published: before=%s after=%s\n' "$previous" "$current" >&2
    return 1
}

hardening_scale_group() {
    local percent="${ARCHDOCK_HARDENING_MATRIX_GROUP#scale}" scale previous edge output
    scale="$(awk -v p="$percent" 'BEGIN {print p/100}')"
    previous="$(preset_variant_json "$(panel_property screenRevision)")"
    kscreen-doctor "output.$HARDENING_PRIMARY.scale.$scale" \
        "output.$HARDENING_SECONDARY.scale.$scale" \
        "output.$HARDENING_PRIMARY.position.0,0" \
        "output.$HARDENING_SECONDARY.position.1400,0" >/dev/null
    hardening_wait_revision "$previous"
    kscreen-doctor --json | jq -e --argjson scale "$scale" \
        --arg first "$HARDENING_PRIMARY" --arg second "$HARDENING_SECONDARY" \
        '([.outputs[] | select(.enabled) | .scale] | length==2 and all(.[]; .==$scale)) and
         ([.outputs[] | select(.name==$first) | .pos.x] == [0]) and
         ([.outputs[] | select(.name==$second) | .pos.x] == [1400])' >/dev/null
    for output in 0 1; do
        panel_call setPanelScreen bottom "$output" >/dev/null
        for edge in bottom top left right; do
            local width=320 height=64
            if [[ "$edge" == left || "$edge" == right ]]; then width=64; height=320; fi
            require_structured_placement_reply "$(panel_call applyNativePanelPlacementDraft bottom \
                "{'edge': <'$edge'>, 'alignment': <'center'>, 'dynamic': <false>, 'width': <int32 $width>, 'height': <int32 $height>}")" applied true ''
            wait_for_native_panel_placement "$HARDENING_BOTTOM_HOST" bottom "$edge" center "$edge at $percent%"
            local surface_attempt
            for ((surface_attempt=0; surface_attempt<60; ++surface_attempt)); do
                reload_visibility_probe
                if awk -F'|' -v output="$output" -v edge="$edge" \
                    '$15 == "true" && $17 == output && ((edge == "left" || edge == "right") ? $9 == 320 : $8 == 320) { found=1 } END { exit !found }' \
                    "$ARCHDOCK_VISIBILITY_PROBE_SNAPSHOT"; then break; fi
                sleep 0.1
            done
            if (( surface_attempt == 60 )); then
                printf 'Native surface did not reach output %s edge %s at %s%%.\n' "$output" "$edge" "$percent" >&2
                cat "$ARCHDOCK_VISIBILITY_PROBE_SNAPSHOT" >&2
                panel_call availableScreens >&2
                kscreen-doctor --json | jq '[.outputs[] | {name,enabled,pos,scale,priority}]' >&2
                plasma_script "print(JSON.stringify([screenGeometry(0),screenGeometry(1)]));" >&2
                return 1
            fi
        done
    done
    panel_call applyNativePanelPlacementDraft bottom "{'edge': <'bottom'>, 'width': <int32 320>, 'height': <int32 64>}" >/dev/null
    # Real installed cards, the same shared-renderer pixel assertions as the
    # staged preview gate, now delivered through the scaled Wayland compositor.
    ARCHDOCK_PRESET_STAGE_PREFIX="$ARCHDOCK_HARDENING_STAGE_PREFIX" \
        "$ARCHDOCK_HARDENING_BUILD_DIR/preset-library-test" \
        everyBuiltInCardRendersThroughTheSharedRenderer browsingEveryPresetPageChangesNoPanel \
        >"$ARCHDOCK_TEST_LOG_DIR/cards-$percent.log" 2>&1
    "$ARCHDOCK_HARDENING_QMLTESTRUNNER" -input "$ARCHDOCK_HARDENING_SOURCE_ROOT/tests/tst_CoreAccessibility.qml" \
        -import "$ARCHDOCK_HARDENING_STAGE_PREFIX/${ARCHDOCK_QML_INSTALL_DIR:-lib/qt6/qml}" \
        >"$ARCHDOCK_TEST_LOG_DIR/accessibility-$percent.log" 2>&1
    "$ARCHDOCK_HARDENING_QMLTESTRUNNER" -input "$ARCHDOCK_HARDENING_SOURCE_ROOT/tests/tst_PanelSkin2D.qml" \
        -import "$ARCHDOCK_HARDENING_STAGE_PREFIX/${ARCHDOCK_QML_INSTALL_DIR:-lib/qt6/qml}" \
        PanelSkin2D::test_replacedAlphaMasksReleaseCanvasImages \
        >"$ARCHDOCK_TEST_LOG_DIR/input-$percent.log" 2>&1
    preset_begin panel circular-blue-ring true >/dev/null
    preset_ok cancel >/dev/null
    preset_no_orphans
    preset_unrelated_unchanged "scale$percent"
    log_session_phase "TASK-0044 $percent%: two outputs, all edges, 30 rendered cards, keyboard controls and audition cleanup passed"
}

hardening_hotplug_group() {
    local requested count id token state attempt
    panel_call setPanelScreen bottom 0 >/dev/null
    wait_for_native_panel_screen "$HARDENING_BOTTOM_HOST" 0
    requested="$(panel_registry_value bottom screenId)"
    [[ -n "$requested" ]]
    count="$(free_panel_count)"
    preset_begin panel circular-blue-ring true >/dev/null
    id="$(preset_call getStatus | jq -r '.panelId')"
    token="$(jq -r '.record.previewToken' "$(preset_journal_path)")"
    jq -e --arg requested "$requested" '.record.snapshot.screenId==$requested' "$(preset_journal_path)" >/dev/null
    local previous
    previous="$(preset_variant_json "$(panel_property screenRevision)")"
    kscreen-doctor "output.$HARDENING_SECONDARY.scale.1.25" >/dev/null
    hardening_wait_revision "$previous"
    preset_require_equal ACTIVE "$(preset_call getStatus | jq -r '.state')" 'connected scale change preserves audition'
    kscreen-doctor "output.$HARDENING_PRIMARY.disable" >/dev/null
    wait_for_screen_count 1
    wait_for_native_panel_screen "$HARDENING_BOTTOM_HOST" 0
    for ((attempt=0; attempt<100; ++attempt)); do
        state="$(preset_call getStatus | jq -r '.state')"
        [[ "$state" == IDLE || "$state" == BLOCKED ]] && break
        sleep 0.1
    done
    if [[ "$state" == BLOCKED ]]; then preset_ok recoverInterruptedPreview >/dev/null; fi
    preset_wait_idle; preset_no_orphans
    preset_require_equal 0 "$(free_host_match_count "$id" "$token")" 'hotplug temporary host removal'
    preset_require_equal "$count" "$(free_panel_count)" 'hotplug managed count'
    preset_require_equal "$requested" "$(panel_registry_value bottom screenId)" 'hotplug stable identity retained'
    kscreen-doctor "output.$HARDENING_PRIMARY.enable" >/dev/null
    wait_for_screen_count 2
    local resolved
    resolved="$(panel_call screenIndexForPanel bottom | gvariant_integer)"
    wait_for_native_panel_screen "$HARDENING_BOTTOM_HOST" "$resolved"
    preset_begin panel circular-blue-ring true >/dev/null
    preset_crash_service hardening
    preset_no_orphans
    preset_require_equal "$count" "$(free_panel_count)" 'restart managed count'
    preset_unrelated_unchanged hotplug-restart
    log_session_phase 'TASK-0044 single/multiple outputs, stable reassignment, hotplug audition rollback and service restart passed'
}

hardening_resource_group() {
    local before after
    before="$(awk '/VmRSS:/ {print $2}' "/proc/$ARCHDOCK_SESSION_ARCH_DOCK_PID/status")"
    for ((round=0; round<8; ++round)); do
        preset_begin panel circular-blue-ring true >/dev/null
        preset_ok cancel >/dev/null
        preset_no_orphans
    done
    after="$(awk '/VmRSS:/ {print $2}' "/proc/$ARCHDOCK_SESSION_ARCH_DOCK_PID/status")"
    (( after-before < 65536 )) || { printf 'Audition cycles grew resident memory by >=64 MiB.\n' >&2; return 1; }
    printf 'Eight create/cancel cycles: RSS before=%s KiB after=%s KiB; zero stale hosts.\n' "$before" "$after"
    # ADREP-TASK-004: the same private backend must return to idle after the
    # eight cycles. Read its own CPU time; do not mix in the compositor or a
    # polling UI fixture. The existing memory bound above is unchanged.
    python3 - "$ARCHDOCK_SESSION_ARCH_DOCK_PID" <<'PY'
import os, pathlib, sys, time
process = pathlib.Path('/proc') / sys.argv[1] / 'stat'
def ticks():
    fields = process.read_text().rsplit(')', 1)[1].split()
    return int(fields[11]) + int(fields[12])
time.sleep(1)
before, started = ticks(), time.monotonic()
time.sleep(5)
percent = 100 * (ticks() - before) / os.sysconf('SC_CLK_TCK') / (time.monotonic() - started)
print(f'Private backend settled idle CPU: {percent:.1f}% of one core over 5 seconds.')
assert percent < 0.3, percent
PY
    "$ARCHDOCK_HARDENING_BUILD_DIR/overlay-model-test" idleExpiryStopsWithoutLosingSourceRecovery \
        >"$ARCHDOCK_TEST_LOG_DIR/overlay-resource.log" 2>&1
    "$ARCHDOCK_HARDENING_BUILD_DIR/panel-registry-test" generatedRenderHistoryIsBoundedAndProtectsReferences \
        >"$ARCHDOCK_TEST_LOG_DIR/render-resource.log" 2>&1
    preset_unrelated_unchanged resources
}

run_wayland_hardening_matrix() {
    [[ "${ARCHDOCK_PLASMA_LIFECYCLE_SESSION:-}" == 1 && "$XDG_CURRENT_DESKTOP" == archdock-test ]] || return 2
    hardening_group_valid "$ARCHDOCK_HARDENING_MATRIX_GROUP"
    source "$ARCHDOCK_PRESET_MATRIX_SCRIPT"
    trap record_session_result EXIT
    log_session_phase "TASK-0044 $ARCHDOCK_HARDENING_MATRIX_GROUP private startup"
    start_compositor
    if [[ "${ARCHDOCK_CARD_REGRESSION_ONLY:-0}" == 1 ]]; then
        ARCHDOCK_PRESET_STAGE_PREFIX="$ARCHDOCK_HARDENING_STAGE_PREFIX" \
            "$ARCHDOCK_HARDENING_BUILD_DIR/preset-library-test" \
            "${ARCHDOCK_CARD_REGRESSION_FUNCTION:-energyFrameCardsAreDeterministic}" \
            >"$ARCHDOCK_TEST_LOG_DIR/card-regression.log" 2>&1
        log_session_phase 'TASK-0044 focused card regression passed'
        return
    fi
    start_plasmashell plasmashell.log
    PRESET_UNRELATED_ID="$(create_unrelated_panel_fixture)"
    wait_for_native_panel_screen "$PRESET_UNRELATED_ID" 0
    PRESET_UNRELATED_SNAPSHOT="$(unrelated_panel_snapshot "$PRESET_UNRELATED_ID")"
    local free_fixture
    free_fixture="$(create_unrelated_free_host_sentinel)"
    PRESET_FREE_DESKTOP="${free_fixture%%|*}"; PRESET_FREE_APPLET="${free_fixture#*|}"
    PRESET_FREE_SNAPSHOT="$(wait_for_free_host_snapshot_stable "$PRESET_FREE_DESKTOP" "$PRESET_FREE_APPLET" fixture)"
    start_arch_dock arch-dock.log
    wait_for_owned_panel bottom
    local native_screens
    native_screens="$(preset_variant_json "$(panel_call availableScreens)")"
    HARDENING_PRIMARY="$(jq -er '.[] | select(.index==0) | .id | sub("^output:"; "")' <<<"$native_screens")"
    HARDENING_SECONDARY="$(jq -er '.[] | select(.index==1) | .id | sub("^output:"; "")' <<<"$native_screens")"
    enabled_output_names | grep -Fxq "$HARDENING_PRIMARY"
    enabled_output_names | grep -Fxq "$HARDENING_SECONDARY"
    HARDENING_BOTTOM_HOST="$(owned_panel_record bottom)"; HARDENING_BOTTOM_HOST="${HARDENING_BOTTOM_HOST%%|*}"
    PRESET_BOTTOM_HOST="$HARDENING_BOTTOM_HOST"
    wait_for_native_panel_placement "$HARDENING_BOTTOM_HOST" bottom bottom center baseline
    preset_wait_idle
    ARCHDOCK_VISIBILITY_PROBE_SNAPSHOT="$XDG_RUNTIME_DIR/archdock-hardening-geometry-snapshot"
    case "$ARCHDOCK_HARDENING_MATRIX_GROUP" in
        scale*) hardening_scale_group ;;
        hotplug-recovery) hardening_hotplug_group ;;
        resources) hardening_resource_group ;;
    esac
    if rg -n 'ReferenceError:|TypeError:|Binding loop detected|Cannot assign|Unable to assign|is not a type|module .* is not installed|Error loading QML file|error when loading applet "org.archdock.dock"' "$ARCHDOCK_TEST_LOG_DIR"/*.log; then
        printf 'TASK-0044 runtime QML diagnostic in matrix.\n' >&2; return 1
    fi
    log_session_phase "TASK-0044 $ARCHDOCK_HARDENING_MATRIX_GROUP completed"
}

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
    if [[ "${1:-}" == --parser-fixture ]]; then
        hardening_group_valid scale125
        if hardening_group_valid invalid; then exit 1; fi
        printf 'Hardening group selectors passed.\n'
    elif [[ "${1:-}" == --energy-regression ]]; then
        script_root="$(cd -- "$(dirname -- "$0")" && pwd)"
        exec env ARCHDOCK_CARD_REGRESSION_ONLY=1 ARCHDOCK_HARDENING_MATRIX_GROUP=scale100 \
            bash "$script_root/run-plasma-lifecycle.sh"
    elif [[ "${1:-}" == --browser-regression ]]; then
        script_root="$(cd -- "$(dirname -- "$0")" && pwd)"
        exec env ARCHDOCK_CARD_REGRESSION_ONLY=1 ARCHDOCK_HARDENING_MATRIX_GROUP=scale100 \
            ARCHDOCK_CARD_REGRESSION_FUNCTION=browsingEveryPresetPageChangesNoPanel \
            bash "$script_root/run-plasma-lifecycle.sh"
    else
        hardening_group_valid "${1:-scale100}"
        script_root="$(cd -- "$(dirname -- "$0")" && pwd)"
        exec env ARCHDOCK_HARDENING_MATRIX_GROUP="${1:-scale100}" \
            bash "$script_root/run-plasma-lifecycle.sh"
    fi
fi
