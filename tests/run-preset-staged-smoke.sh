#!/usr/bin/env bash
# Installs Arch Dock into a temporary prefix and proves the preset catalogs
# work from there: exactly 15 + 15 installed definitions, loaded and drawn by
# the shared renderer from the staged data and the staged ArchDock.Rendering
# module. Nothing outside the temporary prefix is written, and the prefix is
# removed on every exit path.
set -euo pipefail

build_dir="${ARCHDOCK_BUILD_DIR:?ARCHDOCK_BUILD_DIR is required}"
qml_install_dir="${ARCHDOCK_QML_INSTALL_DIR:?ARCHDOCK_QML_INSTALL_DIR is required}"

stage="$(mktemp -d "${TMPDIR:-/tmp}/archdock-preset-stage.XXXXXX")"
cleanup() {
    rm -rf -- "$stage"
}
trap cleanup EXIT

prefix="$stage/prefix"
cmake --install "$build_dir" --prefix "$prefix" >"$stage/install.log" 2>&1 || {
    cat "$stage/install.log" >&2
    echo "staged install failed" >&2
    exit 1
}
# A package's install runs the icon-cache hook on hicolor; a staged theme
# without its index and cache is searched file by file on every icon lookup.
if [[ -d "$prefix/share/icons/hicolor" ]]; then
    cp /usr/share/icons/hicolor/index.theme "$prefix/share/icons/hicolor/index.theme"
    gtk-update-icon-cache --force --quiet "$prefix/share/icons/hicolor"
fi

presets="$prefix/share/arch-dock/presets"
for kind in panels icons; do
    index="builtin-${kind%s}-presets.json"
    [[ -f "$presets/$kind/$index" ]] || {
        echo "missing staged catalog index: $kind/$index" >&2
        exit 1
    }
    definitions="$(find "$presets/$kind" -maxdepth 1 -type f -name '*.json' ! -name "$index" | wc -l)"
    entries="$(find "$presets/$kind" -mindepth 1 | wc -l)"
    if [[ "$definitions" -ne 15 || "$entries" -ne 16 ]]; then
        echo "staged $kind catalog holds $definitions definitions in $entries entries, expected 15 in 16" >&2
        exit 1
    fi
done

[[ -f "$prefix/$qml_install_dir/ArchDock/Rendering/qmldir" ]] || {
    echo "the staged ArchDock.Rendering module is missing" >&2
    exit 1
}

# The staged prefix comes first, so every catalog, theme package and icon
# style is the installed one. The session bus address points nowhere.
env \
    ARCHDOCK_PRESET_STAGE_PREFIX="$prefix" \
    ARCHDOCK_RENDERING_IMPORT_ROOT="$prefix/$qml_install_dir" \
    XDG_DATA_DIRS="$prefix/share:${XDG_DATA_DIRS:-/usr/local/share:/usr/share}" \
    XDG_DATA_HOME="$stage/data" \
    XDG_CONFIG_HOME="$stage/config" \
    XDG_CACHE_HOME="$stage/cache" \
    DBUS_SESSION_BUS_ADDRESS="unix:path=$stage/no-session-bus" \
    QT_QPA_PLATFORM=offscreen \
    QT_QUICK_BACKEND=software \
    "$build_dir/preset-library-test"

echo "Staged preset catalogs (15 panel, 15 icon) load and render from $presets"
