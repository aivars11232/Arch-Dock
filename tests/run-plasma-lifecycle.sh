#!/usr/bin/env bash

set -euo pipefail

readonly script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
readonly project_root="$(cd -- "$script_dir/.." && pwd)"
ARCHDOCK_LIFECYCLE_STATE_ROOT=''
ARCHDOCK_SESSION_ARCH_DOCK_PID=''
ARCHDOCK_SESSION_PLASMASHELL_PID=''
ARCHDOCK_SESSION_KWIN_PID=''
ARCHDOCK_SIGNAL_MONITOR_PID=''
ARCHDOCK_VISIBILITY_WINDOW_PID=''
ARCHDOCK_VISIBILITY_COMMAND_FILE=''
ARCHDOCK_VISIBILITY_WINDOW_LOG=''
ARCHDOCK_VISIBILITY_PROBE_SNAPSHOT=''
ARCHDOCK_SETTINGS_FIXTURE_FILE=''
ARCHDOCK_SETTINGS_FIXTURE_DIRECTORY=''
ARCHDOCK_SETTINGS_FIXTURE_FILE_MODE=''
ARCHDOCK_SETTINGS_FIXTURE_DIRECTORY_MODE=''
ARCHDOCK_SESSION_RESULT_FILE="${ARCHDOCK_SESSION_RESULT_FILE:-}"

require_command() {
    command -v "$1" >/dev/null 2>&1 || {
        printf 'Required command is unavailable: %s\n' "$1" >&2
        exit 1
    }
}

validate_lifecycle_stop_after() {
    local selector="${ARCHDOCK_LIFECYCLE_STOP_AFTER:-}"
    case "$selector" in
    ''|transaction)
        ;;
    *)
        printf 'Unsupported ARCHDOCK_LIFECYCLE_STOP_AFTER value: %s\n' \
            "$selector" >&2
        return 2
        ;;
    esac
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

wait_for_kwin_script_state() {
    local plugin_name="$1"
    local expected_state="$2"
    local phase="$3"
    local attempt
    local actual_state=''
    for ((attempt = 0; attempt < 100; ++attempt)); do
        actual_state="$(gdbus call \
            --session \
            --dest org.kde.KWin \
            --object-path /Scripting \
            --method org.kde.kwin.Scripting.isScriptLoaded \
            "$plugin_name" 2>/dev/null || true)"
        [[ "$actual_state" == "($expected_state,)" ]] && return
        if [[ -n "$ARCHDOCK_SESSION_KWIN_PID" ]] &&
            ! kill -0 "$ARCHDOCK_SESSION_KWIN_PID" 2>/dev/null; then
            break
        fi
        sleep 0.1
    done
    printf 'Timed out waiting for KWin script state during %s: plugin=%s expected=%s actual=%s\n' \
        "$phase" "$plugin_name" "$expected_state" \
        "${actual_state:-unavailable}" >&2
    return 1
}

unload_kwin_script() {
    local plugin_name="$1"
    if [[ -z "$ARCHDOCK_SESSION_KWIN_PID" ]] ||
        ! kill -0 "$ARCHDOCK_SESSION_KWIN_PID" 2>/dev/null; then
        return
    fi
    gdbus call \
        --session \
        --dest org.kde.KWin \
        --object-path /Scripting \
        --method org.kde.kwin.Scripting.unloadScript \
        "$plugin_name" >/dev/null
    wait_for_kwin_script_state "$plugin_name" false "unloading $plugin_name"
}

stop_arch_dock() {
    unload_kwin_script org.archdock.windowwatcher
    stop_process "$ARCHDOCK_SESSION_ARCH_DOCK_PID"
    ARCHDOCK_SESSION_ARCH_DOCK_PID=''

    local attempt
    local owner_reply=''
    for ((attempt = 0; attempt < 100; ++attempt)); do
        owner_reply="$(gdbus call \
            --session \
            --dest org.freedesktop.DBus \
            --object-path /org/freedesktop/DBus \
            --method org.freedesktop.DBus.NameHasOwner \
            org.archdock.ArchDock 2>/dev/null || true)"
        [[ "$owner_reply" == '(false,)' ]] && return
        sleep 0.1
    done
    printf 'Timed out stopping the private Arch Dock service: owner=%s\n' \
        "${owner_reply:-unavailable}" >&2
    return 1
}

restore_settings_fixture_permissions() {
    if [[ -n "$ARCHDOCK_SETTINGS_FIXTURE_DIRECTORY" &&
        -d "$ARCHDOCK_SETTINGS_FIXTURE_DIRECTORY" &&
        -n "$ARCHDOCK_SETTINGS_FIXTURE_DIRECTORY_MODE" ]]; then
        chmod "$ARCHDOCK_SETTINGS_FIXTURE_DIRECTORY_MODE" \
            "$ARCHDOCK_SETTINGS_FIXTURE_DIRECTORY"
    fi
    if [[ -n "$ARCHDOCK_SETTINGS_FIXTURE_FILE" &&
        -f "$ARCHDOCK_SETTINGS_FIXTURE_FILE" &&
        -n "$ARCHDOCK_SETTINGS_FIXTURE_FILE_MODE" ]]; then
        chmod "$ARCHDOCK_SETTINGS_FIXTURE_FILE_MODE" \
            "$ARCHDOCK_SETTINGS_FIXTURE_FILE"
    fi
    ARCHDOCK_SETTINGS_FIXTURE_FILE=''
    ARCHDOCK_SETTINGS_FIXTURE_DIRECTORY=''
    ARCHDOCK_SETTINGS_FIXTURE_FILE_MODE=''
    ARCHDOCK_SETTINGS_FIXTURE_DIRECTORY_MODE=''
}

cleanup_session() {
    stop_process "$ARCHDOCK_SIGNAL_MONITOR_PID"
    stop_process "$ARCHDOCK_VISIBILITY_WINDOW_PID"
    if [[ -n "$ARCHDOCK_SESSION_KWIN_PID" ]] &&
        kill -0 "$ARCHDOCK_SESSION_KWIN_PID" 2>/dev/null; then
        unload_kwin_script org.archdock.visibilityprobe || true
    fi
    stop_arch_dock || true
    stop_process "$ARCHDOCK_SESSION_PLASMASHELL_PID"
    stop_process "$ARCHDOCK_SESSION_KWIN_PID"
    restore_settings_fixture_permissions
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

panel_property() {
    gdbus call \
        --session \
        --dest org.archdock.ArchDock \
        --object-path /Control \
        --method org.freedesktop.DBus.Properties.Get \
        local.PanelWindow \
        "$1"
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

gvariant_map_string() {
    local key="$1"
    sed -n "s/.*'$key': <'\([^']*\)'>.*/\1/p"
}

gvariant_map_integer() {
    local key="$1"
    sed -n "s/.*'$key': <\(int32 \)\{0,1\}\(-\{0,1\}[0-9]\+\)>.*/\2/p"
}

gvariant_map_boolean() {
    local key="$1"
    sed -n "s/.*'$key': <\(true\|false\)>.*/\1/p"
}

gvariant_nested_map_string() {
    local map="$1"
    local key="$2"
    sed -n "s/.*'$map': <{[^}]*'$key': <'\([^']*\)'>[^}]*}>.*/\1/p"
}

transaction_outer_fields() {
    local reply="$1"
    python - "$reply" <<'PY'
import ast
import re
import sys


PAIRS = {"(": ")", "[": "]", "{": "}", "<": ">"}
CLOSERS = set(PAIRS.values())


def scan(text, delimiter=None, stop_when_empty=False):
    stack = []
    quote = None
    escaped = False
    starts = [0]
    for index, character in enumerate(text):
        if quote is not None:
            if escaped:
                escaped = False
            elif character == "\\":
                escaped = True
            elif character == quote:
                quote = None
            continue
        if character in ("'", '"'):
            quote = character
            continue
        if character in PAIRS:
            stack.append(character)
            continue
        if character in CLOSERS:
            if not stack or PAIRS[stack[-1]] != character:
                raise ValueError("unbalanced GVariant reply")
            stack.pop()
            if stop_when_empty and not stack:
                return index
            continue
        if delimiter is not None and character == delimiter and not stack:
            starts.append(index + 1)
    if quote is not None or stack:
        raise ValueError("unterminated GVariant reply")
    if delimiter is None:
        return None
    boundaries = [start - 1 for start in starts[1:]] + [len(text)]
    return [text[start:end].strip() for start, end in zip(starts, boundaries)]


def split_entry(entry):
    stack = []
    quote = None
    escaped = False
    for index, character in enumerate(entry):
        if quote is not None:
            if escaped:
                escaped = False
            elif character == "\\":
                escaped = True
            elif character == quote:
                quote = None
            continue
        if character in ("'", '"'):
            quote = character
        elif character in PAIRS:
            stack.append(character)
        elif character in CLOSERS:
            if not stack or PAIRS[stack[-1]] != character:
                raise ValueError("unbalanced GVariant map entry")
            stack.pop()
        elif character == ":" and not stack:
            return entry[:index].strip(), entry[index + 1:].strip()
    raise ValueError("GVariant map entry has no top-level separator")


def variant_inner(raw):
    raw = raw.strip()
    if len(raw) < 2 or raw[0] != "<" or raw[-1] != ">":
        raise ValueError("outer map field is not a variant")
    return raw[1:-1].strip()


def string_field(fields, key):
    value = ast.literal_eval(variant_inner(fields[key]))
    if not isinstance(value, str):
        raise ValueError(f"{key} is not a string")
    return value


def boolean_field(fields, key):
    value = variant_inner(fields[key])
    if value not in ("true", "false"):
        raise ValueError(f"{key} is not a boolean")
    return value


def revision_field(fields, key):
    match = re.fullmatch(r"(?:uint64\s+)?([0-9]+)", variant_inner(fields[key]))
    if match is None:
        raise ValueError(f"{key} is not an unsigned revision")
    return match.group(1)


try:
    reply = sys.argv[1].strip()
    map_start = reply.find("{")
    if map_start < 0 or reply[:map_start].strip() != "(":
        raise ValueError("reply is not a one-map D-Bus tuple")
    map_length = scan(reply[map_start:], stop_when_empty=True)
    map_end = map_start + map_length
    if "".join(reply[map_end + 1:].split()) != ",)":
        raise ValueError("reply contains data outside the outer transaction map")

    fields = {}
    body = reply[map_start + 1:map_end]
    for entry in scan(body, delimiter=","):
        if not entry:
            continue
        raw_key, raw_value = split_entry(entry)
        key = ast.literal_eval(raw_key)
        if not isinstance(key, str) or key in fields:
            raise ValueError("invalid or duplicate outer transaction key")
        fields[key] = raw_value

    required = {
        "status",
        "success",
        "errorCode",
        "rolledBack",
        "rollbackRevision",
        "hostResults",
    }
    missing = sorted(required.difference(fields))
    if missing:
        raise ValueError("missing outer transaction fields: " + ", ".join(missing))

    print(string_field(fields, "status"))
    print(string_field(fields, "errorCode"))
    print(boolean_field(fields, "success"))
    print(boolean_field(fields, "rolledBack"))
    print(revision_field(fields, "rollbackRevision"))
    print("true")
except (KeyError, SyntaxError, ValueError) as error:
    print(f"Could not parse outer settings transaction: {error}", file=sys.stderr)
    raise SystemExit(1)
PY
}

require_structured_placement_reply() {
    local reply="$1"
    local expected_status="$2"
    local expected_success="$3"
    local expected_error="$4"
    local actual_status
    actual_status="$(gvariant_map_string status <<<"$reply")"
    local actual_success
    actual_success="$(gvariant_map_boolean success <<<"$reply")"
    [[ "$actual_status" == "$expected_status" &&
        "$actual_success" == "$expected_success" &&
        "$reply" == *"'errorCode': <'$expected_error'>"* &&
        "$reply" == *"'requested':"* &&
        "$reply" == *"'applied':"* &&
        "$reply" == *"'unsupported':"* &&
        "$reply" == *"'failed':"* &&
        "$reply" == *"'savedIntent':"* &&
        "$reply" == *"'hostState':"* &&
        "$reply" == *"'ownershipVerified':"* &&
        "$reply" == *"'rollbackAttempted':"* &&
        "$reply" == *"'rollbackSucceeded':"* &&
        "$reply" == *"'rollbackErrorCode':"* ]] || {
        printf 'Unexpected structured placement result: expected=%s/%s/%s reply=%s\n' \
            "$expected_status" "$expected_success" "$expected_error" "$reply" >&2
        return 1
    }
}

require_structured_visibility_reply() {
    local reply="$1"
    local expected_status="$2"
    local expected_success="$3"
    local expected_error="$4"
    local expected_effective_mode="$5"
    local expected_host_mode="$6"
    local actual_status
    actual_status="$(gvariant_map_string status <<<"$reply")"
    local actual_success
    actual_success="$(gvariant_map_boolean success <<<"$reply")"
    local actual_error
    actual_error="$(gvariant_map_string errorCode <<<"$reply")"
    local actual_effective_mode
    actual_effective_mode="$(gvariant_map_string effectiveMode <<<"$reply")"
    local actual_host_mode
    actual_host_mode="$(gvariant_map_string hostMode <<<"$reply")"
    [[ "$actual_status" == "$expected_status" &&
        "$actual_success" == "$expected_success" &&
        "$actual_error" == "$expected_error" &&
        "$actual_effective_mode" == "$expected_effective_mode" &&
        "$actual_host_mode" == "$expected_host_mode" &&
        "$reply" == *"'requestedMode':"* &&
        "$reply" == *"'supportedModes':"* &&
        "$reply" == *"'watcherAvailable': <true>"* &&
        "$reply" == *"'verified':"* &&
        "$reply" == *"'fallbackApplied':"* &&
        "$reply" == *"'fallbackReason':"* &&
        "$reply" == *"'ownershipVerified':"* &&
        "$reply" == *"'rollbackAttempted':"* &&
        "$reply" == *"'rollbackSucceeded':"* &&
        "$reply" == *"'rollbackErrorCode':"* ]] || {
        printf 'Unexpected structured visibility result: expected=%s/%s/%s/%s/%s reply=%s\n' \
            "$expected_status" "$expected_success" "$expected_error" \
            "$expected_effective_mode" "$expected_host_mode" "$reply" >&2
        return 1
    }
}

require_structured_transaction_reply() {
    local reply="$1"
    local expected_status="$2"
    local expected_success="$3"
    local expected_error="$4"
    local expected_rolled_back="$5"
    local expected_rollback_revision="$6"
    local parsed
    parsed="$(transaction_outer_fields "$reply")" || return 1
    local -a fields
    mapfile -t fields <<<"$parsed"
    [[ "${#fields[@]}" == '6' ]] || {
        printf 'Outer settings transaction parser returned %s fields: %s\n' \
            "${#fields[@]}" "$parsed" >&2
        return 1
    }
    local actual_status="${fields[0]}"
    local actual_error="${fields[1]}"
    local actual_success="${fields[2]}"
    local actual_rolled_back="${fields[3]}"
    local actual_rollback_revision="${fields[4]}"
    local host_results_present="${fields[5]}"
    [[ "$actual_status" == "$expected_status" &&
        "$actual_success" == "$expected_success" &&
        "$actual_error" == "$expected_error" &&
        "$actual_rolled_back" == "$expected_rolled_back" &&
        "$actual_rollback_revision" == "$expected_rollback_revision" &&
        "$host_results_present" == 'true' &&
        "$reply" == *"'expectedRevision':"* &&
        "$reply" == *"'previousRevision':"* &&
        "$reply" == *"'revision':"* ]] || {
        printf 'Unexpected outer settings transaction: expected=%s/%s/%s/%s/%s actual=%s/%s/%s/%s/%s reply=%s\n' \
            "$expected_status" "$expected_success" "$expected_error" \
            "$expected_rolled_back" "$expected_rollback_revision" \
            "$actual_status" "$actual_success" "$actual_error" \
            "$actual_rolled_back" "$actual_rollback_revision" "$reply" >&2
        return 1
    }
}

run_transaction_parser_fixture() {
    local reply="({'status': <'outer-status'>, 'errorCode': <'outer-error'>, 'rolledBack': <true>, 'rollbackRevision': <uint64 44>, 'success': <false>, 'hostResults': <[<{'status': <'nested-status'>, 'errorCode': <'nested-error'>, 'rolledBack': <false>, 'rollbackRevision': <uint64 999>, 'success': <true>}>]>, 'expectedRevision': <uint64 42>, 'previousRevision': <uint64 42>, 'revision': <uint64 43>},)"
    require_structured_transaction_reply \
        "$reply" outer-status false outer-error true 44
    printf 'Focused outer transaction parser fixture succeeded.\n'
}

panel_ids() {
    plasma_script \
        "var ids = []; var all = panels(); for (var index = 0; index < all.length; ++index) { ids.push(String(all[index].id)); } ids.sort(); print(ids.join(','));"
}

panel_count() {
    local ids
    ids="$(panel_ids)"
    if [[ -z "$ids" ]]; then
        printf '0\n'
    else
        awk -F, '{ print NF }' <<<"$ids"
    fi
}

arch_dock_settings_file() {
    printf '%s/Arch Dock/Arch Dock.conf\n' "$XDG_CONFIG_HOME"
}

panel_registry_json() {
    local encoded
    encoded="$(kreadconfig6 \
        --file "$(arch_dock_settings_file)" \
        --group dock \
        --key panels)"
    if [[ "${encoded:0:1}" == '"' ]]; then
        encoded="$(jq -er '.' <<<"$encoded")"
    fi
    [[ "$encoded" == '@ByteArray('*')' ]] || {
        printf 'Arch Dock panel registry is not a QSettings byte array.\n' >&2
        return 1
    }
    encoded="${encoded#@ByteArray(}"
    encoded="${encoded%)}"
    jq -ce 'if type == "array" then . else error("panel registry is not an array") end' \
        <<<"$encoded"
}

write_panel_registry_json() {
    local registry_json
    registry_json="$(jq -ce \
        'if type == "array" then . else error("panel registry is not an array") end' \
        <<<"$1")"
    python - "$(arch_dock_settings_file)" "$registry_json" <<'PY'
import sys

from PySide6.QtCore import QByteArray, QSettings

settings = QSettings(sys.argv[1], QSettings.IniFormat)
settings.beginGroup("dock")
settings.setValue("panels", QByteArray(sys.argv[2].encode("utf-8")))
settings.endGroup()
settings.sync()
if settings.status() != QSettings.Status.NoError:
    raise SystemExit(1)
PY
}

set_private_monitor_index() {
    local monitor_index="$1"
    python - "$(arch_dock_settings_file)" "$monitor_index" <<'PY'
import sys

from PySide6.QtCore import QSettings

settings = QSettings(sys.argv[1], QSettings.IniFormat)
settings.beginGroup("dock")
settings.setValue("monitorIndex", int(sys.argv[2]))
settings.endGroup()
settings.sync()
if settings.status() != QSettings.Status.NoError:
    raise SystemExit(1)
PY
}

panel_registry_value() {
    local panel_id="$1"
    local key="$2"
    panel_registry_json | jq -er \
        --arg panel_id "$panel_id" \
        --arg key "$key" \
        'first(.[] | select(.id == $panel_id) | .[$key])'
}

panel_registry_record_snapshot() {
    local panel_id="$1"
    panel_registry_json | jq -cSe \
        --arg panel_id "$panel_id" \
        'first(.[] | select(.id == $panel_id))'
}

# TASK-0033: free-panel content helpers.
panel_settings_revision() {
    panel_call dockConfiguration "$1" | sed -n \
        "s/.*'settingsRevision': <uint64 \\([0-9][0-9]*\\)>.*/\\1/p"
}

apply_panel_settings() {
    local panel_id="$1"
    local values="$2"
    local context="$3"
    local revision
    revision="$(panel_settings_revision "$panel_id")"
    [[ "$revision" =~ ^[0-9]+$ ]] || {
        printf 'Could not read the settings revision of %s before %s.\n' \
            "$panel_id" "$context" >&2
        exit 1
    }
    local reply
    reply="$(panel_call applyPanelSettingsTransaction \
        "$panel_id" "uint64 $revision" "$values" '{}')"
    [[ "$reply" == *"'success': <true>"* &&
        "$reply" == *"'status': <'succeeded'>"* ]] || {
        printf 'Settings transaction for %s failed during %s: %s\n' \
            "$panel_id" "$context" "$reply" >&2
        exit 1
    }
}

# The entry ids the backend currently shows for a panel, comma-joined in order.
# The requested type is deliberately meaningless for free panels; the record
# decides.
panel_entry_ids() {
    local reply
    reply="$(panel_call dockEntriesForPanel "$1" empty)"
    # No entries is a valid answer, so a non-matching grep must not fail the
    # pipeline under `set -o pipefail`.
    { grep -o "'appId': <'[^']*'>" <<<"$reply" || true; } \
        | sed "s/'appId': <'\\(.*\\)'>/\\1/" \
        | paste -sd, -
}

wait_for_panel_entry_ids() {
    local panel_id="$1"
    local expected="$2"
    local context="$3"
    local attempt
    local actual=''
    for ((attempt = 0; attempt < 50; ++attempt)); do
        actual="$(panel_entry_ids "$panel_id")"
        [[ "$actual" == "$expected" ]] && return
        sleep 0.1
    done
    printf 'Free panel %s did not show the expected entries during %s: expected=%s actual=%s\n' \
        "$panel_id" "$context" "$expected" "$actual" >&2
    exit 1
}

free_panel_ids_json() {
    panel_registry_json | jq -c '[.[] | select(.edge == "free") | .id] | sort'
}

free_panel_count() {
    free_panel_ids_json | jq -r 'length'
}

wait_for_free_panel_count() {
    local expected_count="$1"
    local attempt
    local actual_count=''
    for ((attempt = 0; attempt < 100; ++attempt)); do
        actual_count="$(free_panel_count 2>/dev/null || true)"
        [[ "$actual_count" == "$expected_count" ]] && return
        sleep 0.1
    done
    printf 'Timed out waiting for %s free-panel records; last=%s.\n' \
        "$expected_count" "${actual_count:-unavailable}" >&2
    return 1
}

new_free_panel_id() {
    local previous_ids_json="$1"
    panel_registry_json | jq -er \
        --argjson previous "$previous_ids_json" \
        '[.[] | select(.edge == "free") | .id as $id |
          select(($previous | index($id)) == null) | $id] |
         if length == 1 then .[0] else error("expected exactly one new free panel") end'
}

wait_for_panel_ids() {
    local expected_ids="$1"
    local attempt
    local actual_ids=''
    for ((attempt = 0; attempt < 100; ++attempt)); do
        actual_ids="$(panel_ids 2>/dev/null || true)"
        [[ "$actual_ids" == "$expected_ids" ]] && return
        sleep 0.1
    done
    printf 'Timed out waiting for the Plasma panel set to converge: expected=%s actual=%s\n' \
        "$expected_ids" "${actual_ids:-unavailable}" >&2
    return 1
}

free_host_snapshot() {
    local desktop_containment_id="$1"
    local dock_applet_id="$2"
    plasma_script \
        "var desktop = desktopById($desktop_containment_id); var dock = desktop ? desktop.widgetById($dock_applet_id) : null; if (!desktop || !dock || dock.type !== 'org.archdock.dock') { print('missing'); } else { dock.currentConfigGroup = ['General']; var geometry = dock.geometry; print([String(desktop.id), String(desktop.screen), String(dock.id), String(dock.type), String(dock.readConfig('panelId', '')), String(dock.readConfig('panelType', '')), String(dock.readConfig('ownerToken', '')), String(dock.readConfig('bootstrapFreeDock', true)), String(Number(geometry.width)), String(Number(geometry.height))].join('|')); }" |
        gvariant_string
}

wait_for_free_host_snapshot_stable() {
    local desktop_containment_id="$1"
    local dock_applet_id="$2"
    local phase="$3"
    local previous_snapshot=''
    local current_snapshot=''
    local stable_observations=0
    local attempt
    for ((attempt = 0; attempt < 100; ++attempt)); do
        current_snapshot="$(free_host_snapshot \
            "$desktop_containment_id" "$dock_applet_id" 2>/dev/null || true)"
        if [[ "$current_snapshot" != 'missing' &&
            "$current_snapshot" == "$previous_snapshot" ]]; then
            ((stable_observations += 1))
        elif [[ "$current_snapshot" != 'missing' ]]; then
            previous_snapshot="$current_snapshot"
            stable_observations=1
        else
            previous_snapshot=''
            stable_observations=0
        fi
        if ((stable_observations >= 5)); then
            printf '%s\n' "$current_snapshot"
            return
        fi
        sleep 0.1
    done
    printf 'Timed out waiting for the free host snapshot to stabilize during %s: %s\n' \
        "$phase" "${current_snapshot:-unavailable}" >&2
    return 1
}

free_host_match_count() {
    local panel_id="$1"
    local ownership_token="$2"
    plasma_script \
        "var count = 0; var all = desktops(); for (var index = 0; index < all.length; ++index) { var desktop = all[index]; var docks = desktop.widgets('org.archdock.dock'); for (var dockIndex = 0; dockIndex < docks.length; ++dockIndex) { var dock = desktop.widgetById(docks[dockIndex].id); if (!dock || dock.type !== 'org.archdock.dock') { continue; } dock.currentConfigGroup = ['General']; if (String(dock.readConfig('panelId', '')) === '$panel_id' && String(dock.readConfig('ownerToken', '')) === '$ownership_token') { ++count; } } } print(String(count));" |
        gvariant_string
}

free_bootstrap_artifact_count() {
    local bridge_containment_id="$1"
    local bootstrap_token="$2"
    plasma_script \
        "var count = panelById($bridge_containment_id) ? 1 : 0; function countControls(containments) { for (var containmentIndex = 0; containmentIndex < containments.length; ++containmentIndex) { var containment = containments[containmentIndex]; var widgets = containment.widgets(); for (var widgetIndex = 0; widgetIndex < widgets.length; ++widgetIndex) { var widget = containment.widgetById(widgets[widgetIndex].id); if (!widget || widget.type !== 'org.archdock.control') { continue; } widget.currentConfigGroup = ['General']; if (Number(widget.readConfig('bootstrapPanelId', -1)) === $bridge_containment_id && String(widget.readConfig('bootstrapToken', '')) === '$bootstrap_token') { ++count; } } } } countControls(panels()); countControls(desktops()); print(String(count));" |
        gvariant_string
}

wait_for_free_bootstrap_artifacts_absent() {
    local bridge_containment_id="$1"
    local bootstrap_token="$2"
    local attempt
    local actual_count=''
    for ((attempt = 0; attempt < 100; ++attempt)); do
        actual_count="$(free_bootstrap_artifact_count \
            "$bridge_containment_id" "$bootstrap_token" 2>/dev/null || true)"
        [[ "$actual_count" == '0' ]] && return
        sleep 0.1
    done
    printf 'Timed out waiting for template bridge artifacts to disappear: bridge=%s count=%s\n' \
        "$bridge_containment_id" "${actual_count:-unavailable}" >&2
    return 1
}

create_duplicate_free_host_fixture() {
    local desktop_containment_id="$1"
    local panel_id="$2"
    local ownership_token="$3"
    plasma_script \
        "var desktop = desktopById($desktop_containment_id); if (!desktop) { print('missing'); } else { var dock = desktop.addWidget('org.archdock.dock', Math.round(gridUnit * 4), Math.round(gridUnit * 4), Math.round(gridUnit * 12), Math.round(gridUnit * 12)); if (!dock || Number(dock.id) < 0) { print('missing'); } else { dock.currentConfigGroup = ['General']; dock.writeConfig('panelId', '$panel_id'); dock.writeConfig('panelType', 'empty'); dock.writeConfig('ownerToken', '$ownership_token'); dock.writeConfig('bootstrapFreeDock', false); dock.reloadConfig(); print(String(desktop.id) + '|' + String(dock.id)); } }" |
        gvariant_string
}

remove_owned_free_host_fixture() {
    local desktop_containment_id="$1"
    local dock_applet_id="$2"
    local panel_id="$3"
    local ownership_token="$4"
    plasma_script \
        "var desktop = desktopById($desktop_containment_id); var dock = desktop ? desktop.widgetById($dock_applet_id) : null; if (!desktop || Number(desktop.id) !== $desktop_containment_id || !dock || Number(dock.id) !== $dock_applet_id || dock.type !== 'org.archdock.dock') { print(0); } else { dock.currentConfigGroup = ['General']; var bootstrap = String(dock.readConfig('bootstrapFreeDock', true)).toLowerCase(); var owned = String(dock.readConfig('panelId', '')) === '$panel_id' && String(dock.readConfig('panelType', '')) === 'empty' && String(dock.readConfig('ownerToken', '')) === '$ownership_token' && (bootstrap === 'false' || bootstrap === '0'); if (!owned) { print(0); } else { dock.remove(); print(1); } }" |
        gvariant_string
}

create_unrelated_free_host_sentinel() {
    plasma_script \
        "var desktop = desktopForScreen(0); if (!desktop) { print('missing'); } else { var dock = desktop.addWidget('org.archdock.dock', Math.round(gridUnit * 2), Math.round(gridUnit * 2), Math.round(gridUnit * 12), Math.round(gridUnit * 12)); if (!dock || Number(dock.id) < 0) { print('missing'); } else { dock.currentConfigGroup = ['General']; dock.writeConfig('panelId', 'archdock-unrelated-sentinel'); dock.writeConfig('panelType', 'empty'); dock.writeConfig('ownerToken', 'archdock-unrelated-sentinel-token'); dock.writeConfig('bootstrapFreeDock', false); dock.reloadConfig(); print(String(desktop.id) + '|' + String(dock.id)); } }" |
        gvariant_string
}

require_free_panel_host() {
    local panel_id="$1"
    local expected_containment_id="$2"
    local expected_applet_id="$3"
    local route="$4"
    local allow_disconnected_screen="${5:-false}"
    local ownership_token
    ownership_token="$(panel_registry_value "$panel_id" freeOwnershipToken)"
    local registry_containment_id
    registry_containment_id="$(panel_registry_value "$panel_id" freeDesktopContainmentId)"
    local registry_applet_id
    registry_applet_id="$(panel_registry_value "$panel_id" freeDockAppletId)"
    local registry_screen
    registry_screen="$(panel_registry_value "$panel_id" screen)"
    local registry_screen_id
    registry_screen_id="$(panel_registry_value "$panel_id" screenId)"

    [[ "$(panel_registry_value "$panel_id" freeHostState)" == 'hosted-owned' &&
        "$(panel_registry_value "$panel_id" freeHostMode)" == 'desktop' &&
        "$registry_containment_id" == "$expected_containment_id" &&
        "$registry_applet_id" == "$expected_applet_id" &&
        "$registry_screen" =~ ^[0-9]+$ && -n "$registry_screen_id" &&
        -n "$ownership_token" ]] || {
        printf 'The %s free-panel registry association is incomplete: %s\n' \
            "$route" "$(panel_registry_record_snapshot "$panel_id" 2>/dev/null || true)" >&2
        return 1
    }

    local attempt
    local snapshot=''
    for ((attempt = 0; attempt < 100; ++attempt)); do
        snapshot="$(free_host_snapshot "$expected_containment_id" "$expected_applet_id" 2>/dev/null || true)"
        local live_containment_id=''
        local live_screen=''
        local live_applet_id=''
        local live_type=''
        local live_panel_id=''
        local live_panel_type=''
        local live_token=''
        local live_bootstrap=''
        local live_width=''
        local live_height=''
        IFS='|' read -r \
            live_containment_id live_screen live_applet_id live_type live_panel_id \
            live_panel_type live_token live_bootstrap live_width live_height <<<"$snapshot"
        if [[ "$live_containment_id" == "$expected_containment_id" &&
            ("$live_screen" == "$registry_screen" ||
             ("$allow_disconnected_screen" == 'true' && "$live_screen" == '-1')) &&
            "$live_applet_id" == "$expected_applet_id" &&
            "$live_type" == 'org.archdock.dock' &&
            "$live_panel_id" == "$panel_id" &&
            "$live_panel_type" == 'empty' &&
            "$live_token" == "$ownership_token" &&
            "$live_bootstrap" == 'false' &&
            "$live_width" =~ ^[1-9][0-9]*$ &&
            "$live_height" =~ ^[1-9][0-9]*$ ]]; then
            [[ "$(free_host_match_count "$panel_id" "$ownership_token")" == '1' ]] || {
                printf 'The %s free-panel token does not identify exactly one live applet.\n' \
                    "$route" >&2
                return 1
            }
            printf '%s\n' "$ownership_token"
            return
        fi
        sleep 0.1
    done

    printf 'Timed out waiting for the %s free-panel host: %s\n' \
        "$route" "${snapshot:-unavailable}" >&2
    return 1
}

require_current_free_panel_host() {
    local panel_id="$1"
    local expected_token="$2"
    local route="$3"
    local allow_disconnected_screen="${4:-false}"
    local desktop_containment_id
    desktop_containment_id="$(panel_registry_value \
        "$panel_id" freeDesktopContainmentId)"
    local dock_applet_id
    dock_applet_id="$(panel_registry_value "$panel_id" freeDockAppletId)"
    local actual_token
    actual_token="$(require_free_panel_host \
        "$panel_id" "$desktop_containment_id" "$dock_applet_id" "$route" \
        "$allow_disconnected_screen")"
    [[ "$actual_token" == "$expected_token" ]] || {
        printf 'The %s free-panel ownership token changed: expected=%s actual=%s\n' \
            "$route" "$expected_token" "${actual_token:-unavailable}" >&2
        return 1
    }
}

set_stale_native_ids() {
    local panel_id="$1"
    local containment_id="$2"
    local dock_applet_id="$3"
    local registry_json
    registry_json="$(panel_registry_json | jq -ce \
        --arg panel_id "$panel_id" \
        --argjson containment_id "$containment_id" \
        --argjson dock_applet_id "$dock_applet_id" \
        'map(if .id == $panel_id then
            .nativePanelId = $containment_id |
            .nativeControlAppletId = -1 |
            .nativeDockAppletId = $dock_applet_id
        else . end)')"
    write_panel_registry_json "$registry_json"
}

set_panel_registry_placement() {
    local panel_id="$1"
    local edge="$2"
    local alignment="$3"
    local registry_json
    registry_json="$(panel_registry_json | jq -ce \
        --arg panel_id "$panel_id" \
        --arg edge "$edge" \
        --arg alignment "$alignment" \
        'if ([.[] | select(.id == $panel_id)] | length) != 1 then
            error("expected exactly one panel registry record")
         else
            map(if .id == $panel_id then
                .edge = $edge |
                .alignment = $alignment
            else . end)
         end')"
    write_panel_registry_json "$registry_json"
}

set_panel_registry_geometry() {
    local panel_id="$1"
    local dynamic="$2"
    local width="$3"
    local height="$4"
    local floating_margin="${5:-}"
    local registry_json
    registry_json="$(panel_registry_json | jq -ce \
        --arg panel_id "$panel_id" \
        --argjson dynamic "$dynamic" \
        --argjson width "$width" \
        --argjson height "$height" \
        --arg floating_margin "$floating_margin" \
        'if ([.[] | select(.id == $panel_id)] | length) != 1 then
            error("expected exactly one panel registry record")
         else
            map(if .id == $panel_id then
                .dynamic = $dynamic |
                .width = $width |
                .height = $height |
                if $floating_margin == "" then
                    del(.floatingMargin)
                else
                    .floatingMargin = ($floating_margin | tonumber)
                end
            else . end)
         end')"
    write_panel_registry_json "$registry_json"
}

set_stale_free_ids() {
    local panel_id="$1"
    local desktop_containment_id="$2"
    local dock_applet_id="$3"
    local registry_json
    registry_json="$(panel_registry_json | jq -ce \
        --arg panel_id "$panel_id" \
        --argjson desktop_containment_id "$desktop_containment_id" \
        --argjson dock_applet_id "$dock_applet_id" \
        'map(if .id == $panel_id then
            .freeDesktopContainmentId = $desktop_containment_id |
            .freeDockAppletId = $dock_applet_id |
            .freeHostState = "hosted-stale" |
            .freeRecoveryError = "stale-id-fixture"
        else . end)')"
    write_panel_registry_json "$registry_json"
}

wait_for_panel_registry_value() {
    local panel_id="$1"
    local key="$2"
    local expected="$3"
    local attempt
    local actual=''
    for ((attempt = 0; attempt < 100; ++attempt)); do
        actual="$(panel_registry_value "$panel_id" "$key" 2>/dev/null || true)"
        [[ "$actual" == "$expected" ]] && return
        sleep 0.1
    done
    printf 'Timed out waiting for registry value: panel=%s key=%s expected=%s actual=%s\n' \
        "$panel_id" "$key" "$expected" "${actual:-unavailable}" >&2
    return 1
}

wait_for_panel_registry_absent() {
    local panel_id="$1"
    local attempt
    for ((attempt = 0; attempt < 100; ++attempt)); do
        if ! panel_registry_record_snapshot "$panel_id" >/dev/null 2>&1; then
            return
        fi
        sleep 0.1
    done
    printf 'Timed out waiting for the panel registry record to be removed: %s record=%s\n' \
        "$panel_id" \
        "$(panel_registry_record_snapshot "$panel_id" 2>/dev/null || true)" >&2
    return 1
}

wait_for_free_host_match_count() {
    local panel_id="$1"
    local ownership_token="$2"
    local expected_count="$3"
    local attempt
    local actual_count=''
    for ((attempt = 0; attempt < 100; ++attempt)); do
        actual_count="$(free_host_match_count \
            "$panel_id" "$ownership_token" 2>/dev/null || true)"
        [[ "$actual_count" == "$expected_count" ]] && return
        sleep 0.1
    done
    printf 'Timed out waiting for free-host matches: panel=%s expected=%s actual=%s\n' \
        "$panel_id" "$expected_count" "${actual_count:-unavailable}" >&2
    return 1
}

wait_for_free_host_absent() {
    local desktop_containment_id="$1"
    local dock_applet_id="$2"
    local attempt
    local snapshot=''
    for ((attempt = 0; attempt < 100; ++attempt)); do
        snapshot="$(free_host_snapshot \
            "$desktop_containment_id" "$dock_applet_id" 2>/dev/null || true)"
        [[ "$snapshot" == 'missing' ]] && return
        sleep 0.1
    done
    printf 'Timed out waiting for a free-host applet to disappear: containment=%s applet=%s snapshot=%s\n' \
        "$desktop_containment_id" "$dock_applet_id" "${snapshot:-unavailable}" >&2
    return 1
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

owned_panel_match_count() {
    local panel_id="$1"
    local ownership_token="$2"
    plasma_script \
        "var count = 0; var all = panels(); for (var index = 0; index < all.length; ++index) { var panel = all[index]; panel.currentConfigGroup = ['ArchDock']; if (panel.readConfig('panelId', '') === '$panel_id' && panel.readConfig('ownerToken', '') === '$ownership_token') { ++count; } } print(count);" |
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

wait_for_native_panel_presentation() {
    local containment_id="$1"
    local expected_presentation="$2"
    local phase="$3"
    local attempt
    local actual_presentation=''
    for ((attempt = 0; attempt < 100; ++attempt)); do
        actual_presentation="$(native_panel_presentation \
            "$containment_id" 2>/dev/null || true)"
        [[ "$actual_presentation" == "$expected_presentation" ]] && return
        sleep 0.1
    done
    printf 'Timed out waiting for native visibility during %s: expected=%s actual=%s\n' \
        "$phase" "$expected_presentation" \
        "${actual_presentation:-unavailable}" >&2
    return 1
}

reload_visibility_probe() {
    local kwin_log="$ARCHDOCK_TEST_LOG_DIR/kwin.log"
    local first_line
    first_line="$(( $(wc -l <"$kwin_log") + 1 ))"
    gdbus call \
        --session \
        --dest org.kde.KWin \
        --object-path /Scripting \
        --method org.kde.kwin.Scripting.unloadScript \
        org.archdock.visibilityprobe >/dev/null 2>&1 || true
    local load_reply
    load_reply="$(gdbus call \
        --session \
        --dest org.kde.KWin \
        --object-path /Scripting \
        --method org.kde.kwin.Scripting.loadScript \
        "$ARCHDOCK_VISIBILITY_PROBE_SCRIPT" \
        org.archdock.visibilityprobe)"
    local script_id
    script_id="$(gvariant_integer <<<"$load_reply")"
    [[ "$script_id" =~ ^[0-9]+$ ]] || {
        printf 'Could not load the private KWin visibility probe: %s\n' \
            "$load_reply" >&2
        return 1
    }
    gdbus call \
        --session \
        --dest org.kde.KWin \
        --object-path "/Scripting/Script$script_id" \
        --method org.kde.kwin.Script.run >/dev/null
    local loaded_reply
    loaded_reply="$(gdbus call \
        --session \
        --dest org.kde.KWin \
        --object-path /Scripting \
        --method org.kde.kwin.Scripting.isScriptLoaded \
        org.archdock.visibilityprobe)"
    [[ "$loaded_reply" == '(true,)' ]] || {
        printf 'Private KWin visibility probe did not remain loaded: %s\n' \
            "$loaded_reply" >&2
        return 1
    }

    local attempt
    for ((attempt = 0; attempt < 100; ++attempt)); do
        sed -n "${first_line},\$p" "$kwin_log" >"$ARCHDOCK_VISIBILITY_PROBE_SNAPSHOT"
        if grep -Fq 'ARCHDOCK_VISIBILITY_PROBE_END' \
                "$ARCHDOCK_VISIBILITY_PROBE_SNAPSHOT"; then
            return
        fi
        sleep 0.1
    done
    printf 'Timed out waiting for the private KWin visibility probe snapshot.\n' >&2
    return 1
}

identify_visibility_panel_window() {
    local expected_length="$1"
    local expected_output="$2"
    reload_visibility_probe
    local candidates
    candidates="$(awk -F'|' \
        -v expected_length="$expected_length" \
        -v expected_output="$expected_output" \
        '$1 ~ /ARCHDOCK_VISIBILITY_PROBE$/ &&
         $8 == expected_length &&
         $17 == expected_output &&
         ($15 == "true" || tolower($3) ~ /plasma/) { print $2 }' \
        "$ARCHDOCK_VISIBILITY_PROBE_SNAPSHOT")"
    local candidate_count
    candidate_count="$(sed '/^$/d' <<<"$candidates" | wc -l)"
    [[ "$candidate_count" == '1' ]] || {
        printf 'Expected one panel surface with length %s on output %s; candidates=%s snapshot=%s\n' \
            "$expected_length" "$expected_output" "$candidate_count" \
            "$(grep -F 'ARCHDOCK_VISIBILITY_PROBE|' \
                "$ARCHDOCK_VISIBILITY_PROBE_SNAPSHOT" | tr '\n' ';')" >&2
        return 1
    }
    sed -n '1p' <<<"$candidates"
}

wait_for_visibility_panel_hidden() {
    local internal_id="$1"
    local expected_hidden="$2"
    local phase="$3"
    local attempt
    local actual_hidden=''
    for ((attempt = 0; attempt < 100; ++attempt)); do
        reload_visibility_probe
        actual_hidden="$(awk -F'|' -v internal_id="$internal_id" \
            '$1 ~ /ARCHDOCK_VISIBILITY_PROBE$/ && $2 == internal_id { print $10 }' \
            "$ARCHDOCK_VISIBILITY_PROBE_SNAPSHOT" | tail -n 1)"
        [[ "$actual_hidden" == "$expected_hidden" ]] && return
        sleep 0.1
    done
    printf 'Timed out waiting for compositor panel state during %s: expected-hidden=%s actual=%s\n' \
        "$phase" "$expected_hidden" "${actual_hidden:-unavailable}" >&2
    return 1
}

wait_for_visibility_fixture_state() {
    local expected_state="$1"
    local attempt
    local fixture_row=''
    for ((attempt = 0; attempt < 100; ++attempt)); do
        reload_visibility_probe
        fixture_row="$(awk -F'|' \
            '$1 ~ /ARCHDOCK_VISIBILITY_PROBE$/ &&
             index($5, "Arch Dock Visibility Fixture [") == 1 { print }' \
            "$ARCHDOCK_VISIBILITY_PROBE_SNAPSHOT" | tail -n 1)"
        if [[ -n "$fixture_row" ]]; then
            local caption=''
            local geometry_x=''
            local geometry_y=''
            local geometry_width=''
            local geometry_height=''
            local active=''
            local fullscreen=''
            local normal=''
            local maximized=''
            local output_index=''
            IFS='|' read -r \
                _ _ _ _ caption geometry_x geometry_y geometry_width \
                geometry_height _ active fullscreen _ normal _ maximized \
                output_index \
                <<<"$fixture_row"
            if [[ "$active" == 'true' ]]; then
                case "$expected_state" in
                normal|overlap)
                    local geometry_pattern="^Arch Dock Visibility Fixture \\[$expected_state:(-?[0-9]+):(-?[0-9]+):([0-9]+):([0-9]+)\\]$"
                    if [[ "$caption" =~ $geometry_pattern ]]; then
                        local requested_x="${BASH_REMATCH[1]}"
                        local requested_y="${BASH_REMATCH[2]}"
                        local requested_width="${BASH_REMATCH[3]}"
                        local requested_height="${BASH_REMATCH[4]}"
                        [[ "$fullscreen" == 'false' && "$normal" == 'true' &&
                            "$maximized" == 'false' &&
                            "$geometry_x" == "$requested_x" &&
                            "$geometry_y" == "$requested_y" &&
                            "$geometry_width" == "$requested_width" &&
                            "$geometry_height" == "$requested_height" ]] && return
                    fi
                    ;;
                maximized)
                    [[ "$caption" == 'Arch Dock Visibility Fixture [maximized]' &&
                        "$fullscreen" == 'false' && "$maximized" == 'true' ]] && return
                    ;;
                fullscreen)
                    [[ "$caption" == 'Arch Dock Visibility Fixture [fullscreen]' &&
                        "$fullscreen" == 'true' ]] && return
                    ;;
                esac
            fi
        fi
        sleep 0.1
    done
    printf 'Timed out waiting for compositor fixture state %s: %s\n' \
        "$expected_state" "${fixture_row:-unavailable}" >&2
    return 1
}

start_visibility_window() {
    local screen_index="$1"
    ARCHDOCK_VISIBILITY_COMMAND_FILE="$XDG_RUNTIME_DIR/archdock-visibility-command"
    ARCHDOCK_VISIBILITY_WINDOW_LOG="$ARCHDOCK_TEST_LOG_DIR/visibility-window.log"
    ARCHDOCK_VISIBILITY_PROBE_SNAPSHOT="$XDG_RUNTIME_DIR/archdock-visibility-probe-snapshot"
    printf '0 normal\n' >"$ARCHDOCK_VISIBILITY_COMMAND_FILE"
    python "$ARCHDOCK_VISIBILITY_WINDOW_SCRIPT" \
        "$ARCHDOCK_VISIBILITY_COMMAND_FILE" "$screen_index" \
        >"$ARCHDOCK_VISIBILITY_WINDOW_LOG" 2>&1 &
    ARCHDOCK_VISIBILITY_WINDOW_PID=$!

    local attempt
    for ((attempt = 0; attempt < 100; ++attempt)); do
        grep -Fq 'STATE:0:normal' "$ARCHDOCK_VISIBILITY_WINDOW_LOG" && return
        if ! kill -0 "$ARCHDOCK_VISIBILITY_WINDOW_PID" 2>/dev/null; then
            printf 'The visibility window fixture exited during startup: %s\n' \
                "$(<"$ARCHDOCK_VISIBILITY_WINDOW_LOG")" >&2
            return 1
        fi
        sleep 0.1
    done
    printf 'Timed out starting the visibility window fixture.\n' >&2
    return 1
}

command_visibility_window() {
    local serial="$1"
    local command="$2"
    printf '%s %s\n' "$serial" "$command" >"$ARCHDOCK_VISIBILITY_COMMAND_FILE"
    local attempt
    for ((attempt = 0; attempt < 100; ++attempt)); do
        grep -Fq "STATE:$serial:$command" "$ARCHDOCK_VISIBILITY_WINDOW_LOG" && return
        if ! kill -0 "$ARCHDOCK_VISIBILITY_WINDOW_PID" 2>/dev/null; then
            printf 'The visibility window fixture exited while applying %s: %s\n' \
                "$command" "$(<"$ARCHDOCK_VISIBILITY_WINDOW_LOG")" >&2
            return 1
        fi
        sleep 0.1
    done
    printf 'Timed out commanding the visibility window fixture: %s\n' \
        "$command" >&2
    return 1
}

native_panel_placement_snapshot() {
    local containment_id="$1"
    plasma_script \
        "var panel = panelById($containment_id); if (!panel) { print('missing'); } else { print([String(panel.location).toLowerCase(), String(panel.screen), String(panel.alignment).toLowerCase(), String(panel.offset)].join('|')); }" |
        gvariant_string
}

native_panel_geometry_snapshot() {
    local containment_id="$1"
    plasma_script \
        "var panel = panelById($containment_id); if (!panel) { print('missing'); } else { print([String(panel.height), String(panel.lengthMode).toLowerCase(), String(panel.minimumLength), String(panel.maximumLength), String(panel.length)].join('|')); }" |
        gvariant_string
}

native_panel_geometry_matches() {
    local actual_snapshot="$1"
    local expected_thickness="$2"
    local expected_mode="$3"
    local expected_minimum="$4"
    local expected_maximum="$5"
    local expected_length="$6"
    local actual_thickness=''
    local actual_mode=''
    local actual_minimum=''
    local actual_maximum=''
    local actual_length=''
    IFS='|' read -r \
        actual_thickness actual_mode actual_minimum actual_maximum actual_length \
        <<<"$actual_snapshot"
    [[ "$actual_thickness" == "$expected_thickness" &&
        "$actual_mode" == "$expected_mode" &&
        "$actual_minimum" == "$expected_minimum" &&
        "$actual_maximum" == "$expected_maximum" &&
        ("$expected_length" == '*' || "$actual_length" == "$expected_length") ]]
}

require_native_panel_geometry() {
    local containment_id="$1"
    local expected_thickness="$2"
    local expected_mode="$3"
    local expected_minimum="$4"
    local expected_maximum="$5"
    local expected_length="$6"
    local phase="$7"
    local actual_snapshot
    actual_snapshot="$(native_panel_geometry_snapshot "$containment_id")"
    native_panel_geometry_matches \
        "$actual_snapshot" "$expected_thickness" "$expected_mode" \
        "$expected_minimum" "$expected_maximum" "$expected_length" || {
        printf 'Native geometry mismatch during %s: expected=%s|%s|%s|%s|%s actual=%s\n' \
            "$phase" "$expected_thickness" "$expected_mode" "$expected_minimum" \
            "$expected_maximum" "$expected_length" "${actual_snapshot:-unavailable}" >&2
        exit 1
    }
}

wait_for_native_panel_geometry() {
    local containment_id="$1"
    local expected_thickness="$2"
    local expected_mode="$3"
    local expected_minimum="$4"
    local expected_maximum="$5"
    local expected_length="$6"
    local phase="$7"
    local attempt
    local actual_snapshot=''
    for ((attempt = 0; attempt < 100; ++attempt)); do
        actual_snapshot="$(native_panel_geometry_snapshot "$containment_id" 2>/dev/null || true)"
        native_panel_geometry_matches \
            "$actual_snapshot" "$expected_thickness" "$expected_mode" \
            "$expected_minimum" "$expected_maximum" "$expected_length" && return
        sleep 0.1
    done
    printf 'Timed out waiting for native geometry during %s: expected=%s|%s|%s|%s|%s actual=%s\n' \
        "$phase" "$expected_thickness" "$expected_mode" "$expected_minimum" \
        "$expected_maximum" "$expected_length" "${actual_snapshot:-unavailable}" >&2
    exit 1
}

expected_native_panel_offset() {
    local panel_id="$1"
    panel_registry_json | jq -er \
        --arg panel_id "$panel_id" \
        '[.[] | select(.edge == "top" or .edge == "bottom" or
                       .edge == "left" or .edge == "right")] as $panels |
         ($panels | to_entries |
          first(.[] | select(.value.id == $panel_id))) as $current |
         reduce ($panels[0:$current.key][]) as $panel
             (8;
              if ($panel.visible == true and
                  $panel.edge == $current.value.edge and
                  $panel.screen == $current.value.screen)
              then
                  . + ([0,
                        (($panel |
                          if ($current.value.edge == "left" or
                              $current.value.edge == "right")
                          then .width else .height end) // 0)] | max) + 8
              else . end)'
}

expected_native_panel_placement() {
    local panel_id="$1"
    local edge="$2"
    local plasma_alignment="$3"
    local screen_index
    screen_index="$(panel_call screenIndexForPanel "$panel_id" | gvariant_integer)"
    [[ "$screen_index" =~ ^[0-9]+$ ]] || {
        printf 'Could not resolve native placement screen for %s: %s\n' \
            "$panel_id" "${screen_index:-unavailable}" >&2
        return 1
    }
    printf '%s|%s|%s|%s\n' \
        "$edge" "$screen_index" "$plasma_alignment" \
        "$(expected_native_panel_offset "$panel_id")"
}

require_native_panel_placement() {
    local containment_id="$1"
    local panel_id="$2"
    local edge="$3"
    local plasma_alignment="$4"
    local phase="$5"
    local expected_snapshot
    expected_snapshot="$(expected_native_panel_placement \
        "$panel_id" "$edge" "$plasma_alignment")"
    local actual_snapshot
    actual_snapshot="$(native_panel_placement_snapshot "$containment_id")"
    [[ "$actual_snapshot" == "$expected_snapshot" ]] || {
        printf 'Native placement mismatch during %s: expected=%s actual=%s\n' \
            "$phase" "$expected_snapshot" "${actual_snapshot:-unavailable}" >&2
        exit 1
    }
}

wait_for_native_panel_placement() {
    local containment_id="$1"
    local panel_id="$2"
    local edge="$3"
    local plasma_alignment="$4"
    local phase="$5"
    local attempt
    local expected_snapshot=''
    local actual_snapshot=''
    for ((attempt = 0; attempt < 100; ++attempt)); do
        expected_snapshot="$(expected_native_panel_placement \
            "$panel_id" "$edge" "$plasma_alignment")"
        actual_snapshot="$(native_panel_placement_snapshot "$containment_id" 2>/dev/null || true)"
        [[ "$actual_snapshot" == "$expected_snapshot" ]] && return
        sleep 0.1
    done
    printf 'Timed out waiting for native placement during %s: expected=%s actual=%s\n' \
        "$phase" "${expected_snapshot:-unavailable}" \
        "${actual_snapshot:-unavailable}" >&2
    exit 1
}

native_widget_count() {
    local containment_id="$1"
    plasma_script "var panel = panelById($containment_id); print(panel ? panel.widgets().length : -1);" |
        gvariant_string
}

create_unrelated_panel_fixture() {
    plasma_script \
        "var result = (function() { var panel = null; try { panel = new Panel; if (!panel || panel.id < 0) { return -1; } panel.screen = 0; var widget = panel.addWidget('org.kde.plasma.digitalclock'); if (!widget) { panel.remove(); return -2; } return panel.id; } catch (error) { if (panel) { panel.remove(); } return -3; } })(); print(result);" |
        gvariant_string
}

unrelated_panel_archdock_marker() {
    local containment_id="$1"
    plasma_script \
        "var panel = panelById($containment_id); if (!panel) { print('missing'); } else { panel.currentConfigGroup = ['ArchDock']; print(String(panel.readConfig('ownerToken', '')) + '|' + String(panel.readConfig('panelId', ''))); }" |
        gvariant_string
}

unrelated_panel_snapshot() {
    local containment_id="$1"
    plasma_script \
        "var panel = panelById($containment_id); if (!panel) { print('missing'); } else { var widgets = panel.widgets(); var inventory = []; for (var index = 0; index < widgets.length; ++index) { inventory.push(String(widgets[index].id) + ':' + String(widgets[index].type)); } inventory.sort(); panel.currentConfigGroup = ['ArchDock']; var ownerToken = String(panel.readConfig('ownerToken', '')); var panelId = String(panel.readConfig('panelId', '')); var temporaryHidden = String(panel.readConfig('temporaryHidden', '')); print([String(panel.id), String(panel.location), String(panel.hiding), String(panel.screen), inventory.join(','), ownerToken, panelId, temporaryHidden].join('|')); }" |
        gvariant_string
}

require_unrelated_panel_unchanged() {
    local containment_id="$1"
    local expected_snapshot="$2"
    local phase="$3"
    local actual_snapshot
    actual_snapshot="$(unrelated_panel_snapshot "$containment_id")"
    [[ "$actual_snapshot" == "$expected_snapshot" ]] || {
        printf 'Unrelated Plasma panel changed during %s: expected=%s actual=%s\n' \
            "$phase" "$expected_snapshot" "${actual_snapshot:-unavailable}" >&2
        exit 1
    }
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
    # Qt routes messages to journald when stderr is not a console; forcing
    # stderr keeps the private shell's diagnostics in the session log tail.
    QT_FORCE_STDERR_LOGGING=1 plasmashell --no-respawn \
        >"$ARCHDOCK_TEST_LOG_DIR/$log_name" 2>&1 &
    ARCHDOCK_SESSION_PLASMASHELL_PID=$!
    gdbus wait --session --timeout=20 org.kde.plasmashell
}

arch_dock_service_pid() {
    gdbus call \
        --session \
        --dest org.freedesktop.DBus \
        --object-path /org/freedesktop/DBus \
        --method org.freedesktop.DBus.GetConnectionUnixProcessID \
        org.archdock.ArchDock |
        sed -n 's/^(uint32 \([0-9]\+\),)$/\1/p'
}

start_arch_dock() {
    local log_name="$1"
    QT_FORCE_STDERR_LOGGING=1 "$ARCHDOCK_TEST_BINARY" \
        >"$ARCHDOCK_TEST_LOG_DIR/$log_name" 2>&1 &
    local launched_pid=$!
    gdbus wait --session --timeout=20 org.archdock.ArchDock
    ARCHDOCK_SESSION_ARCH_DOCK_PID="$(arch_dock_service_pid)"
    [[ "$ARCHDOCK_SESSION_ARCH_DOCK_PID" =~ ^[0-9]+$ ]] || {
        printf 'Could not resolve the Arch Dock D-Bus service process.\n' >&2
        stop_process "$launched_pid"
        return 1
    }
    wait_for_kwin_script_state \
        org.archdock.windowwatcher true 'starting Arch Dock'
}

start_compositor() {
    env \
        QT_FORCE_STDERR_LOGGING=1 \
        QT_LOGGING_RULES='js.debug=true' \
        XDG_CURRENT_DESKTOP=KDE \
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
    validate_lifecycle_stop_after
    trap record_session_result EXIT

    log_session_phase 'starting private KWin'
    start_compositor
    log_session_phase 'starting PlasmaShell'
    start_plasmashell plasmashell.log

    local unrelated_containment_id
    unrelated_containment_id="$(create_unrelated_panel_fixture)"
    [[ "$unrelated_containment_id" =~ ^[0-9]+$ ]] || {
        printf 'Could not create the named unrelated Plasma panel fixture: %s\n' \
            "$unrelated_containment_id" >&2
        exit 1
    }
    wait_for_native_panel_screen "$unrelated_containment_id" 0
    [[ "$(unrelated_panel_archdock_marker "$unrelated_containment_id")" == '|' ]] || {
        printf 'The unrelated Plasma panel fixture unexpectedly has Arch Dock ownership.\n' >&2
        exit 1
    }
    local unrelated_snapshot
    unrelated_snapshot="$(unrelated_panel_snapshot "$unrelated_containment_id")"
    [[ "$unrelated_snapshot" == "$unrelated_containment_id|"* &&
        "$unrelated_snapshot" == *':org.kde.plasma.digitalclock|'* ]] || {
        printf 'The unrelated Plasma panel fixture snapshot is incomplete: %s\n' \
            "$unrelated_snapshot" >&2
        exit 1
    }
    log_session_phase 'created named unrelated Plasma panel fixture'

    set_private_monitor_index 1
    log_session_phase 'starting Arch Dock with the Studio route on virtual output 1'
    start_arch_dock arch-dock.log

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

    local free_count_before_studio
    free_count_before_studio="$(free_panel_count)"
    local studio_reply
    studio_reply="$(panel_call createFreePanel)"
    local studio_panel_id
    studio_panel_id="$(gvariant_map_string panelId <<<"$studio_reply")"
    local studio_containment_id
    studio_containment_id="$(gvariant_map_integer desktopContainmentId <<<"$studio_reply")"
    local studio_applet_id
    studio_applet_id="$(gvariant_map_integer dockAppletId <<<"$studio_reply")"
    [[ "$(gvariant_map_boolean success <<<"$studio_reply")" == 'true' &&
        "$(gvariant_map_boolean ownershipVerified <<<"$studio_reply")" == 'true' &&
        "$(gvariant_map_string status <<<"$studio_reply")" == 'created' &&
        "$studio_panel_id" == free-* &&
        "$studio_containment_id" =~ ^[0-9]+$ &&
        "$studio_applet_id" =~ ^[0-9]+$ &&
        "$(panel_registry_value "$studio_panel_id" screen)" == '1' ]] || {
        printf 'Panel Studio returned an invalid free-panel transaction: %s\n' \
            "$studio_reply" >&2
        exit 1
    }
    wait_for_free_panel_count "$((free_count_before_studio + 1))"
    local studio_token
    studio_token="$(require_free_panel_host \
        "$studio_panel_id" "$studio_containment_id" "$studio_applet_id" 'Studio')"
    [[ "$studio_token" == archdock-free-* ]] || {
        printf 'Panel Studio did not persist a generated ownership token.\n' >&2
        exit 1
    }
    require_unrelated_panel_unchanged \
        "$unrelated_containment_id" "$unrelated_snapshot" 'Studio free-panel creation'
    log_session_phase 'created verified Studio free-panel host'

    local free_ids_before_template
    free_ids_before_template="$(free_panel_ids_json)"
    local free_count_before_template
    free_count_before_template="$(jq -r 'length' <<<"$free_ids_before_template")"
    local panel_ids_before_template
    panel_ids_before_template="$(panel_ids)"
    [[ -r "${ARCHDOCK_FREE_TEMPLATE_SCRIPT:-}" ]] || {
        printf 'The staged free-panel template script is unavailable.\n' >&2
        exit 1
    }
    plasma_script "$(<"$ARCHDOCK_FREE_TEMPLATE_SCRIPT")" >/dev/null
    wait_for_free_panel_count "$((free_count_before_template + 1))"
    local template_panel_id
    template_panel_id="$(new_free_panel_id "$free_ids_before_template")"
    wait_for_panel_registry_value "$template_panel_id" freeHostState hosted-owned
    local template_containment_id
    template_containment_id="$(panel_registry_value \
        "$template_panel_id" freeDesktopContainmentId)"
    local template_applet_id
    template_applet_id="$(panel_registry_value "$template_panel_id" freeDockAppletId)"
    local template_token
    template_token="$(require_free_panel_host \
        "$template_panel_id" "$template_containment_id" "$template_applet_id" 'template')"
    [[ "$template_token" == archdock-free-template-* ]] || {
        printf 'The template route did not persist its verified bootstrap token.\n' >&2
        exit 1
    }
    local template_bridge_id="${template_token#archdock-free-template-}"
    template_bridge_id="${template_bridge_id%%:*}"
    [[ "$template_bridge_id" =~ ^[0-9]+$ ]] || {
        printf 'Could not recover the verified bridge ID from the template token: %s\n' \
            "$template_token" >&2
        exit 1
    }
    wait_for_panel_ids "$panel_ids_before_template"
    wait_for_free_bootstrap_artifacts_absent "$template_bridge_id" "$template_token"
    require_unrelated_panel_unchanged \
        "$unrelated_containment_id" "$unrelated_snapshot" 'template free-panel creation'
    log_session_phase 'created verified template free-panel host and removed bridge'

    local duplicate_reply
    duplicate_reply="$(panel_call createFreePanelFromTemplate \
        "$template_bridge_id" "$template_token")"
    [[ "$(gvariant_map_boolean success <<<"$duplicate_reply")" == 'true' &&
        "$(gvariant_map_boolean ownershipVerified <<<"$duplicate_reply")" == 'true' &&
        "$(gvariant_map_string status <<<"$duplicate_reply")" == 'existing' &&
        "$(gvariant_map_string panelId <<<"$duplicate_reply")" == "$template_panel_id" &&
        "$(gvariant_map_integer desktopContainmentId <<<"$duplicate_reply")" == "$template_containment_id" &&
        "$(gvariant_map_integer dockAppletId <<<"$duplicate_reply")" == "$template_applet_id" &&
        "$(free_panel_count)" == "$((free_count_before_template + 1))" &&
        "$(free_host_match_count "$template_panel_id" "$template_token")" == '1' &&
        "$(panel_ids)" == "$panel_ids_before_template" ]] || {
        printf 'Repeated template bootstrap did not converge on the existing host: %s\n' \
            "$duplicate_reply" >&2
        exit 1
    }
    require_unrelated_panel_unchanged \
        "$unrelated_containment_id" "$unrelated_snapshot" 'duplicate template bootstrap'
    log_session_phase 'reused template free-panel host without duplication'

    # TASK-0033 Phases A, C and D on a real free host: the record's content type
    # decides what the panel shows, entries are panel-specific and ordered, the
    # order and the rotation configuration are committed as revisions, free ids
    # never reach the shared application reorder, and a native panel refuses
    # panel-specific content. Persistence is re-checked after the PlasmaShell
    # restart and after a service restart later in this session.
    local content_root="$XDG_DATA_HOME/archdock-lifecycle-content"
    mkdir -p "$content_root/LifecycleFolder"
    printf '[Desktop Entry]\nType=Application\nName=Lifecycle Alpha\nIcon=applications-system\nExec=/bin/true\n' \
        >"$content_root/lifecycle-alpha.desktop"
    printf '[Desktop Entry]\nType=Application\nName=Lifecycle Beta\nIcon=applications-utilities\nExec=/bin/true\n' \
        >"$content_root/lifecycle-beta.desktop"
    local alpha_url="file://$content_root/lifecycle-alpha.desktop"
    local beta_url="file://$content_root/lifecycle-beta.desktop"
    local folder_url="file://$content_root/LifecycleFolder"
    local alpha_id="free-url:$alpha_url"
    local beta_id="free-url:$beta_url"
    local folder_id="free-url:$folder_url"
    [[ "$(panel_registry_value "$template_panel_id" type)" == 'empty' ]] || {
        printf 'A new free panel must start with the empty content type.\n' >&2
        exit 1
    }
    require_true_reply "$(panel_call addPanelEntries \
        "$template_panel_id" "['$alpha_url', '$folder_url']")"
    [[ "$(panel_registry_value "$template_panel_id" contentOrder | jq -r 'join(",")')" == "$alpha_id,$folder_id" ]] || {
        printf 'Added free entries were not persisted in order.\n' >&2
        exit 1
    }
    [[ -z "$(panel_entry_ids "$template_panel_id")" ]] || {
        printf 'An empty free panel showed entries.\n' >&2
        exit 1
    }
    apply_panel_settings "$template_panel_id" "{'type': <'launcher'>}" 'launcher content'
    wait_for_panel_entry_ids "$template_panel_id" "$alpha_id,$folder_id" 'launcher content'
    require_true_reply "$(panel_call addPanelEntries "$template_panel_id" "['$beta_url']")"
    require_true_reply "$(panel_call movePanelEntryBefore \
        "$template_panel_id" "$beta_id" "$alpha_id")"
    local lifecycle_order="$beta_id,$alpha_id,$folder_id"
    wait_for_panel_entry_ids "$template_panel_id" "$lifecycle_order" 'free reorder'
    require_false_reply "$(panel_call moveDockEntryBefore "$alpha_id" "$beta_id")"
    require_false_reply "$(panel_call setPanelEntryOrder "$template_panel_id" "['$alpha_id']")"
    require_false_reply "$(panel_call addPanelEntries bottom "['$alpha_url']")"
    require_false_reply "$(panel_call movePanelEntryBefore bottom "$alpha_id" '')"
    apply_panel_settings "$template_panel_id" "{'type': <'tasks'>}" 'tasks content'
    local attempt
    local tasks_entries=''
    for ((attempt = 0; attempt < 50; ++attempt)); do
        tasks_entries="$(panel_entry_ids "$template_panel_id")"
        [[ "$tasks_entries" != *'free-url:'* ]] && break
        sleep 0.1
    done
    [[ "$tasks_entries" != *'free-url:'* ]] || {
        printf 'A tasks free panel still showed pinned entries: %s\n' "$tasks_entries" >&2
        exit 1
    }
    apply_panel_settings "$template_panel_id" "{'type': <'hybrid'>}" 'hybrid content'
    local hybrid_entries=''
    for ((attempt = 0; attempt < 50; ++attempt)); do
        hybrid_entries="$(panel_entry_ids "$template_panel_id")"
        [[ "$hybrid_entries" == "$lifecycle_order"* ]] && break
        sleep 0.1
    done
    [[ "$hybrid_entries" == "$lifecycle_order"* ]] || {
        printf 'A hybrid free panel did not lead with its own ordered entries: %s\n' \
            "$hybrid_entries" >&2
        exit 1
    }
    apply_panel_settings "$template_panel_id" "{'type': <'launcher'>}" 'launcher content restore'
    wait_for_panel_entry_ids "$template_panel_id" "$lifecycle_order" 'launcher content restore'
    apply_panel_settings "$template_panel_id" \
        "{'layout': <'ring'>, 'panelRotationMode': <'clockwise'>, 'panelRotationSpeed': <45.0>, 'panelRotationTrigger': <'idle'>}" \
        'free rotation'
    local rotation_configuration
    rotation_configuration="$(panel_call panelRendererConfiguration "$template_panel_id")"
    [[ "$rotation_configuration" == *"'panelRotationMode': <'clockwise'>"* &&
        "$rotation_configuration" == *"'layout': <'ring'>"* &&
        "$(panel_registry_value "$template_panel_id" panelRotationMode)" == 'clockwise' ]] || {
        printf 'Free rotation configuration did not reach the renderer configuration: %s\n' \
            "$rotation_configuration" >&2
        exit 1
    }
    require_current_free_panel_host \
        "$template_panel_id" "$template_token" 'content-configured template'
    require_unrelated_panel_unchanged \
        "$unrelated_containment_id" "$unrelated_snapshot" 'free content and rotation configuration'
    log_session_phase 'applied free content semantics, panel-specific order, and rotation'

    local sentinel_host
    sentinel_host="$(create_unrelated_free_host_sentinel)"
    local sentinel_desktop_id=''
    local sentinel_applet_id=''
    IFS='|' read -r sentinel_desktop_id sentinel_applet_id <<<"$sentinel_host"
    [[ "$sentinel_desktop_id" =~ ^[0-9]+$ &&
        "$sentinel_applet_id" =~ ^[0-9]+$ ]] || {
        printf 'Could not create the unrelated free-host sentinel: %s\n' \
            "$sentinel_host" >&2
        exit 1
    }
    local sentinel_snapshot
    sentinel_snapshot="$(wait_for_free_host_snapshot_stable \
        "$sentinel_desktop_id" "$sentinel_applet_id" \
        'unrelated free-host sentinel creation')"
    [[ "$sentinel_snapshot" == "$sentinel_desktop_id|"* &&
        "$sentinel_snapshot" == *"|$sentinel_applet_id|org.archdock.dock|archdock-unrelated-sentinel|empty|archdock-unrelated-sentinel-token|false|"* ]] || {
        printf 'The unrelated free-host sentinel snapshot is incomplete: %s\n' \
            "$sentinel_snapshot" >&2
        exit 1
    }

    local free_count_before_failed_adoption
    free_count_before_failed_adoption="$(free_panel_count)"
    local failed_adoption_reply
    failed_adoption_reply="$(panel_call adoptFreePanelApplet \
        "$sentinel_desktop_id" "$sentinel_applet_id")"
    [[ "$(gvariant_map_boolean success <<<"$failed_adoption_reply")" == 'false' &&
        "$(gvariant_map_string status <<<"$failed_adoption_reply")" == 'rolled-back' &&
        "$(gvariant_map_string errorCode <<<"$failed_adoption_reply")" == 'host-adoption-failed' &&
        "$(gvariant_map_string failureStage <<<"$failed_adoption_reply")" == 'host-adoption' &&
        "$(gvariant_map_boolean rollbackAttempted <<<"$failed_adoption_reply")" == 'true' &&
        "$(gvariant_map_boolean rollbackSucceeded <<<"$failed_adoption_reply")" == 'true' &&
        "$(gvariant_map_string rollbackErrorCode <<<"$failed_adoption_reply")" == '' &&
        "$(gvariant_map_boolean recoverable <<<"$failed_adoption_reply")" == 'false' &&
        "$(gvariant_map_string panelId <<<"$failed_adoption_reply")" == '' &&
        "$(free_panel_count)" == "$free_count_before_failed_adoption" &&
        "$(free_host_snapshot "$sentinel_desktop_id" "$sentinel_applet_id")" == "$sentinel_snapshot" &&
        "$(free_host_match_count \
            archdock-unrelated-sentinel archdock-unrelated-sentinel-token)" == '1' ]] || {
        printf 'Failed adoption did not roll back only its record: reply=%s sentinel=%s\n' \
            "$failed_adoption_reply" \
            "$(free_host_snapshot "$sentinel_desktop_id" "$sentinel_applet_id" 2>/dev/null || true)" >&2
        exit 1
    }
    require_unrelated_panel_unchanged \
        "$unrelated_containment_id" "$unrelated_snapshot" 'failed free-host adoption rollback'
    log_session_phase 'rolled back failed free-host adoption without touching unrelated applets'

    local template_snapshot_before_free_restart
    template_snapshot_before_free_restart="$(free_host_snapshot \
        "$template_containment_id" "$template_applet_id")"
    local free_count_before_recovery_matrix
    free_count_before_recovery_matrix="$(free_panel_count)"
    stop_arch_dock
    set_stale_free_ids "$studio_panel_id" 999999997 999999996
    [[ "$(panel_registry_value "$studio_panel_id" freeOwnershipToken)" == "$studio_token" &&
        "$(panel_registry_value "$studio_panel_id" freeDesktopContainmentId)" == '999999997' &&
        "$(panel_registry_value "$studio_panel_id" freeDockAppletId)" == '999999996' &&
        "$(panel_registry_value "$studio_panel_id" freeHostState)" == 'hosted-stale' ]] || {
        printf 'The free-panel stale-id fixture changed more than its stored association.\n' >&2
        exit 1
    }
    start_arch_dock arch-dock-free-stale-rebind.log
    start_signal_monitor nativePanelRecoveryFinished
    wait_for_panel_registry_value \
        "$studio_panel_id" freeDesktopContainmentId "$studio_containment_id"
    wait_for_panel_registry_value \
        "$studio_panel_id" freeDockAppletId "$studio_applet_id"
    wait_for_panel_registry_value "$studio_panel_id" freeHostState hosted-owned
    local rebound_studio_token
    rebound_studio_token="$(require_free_panel_host \
        "$studio_panel_id" "$studio_containment_id" "$studio_applet_id" \
        'restarted Studio')"
    [[ "$rebound_studio_token" == "$studio_token" &&
        -z "$(panel_registry_value "$studio_panel_id" freeRecoveryError)" &&
        "$(free_panel_count)" == "$free_count_before_recovery_matrix" &&
        "$(free_host_snapshot "$template_containment_id" "$template_applet_id")" == "$template_snapshot_before_free_restart" &&
        "$(free_host_snapshot "$sentinel_desktop_id" "$sentinel_applet_id")" == "$sentinel_snapshot" ]] || {
        printf 'Free-panel startup recovery did not uniquely rebind the stale record.\n' >&2
        exit 1
    }
    require_unrelated_panel_unchanged \
        "$unrelated_containment_id" "$unrelated_snapshot" 'free-host startup recovery'
    wait_for_signal_monitor nativePanelRecoveryFinished
    log_session_phase 'rebound stale free-host ids after completed private service recovery'

    local duplicate_free_host
    duplicate_free_host="$(create_duplicate_free_host_fixture \
        "$template_containment_id" "$template_panel_id" "$template_token")"
    local duplicate_free_desktop_id=''
    local duplicate_free_applet_id=''
    IFS='|' read -r duplicate_free_desktop_id duplicate_free_applet_id \
        <<<"$duplicate_free_host"
    [[ "$duplicate_free_desktop_id" == "$template_containment_id" &&
        "$duplicate_free_applet_id" =~ ^[0-9]+$ ]] || {
        printf 'Could not create the duplicate free-host conflict fixture: %s\n' \
            "$duplicate_free_host" >&2
        exit 1
    }
    wait_for_free_host_match_count "$template_panel_id" "$template_token" 2
    local duplicate_free_snapshot
    duplicate_free_snapshot="$(free_host_snapshot \
        "$duplicate_free_desktop_id" "$duplicate_free_applet_id")"

    stop_arch_dock
    start_arch_dock arch-dock-free-conflict.log
    start_signal_monitor nativePanelRecoveryFinished
    wait_for_panel_registry_value "$template_panel_id" freeRecoveryError owned-host-conflict
    wait_for_signal_monitor nativePanelRecoveryFinished
    [[ "$(panel_registry_value "$template_panel_id" freeHostState)" == 'hosted-stale' &&
        "$(panel_registry_value "$template_panel_id" freeDesktopContainmentId)" == "$template_containment_id" &&
        "$(panel_registry_value "$template_panel_id" freeDockAppletId)" == "$template_applet_id" &&
        "$(panel_registry_value "$template_panel_id" freeOwnershipToken)" == "$template_token" &&
        "$(free_host_match_count "$template_panel_id" "$template_token")" == '2' &&
        "$(free_host_snapshot "$template_containment_id" "$template_applet_id")" == "$template_snapshot_before_free_restart" &&
        "$(free_host_snapshot "$duplicate_free_desktop_id" "$duplicate_free_applet_id")" == "$duplicate_free_snapshot" &&
        "$(free_panel_count)" == "$free_count_before_recovery_matrix" &&
        "$(free_host_snapshot "$sentinel_desktop_id" "$sentinel_applet_id")" == "$sentinel_snapshot" ]] || {
        printf 'Multiple free-host token matches were not preserved as a non-mutating conflict.\n' >&2
        exit 1
    }
    require_unrelated_panel_unchanged \
        "$unrelated_containment_id" "$unrelated_snapshot" 'multiple free-host match conflict'

    [[ "$(remove_owned_free_host_fixture \
        "$duplicate_free_desktop_id" "$duplicate_free_applet_id" \
        "$template_panel_id" "$template_token")" == '1' ]] || {
        printf 'Could not remove the exact duplicate free-host conflict fixture.\n' >&2
        exit 1
    }
    wait_for_free_host_absent "$duplicate_free_desktop_id" "$duplicate_free_applet_id"
    wait_for_free_host_match_count "$template_panel_id" "$template_token" 1

    stop_arch_dock
    start_arch_dock arch-dock-free-conflict-converged.log
    start_signal_monitor nativePanelRecoveryFinished
    wait_for_panel_registry_value "$template_panel_id" freeHostState hosted-owned
    wait_for_panel_registry_value "$template_panel_id" freeRecoveryError ''
    wait_for_signal_monitor nativePanelRecoveryFinished
    [[ "$(require_free_panel_host \
        "$template_panel_id" "$template_containment_id" "$template_applet_id" \
        'conflict-converged template')" == "$template_token" &&
        "$(free_panel_count)" == "$free_count_before_recovery_matrix" &&
        "$(free_host_snapshot "$sentinel_desktop_id" "$sentinel_applet_id")" == "$sentinel_snapshot" ]] || {
        printf 'Free-host recovery did not converge from two token matches to one.\n' >&2
        exit 1
    }
    require_unrelated_panel_unchanged \
        "$unrelated_containment_id" "$unrelated_snapshot" 'free-host conflict convergence'
    log_session_phase 'preserved two free-host matches as conflict and converged to one'

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
    require_unrelated_panel_unchanged \
        "$unrelated_containment_id" "$unrelated_snapshot" 'cold missing-id recovery'
    log_session_phase 'recovered visible panel from missing ids'

    local baseline_ids
    baseline_ids="$(panel_ids)"

    start_signal_monitor nativePanelRecoveryFinished
    local panel_id
    panel_id="$(panel_call createNativePanel bottom hybrid | gvariant_string)"
    [[ -n "$panel_id" ]] || {
        printf 'Could not create an Arch Dock test panel: placement=%s\n' \
            "$(panel_call nativePanelPlacementStatus panel-1)" >&2
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
    require_native_panel_placement \
        "$first_containment_id" "$panel_id" bottom center 'managed panel creation'
    require_native_panel_geometry \
        "$first_containment_id" 76 custom 720 720 '*' 'managed panel creation'

    local transaction_revision
    transaction_revision="$(panel_registry_value "$panel_id" settingsRevision)"
    local transaction_reply
    transaction_reply="$(panel_call applyPanelSettingsTransaction \
        "$panel_id" "uint64 $transaction_revision" \
        "{'opacity': <0.61>}" "{'magnification': <1.77>}")"
    require_structured_transaction_reply \
        "$transaction_reply" succeeded true '' false 0
    local successful_transaction_revision
    successful_transaction_revision="$(panel_registry_value \
        "$panel_id" settingsRevision)"
    [[ "$successful_transaction_revision" == "$((transaction_revision + 1))" &&
        "$(panel_registry_value "$panel_id" opacity)" == '0.61' ]] || {
        printf 'Successful settings transaction did not persist one complete revision: %s\n' \
            "$transaction_reply" >&2
        exit 1
    }

    local validation_snapshot
    validation_snapshot="$(panel_registry_record_snapshot "$panel_id")"
    local validation_reply
    validation_reply="$(panel_call applyPanelSettingsTransaction \
        "$panel_id" "uint64 $successful_transaction_revision" \
        "{'id': <'not-the-owned-panel'>}" '{}')"
    require_structured_transaction_reply \
        "$validation_reply" validation-failed false protected-panel-field false 0
    [[ "$(panel_registry_record_snapshot "$panel_id")" == "$validation_snapshot" ]] || {
        printf 'Validation failure changed the persisted panel record.\n' >&2
        exit 1
    }

    local stale_reply
    stale_reply="$(panel_call applyPanelSettingsTransaction \
        "$panel_id" "uint64 $transaction_revision" \
        "{'opacity': <0.22>}" '{}')"
    require_structured_transaction_reply \
        "$stale_reply" revision-conflict false stale-revision false 0
    [[ "$(panel_registry_record_snapshot "$panel_id")" == "$validation_snapshot" ]] || {
        printf 'Stale settings draft overwrote the current panel record.\n' >&2
        exit 1
    }

    local host_failure_before
    host_failure_before="$(panel_registry_record_snapshot "$panel_id")"
    local host_failure_reply
    host_failure_reply="$(panel_call applyPanelSettingsTransaction \
        "$panel_id" "uint64 $successful_transaction_revision" \
        "{'floatingMargin': <12>}" '{}')"
    require_structured_transaction_reply \
        "$host_failure_reply" host-failed false required-host-apply-failed true \
        "$((successful_transaction_revision + 2))"
    local host_failure_after
    host_failure_after="$(panel_registry_record_snapshot "$panel_id")"
    local rollback_revision
    rollback_revision="$(panel_registry_value "$panel_id" settingsRevision)"
    [[ "$rollback_revision" == "$((successful_transaction_revision + 2))" &&
        "$(jq -cS 'del(.settingsRevision)' <<<"$host_failure_after")" == \
            "$(jq -cS 'del(.settingsRevision)' <<<"$host_failure_before")" &&
        "$host_failure_reply" == *"'rolledBack': <true>"* ]] || {
        printf 'Required-host failure did not restore the complete prior settings: %s\n' \
            "$host_failure_reply" >&2
        exit 1
    }
    require_native_panel_placement \
        "$first_containment_id" "$panel_id" bottom center \
        'transaction host-failure rollback'
    require_native_panel_geometry \
        "$first_containment_id" 76 custom 720 720 '*' \
        'transaction host-failure rollback'
    require_unrelated_panel_unchanged \
        "$unrelated_containment_id" "$unrelated_snapshot" \
        'settings transaction matrix'
    log_session_phase \
        'verified settings success validation conflict and host rollback transactions'

    if [[ "${ARCHDOCK_LIFECYCLE_STOP_AFTER:-}" == 'transaction' ]]; then
        return 0
    fi

    local first_dock_id
    first_dock_id="$(owned_dock_id "$first_containment_id" "$panel_id")"
    local ids_before_hide
    ids_before_hide="$(panel_ids)"
    require_unrelated_panel_unchanged \
        "$unrelated_containment_id" "$unrelated_snapshot" 'managed panel creation'

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
    require_unrelated_panel_unchanged \
        "$unrelated_containment_id" "$unrelated_snapshot" 'temporary hide'

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
    require_unrelated_panel_unchanged \
        "$unrelated_containment_id" "$unrelated_snapshot" 'show after temporary hide'

    local visibility_screen_index
    visibility_screen_index="$(panel_call screenIndexForPanel "$panel_id" | gvariant_integer)"
    [[ "$visibility_screen_index" =~ ^[0-9]+$ ]] || {
        printf 'Could not resolve the visibility fixture output: %s\n' \
            "${visibility_screen_index:-unavailable}" >&2
        exit 1
    }
    start_visibility_window "$visibility_screen_index"
    wait_for_visibility_fixture_state normal

    local visibility_reply
    visibility_reply="$(panel_call applyNativePanelVisibilityMode \
        "$panel_id" always)"
    require_structured_visibility_reply \
        "$visibility_reply" applied true '' always none
    wait_for_native_panel_presentation \
        "$first_containment_id" 'none|0' 'always-visible mode'
    local visibility_panel_window_id
    visibility_panel_window_id="$(identify_visibility_panel_window \
        720 "$visibility_screen_index")"
    [[ -n "$visibility_panel_window_id" ]] || {
        printf 'Could not identify the owned panel surface for visibility checks.\n' >&2
        exit 1
    }
    wait_for_visibility_panel_hidden \
        "$visibility_panel_window_id" false 'always-visible mode'
    require_unrelated_panel_unchanged \
        "$unrelated_containment_id" "$unrelated_snapshot" 'always-visible mode'

    visibility_reply="$(panel_call applyNativePanelVisibilityMode \
        "$panel_id" auto-hide)"
    require_structured_visibility_reply \
        "$visibility_reply" applied true '' auto-hide autohide
    wait_for_native_panel_presentation \
        "$first_containment_id" 'autohide|0' 'auto-hide mode'
    wait_for_visibility_panel_hidden \
        "$visibility_panel_window_id" true 'auto-hide mode'
    require_unrelated_panel_unchanged \
        "$unrelated_containment_id" "$unrelated_snapshot" 'auto-hide mode'

    command_visibility_window 1 normal
    wait_for_visibility_fixture_state normal
    visibility_reply="$(panel_call applyNativePanelVisibilityMode \
        "$panel_id" dodge)"
    require_structured_visibility_reply \
        "$visibility_reply" applied true '' dodge dodgewindows
    wait_for_native_panel_presentation \
        "$first_containment_id" 'dodgewindows|0' 'native dodge mode'
    wait_for_visibility_panel_hidden \
        "$visibility_panel_window_id" false 'native dodge without overlap'
    command_visibility_window 2 overlap
    wait_for_visibility_fixture_state overlap
    wait_for_visibility_panel_hidden \
        "$visibility_panel_window_id" true 'native dodge with overlap'
    command_visibility_window 3 normal
    wait_for_visibility_fixture_state normal
    wait_for_visibility_panel_hidden \
        "$visibility_panel_window_id" false 'native dodge after overlap'
    require_unrelated_panel_unchanged \
        "$unrelated_containment_id" "$unrelated_snapshot" 'native dodge mode'

    visibility_reply="$(panel_call applyNativePanelVisibilityMode \
        "$panel_id" cover)"
    require_structured_visibility_reply \
        "$visibility_reply" applied true '' cover none
    wait_for_native_panel_presentation \
        "$first_containment_id" 'none|0' 'cover normal window'
    wait_for_visibility_panel_hidden \
        "$visibility_panel_window_id" false 'cover normal window'

    command_visibility_window 4 maximized
    wait_for_visibility_fixture_state maximized
    wait_for_native_panel_presentation \
        "$first_containment_id" 'autohide|0' 'cover maximized window'
    wait_for_visibility_panel_hidden \
        "$visibility_panel_window_id" true 'cover maximized window'
    require_structured_visibility_reply \
        "$(panel_call nativePanelVisibilityStatus "$panel_id")" \
        applied true '' cover autohide

    command_visibility_window 5 normal
    wait_for_visibility_fixture_state normal
    wait_for_native_panel_presentation \
        "$first_containment_id" 'none|0' 'cover restored window'
    wait_for_visibility_panel_hidden \
        "$visibility_panel_window_id" false 'cover restored window'

    command_visibility_window 6 fullscreen
    wait_for_visibility_fixture_state fullscreen
    wait_for_native_panel_presentation \
        "$first_containment_id" 'autohide|0' 'cover fullscreen window'
    wait_for_visibility_panel_hidden \
        "$visibility_panel_window_id" true 'cover fullscreen window'
    require_structured_visibility_reply \
        "$(panel_call nativePanelVisibilityStatus "$panel_id")" \
        applied true '' cover autohide

    command_visibility_window 7 normal
    wait_for_visibility_fixture_state normal
    wait_for_native_panel_presentation \
        "$first_containment_id" 'none|0' 'cover after fullscreen'
    wait_for_visibility_panel_hidden \
        "$visibility_panel_window_id" false 'cover after fullscreen'
    require_unrelated_panel_unchanged \
        "$unrelated_containment_id" "$unrelated_snapshot" \
        'maximized and fullscreen cover mode'

    require_true_reply "$(panel_call setPanelVisible "$panel_id" false)"
    wait_for_native_panel_presentation \
        "$first_containment_id" 'autohide|1' 'manual hide in cover mode'
    wait_for_visibility_panel_hidden \
        "$visibility_panel_window_id" true 'manual hide in cover mode'
    require_true_reply "$(panel_call setPanelVisible "$panel_id" true)"
    wait_for_native_panel_presentation \
        "$first_containment_id" 'none|0' 'manual restore in cover mode'
    wait_for_visibility_panel_hidden \
        "$visibility_panel_window_id" false 'manual restore in cover mode'

    visibility_reply="$(panel_call applyNativePanelVisibilityMode \
        "$panel_id" always)"
    require_structured_visibility_reply \
        "$visibility_reply" applied true '' always none
    command_visibility_window 8 quit
    stop_process "$ARCHDOCK_VISIBILITY_WINDOW_PID"
    ARCHDOCK_VISIBILITY_WINDOW_PID=''

    require_native_panel_geometry \
        "$first_containment_id" 76 custom 720 720 '*' \
        'visibility geometry preservation'
    [[ "$(owned_panel_record "$panel_id")" == "$first_record" &&
        "$(owned_dock_id "$first_containment_id" "$panel_id")" == "$first_dock_id" &&
        "$(panel_ids)" == "$ids_before_hide" ]] || {
        printf 'Visibility modes changed native panel identity.\n' >&2
        exit 1
    }
    require_unrelated_panel_unchanged \
        "$unrelated_containment_id" "$unrelated_snapshot" \
        'visibility mode restoration'
    log_session_phase \
        'verified always, auto-hide, dodge, maximized/fullscreen cover, and manual restore'

    local launcher_type_reply
    launcher_type_reply="$(panel_call setNativePanelType "$panel_id" launcher)"
    [[ "$launcher_type_reply" == '(true,)' ]] || {
        printf 'Could not reconcile the launcher renderer: reply=%s placement=%s\n' \
            "$launcher_type_reply" \
            "$(panel_call nativePanelPlacementStatus "$panel_id")" >&2
        exit 1
    }
    require_visual_dock "$first_containment_id" "$panel_id" launcher
    require_unrelated_panel_unchanged \
        "$unrelated_containment_id" "$unrelated_snapshot" 'launcher renderer reconciliation'
    require_true_reply "$(panel_call setNativePanelType "$panel_id" hybrid)"
    require_visual_dock "$first_containment_id" "$panel_id" hybrid
    require_unrelated_panel_unchanged \
        "$unrelated_containment_id" "$unrelated_snapshot" 'hybrid renderer reconciliation'

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
    require_unrelated_panel_unchanged \
        "$unrelated_containment_id" "$unrelated_snapshot" 'legacy renderer cleanup'

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
    require_native_panel_geometry \
        "$empty_containment_id" 76 custom 720 720 '*' 'top empty panel creation'
    require_unrelated_panel_unchanged \
        "$unrelated_containment_id" "$unrelated_snapshot" 'empty panel creation'
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
    require_unrelated_panel_unchanged \
        "$unrelated_containment_id" "$unrelated_snapshot" 'empty renderer reconciliation'
    require_true_reply "$(panel_call removeNativeKdePanel "$empty_panel_id")"
    panel_call removePanel "$empty_panel_id" >/dev/null
    require_unrelated_panel_unchanged \
        "$unrelated_containment_id" "$unrelated_snapshot" 'empty panel permanent removal'

    local typed_edge
    local typed_type
    for typed_edge in top left right; do
        if [[ "$typed_edge" == top ]]; then
            typed_type=hybrid
        elif [[ "$typed_edge" == left ]]; then
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
        require_native_panel_geometry \
            "$typed_containment_id" 76 fit 48 4096 '*' "$typed_edge fit panel creation"
        require_unrelated_panel_unchanged \
            "$unrelated_containment_id" "$unrelated_snapshot" "$typed_type panel creation"
        require_true_reply "$(panel_call removeNativeKdePanel "$typed_panel_id")"
        panel_call removePanel "$typed_panel_id" >/dev/null
        require_unrelated_panel_unchanged \
            "$unrelated_containment_id" "$unrelated_snapshot" "$typed_type panel permanent removal"
    done

    stop_arch_dock
    set_panel_registry_placement "$panel_id" left start
    set_panel_registry_geometry "$panel_id" false 92 640
    [[ "$(panel_registry_value "$panel_id" edge)" == 'left' &&
        "$(panel_registry_value "$panel_id" alignment)" == 'start' &&
        "$(panel_registry_value "$panel_id" dynamic)" == 'false' &&
        "$(panel_registry_value "$panel_id" width)" == '92' &&
        "$(panel_registry_value "$panel_id" height)" == '640' ]] || {
        printf 'Could not establish the vertical-start geometry fixture.\n' >&2
        exit 1
    }
    start_arch_dock arch-dock-placement-recovery.log
    wait_for_native_panel_placement \
        "$first_containment_id" "$panel_id" left right 'vertical-start recovery'
    wait_for_native_panel_geometry \
        "$first_containment_id" 92 custom 640 640 '*' 'vertical fixed geometry recovery'
    [[ "$(owned_panel_record "$panel_id")" == "$first_record" &&
        "$(owned_dock_id "$first_containment_id" "$panel_id")" == "$first_dock_id" ]] || {
        printf 'Vertical-start recovery changed the owned native panel identity.\n' >&2
        exit 1
    }
    require_unrelated_panel_unchanged \
        "$unrelated_containment_id" "$unrelated_snapshot" 'vertical-start recovery'
    log_session_phase 'recovered vertical start and fixed geometry through the adapter'

    stop_arch_dock
    set_panel_registry_geometry "$panel_id" false 104 680 8
    start_arch_dock arch-dock-unsupported-floating.log
    require_false_reply "$(panel_call createNativeKdePanel "$panel_id")"
    require_native_panel_geometry \
        "$first_containment_id" 92 custom 640 640 '*' 'unsupported floating refusal'
    [[ "$(owned_panel_record "$panel_id")" == "$first_record" &&
        "$(owned_dock_id "$first_containment_id" "$panel_id")" == "$first_dock_id" ]] || {
        printf 'Unsupported floating request changed the owned native panel identity.\n' >&2
        exit 1
    }
    require_unrelated_panel_unchanged \
        "$unrelated_containment_id" "$unrelated_snapshot" 'unsupported floating refusal'
    log_session_phase 'rejected unsupported numeric floating margin without host mutation'

    stop_arch_dock
    set_panel_registry_geometry "$panel_id" false 104 680
    start_arch_dock arch-dock-supported-geometry.log
    wait_for_native_panel_geometry \
        "$first_containment_id" 104 custom 680 680 '*' 'supported geometry after refusal'
    [[ "$(owned_panel_record "$panel_id")" == "$first_record" &&
        "$(owned_dock_id "$first_containment_id" "$panel_id")" == "$first_dock_id" ]] || {
        printf 'Supported geometry update changed the owned native panel identity.\n' >&2
        exit 1
    }
    require_unrelated_panel_unchanged \
        "$unrelated_containment_id" "$unrelated_snapshot" 'supported geometry after refusal'
    log_session_phase 'applied later thickness and fixed length on the same native host'

    local structured_apply_reply
    structured_apply_reply="$(panel_call applyNativePanelPlacementDraft \
        "$panel_id" \
        "{'alignment': <'end'>, 'dynamic': <false>, 'width': <int32 112>, 'height': <int32 700>}")"
    require_structured_placement_reply \
        "$structured_apply_reply" applied true ''
    [[ "$(gvariant_map_boolean ownershipVerified <<<"$structured_apply_reply")" == 'true' &&
        "$(gvariant_map_boolean rollbackAttempted <<<"$structured_apply_reply")" == 'false' &&
        "$(panel_registry_value "$panel_id" alignment)" == 'end' &&
        "$(panel_registry_value "$panel_id" dynamic)" == 'false' &&
        "$(panel_registry_value "$panel_id" width)" == '112' &&
        "$(panel_registry_value "$panel_id" height)" == '700' ]] || {
        printf 'Structured native placement did not persist the verified intent: %s\n' \
            "$structured_apply_reply" >&2
        exit 1
    }
    require_native_panel_placement \
        "$first_containment_id" "$panel_id" left left 'structured placement success'
    require_native_panel_geometry \
        "$first_containment_id" 112 custom 700 700 '*' 'structured placement success'
    require_structured_placement_reply \
        "$(panel_call nativePanelPlacementStatus "$panel_id")" applied true ''
    require_unrelated_panel_unchanged \
        "$unrelated_containment_id" "$unrelated_snapshot" 'structured placement success'
    log_session_phase 'reported saved intent and verified Plasma host state separately'

    local structured_saved_registry
    structured_saved_registry="$(panel_registry_json)"
    local structured_unsupported_reply
    structured_unsupported_reply="$(panel_call applyNativePanelPlacementDraft \
        "$panel_id" "{'floatingMargin': <int32 8>}")"
    require_structured_placement_reply \
        "$structured_unsupported_reply" unsupported false \
        unsupported-capability-unavailable
    [[ "$(panel_registry_json)" == "$structured_saved_registry" &&
        "$structured_unsupported_reply" == *"'field': <'floatingMargin'>"* &&
        "$structured_unsupported_reply" == *"'requested': <'8'>"* ]] || {
        printf 'Unsupported structured placement changed saved intent or hid its field: %s\n' \
            "$structured_unsupported_reply" >&2
        exit 1
    }
    require_native_panel_placement \
        "$first_containment_id" "$panel_id" left left 'structured unsupported refusal'
    require_native_panel_geometry \
        "$first_containment_id" 112 custom 700 700 '*' 'structured unsupported refusal'
    require_unrelated_panel_unchanged \
        "$unrelated_containment_id" "$unrelated_snapshot" 'structured unsupported refusal'
    log_session_phase 'reported unsupported floating margin without changing saved or host state'

    local settings_file
    settings_file="$(arch_dock_settings_file)"
    local settings_directory
    settings_directory="$(dirname -- "$settings_file")"
    [[ -f "$settings_file" && -d "$settings_directory" ]] || {
        printf 'Could not locate the private settings file for rollback coverage.\n' >&2
        exit 1
    }

    local structured_saved_revision
    structured_saved_revision="$(panel_property dockRevision)"
    local structured_saved_hash
    structured_saved_hash="$(sha256sum "$settings_file" | awk '{ print $1 }')"
    local structured_saved_file_state
    structured_saved_file_state="$(stat -c '%i|%s|%Y|%y' "$settings_file")"

    ARCHDOCK_SETTINGS_FIXTURE_FILE="$settings_file"
    ARCHDOCK_SETTINGS_FIXTURE_DIRECTORY="$settings_directory"
    ARCHDOCK_SETTINGS_FIXTURE_FILE_MODE="$(stat -c '%a' "$settings_file")"
    ARCHDOCK_SETTINGS_FIXTURE_DIRECTORY_MODE="$(stat -c '%a' "$settings_directory")"
    chmod 400 "$settings_file"
    chmod 500 "$settings_directory"
    [[ ! -w "$settings_file" && ! -w "$settings_directory" ]] || {
        printf 'Private settings rollback fixture remained writable.\n' >&2
        exit 1
    }
    local structured_rollback_reply
    local structured_rollback_call_status
    set +e
    structured_rollback_reply="$(panel_call applyNativePanelPlacementDraft \
        "$panel_id" \
        "{'alignment': <'start'>}" 2>&1)"
    structured_rollback_call_status=$?
    set -e

    local structured_after_hash
    structured_after_hash="$(sha256sum "$settings_file" | awk '{ print $1 }')"
    local structured_after_file_state
    structured_after_file_state="$(stat -c '%i|%s|%Y|%y' "$settings_file")"
    local structured_after_revision
    structured_after_revision="$(panel_property dockRevision)"
    restore_settings_fixture_permissions
    [[ "$structured_rollback_call_status" == '0' ]] || {
        printf 'Structured rollback call failed before returning its result: %s\n' \
            "$structured_rollback_reply" >&2
        exit 1
    }
    require_structured_placement_reply \
        "$structured_rollback_reply" rolled-back false persistence-failed
    [[ "$(gvariant_map_boolean rollbackAttempted <<<"$structured_rollback_reply")" == 'true' &&
        "$(gvariant_map_boolean rollbackSucceeded <<<"$structured_rollback_reply")" == 'true' &&
        "$(gvariant_nested_map_string requested height <<<"$structured_rollback_reply")" == '112' &&
        "$(gvariant_nested_map_string requested fixedLength <<<"$structured_rollback_reply")" == '700' &&
        "$(gvariant_nested_map_string applied height <<<"$structured_rollback_reply")" == '112' &&
        "$(gvariant_nested_map_string applied fixedLength <<<"$structured_rollback_reply")" == '700' &&
        "$(gvariant_nested_map_string hostState height <<<"$structured_rollback_reply")" == '112' &&
        "$(gvariant_nested_map_string hostState fixedLength <<<"$structured_rollback_reply")" == '700' &&
        "$(panel_registry_json)" == "$structured_saved_registry" &&
        "$structured_after_hash" == "$structured_saved_hash" &&
        "$structured_after_file_state" == "$structured_saved_file_state" &&
        "$structured_after_revision" == "$structured_saved_revision" ]] || {
        printf 'Failed persistence did not preserve the prior saved revision: %s\n' \
            "$structured_rollback_reply" >&2
        exit 1
    }
    require_native_panel_placement \
        "$first_containment_id" "$panel_id" left left 'structured persistence rollback'
    require_native_panel_geometry \
        "$first_containment_id" 112 custom 700 700 '*' 'structured persistence rollback'
    require_unrelated_panel_unchanged \
        "$unrelated_containment_id" "$unrelated_snapshot" 'structured persistence rollback'
    log_session_phase 'rolled Plasma host back after checked persistence failure'

    panel_call setPanelScreen "$panel_id" 1 >/dev/null
    wait_for_native_panel_placement \
        "$first_containment_id" "$panel_id" left left 'managed screen reassignment'
    [[ "$(panel_call screenIndexForPanel "$panel_id" | gvariant_integer)" == '1' ]] || {
        printf 'Native panel did not move to virtual output 1.\n' >&2
        exit 1
    }
    local requested_screen_id
    requested_screen_id="$(panel_registry_value "$panel_id" screenId)"
    [[ -n "$requested_screen_id" ]] || {
        printf 'Native panel did not persist a stable output identity.\n' >&2
        exit 1
    }
    require_unrelated_panel_unchanged \
        "$unrelated_containment_id" "$unrelated_snapshot" 'managed screen reassignment'

    local enabled_outputs
    enabled_outputs="$(enabled_output_names)"
    local secondary_output
    secondary_output="$(printf '%s\n' "$enabled_outputs" | sed -n '2p')"
    [[ -n "$secondary_output" ]] || {
        printf 'Could not identify the secondary virtual output.\n' >&2
        exit 1
    }

    local free_count_before_screen_change
    free_count_before_screen_change="$(free_panel_count)"
    start_signal_monitor nativePanelRecoveryFinished
    log_session_phase 'disabling secondary virtual output'
    kscreen-doctor "output.$secondary_output.disable" >/dev/null
    wait_for_screen_count 1
    screen_count="$(available_screen_count)"
    [[ "$screen_count" == '1' ]] || {
        printf 'Disabling the secondary virtual output did not remove it.\n' >&2
        exit 1
    }
    wait_for_native_panel_screen "$first_containment_id" 0
    wait_for_signal_monitor nativePanelRecoveryFinished
    require_native_panel_placement \
        "$first_containment_id" "$panel_id" left left 'managed screen fallback'
    [[ "$(panel_registry_value "$panel_id" screen)" == '0' &&
        "$(panel_registry_value "$panel_id" screenId)" == "$requested_screen_id" ]] || {
        printf 'Screen fallback did not record index 0 while retaining the stable output identity.\n' >&2
        exit 1
    }
    wait_for_panel_registry_value "$studio_panel_id" freeHostState hosted-owned
    wait_for_panel_registry_value "$template_panel_id" freeHostState hosted-owned
    require_current_free_panel_host \
        "$studio_panel_id" "$studio_token" 'output-loss Studio' true
    require_current_free_panel_host \
        "$template_panel_id" "$template_token" 'screen-fallback template'
    [[ "$(free_panel_count)" == "$free_count_before_screen_change" &&
        "$(free_host_match_count "$studio_panel_id" "$studio_token")" == '1' &&
        "$(free_host_match_count "$template_panel_id" "$template_token")" == '1' &&
        "$(free_host_snapshot "$sentinel_desktop_id" "$sentinel_applet_id")" == "$sentinel_snapshot" ]] || {
        printf 'Free-host screen fallback duplicated, detached, or changed a sentinel applet.\n' >&2
        exit 1
    }
    require_unrelated_panel_unchanged \
        "$unrelated_containment_id" "$unrelated_snapshot" 'managed screen fallback'
    start_signal_monitor nativePanelRecoveryFinished
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
    wait_for_signal_monitor nativePanelRecoveryFinished
    require_native_panel_placement \
        "$first_containment_id" "$panel_id" left left 'managed screen restoration'
    [[ "$(panel_registry_value "$panel_id" screenId)" == "$requested_screen_id" ]] || {
        printf 'Restoring the output replaced the requested stable output identity.\n' >&2
        exit 1
    }
    wait_for_panel_registry_value "$studio_panel_id" freeHostState hosted-owned
    wait_for_panel_registry_value "$template_panel_id" freeHostState hosted-owned
    require_current_free_panel_host \
        "$studio_panel_id" "$studio_token" 'screen-restored Studio'
    require_current_free_panel_host \
        "$template_panel_id" "$template_token" 'screen-restored template'
    [[ "$(free_panel_count)" == "$free_count_before_screen_change" &&
        "$(free_host_match_count "$studio_panel_id" "$studio_token")" == '1' &&
        "$(free_host_match_count "$template_panel_id" "$template_token")" == '1' &&
        "$(free_host_snapshot "$sentinel_desktop_id" "$sentinel_applet_id")" == "$sentinel_snapshot" ]] || {
        printf 'Free-host screen restoration duplicated, detached, or changed a sentinel applet.\n' >&2
        exit 1
    }
    require_unrelated_panel_unchanged \
        "$unrelated_containment_id" "$unrelated_snapshot" 'managed screen restoration'
    log_session_phase 'preserved unique free hosts through output fallback and restoration'

    local externally_removed
    externally_removed="$(plasma_script "var panel = panelById($first_containment_id); if (panel === null) { print(0); } else { panel.currentConfigGroup = ['ArchDock']; var owned = panel.readConfig('panelId', '') === '$panel_id' && panel.readConfig('ownerToken', '') === '$first_token'; if (owned) { panel.remove(); print(1); } else { print(0); } }" | gvariant_string)"
    [[ "$externally_removed" == '1' ]] || {
        printf 'Refused to remove the verified disposable containment.\n' >&2
        exit 1
    }
    require_unrelated_panel_unchanged \
        "$unrelated_containment_id" "$unrelated_snapshot" 'managed-host loss fixture'

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
    require_native_panel_placement \
        "$replacement_containment_id" "$panel_id" left left 'missing-host recreation'
    local replacement_dock_id
    replacement_dock_id="$(owned_dock_id "$replacement_containment_id" "$panel_id")"
    local ids_after_replacement
    ids_after_replacement="$(panel_ids)"
    require_unrelated_panel_unchanged \
        "$unrelated_containment_id" "$unrelated_snapshot" 'visible missing-host recreation'

    require_true_reply "$(panel_call setPanelVisible "$panel_id" true)"
    require_true_reply "$(panel_call setPanelVisible "$panel_id" true)"
    [[ "$(owned_panel_record "$panel_id")" == "$replacement_record" &&
        "$(owned_dock_id "$replacement_containment_id" "$panel_id")" == "$replacement_dock_id" &&
        "$(panel_ids)" == "$ids_after_replacement" ]] || {
        printf 'Repeated synchronization duplicated or replaced a recovered native panel.\n' >&2
        exit 1
    }
    require_unrelated_panel_unchanged \
        "$unrelated_containment_id" "$unrelated_snapshot" 'repeated recovery convergence'
    log_session_phase 'repeated missing-host synchronization remained idempotent'

    local hidden_panel_id
    hidden_panel_id="$(panel_call createNativePanel top launcher | gvariant_string)"
    [[ -n "$hidden_panel_id" ]] || {
        printf 'Could not create a hidden-detach test panel.\n' >&2
        exit 1
    }
    local hidden_record
    hidden_record="$(owned_panel_record "$hidden_panel_id")"
    local hidden_containment_id="${hidden_record%%|*}"
    local hidden_token="${hidden_record#*|}"
    [[ "$hidden_containment_id" =~ ^[0-9]+$ && "$hidden_token" =~ ^[0-9a-f-]+$ ]] || {
        printf 'Hidden-detach panel ownership marker was not persisted: %s\n' "$hidden_record" >&2
        exit 1
    }
    require_unrelated_panel_unchanged \
        "$unrelated_containment_id" "$unrelated_snapshot" 'hidden-detach panel creation'
    require_true_reply "$(panel_call setPanelVisible "$hidden_panel_id" false)"
    require_unrelated_panel_unchanged \
        "$unrelated_containment_id" "$unrelated_snapshot" 'hidden-detach temporary hide'
    local hidden_removed
    hidden_removed="$(plasma_script "var panel = panelById($hidden_containment_id); if (!panel) { print(0); } else { panel.currentConfigGroup = ['ArchDock']; var owned = panel.readConfig('panelId', '') === '$hidden_panel_id' && panel.readConfig('ownerToken', '') === '$hidden_token'; if (owned) { panel.remove(); print(1); } else { print(0); } }" | gvariant_string)"
    [[ "$hidden_removed" == '1' ]] || {
        printf 'Could not remove the verified hidden-detach test host.\n' >&2
        exit 1
    }
    local ids_after_hidden_removal
    ids_after_hidden_removal="$(panel_ids)"
    require_true_reply "$(panel_call setPanelVisible "$hidden_panel_id" false)"
    [[ "$(panel_ids)" == "$ids_after_hidden_removal" &&
        "$(owned_panel_match_count "$hidden_panel_id" "$hidden_token")" == '0' ]] || {
        printf 'Hidden zero-match recovery created or retained an owned host.\n' >&2
        exit 1
    }
    [[ "$(panel_registry_value "$hidden_panel_id" nativePanelId)" == '-1' &&
        "$(panel_registry_value "$hidden_panel_id" nativeDockAppletId)" == '-1' &&
        -z "$(panel_registry_value "$hidden_panel_id" nativeOwnershipToken)" &&
        "$(panel_registry_value "$hidden_panel_id" nativeRecoveryState)" == 'detached' &&
        "$(panel_registry_value "$hidden_panel_id" nativeRecoveryError)" == 'owned-host-not-found' ]] || {
        printf 'Hidden zero-match recovery did not persist a safe detached state.\n' >&2
        exit 1
    }
    panel_call removePanel "$hidden_panel_id" >/dev/null
    require_unrelated_panel_unchanged \
        "$unrelated_containment_id" "$unrelated_snapshot" 'hidden missing-host detach'
    log_session_phase 'safely detached hidden panel with no owned host'

    local panel_count_before_stale_rebind
    panel_count_before_stale_rebind="$(panel_count)"
    stop_process "$ARCHDOCK_SESSION_PLASMASHELL_PID"
    ARCHDOCK_SESSION_PLASMASHELL_PID=''
    stop_arch_dock
    set_stale_native_ids "$panel_id" 999999999 999999998
    [[ "$(panel_registry_value "$panel_id" nativeOwnershipToken)" == "$replacement_token" ]] || {
        printf 'Stale-id fixture changed the ownership token.\n' >&2
        exit 1
    }
    start_arch_dock arch-dock-stale-rebind.log
    start_signal_monitor nativePanelRecoveryFinished
    start_plasmashell plasmashell-stale-rebind.log
    wait_for_signal_monitor nativePanelRecoveryFinished
    wait_for_owned_panel "$panel_id"
    replacement_record="$(owned_panel_record "$panel_id")"
    replacement_containment_id="${replacement_record%%|*}"
    local rebound_token="${replacement_record#*|}"
    [[ "$replacement_containment_id" =~ ^[0-9]+$ &&
        "$rebound_token" == "$replacement_token" ]] || {
        printf 'Stale-id restart did not recover the original ownership token: %s\n' \
            "$replacement_record" >&2
        exit 1
    }
    replacement_dock_id="$(owned_dock_id "$replacement_containment_id" "$panel_id")"
    wait_for_panel_registry_value "$panel_id" nativePanelId "$replacement_containment_id"
    wait_for_panel_registry_value "$panel_id" nativeDockAppletId "$replacement_dock_id"
    ids_after_replacement="$(panel_ids)"
    [[ "$(panel_registry_value "$panel_id" nativeOwnershipToken)" == "$replacement_token" &&
        "$(panel_registry_value "$panel_id" nativeRecoveryState)" == 'ready' &&
        -z "$(panel_registry_value "$panel_id" nativeRecoveryError)" &&
        "$(owned_panel_record "$panel_id")" == "$replacement_record" &&
        "$(owned_panel_match_count "$panel_id" "$replacement_token")" == '1' &&
        "$(panel_count)" == "$panel_count_before_stale_rebind" ]] || {
        printf 'Unique ownership-token rediscovery did not rebind the stale registry exactly once.\n' >&2
        exit 1
    }
    require_unrelated_panel_unchanged \
        "$unrelated_containment_id" "$unrelated_snapshot" 'stale native id rebind'
    log_session_phase 'rebound stale native ids to one verified token match'

    local duplicate_containment_id
    duplicate_containment_id="$(plasma_script "var panel = new Panel; if (!panel || panel.id < 0) { print(-1); } else { panel.currentConfigGroup = ['ArchDock']; panel.writeConfig('ownerToken', '$replacement_token'); panel.writeConfig('panelId', '$panel_id'); panel.writeConfig('temporaryHidden', '0'); panel.reloadConfig(); print(panel.id); }" | gvariant_string)"
    [[ "$duplicate_containment_id" =~ ^[0-9]+$ ]] || {
        printf 'Could not create a duplicate ownership-token conflict fixture: %s\n' \
            "$duplicate_containment_id" >&2
        exit 1
    }
    local ids_with_conflict
    ids_with_conflict="$(panel_ids)"
    require_false_reply "$(panel_call setPanelVisible "$panel_id" true)"
    [[ "$(panel_ids)" == "$ids_with_conflict" &&
        "$(owned_panel_match_count "$panel_id" "$replacement_token")" == '2' &&
        "$(panel_registry_value "$panel_id" nativePanelId)" == "$replacement_containment_id" &&
        "$(panel_registry_value "$panel_id" nativeDockAppletId)" == "$replacement_dock_id" &&
        "$(panel_registry_value "$panel_id" nativeOwnershipToken)" == "$replacement_token" &&
        "$(panel_registry_value "$panel_id" nativeRecoveryState)" == 'conflict' &&
        "$(panel_registry_value "$panel_id" nativeRecoveryError)" == 'multiple-owned-hosts' ]] || {
        printf 'Multiple ownership matches were not preserved as a non-mutating conflict.\n' >&2
        exit 1
    }
    require_unrelated_panel_unchanged \
        "$unrelated_containment_id" "$unrelated_snapshot" 'multiple ownership conflict refusal'
    local duplicate_removed
    duplicate_removed="$(plasma_script "var panel = panelById($duplicate_containment_id); if (!panel) { print(0); } else { panel.currentConfigGroup = ['ArchDock']; var owned = panel.readConfig('panelId', '') === '$panel_id' && panel.readConfig('ownerToken', '') === '$replacement_token'; if (owned) { panel.remove(); print(1); } else { print(0); } }" | gvariant_string)"
    [[ "$duplicate_removed" == '1' ]] || {
        printf 'Could not remove the exact duplicate conflict fixture.\n' >&2
        exit 1
    }
    require_true_reply "$(panel_call setPanelVisible "$panel_id" true)"
    [[ "$(panel_ids)" == "$ids_after_replacement" &&
        "$(owned_panel_match_count "$panel_id" "$replacement_token")" == '1' &&
        "$(panel_registry_value "$panel_id" nativeRecoveryState)" == 'ready' &&
        -z "$(panel_registry_value "$panel_id" nativeRecoveryError)" ]] || {
        printf 'Native panel did not converge after the duplicate conflict was removed.\n' >&2
        exit 1
    }
    require_unrelated_panel_unchanged \
        "$unrelated_containment_id" "$unrelated_snapshot" 'ownership conflict convergence'
    log_session_phase 'preserved multiple token matches as an explicit conflict'

    local panel_count_before_restart
    panel_count_before_restart="$(panel_count)"
    local free_count_before_plasma_restart
    free_count_before_plasma_restart="$(free_panel_count)"
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
    local restarted_dock_id
    restarted_dock_id="$(owned_dock_id "$restarted_containment_id" "$panel_id")"
    [[ "$restarted_token" == "$replacement_token" &&
        "$(owned_panel_match_count "$panel_id" "$replacement_token")" == '1' &&
        "$(panel_count)" == "$panel_count_before_restart" &&
        "$(panel_registry_value "$panel_id" nativePanelId)" == "$restarted_containment_id" &&
        "$(panel_registry_value "$panel_id" nativeDockAppletId)" == "$restarted_dock_id" &&
        "$(panel_registry_value "$panel_id" nativeOwnershipToken)" == "$replacement_token" &&
        "$(panel_registry_value "$panel_id" nativeRecoveryState)" == 'ready' ]] || {
        printf 'PlasmaShell restart duplicated the managed panel or lost its token-bound association.\n' >&2
        exit 1
    }
    local restarted_expected_screen
    restarted_expected_screen="$(panel_call screenIndexForPanel "$panel_id" | gvariant_integer)"
    [[ "$restarted_expected_screen" =~ ^[0-9]+$ ]] || {
        printf 'Could not resolve the stable virtual output after PlasmaShell restart: %s\n' \
            "$restarted_expected_screen" >&2
        exit 1
    }
    require_native_panel_placement \
        "$restarted_containment_id" "$panel_id" left left 'PlasmaShell restart'
    wait_for_panel_registry_value "$studio_panel_id" freeHostState hosted-owned
    wait_for_panel_registry_value "$template_panel_id" freeHostState hosted-owned
    require_current_free_panel_host \
        "$studio_panel_id" "$studio_token" 'PlasmaShell-restarted Studio'
    require_current_free_panel_host \
        "$template_panel_id" "$template_token" 'PlasmaShell-restarted template'
    [[ "$(free_panel_count)" == "$free_count_before_plasma_restart" &&
        "$(free_host_match_count "$studio_panel_id" "$studio_token")" == '1' &&
        "$(free_host_match_count "$template_panel_id" "$template_token")" == '1' &&
        "$(free_host_snapshot "$sentinel_desktop_id" "$sentinel_applet_id")" == "$sentinel_snapshot" &&
        "$(free_host_match_count \
            archdock-unrelated-sentinel archdock-unrelated-sentinel-token)" == '1' &&
        "$(free_bootstrap_artifact_count "$template_bridge_id" "$template_token")" == '0' ]] || {
        printf 'PlasmaShell restart duplicated or lost a managed free host or bootstrap cleanup.\n' >&2
        exit 1
    }
    require_unrelated_panel_unchanged \
        "$unrelated_containment_id" "$unrelated_snapshot" 'PlasmaShell restart recovery'
    log_session_phase 'recovered unique free hosts after a real private PlasmaShell restart'

    [[ "$(panel_entry_ids "$template_panel_id")" == "$lifecycle_order" &&
        "$(panel_registry_value "$template_panel_id" contentOrder | jq -r 'join(",")')" == "$lifecycle_order" &&
        "$(panel_registry_value "$template_panel_id" layout)" == 'ring' &&
        "$(panel_registry_value "$template_panel_id" panelRotationMode)" == 'clockwise' ]] || {
        printf 'Free content order or rotation did not survive the PlasmaShell restart.\n' >&2
        exit 1
    }
    log_session_phase 'preserved free content order and rotation through the PlasmaShell restart'

    local free_count_before_cleanup
    free_count_before_cleanup="$(free_panel_count)"
    studio_containment_id="$(panel_registry_value \
        "$studio_panel_id" freeDesktopContainmentId)"
    studio_applet_id="$(panel_registry_value "$studio_panel_id" freeDockAppletId)"
    [[ "$(remove_owned_free_host_fixture \
        "$studio_containment_id" "$studio_applet_id" \
        "$studio_panel_id" "$studio_token")" == '1' ]] || {
        printf 'Could not remove the exact Studio free-host zero-match fixture.\n' >&2
        exit 1
    }
    wait_for_free_host_absent "$studio_containment_id" "$studio_applet_id"
    wait_for_free_host_match_count "$studio_panel_id" "$studio_token" 0

    stop_arch_dock
    start_arch_dock arch-dock-free-zero-match.log
    start_signal_monitor nativePanelRecoveryFinished
    wait_for_panel_registry_value "$studio_panel_id" freeHostState detached
    wait_for_signal_monitor nativePanelRecoveryFinished
    [[ "$(panel_entry_ids "$template_panel_id")" == "$lifecycle_order" &&
        "$(panel_registry_value "$template_panel_id" panelRotationMode)" == 'clockwise' ]] || {
        printf 'Free content order or rotation did not survive a service restart.\n' >&2
        exit 1
    }
    log_session_phase 'preserved free content order and rotation through a service restart'
    [[ "$(panel_registry_value "$studio_panel_id" freeDesktopContainmentId)" == '-1' &&
        "$(panel_registry_value "$studio_panel_id" freeDockAppletId)" == '-1' &&
        -z "$(panel_registry_value "$studio_panel_id" freeOwnershipToken)" &&
        "$(panel_registry_value "$studio_panel_id" freeRecoveryError)" == 'owned-host-not-found' &&
        "$(free_panel_count)" == "$free_count_before_cleanup" &&
        "$(free_host_match_count "$studio_panel_id" "$studio_token")" == '0' &&
        "$(free_host_match_count "$template_panel_id" "$template_token")" == '1' &&
        "$(free_host_snapshot "$sentinel_desktop_id" "$sentinel_applet_id")" == "$sentinel_snapshot" ]] || {
        printf 'Zero-match free-host recovery did not persist one safe detached record.\n' >&2
        exit 1
    }
    require_current_free_panel_host \
        "$template_panel_id" "$template_token" 'zero-match-preserved template'
    require_unrelated_panel_unchanged \
        "$unrelated_containment_id" "$unrelated_snapshot" 'zero free-host match detach'

    local detached_studio_snapshot
    detached_studio_snapshot="$(panel_registry_record_snapshot "$studio_panel_id")"
    stop_arch_dock
    start_arch_dock arch-dock-free-zero-match-repeated.log
    start_signal_monitor nativePanelRecoveryFinished
    wait_for_signal_monitor nativePanelRecoveryFinished
    [[ "$(panel_registry_record_snapshot "$studio_panel_id")" == "$detached_studio_snapshot" &&
        "$(free_host_match_count "$studio_panel_id" "$studio_token")" == '0' &&
        "$(free_panel_count)" == "$free_count_before_cleanup" ]] || {
        printf 'Repeated zero-match synchronization changed the detached free-panel record.\n' >&2
        exit 1
    }
    require_current_free_panel_host \
        "$template_panel_id" "$template_token" 'repeated-zero-preserved template'

    start_signal_monitor nativePanelRecoveryFinished
    panel_call removePanel "$studio_panel_id" >/dev/null
    wait_for_panel_registry_absent "$studio_panel_id"
    wait_for_signal_monitor nativePanelRecoveryFinished
    [[ "$(free_panel_count)" == "$((free_count_before_cleanup - 1))" &&
        "$(free_host_match_count "$studio_panel_id" "$studio_token")" == '0' &&
        "$(free_host_match_count "$template_panel_id" "$template_token")" == '1' &&
        "$(free_host_snapshot "$sentinel_desktop_id" "$sentinel_applet_id")" == "$sentinel_snapshot" ]] || {
        printf 'Removing the detached free record changed a live or unrelated applet.\n' >&2
        exit 1
    }
    require_unrelated_panel_unchanged \
        "$unrelated_containment_id" "$unrelated_snapshot" 'detached free record removal'

    template_containment_id="$(panel_registry_value \
        "$template_panel_id" freeDesktopContainmentId)"
    template_applet_id="$(panel_registry_value "$template_panel_id" freeDockAppletId)"
    start_signal_monitor nativePanelRecoveryFinished
    panel_call removePanel "$template_panel_id" >/dev/null
    wait_for_panel_registry_absent "$template_panel_id"
    wait_for_free_host_match_count "$template_panel_id" "$template_token" 0
    wait_for_free_host_absent "$template_containment_id" "$template_applet_id"
    wait_for_signal_monitor nativePanelRecoveryFinished
    [[ "$(free_panel_count)" == "$((free_count_before_cleanup - 2))" &&
        "$(free_host_snapshot "$sentinel_desktop_id" "$sentinel_applet_id")" == "$sentinel_snapshot" &&
        "$(free_host_match_count \
            archdock-unrelated-sentinel archdock-unrelated-sentinel-token)" == '1' &&
        "$(free_bootstrap_artifact_count "$template_bridge_id" "$template_token")" == '0' ]] || {
        printf 'Verified free-host removal left an orphan or changed an unrelated applet.\n' >&2
        exit 1
    }
    require_unrelated_panel_unchanged \
        "$unrelated_containment_id" "$unrelated_snapshot" 'verified free-host cleanup'
    log_session_phase 'detached zero matches idempotently and removed the final verified free host'

    local removal_registry_snapshot
    removal_registry_snapshot="$(panel_registry_record_snapshot "$panel_id")"
    local wrong_token='00000000-0000-0000-0000-000000000010'
    local token_tampered
    token_tampered="$(plasma_script "var panel = panelById($restarted_containment_id); if (!panel) { print(0); } else { panel.currentConfigGroup = ['ArchDock']; if (String(panel.readConfig('panelId', '')) !== '$panel_id' || String(panel.readConfig('ownerToken', '')) !== '$replacement_token') { print(0); } else { panel.writeConfig('ownerToken', '$wrong_token'); panel.reloadConfig(); print(String(panel.readConfig('ownerToken', '')) === '$wrong_token' ? 1 : 0); } }" | gvariant_string)"
    [[ "$token_tampered" == '1' ]] || {
        printf 'Could not establish the wrong-token permanent-removal fixture.\n' >&2
        exit 1
    }
    require_false_reply "$(panel_call removeNativeKdePanel "$panel_id")"
    panel_call removePanel "$panel_id" >/dev/null
    local guarded_containment_exists
    guarded_containment_exists="$(plasma_script "print(panelById($restarted_containment_id) ? 1 : 0);" | gvariant_string)"
    [[ "$guarded_containment_exists" == '1' &&
        "$(panel_registry_record_snapshot "$panel_id")" == "$removal_registry_snapshot" &&
        "$(owned_dock_id "$restarted_containment_id" "$panel_id")" == "$restarted_dock_id" &&
        "$(owned_dock_type "$restarted_containment_id" "$panel_id")" == 'hybrid' ]] || {
        printf 'Wrong-token removal did not preserve the containment, renderer and registry record.\n' >&2
        exit 1
    }
    require_unrelated_panel_unchanged \
        "$unrelated_containment_id" "$unrelated_snapshot" 'wrong-token removal refusal'
    local token_restored
    token_restored="$(plasma_script "var panel = panelById($restarted_containment_id); if (!panel) { print(0); } else { panel.currentConfigGroup = ['ArchDock']; if (String(panel.readConfig('panelId', '')) !== '$panel_id' || String(panel.readConfig('ownerToken', '')) !== '$wrong_token') { print(0); } else { panel.writeConfig('ownerToken', '$replacement_token'); panel.reloadConfig(); print(String(panel.readConfig('ownerToken', '')) === '$replacement_token' ? 1 : 0); } }" | gvariant_string)"
    [[ "$token_restored" == '1' &&
        "$(owned_panel_record "$panel_id")" == "$restarted_containment_id|$replacement_token" ]] || {
        printf 'Could not restore the exact ownership token after the refusal test.\n' >&2
        exit 1
    }

    local renderer_tampered
    renderer_tampered="$(plasma_script "var panel = panelById($restarted_containment_id); var dock = panel ? panel.widgetById($restarted_dock_id) : null; if (!dock || dock.type !== 'org.archdock.dock') { print(0); } else { dock.currentConfigGroup = ['General']; if (String(dock.readConfig('panelId', '')) !== '$panel_id' || String(dock.readConfig('panelType', '')) !== 'hybrid') { print(0); } else { dock.writeConfig('panelType', 'tasks'); dock.reloadConfig(); print(String(dock.readConfig('panelType', '')) === 'tasks' ? 1 : 0); } }" | gvariant_string)"
    [[ "$renderer_tampered" == '1' ]] || {
        printf 'Could not establish the wrong-renderer permanent-removal fixture.\n' >&2
        exit 1
    }
    require_false_reply "$(panel_call removeNativeKdePanel "$panel_id")"
    guarded_containment_exists="$(plasma_script "print(panelById($restarted_containment_id) ? 1 : 0);" | gvariant_string)"
    [[ "$guarded_containment_exists" == '1' &&
        "$(panel_registry_record_snapshot "$panel_id")" == "$removal_registry_snapshot" &&
        "$(owned_dock_id "$restarted_containment_id" "$panel_id")" == "$restarted_dock_id" &&
        "$(owned_dock_type "$restarted_containment_id" "$panel_id")" == 'tasks' ]] || {
        printf 'Wrong-renderer removal did not preserve the containment, applet and registry record.\n' >&2
        exit 1
    }
    require_unrelated_panel_unchanged \
        "$unrelated_containment_id" "$unrelated_snapshot" 'wrong-renderer removal refusal'
    local renderer_restored
    renderer_restored="$(plasma_script "var panel = panelById($restarted_containment_id); var dock = panel ? panel.widgetById($restarted_dock_id) : null; if (!dock || dock.type !== 'org.archdock.dock') { print(0); } else { dock.currentConfigGroup = ['General']; if (String(dock.readConfig('panelId', '')) !== '$panel_id' || String(dock.readConfig('panelType', '')) !== 'tasks') { print(0); } else { dock.writeConfig('panelType', 'hybrid'); dock.reloadConfig(); print(String(dock.readConfig('panelType', '')) === 'hybrid' ? 1 : 0); } }" | gvariant_string)"
    [[ "$renderer_restored" == '1' ]] || {
        printf 'Could not restore the expected renderer association after the refusal test.\n' >&2
        exit 1
    }
    require_visual_dock "$restarted_containment_id" "$panel_id" hybrid

    require_true_reply "$(panel_call removeNativeKdePanel "$panel_id")"
    local removal_result
    removal_result="$(plasma_script "print(panelById($restarted_containment_id) ? 1 : 0);" | gvariant_string)"
    [[ "$removal_result" == '0' ]] || {
        printf 'Verified native panel was not removed.\n' >&2
        exit 1
    }
    [[ "$(panel_registry_value "$panel_id" nativePanelId)" == '-1' &&
        "$(panel_registry_value "$panel_id" nativeDockAppletId)" == '-1' &&
        -z "$(panel_registry_value "$panel_id" nativeOwnershipToken)" ]] || {
        printf 'Verified native removal did not clear the managed association.\n' >&2
        exit 1
    }
    require_unrelated_panel_unchanged \
        "$unrelated_containment_id" "$unrelated_snapshot" 'verified permanent removal'
    panel_call removePanel "$panel_id" >/dev/null
    if panel_registry_record_snapshot "$panel_id" >/dev/null 2>&1; then
        printf 'Removing the verified panel did not remove its registry record.\n' >&2
        exit 1
    fi
    require_unrelated_panel_unchanged \
        "$unrelated_containment_id" "$unrelated_snapshot" 'managed record removal'

    local final_ids
    final_ids="$(panel_ids)"
    [[ "$final_ids" == "$baseline_ids" ]] || {
        printf 'The native lifecycle test changed non-Arch-Dock panels: before=%s after=%s\n' \
            "$baseline_ids" "$final_ids" >&2
        exit 1
    }
    [[ "$(unrelated_panel_archdock_marker "$unrelated_containment_id")" == '|' ]] || {
        printf 'The unrelated Plasma panel acquired an Arch Dock ownership marker.\n' >&2
        exit 1
    }

    log_session_phase 'completed'

}

run_outer() {
    validate_lifecycle_stop_after
    require_command cmake
    require_command chmod
    require_command dbus-run-session
    require_command gdbus
    require_command jq
    require_command kreadconfig6
    require_command kscreen-doctor
    require_command kwin_wayland
    require_command plasmashell
    require_command python
    require_command sha256sum
    require_command stat
    require_command stdbuf
    require_command timeout
    python -c 'from PySide6.QtCore import QByteArray, QSettings; from PySide6.QtGui import QWindow' || {
        printf 'Required Python modules are unavailable: PySide6.QtCore/QtGui\n' >&2
        exit 1
    }

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
    local free_template_script="$stage_root/share/plasma/layout-templates/org.archdock.plasma.desktop.circularFreeDock/contents/layout.js"
    local visibility_window_script="$project_root/tests/visibility-window.py"
    local visibility_probe_script="$project_root/tests/kwin-panel-visibility-probe.js"
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

    if command -v plasma-welcome >/dev/null 2>&1; then
        local plasma_welcome_version
        plasma_welcome_version="$(plasma-welcome --version | awk 'NF >= 2 { print $NF; exit }')"
        [[ "$plasma_welcome_version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || {
            printf 'Could not resolve the installed Plasma Welcome version: %s\n' \
                "$plasma_welcome_version" >&2
            exit 1
        }
        printf '[General]\nLastSeenVersion=%s\n' "$plasma_welcome_version" \
            >"$ARCHDOCK_LIFECYCLE_STATE_ROOT/config/plasma-welcomerc"
    fi

    cp "$0" "$session_script"
    chmod +x "$session_script"

    cmake --install "$build_dir" --prefix "$stage_root"
    [[ -r "$free_template_script" ]] || {
        printf 'Staged free-panel template is unavailable: %s\n' "$free_template_script" >&2
        exit 1
    }
    [[ -r "$visibility_window_script" && -r "$visibility_probe_script" ]] || {
        printf 'Visibility runtime fixtures are unavailable: window=%s probe=%s\n' \
            "$visibility_window_script" "$visibility_probe_script" >&2
        exit 1
    }

    local session_runner_status
    set +e
    env \
        ARCHDOCK_LIFECYCLE_STOP_AFTER="${ARCHDOCK_LIFECYCLE_STOP_AFTER:-}" \
        ARCHDOCK_PLASMA_LIFECYCLE_SESSION=1 \
        ARCHDOCK_SESSION_RESULT_FILE="$session_result_file" \
        ARCHDOCK_TEST_BINARY="$binary_path" \
        ARCHDOCK_TEST_LOG_DIR="$log_dir" \
        ARCHDOCK_FREE_TEMPLATE_SCRIPT="$free_template_script" \
        ARCHDOCK_VISIBILITY_WINDOW_SCRIPT="$visibility_window_script" \
        ARCHDOCK_VISIBILITY_PROBE_SCRIPT="$visibility_probe_script" \
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
        timeout --kill-after=10s 300s "$session_script" >"$log_dir/session.log" 2>&1
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
        tail -n 120 "$log_dir"/arch-dock*.log >&2 || true
        exit 1
    }

    if [[ "${ARCHDOCK_LIFECYCLE_STOP_AFTER:-}" == 'transaction' ]]; then
        cleanup_outer
        ARCHDOCK_LIFECYCLE_STATE_ROOT=''
        printf 'Isolated Plasma TASK-0023 Phase A transaction lifecycle succeeded.\n'
    else
        printf 'Isolated Plasma native/free lifecycle succeeded.\n'
    fi
}

if [[ "${ARCHDOCK_TRANSACTION_PARSER_FIXTURE:-}" == '1' ]]; then
    run_transaction_parser_fixture
elif [[ "${ARCHDOCK_PLASMA_LIFECYCLE_SESSION:-}" == '1' ]]; then
    run_session
else
    run_outer
fi
