#!/usr/bin/env bash
# Configuration upgrade evidence only. Physical/native-panel evidence remains
# in the existing isolated Plasma Wayland gates.
set -euo pipefail
project_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
binary="${ARCHDOCK_BUILD_DIR:?ARCHDOCK_BUILD_DIR is required}/arch-dock"
[[ -x "$binary" ]] || { printf 'Missing freshly built executable: %s\n' "$binary" >&2; exit 1; }
if [[ "${ARCHDOCK_UPGRADE_PRIVATE_SESSION:-0}" != 1 ]]; then
    state="$(mktemp -d "${TMPDIR:-/tmp}/archdock-upgrade.XXXXXX")"
    trap 'rm -rf -- "$state"' EXIT
    mkdir -p "$state/runtime" "$state/config" "$state/data" "$state/cache"
    chmod 700 "$state/runtime"
    env ARCHDOCK_UPGRADE_PRIVATE_SESSION=1 ARCHDOCK_UPGRADE_STATE="$state" \
        XDG_RUNTIME_DIR="$state/runtime" XDG_CONFIG_HOME="$state/config" \
        XDG_DATA_HOME="$state/data" XDG_CACHE_HOME="$state/cache" \
        QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QT_FORCE_STDERR_LOGGING=1 \
        dbus-run-session -- timeout --kill-after=5s 60s bash "$0"
    exit $?
fi
state="${ARCHDOCK_UPGRADE_STATE:?}"
service_pid=''
stop_service() {
    if [[ -n "$service_pid" ]]; then
        kill -TERM "$service_pid" 2>/dev/null || true
        wait "$service_pid" 2>/dev/null || true
        service_pid=''
    fi
}
finish_session() {
    local status=$?
    stop_service
    if (( status != 0 )) && [[ -f "$state/service.log" ]]; then tail -n 50 "$state/service.log" >&2; fi
}
trap finish_session EXIT
fixture() {
    python - "$project_root" "$state" "$1" <<'PY'
import json, pathlib, shutil, sys
from PySide6.QtCore import QByteArray, QCoreApplication, QSettings, QStandardPaths
app = QCoreApplication([])
app.setOrganizationName('Arch Dock'); app.setApplicationName('Arch Dock')
source, state, mode = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2]), sys.argv[3]
settings = QSettings()
config = pathlib.Path(settings.fileName())
data = pathlib.Path(QStandardPaths.writableLocation(QStandardPaths.AppDataLocation))
if mode == 'seed':
    settings.setValue('dock/panels', QByteArray(json.dumps([{'id':'upgrade-test', 'name':'Legacy',
        'edge':'bottom', 'visible':False, 'iconSize':48}]).encode()))
    settings.setValue('upgrade/sentinel', 'retain-exactly'); settings.sync()
    assert settings.status() == QSettings.NoError
    shutil.copy2(config, state / 'original.conf')
    presets = data / 'presets'; (presets / 'panels').mkdir(parents=True)
    panel = json.loads((source / 'data/presets/panels/sci-fi-chassis-blue.json').read_text())
    panel['identity'].update(id='user-upgrade', name='Upgrade fixture', builtIn=False)
    (presets / 'panels/user-upgrade.json').write_text(json.dumps(panel))
    (presets / 'user-presets.json').write_text(json.dumps({'format':'org.archdock.user-preset-store', 'version':1}))
    (presets / 'defaults.json').write_text(json.dumps({'format':'org.archdock.preset-defaults',
        'version':1, 'panelPresetId':'user-upgrade', 'iconPresetId':''}))
elif mode == 'upgraded':
    panels = json.loads(bytes(settings.value('dock/panels')))
    assert len(panels) == 1 and panels[0]['schemaVersion'] == 2
    assert panels[0]['iconSize'] == 48
    assert settings.value('upgrade/sentinel') == 'retain-exactly'
elif mode == 'original':
    assert config.read_bytes() == (state / 'original.conf').read_bytes()
    assert (data / 'presets/panels/user-upgrade.json').is_file()
    assert json.loads((data / 'presets/defaults.json').read_text())['panelPresetId'] == 'user-upgrade'
elif mode == 'future':
    settings.setValue('dock/panels', QByteArray(b'[{"schemaVersion":99,"id":"future"}]')); settings.sync()
    shutil.copy2(config, state / 'future.conf')
elif mode == 'future-unchanged':
    assert config.read_bytes() == (state / 'future.conf').read_bytes()
else:
    raise AssertionError(mode)
PY
}
start_service() {
    "$binary" >"$state/service.log" 2>&1 &
    service_pid=$!
    local deadline=$((SECONDS + 15)) owner
    until owner="$(gdbus call --session --dest org.freedesktop.DBus --object-path /org/freedesktop/DBus \
        --method org.freedesktop.DBus.GetConnectionUnixProcessID org.archdock.ArchDock 2>/dev/null)" &&
        [[ "$owner" == "(uint32 $service_pid,)" ]] && kill -0 "$service_pid" 2>/dev/null &&
        gdbus call --session --dest org.archdock.ArchDock --object-path /Control \
            --method local.PanelWindow.dockConfiguration upgrade-test >/dev/null 2>&1; do
        (( SECONDS < deadline )) && kill -0 "$service_pid" 2>/dev/null || {
            cat "$state/service.log" >&2; printf 'Disposable upgrade service did not become ready.\n' >&2; return 1;
        }
        sleep 0.1
    done
}
fixture seed
data_root="$XDG_DATA_HOME/Arch Dock/Arch Dock"
original_id="$(env QT_QPA_PLATFORM=invalid "$binary" --backup-config)"
[[ "$original_id" =~ ^[0-9]{17}-[a-f0-9]{32}$ ]]
[[ "$("$binary" --list-config-backups)" == "$original_id" ]]
jq -e '[.files[].path] | index("panels/user-upgrade.json") != null and index("defaults.json") != null' \
    "$data_root/config-backups/$original_id/manifest.json" >/dev/null
! rg -q 'builtin-|untrusted|\.sh"' "$data_root/config-backups/$original_id/manifest.json"
start_service
fixture upgraded
if "$binary" --backup-config >"$state/refused.out" 2>"$state/refused.err"; then
    printf 'Recovery command accepted a live owner.\n' >&2; exit 1
fi
rg -q 'requires Arch Dock to be stopped' "$state/refused.err"
kill -0 "$service_pid"
stop_service
env QT_QPA_PLATFORM=invalid "$binary" --restore-config-backup "$original_id"
fixture original
fixture future
start_service
fixture future-unchanged
stop_service
"$binary" --restore-config-backup "$original_id"
fixture original
mv "$data_root/config-backups" "$state/saved-backups"
printf 'blocked' >"$data_root/config-backups"
start_service
fixture original
stop_service
rm -- "$data_root/config-backups"
mv "$state/saved-backups" "$data_root/config-backups"
"$binary" --restore-config-backup "$original_id"
fixture original
printf 'Disposable configuration upgrade, future-version refusal, rollback and offline recovery passed.\n'
