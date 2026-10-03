#!/usr/bin/env bash
# Native package operations are confined to a disposable user namespace/root.
set -euo pipefail
project_root="$(cd -- "$(dirname -- "$0")/.." && pwd)"
package="$(realpath -e -- "${1:?supply the built Arch package}")"
previous_package=''
if [[ -n "${2:-}" ]]; then
    previous_package="$(realpath -e -- "$2")"
fi
build_dir="$(realpath -e -- "${ARCHDOCK_BUILD_DIR:?supply the verified test build}")"
manifest="$(realpath -e -- "${ARCHDOCK_PACKAGE_INSTALL_MANIFEST:?supply the package build install manifest}")"
evidence="$(realpath -e -- "${ARCHDOCK_PACKAGE_EVIDENCE_DIR:?supply the evidence directory}")"
for command in bwrap bsdtar pacman python sha256sum; do
    command -v "$command" >/dev/null || exit 1
done
if [[ -n "$previous_package" ]]; then
    command -v vercmp >/dev/null || exit 1
    read -r previous_name previous_version < <(pacman -Qp "$previous_package")
    read -r current_name current_version < <(pacman -Qp "$package")
    [[ "$previous_name" == arch-dock && "$current_name" == arch-dock &&
       "$(vercmp "$previous_version" "$current_version")" == -1 ]] || {
        printf 'Upgrade requires an older Arch Dock package.\n' >&2; exit 1;
    }
fi
[[ -x "$build_dir/preset-library-test" ]] || exit 1
root="$(mktemp -d "${TMPDIR:-/tmp}/archdock-package.XXXXXX")"
trap 'rm -rf -- "$root"' EXIT
sysroot="$root/system"
mkdir -p "$sysroot/var/lib/pacman" "$sysroot/var/cache/pacman/pkg" \
    "$sysroot/var/log" "$root/empty-hooks" "$root/expected" "$root/tests" \
    "$root/tmp" "$root/config" "$root/cache" "$root/data" "$root/home" "$root/runtime"
chmod 700 "$root/runtime"
# Reuse native dependency records, without copying or modifying host payloads.
cp -a /var/lib/pacman/local "$sysroot/var/lib/pacman/"
cat >"$root/pacman.conf" <<'CONF'
[options]
Architecture = auto
SigLevel = Never
LocalFileSigLevel = Never
CONF
pacman_private() {
    bwrap --die-with-parent --unshare-user --uid 0 --gid 0 \
        --ro-bind / / --bind "$root" "$root" \
        pacman --config "$root/pacman.conf" --root "$sysroot" \
        --dbpath "$sysroot/var/lib/pacman" --cachedir "$sysroot/var/cache/pacman/pkg" \
        --logfile "$sysroot/var/log/pacman.log" --hookdir "$root/empty-hooks" \
        --noconfirm "$@"
}
if pacman_private -Q arch-dock >/dev/null 2>&1; then
    printf 'The dependency database already contains arch-dock.\n' >&2; exit 1
fi
bsdtar -xf "$package" -C "$root/expected"
[[ ! -e "$root/expected/.INSTALL" ]] || exit 1
rg -q '^pkgname = arch-dock$' "$root/expected/.PKGINFO"
! rg -q '^depend = qt6-quick3d([<>=]|$)' "$root/expected/.PKGINFO"
rg -q '^optdepend = qt6-quick3d:' "$root/expected/.PKGINFO"
pacman_private -U "$package" >"$evidence/pacman-install.log" 2>&1
pacman_private -Ql arch-dock >"$evidence/pacman-file-list.log"
audit_installed_payload() {
python - "$root/expected" "$sysroot" "$manifest" "$evidence" "$root/previous" <<'PY'
import hashlib, json, pathlib, sys
expected, installed, manifest, evidence, previous = map(pathlib.Path, sys.argv[1:])
files = sorted(p.relative_to(expected).as_posix() for p in expected.rglob('*') if p.is_file() and not p.name.startswith('.'))
assert files and all(p.startswith('usr/') for p in files), 'unexpected package destination'
declared = {line.strip().lstrip('/') for line in manifest.read_text().splitlines()}
docs = {'usr/share/doc/arch-dock/INSTALL.md', 'usr/share/licenses/arch-dock/LICENSING.md'}
assert set(files) == declared | docs, 'package payload differs from the CMake install manifest'
required = {'usr/bin/arch-dock', 'usr/lib/systemd/user/arch-dock.service',
    'usr/share/dbus-1/services/org.archdock.ArchDock.service',
    'usr/share/applications/org.archdock.ArchDock.desktop',
    'usr/lib/qt6/qml/ArchDock/Rendering/qmldir',
    'usr/share/kwin/scripts/org.archdock.windowwatcher/metadata.json',
    'usr/share/plasma/plasmoids/org.archdock.control/metadata.json',
    'usr/share/plasma/plasmoids/org.archdock.dock/metadata.json'}
assert required <= set(files), 'required runtime resources are absent'
assert sum('/layout-templates/' in p and p.endswith('/metadata.json') for p in files) == 5
for kind in ('panels', 'icons'):
    assert sum(p.startswith(f'usr/share/arch-dock/presets/{kind}/') and p.endswith('.json') for p in files) == 16
hashes = {}
for relative in files:
    original, actual = expected / relative, installed / relative
    assert actual.is_file() and actual.read_bytes() == original.read_bytes(), relative
    assert actual.stat().st_mode & 0o777 == original.stat().st_mode & 0o777, relative
    hashes[relative] = hashlib.sha256(actual.read_bytes()).hexdigest()
if previous.exists():
    for old in previous.rglob('*'):
        if old.is_file() and not old.name.startswith('.'):
            relative = old.relative_to(previous).as_posix()
            if relative not in hashes:
                assert not (installed / relative).exists(), f'obsolete package file survived upgrade: {relative}'
(evidence / 'installed-payload.json').write_text(json.dumps(hashes, indent=2, sort_keys=True) + '\n')
print(f'Package manifest, installed bytes/modes and complete resource coverage PASS: {len(files)} files.')
PY
}
audit_installed_payload
ldd "$sysroot/usr/bin/arch-dock" >"$evidence/package-ldd.log"
! rg -q 'not found|libQt6Quick3D' "$evidence/package-ldd.log"
desktop-file-validate "$sysroot/usr/share/applications/org.archdock.ArchDock.desktop"
bwrap --die-with-parent --ro-bind / / --tmpfs /tmp \
    --overlay-src /usr --overlay-src "$sysroot/usr" --ro-overlay /usr \
    env TMPDIR=/tmp systemd-analyze --user verify /usr/lib/systemd/user/arch-dock.service
cp "$build_dir/preset-library-test" "$root/tests/"
cp -a "$project_root/qml/runtime" "$root/fixture-qml"
presets_installed() {
    local without_3d="$1"
    shift
    local -a namespace=(bwrap --die-with-parent --ro-bind / / --dev-bind /dev /dev
        --tmpfs /tmp --overlay-src /usr --overlay-src "$sysroot/usr" --ro-overlay /usr)
    if [[ "$without_3d" == 1 ]]; then
        namespace+=(--tmpfs /usr/lib/qt6/qml/QtQuick3D)
    fi
    namespace+=(--tmpfs "$project_root" --bind "$root" "$root" --chdir "$root")
    env HOME="$root/home" XDG_CONFIG_HOME="$root/config" XDG_CACHE_HOME="$root/cache" \
        XDG_DATA_HOME="$root/data" XDG_DATA_DIRS=/usr/share XDG_RUNTIME_DIR="$root/runtime" \
        DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent/archdock-package-test-bus \
        TMPDIR=/tmp QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QT_FORCE_STDERR_LOGGING=1 \
        ARCHDOCK_PRESET_STAGE_PREFIX=/usr ARCHDOCK_RENDERING_IMPORT_ROOT=/usr/lib/qt6/qml \
        ARCHDOCK_TEST_RUNTIME_QML_DIR="$root/fixture-qml" \
        "${namespace[@]}" "$root/tests/preset-library-test" "$@"
}
presets_installed 0 locatesTheCatalogItShipsWith listsExactlyTheBuiltInCatalogs \
    cardsDescribeHostRendererBehaviourAndMotion everyBuiltInCardCarriesSharedRendererPreviewData \
    listingAndPreviewingChangeNothing everyBuiltInCardRendersThroughTheSharedRenderer \
    >"$evidence/installed-presets.log" 2>&1
for without_3d in 0 1; do
    env ARCHDOCK_BUILD_DIR="$build_dir" ARCHDOCK_STARTUP_INSTALL_ROOT="$sysroot/usr" \
        ARCHDOCK_STARTUP_WITHOUT_QUICK3D="$without_3d" TMPDIR="${TMPDIR:-/tmp}" \
        bash "$project_root/tests/run-session-startup-smoke.sh" runtime \
        >"$evidence/installed-startup-$without_3d.log" 2>&1
    mkdir -p "$evidence/installed-session-$without_3d"
    cp "$build_dir/session-startup-smoke/"*.log "$evidence/installed-session-$without_3d/"
done
presets_installed 1 listsExactlyTheBuiltInCatalogs everyBuiltInCardRendersThroughTheSharedRenderer \
    >"$evidence/installed-presets-without-3d.log" 2>&1
mkdir -p "$sysroot/home/owner/.config/ArchDock"
printf 'user-owned Plasma configuration\n' >"$sysroot/home/owner/.config/plasma-org.kde.plasma.desktop-appletsrc"
printf 'user-owned Arch Dock configuration\n' >"$sysroot/home/owner/.config/ArchDock/arch-dock.conf"
sha256sum "$sysroot/home/owner/.config/plasma-org.kde.plasma.desktop-appletsrc" \
    "$sysroot/home/owner/.config/ArchDock/arch-dock.conf" >"$root/config-before.sha256"
remove_installed_package() {
local scenario="$1"
pacman_private -R arch-dock >"$evidence/pacman-$scenario-uninstall.log" 2>&1
! pacman_private -Q arch-dock >/dev/null 2>&1
sha256sum --check "$root/config-before.sha256" >"$evidence/config-$scenario-preserved.log"
python - "$sysroot" "$evidence/installed-payload.json" <<'PY'
import json, pathlib, sys
root = pathlib.Path(sys.argv[1])
for relative in json.loads(pathlib.Path(sys.argv[2]).read_text()):
    assert not (root / relative).exists(), f'package file survived removal: {relative}'
print('Native pacman removal and preservation of user configuration PASS.')
PY
}
remove_installed_package fresh
if [[ -n "$previous_package" ]]; then
    mkdir -p "$root/previous"
    bsdtar -xf "$previous_package" -C "$root/previous"
    [[ ! -e "$root/previous/.INSTALL" ]] || exit 1
    pacman_private -U "$previous_package" >"$evidence/pacman-previous-install.log" 2>&1
    [[ "$(pacman_private -Q arch-dock)" == "arch-dock $previous_version" ]] || exit 1
    sha256sum --check "$root/config-before.sha256" >"$evidence/config-previous-preserved.log"
    pacman_private -U "$package" >"$evidence/pacman-upgrade.log" 2>&1
    [[ "$(pacman_private -Q arch-dock)" == "arch-dock $current_version" ]] || exit 1
    audit_installed_payload
    sha256sum --check "$root/config-before.sha256" >"$evidence/config-upgrade-preserved.log"
    # Reuse the existing real QSettings/preset migration, backup, restore and
    # live-owner refusal fixture against /usr/bin from the upgraded package.
    bwrap --die-with-parent --ro-bind / / --dev-bind /dev /dev --tmpfs /tmp \
        --overlay-src /usr --overlay-src "$sysroot/usr" --ro-overlay /usr \
        --bind "$root" "$root" --chdir "$root" \
        env ARCHDOCK_BUILD_DIR=/usr/bin TMPDIR="$root/tmp" \
        XDG_DATA_DIRS=/usr/share QML_IMPORT_PATH=/usr/lib/qt6/qml \
        PYTHONDONTWRITEBYTECODE=1 \
        bash "$project_root/tests/run-configuration-upgrade-smoke.sh" \
        >"$evidence/installed-upgrade-recovery.log" 2>&1
    env ARCHDOCK_BUILD_DIR="$build_dir" ARCHDOCK_STARTUP_INSTALL_ROOT="$sysroot/usr" \
        ARCHDOCK_STARTUP_WITHOUT_QUICK3D=0 TMPDIR="${TMPDIR:-/tmp}" \
        bash "$project_root/tests/run-session-startup-smoke.sh" runtime \
        >"$evidence/upgraded-startup.log" 2>&1
    mkdir -p "$evidence/upgraded-session"
    cp "$build_dir/session-startup-smoke/"*.log "$evidence/upgraded-session/"
    remove_installed_package upgrade
    printf 'Native Arch package upgrade %s -> %s and installed configuration recovery PASS.\n' \
        "$previous_version" "$current_version"
fi
printf 'Arch package install, hidden-source rendering, 15+15 catalogs, optional 3D and uninstall PASS.\n'
