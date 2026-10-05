#!/usr/bin/env bash
# Sourced by the existing disposable Plasma lifecycle harness. Each group owns
# one private session; this file never connects to the personal session.
set -euo pipefail

preset_variant_json() {
    # The reply travels on its own descriptor. A draft reply carries every
    # theme candidate and can exceed what the kernel allows one argument.
    python - 3<<<"$1" <<'PY'
import ast, json, os, re, sys
text = os.fdopen(3).read()
pattern = re.compile(r'''\s*(@[a-zA-Z0-9{}()]+|'(?:\\.|[^'\\])*'|"(?:\\.|[^"\\])*"|[-+]?(?:0x[0-9a-fA-F]+|[0-9]+(?:\.[0-9]*)?(?:[eE][-+]?[0-9]+)?)|[a-zA-Z][a-zA-Z0-9]*|[<>()\[\]{},:])''')
tokens = []
position = 0
while position < len(text):
    match = pattern.match(text, position)
    if not match:
        if text[position:].strip(): raise ValueError('invalid GVariant at ' + str(position))
        break
    tokens.append(match[1]); position = match.end()
index = 0
def take(expected=None):
    global index
    value = tokens[index]; index += 1
    if expected is not None and value != expected: raise ValueError((expected, value))
    return value
def parse():
    token = take()
    if token.startswith('@') or token in ('uint64', 'uint32', 'uint16', 'int64', 'int32', 'int16', 'byte', 'double', 'handle'):
        return parse()
    if token == '<':
        value = parse(); take('>'); return value
    if token == '{':
        result = {}
        while tokens[index] != '}':
            key = parse(); take(':'); result[key] = parse()
            if tokens[index] == ',': take(',')
            elif tokens[index] != '}': raise ValueError('missing dictionary separator')
        take('}'); return result
    if token in ('[', '('):
        end = ']' if token == '[' else ')'; result = []
        while tokens[index] != end:
            result.append(parse())
            if tokens[index] == ',': take(',')
            elif tokens[index] != end: raise ValueError('missing sequence separator')
        take(end); return result
    if token in ('true', 'false'): return token == 'true'
    if token.startswith(("'", '"')): return ast.literal_eval(token)
    return float(token) if any(c in token for c in '.eE') else int(token, 0) if token.startswith('0x') else int(token)
value = parse()
if index != len(tokens) or not isinstance(value, list) or len(value) != 1:
    raise ValueError('expected one D-Bus result')
print(json.dumps(value[0], sort_keys=True, separators=(',', ':')))
PY
}

preset_call() {
    local method="$1"; shift
    local reply
    reply="$(gdbus call --session --timeout=30 --dest org.archdock.ArchDock \
        --object-path /PresetAudition --method "org.archdock.PresetAudition.$method" "$@")"
    preset_variant_json "$reply"
}
preset_ok() {
    local reply
    reply="$(preset_call "$@")"
    jq -e '.success == true' <<<"$reply" >/dev/null || {
        printf 'Preset action %s failed: %s\n' "$1" "$(jq -c '{state,errorCode,panelId,temporary,committed}' <<<"$reply")" >&2
        if [[ -f "$(preset_journal_path)" ]] && [[ "$(jq -r '.record.hostKind' "$(preset_journal_path)")" == free-desktop ]]; then
            local pending_id pending_token
            pending_id="$(jq -r '.record.panelId' "$(preset_journal_path)")"
            pending_token="$(jq -r '.record.previewToken' "$(preset_journal_path)")"
            printf 'Remaining preview-owned free hosts after refusal: %s\n' \
                "$(free_host_match_count "$pending_id" "$pending_token")" >&2
        fi
        return 1;
    }
    printf '%s\n' "$reply"
}
preset_begin() {
    preset_ok beginPreview "{'kind': <'$1'>, 'presetId': <'$2'>, 'panelId': <'bottom'>, 'newPanel': <$3>, 'useRecommendedIcons': <false>}"
}
preset_renderer() { preset_variant_json "$(panel_call panelRendererConfiguration "$1")"; }
preset_require_equal() {
    [[ "$1" == "$2" ]] || { printf 'Preset matrix mismatch: %s\nexpected=%s\nactual=%s\n' "$3" "$1" "$2" >&2; return 1; }
}
preset_journal_path() {
    printf '%s/Arch Dock/Arch Dock/preset-preview-journal.json\n' "$XDG_DATA_HOME"
}
preset_wait_idle() {
    local attempt reply
    for ((attempt=0; attempt<100; ++attempt)); do
        reply="$(preset_call getStatus)"
        [[ "$(jq -r '.state' <<<"$reply")" == IDLE ]] && return
        sleep 0.1
    done
    printf 'Preview did not recover to IDLE: %s\n' "$reply" >&2; return 1
}
preset_no_orphans() {
    [[ ! -e "$(preset_journal_path)" ]] || { printf 'Preview journal remains.\n' >&2; return 1; }
    local count
    count="$(plasma_script "var count = 0; var all = panels().concat(desktops()); for (var i = 0; i < all.length; ++i) { var c = all[i]; c.currentConfigGroup = ['ArchDock']; if (String(c.readConfig('ownerToken', '')).indexOf('archdock-preview-') === 0) ++count; var ws = c.widgets(); for (var j = 0; j < ws.length; ++j) { var w = c.widgetById(ws[j].id); w.currentConfigGroup = ['General']; if (String(w.readConfig('ownerToken', '')).indexOf('archdock-preview-') === 0) ++count; } } print(count);" | gvariant_string)"
    preset_require_equal 0 "$count" 'preview ownership tokens after cleanup'
    panel_registry_json | jq -e 'all(.[]; (.id | test("^(free|panel)-x[0-9a-f]{8}$") | not) or (.freeOwnershipToken // .nativeOwnershipToken | startswith("archdock-managed-")))' >/dev/null
}
preset_unrelated_unchanged() {
    require_unrelated_panel_unchanged "$PRESET_UNRELATED_ID" "$PRESET_UNRELATED_SNAPSHOT" "$1"
    preset_require_equal "$PRESET_FREE_SNAPSHOT" \
        "$(free_host_snapshot "$PRESET_FREE_DESKTOP" "$PRESET_FREE_APPLET")" "unrelated free applet: $1"
}
preset_host_snapshot() {
    printf '%s;%s\n' "$(native_panel_placement_snapshot "$1")" "$(native_panel_geometry_snapshot "$1")"
}
preset_crash_service() {
    kill -KILL "$ARCHDOCK_SESSION_ARCH_DOCK_PID"
    wait "$ARCHDOCK_SESSION_ARCH_DOCK_PID" 2>/dev/null || true
    ARCHDOCK_SESSION_ARCH_DOCK_PID=''
    unload_kwin_script org.archdock.windowwatcher.runtime
    start_arch_dock "arch-dock-recovery-$1.log"
    preset_wait_idle
}

preset_existing_group() {
    local registry renderer host reply revision saved_id builtin_hash
    registry="$(panel_registry_json)"; renderer="$(preset_renderer bottom)"
    host="$(preset_host_snapshot "$PRESET_BOTTOM_HOST")"
    log_session_phase 'S1 existing owned panel Preview / Cancel'
    reply="$(preset_begin panel obsidian-glass-dock false)"
    jq -e '.state == "ACTIVE" and .temporary == false' <<<"$reply" >/dev/null
    preset_require_equal obsidian-glass "$(preset_renderer bottom | jq -r '.completeThemeId')" 'live preview theme'
    preset_require_equal "$registry" "$(panel_registry_json)" 'S1 no registry write'
    preset_ok cancel >/dev/null
    preset_require_equal "$renderer" "$(preset_renderer bottom)" 'S1 renderer restored'
    preset_require_equal "$host" "$(preset_host_snapshot "$PRESET_BOTTOM_HOST")" 'S1 host restored'
    preset_unrelated_unchanged S1

    log_session_phase 'S2 streamed placement / Revert'
    builtin_hash="$(sha256sum "$ARCHDOCK_PRESET_BUILTIN_ROOT/panels/obsidian-glass-dock.json")"
    preset_begin panel obsidian-glass-dock false >/dev/null
    preset_ok updateDraft "{'width': <640>}" >/dev/null
    [[ "$(preset_host_snapshot "$PRESET_BOTTOM_HOST")" != "$host" ]]
    preset_require_equal "$registry" "$(panel_registry_json)" 'S2 no registry write'
    reply="$(preset_ok saveAsCustomPreset 'Private panel snapshot')"
    saved_id="$(jq -r '.presetId' <<<"$reply")"
    [[ "$saved_id" == user-* ]]
    preset_ok revert >/dev/null
    preset_require_equal "$host" "$(preset_host_snapshot "$PRESET_BOTTOM_HOST")" 'S2 exact host rollback'
    preset_require_equal "$renderer" "$(preset_renderer bottom)" 'S2 renderer rollback'
    preset_unrelated_unchanged S2
    log_session_phase 'S10 reusable custom Panel Preset / immutable built-in'
    preset_begin panel "$saved_id" false >/dev/null
    preset_require_equal 640 "$(preset_renderer bottom | jq -r '.width')" 'S10 panel snapshot restored'
    preset_ok cancel >/dev/null
    preset_require_equal "$registry" "$(panel_registry_json)" 'S10 saved copy never activates itself'
    preset_require_equal "$builtin_hash" "$(sha256sum "$ARCHDOCK_PRESET_BUILTIN_ROOT/panels/obsidian-glass-dock.json")" 'S10 built-in bytes unchanged'

    log_session_phase 'S3 Apply exactly one revision'
    revision="$(panel_registry_value bottom settingsRevision)"
    preset_begin panel obsidian-glass-dock false >/dev/null
    preset_ok applyAsActive >/dev/null
    preset_require_equal "$((revision+1))" "$(panel_registry_value bottom settingsRevision)" 'S3 revision'
    preset_require_equal obsidian-glass-dock "$(panel_registry_json | jq -r '.[] | select(.id=="bottom") | .presetOrigin.panelPresetId')" 'S3 lineage'
    preset_require_equal IDLE "$(preset_call getStatus | jq -r '.state')" 'S3 idle'
    preset_unrelated_unchanged S3
    preset_no_orphans
}

preset_temporary_group() {
    local registry count reply id token record defaults theme_values theme_variant
    registry="$(panel_registry_json)"; count="$(panel_count)"
    defaults="$(preset_call defaultSelection)"
    log_session_phase 'S4 free-only incompatible host Preview / Cancel'
    reply="$(preset_begin panel circular-blue-ring false)"; id="$(jq -r '.panelId' <<<"$reply")"
    jq -e '.state=="ACTIVE" and .temporary==true' <<<"$reply" >/dev/null
    record="$(jq -c '.record' "$(preset_journal_path)")"; token="$(jq -r '.previewToken' <<<"$record")"
    preset_require_equal 1 "$(free_host_match_count "$id" "$token")" 'S4 one temporary free host'
    log_session_phase 'S4 theme customization targets the temporary free draft'
    jq -e '.editorProjection.themeCandidates["arc-platform-orange"].success==true' <<<"$reply" >/dev/null
    theme_values="$(jq -c '.editorProjection.themeCandidates["arc-platform-orange"].values' <<<"$reply")"
    jq -e 'has("completeThemeId") and (has("opacity")|not) and all(.[]; type=="string" or type=="number" or type=="boolean")' <<<"$theme_values" >/dev/null
    theme_variant="$(jq -r 'to_entries|map((.key|tojson)+": <"+(.value|tojson)+">")|"{"+join(", ")+"}"' <<<"$theme_values")"
    preset_ok updateDraft "$theme_variant" >/dev/null
    preset_require_equal arc-platform-orange "$(preset_renderer "$id" | jq -r '.completeThemeId')" 'S4 live temporary theme changed'
    preset_require_equal "$registry" "$(panel_registry_json)" 'S4 no registry write'
    preset_ok cancel >/dev/null
    preset_require_equal 0 "$(free_host_match_count "$id" "$token")" 'S4 removed host'
    preset_require_equal "$registry" "$(panel_registry_json)" 'S4 registry restored'
    preset_require_equal "$defaults" "$(preset_call defaultSelection)" 'S4 defaults unchanged'
    preset_no_orphans; preset_unrelated_unchanged S4

    log_session_phase 'S5 free preview converted to exactly one managed panel'
    reply="$(preset_begin panel circular-blue-ring false)"; id="$(jq -r '.panelId' <<<"$reply")"
    record="$(jq -c '.record' "$(preset_journal_path)")"; token="$(jq -r '.previewToken' <<<"$record")"
    preset_ok applyAsActive >/dev/null
    preset_require_equal "$(($(jq 'length' <<<"$registry")+1))" "$(panel_registry_json | jq 'length')" 'S5 one new record'
    preset_require_equal 0 "$(free_host_match_count "$id" "$token")" 'S5 old token gone'
    token="$(panel_registry_value "$id" freeOwnershipToken)"
    [[ "$token" == archdock-managed-* ]]
    preset_require_equal 1 "$(free_host_match_count "$id" "$token")" 'S5 one converted host'
    preset_require_equal 1 "$(panel_registry_value "$id" settingsRevision)" 'S5 initial revision'
    preset_unrelated_unchanged S5; preset_no_orphans

    log_session_phase 'S6 new native preview / Cancel'
    registry="$(panel_registry_json)"
    reply="$(preset_begin panel obsidian-glass-dock true)"
    jq -e '.temporary==true and (.panelId|startswith("panel-x"))' <<<"$reply" >/dev/null
    preset_require_equal "$((count+1))" "$(panel_count)" 'S6 one temporary native host'
    preset_ok cancel >/dev/null
    preset_require_equal "$count" "$(panel_count)" 'S6 native host removed'
    preset_require_equal "$registry" "$(panel_registry_json)" 'S6 registry unchanged'
    preset_no_orphans; preset_unrelated_unchanged S6
}

preset_icons_group() {
    local registry renderer host before after saved_id package_digest
    registry="$(panel_registry_json)"; renderer="$(preset_renderer bottom)"
    host="$(preset_host_snapshot "$PRESET_BOTTOM_HOST")"
    log_session_phase 'S7 icon-only Preview / Cancel / Apply and reusable custom copy'
    preset_begin icon glass-tile false >/dev/null
    before="$(jq -cS 'del(.iconStyle,.iconThemeId,.themeId,.iconStyleDefinition,.iconGlobalDefaults,.iconAnimation,.animationTrigger,.animationSpeed,.animationIntensity,.magnificationRadius,.magnificationFalloff,.presetOrigin,.settingsRevision)' <<<"$renderer")"
    after="$(preset_renderer bottom | jq -cS 'del(.iconStyle,.iconThemeId,.themeId,.iconStyleDefinition,.iconGlobalDefaults,.iconAnimation,.animationTrigger,.animationSpeed,.animationIntensity,.magnificationRadius,.magnificationFalloff,.presetOrigin,.settingsRevision)')"
    preset_require_equal "$before" "$after" 'S7 preview preserves all panel values'
    preset_require_equal "$host" "$(preset_host_snapshot "$PRESET_BOTTOM_HOST")" 'S7 host unchanged'
    preset_require_equal "$registry" "$(panel_registry_json)" 'S7 preview no registry write'
    preset_ok cancel >/dev/null
    preset_require_equal "$renderer" "$(preset_renderer bottom)" 'S7 Cancel exact renderer'
    preset_begin icon glass-tile false >/dev/null
    package_digest="$(sha256sum "$ARCHDOCK_PRESET_BUILTIN_ROOT/icons/glass-tile.json")"
    preset_ok updateDraft "{'animationSpeed': <1.4>}" >/dev/null
    saved_id="$(preset_ok saveAsCustomPreset 'Matrix custom icons' | jq -r '.presetId')"
    [[ "$saved_id" == user-* ]]
    preset_require_equal "$package_digest" "$(sha256sum "$ARCHDOCK_PRESET_BUILTIN_ROOT/icons/glass-tile.json")" 'S7 immutable built-in bytes'
    preset_require_equal "$registry" "$(panel_registry_json)" 'S7 custom save does not Apply'
    preset_ok cancel >/dev/null
    preset_begin icon "$saved_id" false >/dev/null
    preset_require_equal 1.4 "$(preset_renderer bottom | jq -r '.animationSpeed')" 'S7 saved snapshot reusable'
    preset_ok restoreBuiltInDefaults >/dev/null
    preset_require_equal glass-tile "$(preset_call getStatus | jq -r '.presetId')" 'S7 Restore built-in lineage'
    preset_ok applyAsActive >/dev/null
    before="$(jq -cS 'map(del(.iconStyle,.iconThemeId,.themeId,.iconGlobalDefaults,.iconAnimation,.animationTrigger,.animationSpeed,.animationIntensity,.magnificationRadius,.magnificationFalloff,.presetOrigin,.settingsRevision))' <<<"$registry")"
    after="$(panel_registry_json | jq -cS 'map(del(.iconStyle,.iconThemeId,.themeId,.iconGlobalDefaults,.iconAnimation,.animationTrigger,.animationSpeed,.animationIntensity,.magnificationRadius,.magnificationFalloff,.presetOrigin,.settingsRevision))')"
    preset_require_equal "$before" "$after" 'S7 Apply preserves all panel values'
    preset_require_equal "$(($(jq -r '.[] | select(.id=="bottom") | .settingsRevision' <<<"$registry")+1))" \
        "$(panel_registry_value bottom settingsRevision)" 'S7 one icon Apply revision'
    panel_registry_json | jq -e 'first(.[] | select(.id=="bottom")) | .iconThemeId==.iconStyle and .presetOrigin.iconPresetId=="glass-tile"' >/dev/null
    preset_require_equal "$host" "$(preset_host_snapshot "$PRESET_BOTTOM_HOST")" 'S7 Apply host unchanged'
    preset_unrelated_unchanged S7; preset_no_orphans
}

preset_recovery_group() {
    local registry host reply record id token managed journal desktop applet state
    registry="$(panel_registry_json)"; host="$(preset_host_snapshot "$PRESET_BOTTOM_HOST")"
    log_session_phase 'S8 service crash during temporary free preview'
    reply="$(preset_begin panel circular-blue-ring false)"; id="$(jq -r '.panelId' <<<"$reply")"
    token="$(jq -r '.record.previewToken' "$(preset_journal_path)")"
    preset_crash_service free
    preset_require_equal 0 "$(free_host_match_count "$id" "$token")" 'S8 crashed free host removed'
    preset_require_equal "$registry" "$(panel_registry_json)" 'S8 free crash no commit'
    preset_no_orphans; preset_unrelated_unchanged S8-free

    log_session_phase 'S8 service crash during existing placement preview'
    preset_begin panel obsidian-glass-dock false >/dev/null
    preset_ok updateDraft "{'width': <640>}" >/dev/null
    preset_crash_service placement
    preset_require_equal "$host" "$(preset_host_snapshot "$PRESET_BOTTOM_HOST")" 'S8 placement restored'
    preset_require_equal "$registry" "$(panel_registry_json)" 'S8 placement no commit'
    preset_no_orphans; preset_unrelated_unchanged S8-placement

    log_session_phase 'S8 conversion-before-adoption crash window'
    preset_begin panel circular-blue-ring false >/dev/null
    journal="$(preset_journal_path)"; record="$(jq -c '.record' "$journal")"
    id="$(jq -r '.panelId' <<<"$record")"; token="$(jq -r '.previewToken' <<<"$record")"
    desktop="$(jq -r '.containmentId' <<<"$record")"; applet="$(jq -r '.appletId' <<<"$record")"
    managed="archdock-managed-$(cat /proc/sys/kernel/random/uuid)"
    # This fixture reproduces the durable CONVERTING record and native token
    # write, then kills the service before registry adoption.
    python - "$journal" "$managed" <<'PY'
import json, os, sys
path, token = sys.argv[1:]
with open(path) as f: data = json.load(f)
data['record']['phase'] = 'CONVERTING'; data['record']['managedToken'] = token
with open(path + '.fixture', 'w') as f: json.dump(data, f)
os.chmod(path + '.fixture', 0o600); os.replace(path + '.fixture', path)
PY
    reply="$(plasma_script "var d = desktopById($desktop); var w = d ? d.widgetById($applet) : null; if (!w || w.type !== 'org.archdock.dock') { print(0); } else { w.currentConfigGroup = ['General']; if (String(w.readConfig('panelId', '')) !== '$id' || String(w.readConfig('ownerToken', '')) !== '$token') { print(0); } else { w.writeConfig('ownerToken', '$managed'); w.reloadConfig(); print(String(w.readConfig('ownerToken', '')) === '$managed' ? 1 : 0); } }" | gvariant_string)"
    preset_require_equal 1 "$reply" 'S8 fixture verified conversion'
    preset_crash_service conversion
    preset_require_equal 0 "$(free_host_match_count "$id" "$managed")" 'S8 converted unadopted host removed'
    preset_require_equal "$registry" "$(panel_registry_json)" 'S8 conversion no silent adoption'
    preset_no_orphans; preset_unrelated_unchanged S8-conversion

    log_session_phase 'S9 PlasmaShell interruption during temporary preview'
    preset_begin panel circular-blue-ring false >/dev/null
    restart_plasmashell
    wait_for_native_panel_screen "$PRESET_UNRELATED_ID" 0
    for ((attempt=0; attempt<100; ++attempt)); do
        state="$(preset_call getStatus | jq -r '.state')"
        [[ "$state" == IDLE || "$state" == BLOCKED ]] && break
        sleep 0.1
    done
    if [[ "$state" == BLOCKED ]]; then
        [[ -f "$(preset_journal_path)" ]]
        preset_ok recoverInterruptedPreview >/dev/null
    fi
    preset_wait_idle
    preset_require_equal "$registry" "$(panel_registry_json)" 'S9 no silent commit'
    preset_no_orphans; preset_unrelated_unchanged S9
}

preset_defaults_group() {
    local registry id reply desktop applet geometry renderer
    registry="$(panel_registry_json)"
    log_session_phase 'S11 defaults affect only new instances'
    preset_ok setAsDefault panel obsidian-glass-dock false >/dev/null
    preset_ok setAsDefault icon glass-tile false >/dev/null
    preset_require_equal "$registry" "$(panel_registry_json)" 'S11 selecting defaults preserves existing records'
    id="$(panel_call createNativePanel top launcher | gvariant_string)"
    [[ -n "$id" ]]
    preset_require_equal "$registry" "$(panel_registry_json | jq -c --arg id "$id" '[.[] | select(.id!=$id)]')" 'S11 creation preserves all earlier records'
    reply="$(panel_registry_record_snapshot "$id")"
    jq -e '.presetOrigin.panelPresetId=="obsidian-glass-dock" and .presetOrigin.iconPresetId=="glass-tile"' <<<"$reply" >/dev/null
    registry="$(panel_registry_json)"
    reply="$(panel_call createFreePanel)"; reply="$(preset_variant_json "$reply")"
    jq -e '.success==true and .ownershipVerified==true' <<<"$reply" >/dev/null
    id="$(jq -r '.panelId' <<<"$reply")"
    preset_require_equal "$registry" "$(panel_registry_json | jq -c --arg id "$id" '[.[] | select(.id!=$id)]')" 'S11 free creation preserves all earlier records'
    panel_registry_record_snapshot "$id" | jq -e '.presetOrigin.panelPresetId=="obsidian-glass-dock" and .presetOrigin.iconPresetId=="glass-tile"' >/dev/null
    desktop="$(panel_registry_value "$id" freeDesktopContainmentId)"
    applet="$(panel_registry_value "$id" freeDockAppletId)"
    wait_for_free_host_snapshot_stable "$desktop" "$applet" default >/dev/null
    registry="$(panel_registry_json)"; renderer="$(preset_renderer "$id")"
    geometry="$(preset_free_geometry "$desktop" "$applet")"
    log_session_phase 'S12 existing free layout / inactive edit refusal / exact Cancel'
    preset_ok beginPreview "{'kind': <'panel'>, 'presetId': <'obsidian-glass-dock'>, 'panelId': <'$id'>, 'newPanel': <false>, 'useRecommendedIcons': <false>}" >/dev/null
    reply="$(preset_call updateDraft "{'layoutRadius': <181>}")"
    jq -e '.success==false and .errorCode=="unavailable-panel-field" and .state=="ACTIVE"' <<<"$reply" >/dev/null
    preset_ok cancel >/dev/null
    preset_ok beginPreview "{'kind': <'panel'>, 'presetId': <'circular-blue-ring'>, 'panelId': <'$id'>, 'newPanel': <false>, 'useRecommendedIcons': <false>}" >/dev/null
    # A baked platform has no procedural shape. Opacity is not such a field:
    # every renderer applies it, so it is no longer a refused edit.
    reply="$(preset_call updateDraft "{'shape': <'rounded'>}")"
    jq -e '.success==false and .errorCode=="unavailable-panel-field" and .state=="ACTIVE"' <<<"$reply" >/dev/null
    preset_ok updateDraft "{'layoutRadius': <240>}" >/dev/null
    wait_for_free_host_snapshot_stable "$desktop" "$applet" preview >/dev/null
    [[ "$(preset_free_geometry "$desktop" "$applet")" != "$geometry" ]]
    preset_require_equal "$registry" "$(panel_registry_json)" 'S12 no durable preview write'
    preset_ok cancel >/dev/null
    wait_for_free_host_snapshot_stable "$desktop" "$applet" rollback >/dev/null
    preset_require_equal "$geometry" "$(preset_free_geometry "$desktop" "$applet")" 'S12 exact free geometry restored'
    preset_require_equal "$renderer" "$(preset_renderer "$id")" 'S12 free renderer restored'
    preset_require_equal "$registry" "$(panel_registry_json)" 'S12 free registry restored'
    log_session_phase 'S12 existing free service interruption / exact recovery'
    preset_ok beginPreview "{'kind': <'panel'>, 'presetId': <'circular-blue-ring'>, 'panelId': <'$id'>, 'newPanel': <false>, 'useRecommendedIcons': <false>}" >/dev/null
    preset_ok updateDraft "{'layoutRadius': <240>}" >/dev/null
    wait_for_free_host_snapshot_stable "$desktop" "$applet" crash >/dev/null
    preset_crash_service existing-free
    wait_for_free_host_snapshot_stable "$desktop" "$applet" recovery >/dev/null
    preset_require_equal "$geometry" "$(preset_free_geometry "$desktop" "$applet")" 'S12 exact free crash recovery'
    preset_require_equal "$registry" "$(panel_registry_json)" 'S12 recovery never commits'
    # AD3D-TASK-002: desktop 3D editing is an audition of the panel itself.
    log_session_phase 'S13 desktop 3D edit / exact Cancel / single Apply / interruption recovery'
    local revision applied
    revision="$(panel_registry_value "$id" settingsRevision)"
    reply="$(panel_call applyPanelSettingsTransaction "$id" "uint64 $revision" \
        "{'layout': <'ring'>, 'panelThemeId': <'ring-platform-blue'>, 'completeThemeId': <'ring-platform-blue'>, 'rendererTier': <'true3d'>}" '{}')"
    [[ "$reply" == *"'success': <true>"* ]] || { printf 'S13 3D setup failed: %s\n' "$reply" >&2; return 1; }
    wait_for_free_host_snapshot_stable "$desktop" "$applet" scene3d >/dev/null
    registry="$(panel_registry_json)"; renderer="$(preset_renderer "$id")"
    jq -e '.effectiveRendererTier=="true3d" and .sceneEditActive==false' <<<"$renderer" >/dev/null
    reply="$(panel_call updateSceneEditDraft "$id" "{'scene3DRoll': <30.0>}")"
    [[ "$reply" == *"scene-edit-not-active"* ]] || { printf 'S13 edit outside a session: %s\n' "$reply" >&2; return 1; }
    preset_ok beginPreview "{'kind': <'scene3d'>, 'panelId': <'$id'>}" >/dev/null
    preset_renderer "$id" | jq -e '.sceneEditActive==true and .completeThemeId=="ring-platform-blue"' >/dev/null
    reply="$(panel_call updateSceneEditDraft "$id" "{'scene3DRoll': <30.0>, 'scene3DScale': <0.8>}")"
    [[ "$reply" == *"'success': <true>"* ]] || { printf 'S13 gizmo draft refused: %s\n' "$reply" >&2; return 1; }
    preset_renderer "$id" | jq -e '.scene3DRoll==30 and .scene3DScale==0.8 and .sceneEditActive==true' >/dev/null
    reply="$(panel_call updateSceneEditDraft "$id" "{'scene3DPositionX': <0.25>}")"
    [[ "$reply" == *"'success': <true>"* ]]
    preset_renderer "$id" | jq -e '.scene3DRoll==30 and .scene3DPositionX==0.25' >/dev/null
    reply="$(panel_call updateSceneEditDraft "$id" "{'layout': <'vertical'>}")"
    [[ "$reply" == *"unavailable-scene-edit-field"* ]] || { printf 'S13 non-3D edit accepted: %s\n' "$reply" >&2; return 1; }
    preset_require_equal "$registry" "$(panel_registry_json)" 'S13 no durable desktop-edit write'
    preset_ok cancel >/dev/null
    preset_require_equal "$registry" "$(panel_registry_json)" 'S13 Cancel keeps the saved panel'
    preset_require_equal "$renderer" "$(preset_renderer "$id")" 'S13 Cancel restores the drawn panel'
    preset_ok beginPreview "{'kind': <'scene3d'>, 'panelId': <'$id'>}" >/dev/null
    reply="$(panel_call updateSceneEditDraft "$id" "{'scene3DRoll': <20.0>}")"
    [[ "$reply" == *"'success': <true>"* ]]
    preset_ok applyAsActive >/dev/null
    panel_registry_record_snapshot "$id" | jq -e '.scene3DRoll==20 and .completeThemeId=="ring-platform-blue"' >/dev/null
    preset_renderer "$id" | jq -e '.sceneEditActive==false and .scene3DRoll==20' >/dev/null
    applied="$(panel_registry_json)"
    preset_ok beginPreview "{'kind': <'scene3d'>, 'panelId': <'$id'>}" >/dev/null
    reply="$(panel_call updateSceneEditDraft "$id" "{'scene3DRoll': <45.0>}")"
    [[ "$reply" == *"'success': <true>"* ]]
    wait_for_free_host_snapshot_stable "$desktop" "$applet" scene3d-crash >/dev/null
    preset_crash_service scene3d-edit
    wait_for_free_host_snapshot_stable "$desktop" "$applet" scene3d-recovery >/dev/null
    preset_require_equal "$applied" "$(panel_registry_json)" 'S13 an interrupted edit never commits'
    preset_renderer "$id" | jq -e '.sceneEditActive==false and .scene3DRoll==20' >/dev/null
    preset_ok setAsDefault panel obsidian-glass-dock true >/dev/null
    log_session_phase 'S11 independent Icon Preset defaults / new native and free instances'
    registry="$(panel_registry_json)"
    id="$(panel_call createNativePanel right launcher | gvariant_string)"
    [[ -n "$id" ]]
    preset_require_equal "$registry" "$(panel_registry_json | jq -c --arg id "$id" '[.[] | select(.id!=$id)]')" 'S11 icon-only native creation preserves earlier records'
    panel_registry_record_snapshot "$id" | jq -e '.presetOrigin.iconPresetId=="glass-tile" and (.presetOrigin.panelPresetId // "")==""' >/dev/null
    registry="$(panel_registry_json)"
    reply="$(panel_call createFreePanel)"; reply="$(preset_variant_json "$reply")"
    jq -e '.success==true and .ownershipVerified==true' <<<"$reply" >/dev/null
    id="$(jq -r '.panelId' <<<"$reply")"
    preset_require_equal "$registry" "$(panel_registry_json | jq -c --arg id "$id" '[.[] | select(.id!=$id)]')" 'S11 icon-only free creation preserves earlier records'
    panel_registry_record_snapshot "$id" | jq -e '.presetOrigin.iconPresetId=="glass-tile" and (.presetOrigin.panelPresetId // "")==""' >/dev/null
    preset_ok setAsDefault icon glass-tile true >/dev/null
    preset_unrelated_unchanged S11; preset_no_orphans
}

preset_free_geometry() {
    plasma_script "var d=desktopById($1); var w=d ? d.widgetById($2) : null; if (!w) throw Error('missing free host'); var g=w.geometry; w.currentConfigGroup=['General']; print(JSON.stringify({x:Number(g.x),y:Number(g.y),width:Number(g.width),height:Number(g.height),command:String(w.readConfig('auditionRestoreGeometry',''))}));" | gvariant_string | jq -cS .
}

run_preset_audition_matrix() {
    [[ "${ARCHDOCK_PLASMA_LIFECYCLE_SESSION:-}" == 1 && "$XDG_CURRENT_DESKTOP" == archdock-test ]] || return 2
    trap record_session_result EXIT
    log_session_phase "TASK-0041 ${ARCHDOCK_PRESET_MATRIX_GROUP} private startup"
    start_compositor
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
    PRESET_BOTTOM_HOST="$(owned_panel_record bottom)"
    PRESET_BOTTOM_HOST="${PRESET_BOTTOM_HOST%%|*}"
    wait_for_panel_registry_value bottom nativePanelId "$PRESET_BOTTOM_HOST"
    # Native recovery finishes before snapshots; the service's own placement
    # adapter provides the independent initial read-back.
    wait_for_native_panel_placement "$PRESET_BOTTOM_HOST" bottom bottom center baseline
    preset_wait_idle
    case "$ARCHDOCK_PRESET_MATRIX_GROUP" in
        existing) preset_existing_group ;;
        temporary) preset_temporary_group ;;
        icons) preset_icons_group ;;
        recovery) preset_recovery_group ;;
        defaults) preset_defaults_group ;;
        *) return 2 ;;
    esac
    if rg -n 'module "ArchDock.Rendering" is not installed|error when loading applet "org.archdock.dock"|ReferenceError:.*presetAudition' "$ARCHDOCK_TEST_LOG_DIR"/*.log; then
        printf 'Preset runtime renderer or Studio failed to load.\n' >&2
        return 1
    fi
    log_session_phase "TASK-0041 ${ARCHDOCK_PRESET_MATRIX_GROUP} completed"
}

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
    if [[ "${1:-}" == --parser-fixture ]]; then
        preset_require_equal '{"array":[],"success":true,"value":{"revision":7}}' \
            "$(preset_variant_json "({'success': <true>, 'array': <@av []>, 'value': <{'revision': <uint64 7>}>},)")" parser
        printf 'Preset GVariant parser fixture passed.\n'
    else
        script_root="$(cd -- "$(dirname -- "$0")" && pwd)"
        exec env ARCHDOCK_PRESET_MATRIX_SCRIPT="$script_root/run-preset-audition-matrix.sh" \
            ARCHDOCK_PRESET_MATRIX_GROUP="${1:-existing}" \
            bash "$script_root/run-plasma-lifecycle.sh"
    fi
fi
