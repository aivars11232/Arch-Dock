#!/usr/bin/env bash
# Runs only inside run-plasma-lifecycle.sh's disposable KWin/Plasma session.
set -euo pipefail

profile_call() {
    local method="$1"; shift
    preset_variant_json "$(gdbus call --session --timeout=60 --dest org.archdock.ArchDock \
        --object-path /Profiles --method "org.archdock.Profiles.$method" "$@")"
}
profile_ok() {
    local reply
    reply="$(profile_call "$@")"
    jq -e '.success == true' <<<"$reply" >/dev/null || {
        printf 'Profile action %s failed: %s\n' "$1" "$reply" >&2; return 1;
    }
    printf '%s\n' "$reply"
}
profile_data_root() { printf '%s/Arch Dock/Arch Dock\n' "$XDG_DATA_HOME"; }
profile_no_journal() { [[ ! -e "$(profile_data_root)/profile-apply-journal.json" ]]; }
profile_native_snapshot() {
    printf '%s;%s\n' "$(preset_host_snapshot "$1")" \
        "$(plasma_script "var p = panelById($1); if (!p) { print('missing'); } else { p.currentConfigGroup = ['ArchDock']; print(String(p.hiding) + '|' + String(p.readConfig('temporaryHidden', '0'))); }" | gvariant_string)"
}
profile_accel_call() {
    local method="$1"; shift
    preset_variant_json "$(gdbus call --session --timeout=30 --dest org.kde.kglobalaccel \
        --object-path /kglobalaccel --method "org.kde.KGlobalAccel.$method" "$@")"
}
profile_accel_keys() {
    profile_accel_call shortcutKeys "['org.archdock.ArchDock', '$1', '', '']"
}
profile_shortcuts_group() {
    local first second baseline_size keys reply component_path native_key native_code foreign_before
    baseline_size="$(panel_registry_value bottom iconSize)"
    first="$(profile_ok createProfile 'Shortcut first' | jq -r .profileId)"
    apply_panel_settings bottom "{'iconSize': <68>}" 'shortcut second arrangement'
    second="$(profile_ok createProfile 'Shortcut second' | jq -r .profileId)"
    log_session_phase 'S1 opt-in / native registration readback'
    profile_ok setProfileShortcut "$first" 'Ctrl+Alt+F9' >/dev/null
    jq -e '.enabled == false' <<<"$(profile_call getShortcutStatus)" >/dev/null
    profile_ok setShortcutsEnabled true >/dev/null
    keys="$(profile_accel_keys "$first")"
    jq -e 'flatten | length > 0' <<<"$keys" >/dev/null
    jq -e --arg id "$first" '.bindings[] | select(.profileId == $id) | .registeredKeys == ["Ctrl+Alt+F9"]' \
        <<<"$(profile_call getShortcutStatus)" >/dev/null

    log_session_phase 'S2 local and existing KDE conflict / no key stealing'
    reply="$(profile_call setProfileShortcut "$second" 'Ctrl+Alt+F9')"
    jq -e '.success == false and .errorCode == "profile-shortcut-conflict" and (.conflicts | length > 0)' <<<"$reply" >/dev/null
    preset_require_equal "$keys" "$(profile_accel_keys "$first")" 'local conflict preserves first native action'
    # Discover an occupied standard key in this actual private desktop; compare
    # its native owners before and after the rejected request.
    native_key=''
    for pair in 'Meta+D|268435524' 'Meta+E|268435525' 'Alt+F2|150994993'; do
        native_code="${pair#*|}"
        if [[ "$(profile_accel_call globalShortcutAvailable "([$native_code],)" 'org.archdock.ArchDock')" == false ]]; then
            native_key="${pair%%|*}"; break
        fi
    done
    [[ -n "$native_key" ]] || { printf 'Private KDE session supplied no occupied standard shortcut.\n' >&2; return 1; }
    foreign_before="$(profile_accel_call globalShortcutsByKey "([$native_code],)" '(0,)')"
    reply="$(profile_call setProfileShortcut "$second" "$native_key")"
    jq -e '.success == false and .errorCode == "profile-shortcut-conflict" and (.conflicts | length > 0)' <<<"$reply" >/dev/null
    preset_require_equal "$foreign_before" "$(profile_accel_call globalShortcutsByKey "([$native_code],)" '(0,)')" 'existing KDE key owner preserved'
    preset_require_equal "$keys" "$(profile_accel_keys "$first")" 'external conflict preserves first native action'

    log_session_phase 'S3 stable renamed ID / real native activation / full transaction'
    profile_ok renameProfile "$first" 1 'Renamed shortcut first' >/dev/null
    reply="$(gdbus call --session --dest org.kde.kglobalaccel --object-path /kglobalaccel \
        --method org.kde.KGlobalAccel.getComponent 'org.archdock.ArchDock')"
    component_path="$(preset_variant_json "${reply/objectpath /}" | jq -r .)"
    gdbus call --session --dest org.kde.kglobalaccel --object-path "$component_path" \
        --method org.kde.kglobalaccel.Component.invokeShortcut "$first" >/dev/null
    local deadline=$((SECONDS + 20))
    until jq -e --arg id "$first" '.lastActivation.success == true and .lastActivation.profileId == $id' \
        <<<"$(profile_call getShortcutStatus)" >/dev/null; do
        (( SECONDS < deadline )) || { printf 'Native profile shortcut did not finish its transaction.\n' >&2; return 1; }
        sleep 0.1
    done
    wait_for_panel_registry_value bottom iconSize "$baseline_size"
    jq -e --arg id "$first" '.state == "APPLIED" and .profileId == $id' <<<"$(profile_call getStatus)" >/dev/null
    profile_no_journal
    preset_unrelated_unchanged S3

    log_session_phase 'S4 invalid target / disabled feature / deleted profile'
    local names
    names="$(gdbus call --session --dest org.kde.kglobalaccel --object-path "$component_path" \
        --method org.kde.kglobalaccel.Component.shortcutNames)"
    jq -e '.success == false' <<<"$(profile_call setProfileShortcut 'profile-missing' 'Ctrl+Alt+F10')" >/dev/null
    preset_require_equal "$names" "$(gdbus call --session --dest org.kde.kglobalaccel --object-path "$component_path" \
        --method org.kde.kglobalaccel.Component.shortcutNames)" 'invalid profile has no native action'
    profile_ok setShortcutsEnabled false >/dev/null
    jq -e 'flatten | length == 0' <<<"$(profile_accel_keys "$first")" >/dev/null
    profile_ok setShortcutsEnabled true >/dev/null
    preset_require_equal "$keys" "$(profile_accel_keys "$first")" 'explicit re-enable restores saved registration'
    profile_ok deleteProfile "$first" 2 >/dev/null
    jq -e 'flatten | length == 0' <<<"$(profile_accel_keys "$first")" >/dev/null
    jq -e '.bindings | length == 0' <<<"$(profile_call getShortcutStatus)" >/dev/null
    profile_ok deleteProfile "$second" 1 >/dev/null
    profile_ok setShortcutsEnabled false >/dev/null
    preset_unrelated_unchanged S4
    panel_call showSettings >/dev/null
}

profile_apply_group() {
    local before baseline imported profile_path native_id free_id host free_desktop free_applet reply
    before="$(panel_registry_json)"
    baseline="$(profile_ok createProfile 'Private baseline' | jq -r .profileId)"
    profile_ok exportProfile "$baseline" 1 "file://$XDG_DATA_HOME/baseline.json" >/dev/null
    python - "$XDG_DATA_HOME/baseline.json" "$XDG_DATA_HOME/import.json" <<'PY'
import copy, json, sys
with open(sys.argv[1]) as f: profile = json.load(f)
native = next(p for p in profile['panels'] if p['id'] == 'bottom')
free = copy.deepcopy(native)
free.update(id='free-transfer', name='Profile free', hostKind='free-desktop', edge='free',
            type='empty', visible=True, x=350, y=230, layout='ring')
native.update(name='Profile native', width=660, visible=True, visibilityMode='auto-hide')
profile.update(name='Private imported arrangement', panels=[native, free])
with open(sys.argv[2], 'w') as f: json.dump(profile, f)
PY
    imported="$(profile_ok importProfile "file://$XDG_DATA_HOME/import.json" | jq -r .profileId)"
    preset_require_equal "$before" "$(panel_registry_json)" 'profile capture/import are data only'
    profile_no_journal
    profile_path="$(profile_data_root)/profiles/$imported.json"
    log_session_phase 'P1 imported native/free full-set apply'
    profile_ok applyProfile "$imported" 1 >/dev/null
    preset_require_equal "$(jq -c '[.panels[].id] | sort' "$profile_path")" \
        "$(panel_registry_json | jq -c '[.[].id] | sort')" 'declared managed panel set'
    native_id="$(jq -r '.panels[] | select(.hostKind == "native-edge") | .id' "$profile_path")"
    free_id="$(jq -r '.panels[] | select(.hostKind == "free-desktop") | .id' "$profile_path")"
    host="$(panel_registry_value "$native_id" nativePanelId)"
    wait_for_native_panel_placement "$host" "$native_id" bottom center profile
    [[ "$(profile_native_snapshot "$host")" == *';autohide|0' ]]
    preset_require_equal 1 "$(free_host_match_count "$free_id" "$(panel_registry_value "$free_id" freeOwnershipToken)")" 'one owned profile free host'
    preset_require_equal 660 "$(panel_registry_value "$native_id" width)" 'declared native width'
    free_desktop="$(panel_registry_value "$free_id" freeDesktopContainmentId)"
    free_applet="$(panel_registry_value "$free_id" freeDockAppletId)"
    wait_for_free_host_snapshot_stable "$free_desktop" "$free_applet" profile >/dev/null
    preset_unrelated_unchanged P1
    profile_no_journal

    local failed settings_file settings_dir native_before free_before backup journal
    failed="$(profile_ok createProfile 'Rollback fixture' | jq -r .profileId)"
    python - "$(profile_data_root)/profiles/$failed.json" <<'PY'
import json, sys
with open(sys.argv[1]) as f: profile = json.load(f)
for p in profile['panels']:
    if p['hostKind'] == 'native-edge': p.update(width=620, visibilityMode='always')
    else: p['x'] = 410
with open(sys.argv[1], 'w') as f: json.dump(profile, f)
PY
    before="$(panel_registry_json)"
    native_before="$(profile_native_snapshot "$host")"
    free_before="$(free_host_snapshot "$free_desktop" "$free_applet")"
    settings_file="$(arch_dock_settings_file)"; settings_dir="$(dirname -- "$settings_file")"
    ARCHDOCK_SETTINGS_FIXTURE_FILE="$settings_file"
    ARCHDOCK_SETTINGS_FIXTURE_DIRECTORY="$settings_dir"
    ARCHDOCK_SETTINGS_FIXTURE_FILE_MODE="$(stat -c '%a' "$settings_file")"
    ARCHDOCK_SETTINGS_FIXTURE_DIRECTORY_MODE="$(stat -c '%a' "$settings_dir")"
    chmod 400 "$settings_file"; chmod 500 "$settings_dir"
    log_session_phase 'P2 full-set write refusal / verified host rollback'
    reply="$(profile_call applyProfile "$failed" 1)"
    restore_settings_fixture_permissions
    jq -e '.success == false and .errorCode == "profile-registry-unwritable" and .rollbackStatus == "complete"' <<<"$reply" >/dev/null || {
        printf 'Expected complete profile rollback: %s\n' "$reply" >&2; return 1;
    }
    preset_require_equal "$before" "$(panel_registry_json)" 'failed apply registry preserved'
    preset_require_equal "$native_before" "$(profile_native_snapshot "$host")" 'failed apply native host restored'
    preset_require_equal "$free_before" "$(free_host_snapshot "$free_desktop" "$free_applet")" 'failed apply free host restored'
    preset_unrelated_unchanged P2
    profile_no_journal

    log_session_phase 'P3 interrupted apply retains record / explicit recovery'
    backup="$(profile_data_root)/profile-apply-journal.json.backup.json"
    journal="$(profile_data_root)/profile-apply-journal.json"
    python - "$backup" "$journal" <<'PY'
import json, os, sys
with open(sys.argv[1]) as f: record = json.load(f)
record['state'] = 'APPLYING'
for snapshot in record['backup']: snapshot['touched'] = True
with open(sys.argv[2], 'w') as f: json.dump(record, f)
os.chmod(sys.argv[2], 0o600)
PY
    local token
    token="$(panel_registry_value "$native_id" nativeOwnershipToken)"
    preset_require_equal 1 "$(plasma_script "var p = panelById($host); if (!p) { print(0); } else { p.currentConfigGroup = ['ArchDock']; if (String(p.readConfig('panelId', '')) !== '$native_id' || String(p.readConfig('ownerToken', '')) !== '$token') { print(0); } else { p.height = 94; print(p.height === 94 ? 1 : 0); } }" | gvariant_string)" 'interrupted owned host mutation'
    kill -KILL "$ARCHDOCK_SESSION_ARCH_DOCK_PID"
    wait "$ARCHDOCK_SESSION_ARCH_DOCK_PID" 2>/dev/null || true
    ARCHDOCK_SESSION_ARCH_DOCK_PID=''
    unload_kwin_script org.archdock.windowwatcher.runtime
    start_arch_dock arch-dock-profile-recovery.log
    reply="$(profile_call getStatus)"
    jq -e '.state == "BLOCKED" and .recoveryRequired == true' <<<"$reply" >/dev/null
    preset_require_equal "$before" "$(panel_registry_json)" 'restart does not apply a profile'
    [[ "$native_before" != "$(profile_native_snapshot "$host")" ]]
    profile_ok recoverInterruptedApply >/dev/null
    preset_require_equal "$native_before" "$(profile_native_snapshot "$host")" 'explicit recovery restores native host'
    preset_require_equal "$free_before" "$(free_host_snapshot "$free_desktop" "$free_applet")" 'explicit recovery restores free host'
    profile_no_journal
    preset_unrelated_unchanged P3
    profile_ok deleteProfile "$failed" 1 >/dev/null
    profile_ok deleteProfile "$baseline" 1 >/dev/null
    panel_call showSettings >/dev/null
}

run_profile_matrix() {
    [[ "${ARCHDOCK_PLASMA_LIFECYCLE_SESSION:-}" == 1 && "$XDG_CURRENT_DESKTOP" == archdock-test ]] || return 2
    source "$ARCHDOCK_PRESET_MATRIX_SCRIPT"
    trap record_session_result EXIT
    log_session_phase "TASK-0042 ${ARCHDOCK_PROFILE_MATRIX_GROUP} private startup"
    start_compositor
    start_plasmashell plasmashell.log
    PRESET_UNRELATED_ID="$(create_unrelated_panel_fixture)"
    wait_for_native_panel_screen "$PRESET_UNRELATED_ID" 0
    PRESET_UNRELATED_SNAPSHOT="$(unrelated_panel_snapshot "$PRESET_UNRELATED_ID")"
    local fixture
    fixture="$(create_unrelated_free_host_sentinel)"
    PRESET_FREE_DESKTOP="${fixture%%|*}"; PRESET_FREE_APPLET="${fixture#*|}"
    PRESET_FREE_SNAPSHOT="$(wait_for_free_host_snapshot_stable "$PRESET_FREE_DESKTOP" "$PRESET_FREE_APPLET" fixture)"
    start_arch_dock arch-dock.log
    wait_for_owned_panel bottom
    local host
    host="$(owned_panel_record bottom)"
    host="${host%%|*}"
    wait_for_panel_registry_value bottom nativePanelId "$host"
    wait_for_native_panel_placement "$host" bottom bottom center baseline
    case "$ARCHDOCK_PROFILE_MATRIX_GROUP" in
        apply) profile_apply_group ;;
        shortcuts) profile_shortcuts_group ;;
        *) return 2 ;;
    esac
    if rg -n 'ReferenceError:.*profileManager|ProfilePage is not a type|module "ArchDock.Rendering" is not installed|error when loading applet "org.archdock.dock"' "$ARCHDOCK_TEST_LOG_DIR"/*.log; then
        printf 'Profile runtime resources failed to load.\n' >&2; return 1
    fi
    log_session_phase "TASK-0042 ${ARCHDOCK_PROFILE_MATRIX_GROUP} completed"
}

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
    script_root="$(cd -- "$(dirname -- "$0")" && pwd)"
    exec env ARCHDOCK_PROFILE_MATRIX_GROUP="${1:-apply}" bash "$script_root/run-plasma-lifecycle.sh"
fi
