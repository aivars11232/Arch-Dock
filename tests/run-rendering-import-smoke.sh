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
    local exit_status=$?
    if ((exit_status != 0)) && [[ -n "${ARCHDOCK_RENDERING_LOG_DIR:-}" ]]; then
        local log_file
        for log_file in "$ARCHDOCK_RENDERING_LOG_DIR/service.log" \
                        "$ARCHDOCK_RENDERING_LOG_DIR/plasmashell.log" \
                        "$ARCHDOCK_RENDERING_LOG_DIR/kwin.log"; do
            if [[ -r "$log_file" ]]; then
                printf '%s\n' "--- ${log_file##*/} (failure tail) ---" >&2
                tail -n 160 "$log_file" >&2
            fi
        done
    fi
    stop_process "$ARCHDOCK_RENDERING_SERVICE_PID"
    stop_process "$ARCHDOCK_RENDERING_PLASMASHELL_PID"
    stop_process "$ARCHDOCK_RENDERING_KWIN_PID"
    return "$exit_status"
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
            'module "ArchDock\.Rendering" is not installed|RenderingModuleProbe[^[:cntrl:]]*(not a type|unavailable)|Panel(Scene|SurfaceLoader|Procedural2D|Skin2D|SkinLayer2D)[^[:cntrl:]]*(not a type|unavailable|not installed)|AlphaHitMask[^[:cntrl:]]*(not a type|unavailable|not installed)|(IconScene|RunningIndicator|LivePanelPreview)[^[:cntrl:]]*(not a type|unavailable|not installed)|(SettingsPopup|StudioForm|IconProperties|IconPropertiesWindow)\.qml:[0-9]+:[0-9]+:[^[:cntrl:]]*(error|unavailable|not installed|not a type|typeerror|referenceerror|cannot assign|unable to assign|binding loop)|org\.archdock\.dock/contents/ui/(main|DockEntry|IconVisual|RunningIndicator)\.qml:[0-9]+:[0-9]+:[^[:cntrl:]]*(error|unavailable|not installed|not a type|typeerror|referenceerror|cannot assign|unable to assign|binding loop)|ArchDock/Rendering/(PanelScene|PanelSurfaceLoader|renderers/PanelSkin2D|renderers/PanelSkinLayer2D|inputs/AlphaHitMask|IconScene|RunningIndicator|previews/LivePanelPreview)\.qml:[0-9]+:[0-9]+:[^[:cntrl:]]*(typeerror|referenceerror|cannot assign|unable to assign|binding loop)|Error loading QML file[^[:cntrl:]]*org\.archdock\.dock' \
            "$log_file"; then
            printf 'Staged rendering import failed; relevant QML errors were logged in %s.\n' \
                "$log_file" >&2
            return 1
        fi
    done
}

run_private_session() {
    trap cleanup_session EXIT

    [[ -r "$ARCHDOCK_RENDERING_STAGED_THEME_MANIFEST" ]] || {
        printf 'The staged renderer smoke theme manifest is unavailable: %s\n' \
            "$ARCHDOCK_RENDERING_STAGED_THEME_MANIFEST" >&2
        return 1
    }
    [[ -d "$ARCHDOCK_RENDERING_STAGED_THEME_ROOT" ]] || {
        printf 'The staged renderer smoke theme root is unavailable: %s\n' \
            "$ARCHDOCK_RENDERING_STAGED_THEME_ROOT" >&2
        return 1
    }
    [[ -x "$ARCHDOCK_RENDERING_QMLTESTRUNNER" ]] || {
        printf 'The Qt 6 qmltestrunner is unavailable in the private session: %s\n' \
            "$ARCHDOCK_RENDERING_QMLTESTRUNNER" >&2
        return 1
    }
    [[ -x "$ARCHDOCK_RENDERING_ICON_PROPERTIES_INTERACTION_TEST" ]] || {
        printf 'The Icon Properties interaction test is unavailable: %s\n' \
            "$ARCHDOCK_RENDERING_ICON_PROPERTIES_INTERACTION_TEST" >&2
        return 1
    }
    [[ -r "$ARCHDOCK_RENDERING_PANEL_SKIN_TEST" ]] || {
        printf 'The PanelSkin2D pixel test is unavailable: %s\n' \
            "$ARCHDOCK_RENDERING_PANEL_SKIN_TEST" >&2
        return 1
    }
    [[ -d "$ARCHDOCK_RENDERING_STAGED_ICON_STYLE_ROOT" ]] || {
        printf 'The staged icon-style root is unavailable: %s\n' \
            "$ARCHDOCK_RENDERING_STAGED_ICON_STYLE_ROOT" >&2
        return 1
    }

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

    printf 'Driving live DockEntry Icon Properties Apply, Cancel, and Reset under private Wayland.\n'
    ARCHDOCK_PRIVATE_INTERACTION_TEST=1 \
        "$ARCHDOCK_RENDERING_ICON_PROPERTIES_INTERACTION_TEST" \
        iconPropertiesPublicInteractionIsTransactional

    printf 'Running the staged PanelSkin2D energy pixel test under private KWin.\n'
    "$ARCHDOCK_RENDERING_QMLTESTRUNNER" \
        -import "$QML_IMPORT_PATH" \
        -input "$ARCHDOCK_RENDERING_PANEL_SKIN_TEST" \
        PanelSkin2D::test_waylandEnergyEffectPixels

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
        "var result = (function() { var panel = null; var freeWidget = null; try { panel = new Panel; panel.screen = 0; panel.location = 'bottom'; var nativeWidget = panel.addWidget('org.archdock.dock'); if (!nativeWidget) { panel.remove(); return 'missing-native-widget'; } nativeWidget.currentConfigGroup = ['General']; nativeWidget.writeConfig('panelId', 'bottom'); nativeWidget.writeConfig('panelType', 'hybrid'); nativeWidget.reloadConfig(); var desktop = desktopForScreen(0); if (!desktop) { panel.remove(); return 'missing-desktop'; } freeWidget = desktop.addWidget('org.archdock.dock', Math.round(gridUnit * 2), Math.round(gridUnit * 2), Math.round(gridUnit * 20), Math.round(gridUnit * 10)); if (!freeWidget) { panel.remove(); return 'missing-free-widget'; } freeWidget.currentConfigGroup = ['General']; freeWidget.writeConfig('panelId', ''); freeWidget.writeConfig('panelType', 'hybrid'); freeWidget.writeConfig('bootstrapFreeDock', true); freeWidget.reloadConfig(); return [String(panel.id), String(nativeWidget.id), String(desktop.id), String(freeWidget.id)].join('|'); } catch (error) { if (freeWidget) { freeWidget.remove(); } if (panel) { panel.remove(); } return 'exception:' + String(error); } })(); print(result);" | gvariant_string)"

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
    local free_panel_id=''
    for ((attempt = 0; attempt < 50; ++attempt)); do
        native_snapshot="$(plasma_script \
            "var panel = panelById($panel_id); var widget = panel ? panel.widgetById($native_applet_id) : null; if (!widget) { print('missing'); } else { widget.currentConfigGroup = ['General']; var geometry = widget.geometry; print([String(widget.type), String(widget.readConfig('panelId', '')), String(widget.readConfig('panelType', '')), String(Number(geometry.width)), String(Number(geometry.height))].join('|')); }" | gvariant_string)"
        free_snapshot="$(plasma_script \
            "var desktop = desktopById($desktop_id); var widget = desktop ? desktop.widgetById($free_applet_id) : null; if (!widget) { print('missing'); } else { widget.currentConfigGroup = ['General']; var geometry = widget.geometry; print([String(widget.type), String(widget.readConfig('panelId', '')), String(widget.readConfig('panelType', '')), String(widget.readConfig('bootstrapFreeDock', true)), String(Number(geometry.width)), String(Number(geometry.height))].join('|')); }" | gvariant_string)"
        if [[ "$native_snapshot" =~ ^org\.archdock\.dock\|bottom\|hybrid\|[1-9][0-9]*([.][0-9]+)?\|[1-9][0-9]*([.][0-9]+)?$ &&
              "$free_snapshot" =~ ^org\.archdock\.dock\|free-[1-9][0-9]*\|empty\|(false|0)\|[1-9][0-9]*([.][0-9]+)?\|[1-9][0-9]*([.][0-9]+)?$ ]]; then
            break
        fi
        sleep 0.1
    done
    [[ "$native_snapshot" =~ ^org\.archdock\.dock\|bottom\|hybrid\|[1-9][0-9]*([.][0-9]+)?\|[1-9][0-9]*([.][0-9]+)?$ ]] || {
        printf 'The staged native PanelScene host was not ready: %s\n' \
            "$native_snapshot" >&2
        return 1
    }
    [[ "$free_snapshot" =~ ^org\.archdock\.dock\|free-[1-9][0-9]*\|empty\|(false|0)\|[1-9][0-9]*([.][0-9]+)?\|[1-9][0-9]*([.][0-9]+)?$ ]] || {
        printf 'The staged free PanelScene host was not ready: %s\n' \
            "$free_snapshot" >&2
        return 1
    }

    IFS='|' read -r _ free_panel_id _ _ _ _ <<<"$free_snapshot"
    local native_configuration
    local free_configuration
    local native_revision
    local free_revision

    local energy_theme_id
    local energy_tint
    local energy_manifest
    local energy_values
    local native_theme_reply
    local free_theme_reply
    local energy_native_renderer=''
    local energy_free_renderer=''
    for energy_theme_id in energy-frame-green energy-frame-orange \
                           energy-frame-purple energy-frame-cyan; do
        case "$energy_theme_id" in
            energy-frame-green) energy_tint='#4ee68a' ;;
            energy-frame-orange) energy_tint='#ff873c' ;;
            energy-frame-purple) energy_tint='#b96cff' ;;
            energy-frame-cyan) energy_tint='#44ddea' ;;
        esac
        energy_manifest="$ARCHDOCK_RENDERING_STAGED_THEME_ROOT/$energy_theme_id/archdock-theme.json"
        [[ -r "$energy_manifest" ]] || {
            printf 'The staged live-smoke energy theme is unavailable: %s\n' \
                "$energy_manifest" >&2
            return 1
        }

        native_configuration="$(panel_call dockConfiguration bottom)"
        free_configuration="$(panel_call dockConfiguration "$free_panel_id")"
        native_revision="$(sed -n \
            "s/.*'settingsRevision': <uint64 \\([0-9][0-9]*\\)>.*/\\1/p" \
            <<<"$native_configuration")"
        free_revision="$(sed -n \
            "s/.*'settingsRevision': <uint64 \\([0-9][0-9]*\\)>.*/\\1/p" \
            <<<"$free_configuration")"
        [[ "$native_revision" =~ ^[0-9]+$ &&
           "$free_revision" =~ ^[0-9]+$ ]] || {
            printf 'Could not read revisions before selecting %s: native=%s free=%s\n' \
                "$energy_theme_id" "$native_revision" "$free_revision" >&2
            return 1
        }

        energy_values="{'layout': <'horizontal'>, 'rendererTier': <'skinned2d'>, 'panelThemeId': <'$energy_theme_id'>, 'completeThemeId': <'$energy_theme_id'>, 'color': <'$energy_tint'>, 'glowIntensity': <1.15>}"
        printf 'Selecting staged energy theme %s on the private native and free hosts.\n' \
            "$energy_theme_id"
        native_theme_reply="$(panel_call applyPanelSettingsTransaction \
            bottom "uint64 $native_revision" "$energy_values" '{}')"
        free_theme_reply="$(panel_call applyPanelSettingsTransaction \
            "$free_panel_id" "uint64 $free_revision" "$energy_values" '{}')"
        [[ "$native_theme_reply" == *"'success': <true>"* &&
           "$native_theme_reply" == *"'status': <'succeeded'>"* &&
           "$free_theme_reply" == *"'success': <true>"* &&
           "$free_theme_reply" == *"'status': <'succeeded'>"* ]] || {
            printf 'Could not select energy theme %s: native=%s free=%s\n' \
                "$energy_theme_id" "$native_theme_reply" "$free_theme_reply" >&2
            return 1
        }

        energy_native_renderer=''
        energy_free_renderer=''
        for ((attempt = 0; attempt < 50; ++attempt)); do
            energy_native_renderer="$(panel_call panelRendererConfiguration bottom)"
            energy_free_renderer="$(panel_call panelRendererConfiguration "$free_panel_id")"
            if [[ "$energy_native_renderer" == *"'effectiveRendererTier': <'skinned2d'>"* &&
                  "$energy_native_renderer" == *"'themeProjectionStatus': <'ready'>"* &&
                  "$energy_native_renderer" == *"'id': <'$energy_theme_id'>"* &&
                  "$energy_native_renderer" == *"'dynamic-glow'"* &&
                  "$energy_native_renderer" == *"'manifestPath': <'$energy_manifest'>"* &&
                  "$energy_free_renderer" == *"'effectiveRendererTier': <'skinned2d'>"* &&
                  "$energy_free_renderer" == *"'themeProjectionStatus': <'ready'>"* &&
                  "$energy_free_renderer" == *"'id': <'$energy_theme_id'>"* &&
                  "$energy_free_renderer" == *"'dynamic-glow'"* &&
                  "$energy_free_renderer" == *"'manifestPath': <'$energy_manifest'>"* ]]; then
                break
            fi
            sleep 0.1
        done
        [[ "$energy_native_renderer" == *"'effectiveRendererTier': <'skinned2d'>"* &&
           "$energy_native_renderer" == *"'themeProjectionStatus': <'ready'>"* &&
           "$energy_native_renderer" == *"'id': <'$energy_theme_id'>"* &&
           "$energy_native_renderer" == *"'dynamic-glow'"* &&
           "$energy_native_renderer" == *"'manifestPath': <'$energy_manifest'>"* ]] || {
            printf 'The native host did not resolve energy theme %s: %s\n' \
                "$energy_theme_id" "$energy_native_renderer" >&2
            return 1
        }
        [[ "$energy_free_renderer" == *"'effectiveRendererTier': <'skinned2d'>"* &&
           "$energy_free_renderer" == *"'themeProjectionStatus': <'ready'>"* &&
           "$energy_free_renderer" == *"'id': <'$energy_theme_id'>"* &&
           "$energy_free_renderer" == *"'dynamic-glow'"* &&
           "$energy_free_renderer" == *"'manifestPath': <'$energy_manifest'>"* ]] || {
            printf 'The free host did not resolve energy theme %s: %s\n' \
                "$energy_theme_id" "$energy_free_renderer" >&2
            return 1
        }
        kill -0 "$ARCHDOCK_RENDERING_SERVICE_PID"
        kill -0 "$ARCHDOCK_RENDERING_PLASMASHELL_PID"
        require_no_import_errors
    done

    local free_pin_reply
    free_pin_reply="$(panel_call pinPanelUrls "$free_panel_id" "['$smoke_desktop_url']")"
    [[ "$free_pin_reply" == '(true,)' ]] || {
        printf 'Could not seed the free renderer smoke entry: %s\n' \
            "$free_pin_reply" >&2
        return 1
    }

    local style_id
    local style_manifest
    local style_values
    local native_style_reply
    local free_style_reply
    local native_renderer=''
    local free_renderer=''
    local live_style_ids=(
        metallic-blue
        metallic-red
        neon-green
        neon-orange
        dark-orb
    )
    for style_id in "${live_style_ids[@]}"; do
        style_manifest="$ARCHDOCK_RENDERING_STAGED_ICON_STYLE_ROOT/$style_id/archdock-icon-style.json"
        [[ -r "$style_manifest" ]] || {
            printf 'The staged live-smoke icon style is unavailable: %s\n' \
                "$style_manifest" >&2
            return 1
        }

        native_configuration="$(panel_call dockConfiguration bottom)"
        free_configuration="$(panel_call dockConfiguration "$free_panel_id")"
        native_revision="$(sed -n \
            "s/.*'settingsRevision': <uint64 \\([0-9][0-9]*\\)>.*/\\1/p" \
            <<<"$native_configuration")"
        free_revision="$(sed -n \
            "s/.*'settingsRevision': <uint64 \\([0-9][0-9]*\\)>.*/\\1/p" \
            <<<"$free_configuration")"
        [[ "$native_revision" =~ ^[0-9]+$ &&
           "$free_revision" =~ ^[0-9]+$ ]] || {
            printf 'Could not read revisions before selecting %s: native=%s free=%s\n' \
                "$style_id" "$native_revision" "$free_revision" >&2
            return 1
        }

        style_values="{'iconStyle': <'$style_id'>}"
        printf 'Selecting staged icon style %s on the private native and free hosts.\n' \
            "$style_id"
        native_style_reply="$(panel_call applyPanelSettingsTransaction \
            bottom "uint64 $native_revision" "$style_values" '{}')"
        free_style_reply="$(panel_call applyPanelSettingsTransaction \
            "$free_panel_id" "uint64 $free_revision" "$style_values" '{}')"
        [[ "$native_style_reply" == *"'success': <true>"* &&
           "$native_style_reply" == *"'status': <'succeeded'>"* &&
           "$free_style_reply" == *"'success': <true>"* &&
           "$free_style_reply" == *"'status': <'succeeded'>"* ]] || {
            printf 'Could not select icon style %s: native=%s free=%s\n' \
                "$style_id" "$native_style_reply" "$free_style_reply" >&2
            return 1
        }

        native_renderer=''
        free_renderer=''
        for ((attempt = 0; attempt < 50; ++attempt)); do
            native_renderer="$(panel_call panelRendererConfiguration bottom)"
            free_renderer="$(panel_call panelRendererConfiguration "$free_panel_id")"
            if [[ "$native_renderer" == *"'iconStyle': <'$style_id'>"* &&
                  "$native_renderer" == *"'iconStyleProjectionStatus': <'ready'>"* &&
                  "$native_renderer" == *"'manifestPath': <'$style_manifest'>"* &&
                  "$native_renderer" == *"'mode': <'original'>"* &&
                  "$free_renderer" == *"'iconStyle': <'$style_id'>"* &&
                  "$free_renderer" == *"'iconStyleProjectionStatus': <'ready'>"* &&
                  "$free_renderer" == *"'manifestPath': <'$style_manifest'>"* &&
                  "$free_renderer" == *"'mode': <'original'>"* ]]; then
                break
            fi
            sleep 0.1
        done
        [[ "$native_renderer" == *"'iconStyle': <'$style_id'>"* &&
           "$native_renderer" == *"'iconStyleProjectionStatus': <'ready'>"* &&
           "$native_renderer" == *"'manifestPath': <'$style_manifest'>"* &&
           "$native_renderer" == *"'mode': <'original'>"* ]] || {
            printf 'The native live host did not resolve icon style %s: %s\n' \
                "$style_id" "$native_renderer" >&2
            return 1
        }
        [[ "$free_renderer" == *"'iconStyle': <'$style_id'>"* &&
           "$free_renderer" == *"'iconStyleProjectionStatus': <'ready'>"* &&
           "$free_renderer" == *"'manifestPath': <'$style_manifest'>"* &&
           "$free_renderer" == *"'mode': <'original'>"* ]] || {
            printf 'The free live host did not resolve icon style %s: %s\n' \
                "$style_id" "$free_renderer" >&2
            return 1
        }
        kill -0 "$ARCHDOCK_RENDERING_SERVICE_PID"
        kill -0 "$ARCHDOCK_RENDERING_PLASMASHELL_PID"
        require_no_import_errors
    done

    local smoke_identity='desktop.org.archdock.archdock.desktop'
    local icon_properties_reply
    local icon_override_reply
    local icon_reset_reply
    local icon_entries=''
    native_configuration="$(panel_call dockConfiguration bottom)"
    native_revision="$(sed -n \
        "s/.*'settingsRevision': <uint64 \\([0-9][0-9]*\\)>.*/\\1/p" \
        <<<"$native_configuration")"
    [[ "$native_revision" =~ ^[0-9]+$ ]] || {
        printf 'Could not read the revision before the live Icon Properties smoke: %s\n' \
            "$native_configuration" >&2
        return 1
    }

    printf 'Opening Icon Properties for the stable private native entry.\n'
    icon_properties_reply="$(panel_call showIconProperties \
        bottom "$smoke_identity")"
    [[ "$icon_properties_reply" == *"'success': <true>"* &&
       "$icon_properties_reply" == *"'status': <'opened'>"* &&
       "$icon_properties_reply" == *"'entryIdentity': <'$smoke_identity'>"* &&
       "$icon_properties_reply" == *"'editorVisible': <true>"* ]] || {
        printf 'The live Icon Properties window did not open for the stable entry: %s\n' \
            "$icon_properties_reply" >&2
        return 1
    }

    printf 'Applying one stable-identity override through the staged backend path for live-scene refresh evidence.\n'
    icon_override_reply="$(panel_call applyIconOverrideTransaction \
        bottom "uint64 $native_revision" "$smoke_identity" \
        "{'customLabel': <'Private live override'>, 'tileEnabled': <false>, 'styleReference': <'metallic-blue'>}")"
    [[ "$icon_override_reply" == *"'success': <true>"* &&
       "$icon_override_reply" == *"'status': <'succeeded'>"* &&
       "$icon_override_reply" == *"'entryIdentity': <'$smoke_identity'>"* ]] || {
        printf 'The live Icon Properties apply transaction failed: %s\n' \
            "$icon_override_reply" >&2
        return 1
    }

    for ((attempt = 0; attempt < 50; ++attempt)); do
        icon_entries="$(panel_call dockEntriesForPanel bottom hybrid)"
        if [[ "$icon_entries" == *"'stableIdentity': <'$smoke_identity'>"* &&
              "$icon_entries" == *"'iconPropertiesSupported': <true>"* &&
              "$icon_entries" == *"'iconOverrideApplied': <true>"* &&
              "$icon_entries" == *"'displayName': <'Private live override'>"* &&
              "$icon_entries" == *"'tileEnabled': <false>"* &&
              "$icon_entries" == *"'id': <'metallic-blue'>"* ]]; then
            break
        fi
        sleep 0.1
    done
    [[ "$icon_entries" == *"'stableIdentity': <'$smoke_identity'>"* &&
       "$icon_entries" == *"'iconPropertiesSupported': <true>"* &&
       "$icon_entries" == *"'iconOverrideApplied': <true>"* &&
       "$icon_entries" == *"'displayName': <'Private live override'>"* &&
       "$icon_entries" == *"'tileEnabled': <false>"* &&
       "$icon_entries" == *"'id': <'metallic-blue'>"* ]] || {
        printf 'The live scene did not expose the committed icon override: %s\n' \
            "$icon_entries" >&2
        return 1
    }

    native_configuration="$(panel_call dockConfiguration bottom)"
    native_revision="$(sed -n \
        "s/.*'settingsRevision': <uint64 \\([0-9][0-9]*\\)>.*/\\1/p" \
        <<<"$native_configuration")"
    [[ "$native_revision" =~ ^[0-9]+$ ]] || {
        printf 'Could not read the revision before the live Icon Properties reset: %s\n' \
            "$native_configuration" >&2
        return 1
    }
    printf 'Resetting the staged entry through the backend path for live-scene refresh evidence.\n'
    icon_reset_reply="$(panel_call resetIconOverrideTransaction \
        bottom "uint64 $native_revision" "$smoke_identity")"
    [[ "$icon_reset_reply" == *"'success': <true>"* &&
       "$icon_reset_reply" == *"'status': <'succeeded'>"* &&
       "$icon_reset_reply" == *"'reset': <true>"* ]] || {
        printf 'The live Icon Properties reset transaction failed: %s\n' \
            "$icon_reset_reply" >&2
        return 1
    }

    icon_entries=''
    for ((attempt = 0; attempt < 50; ++attempt)); do
        icon_entries="$(panel_call dockEntriesForPanel bottom hybrid)"
        if [[ "$icon_entries" == *"'stableIdentity': <'$smoke_identity'>"* &&
              "$icon_entries" == *"'iconOverrideApplied': <false>"* &&
              "$icon_entries" == *"'displayName': <'Arch Dock'>"* &&
              "$icon_entries" == *"'id': <'dark-orb'>"* ]]; then
            break
        fi
        sleep 0.1
    done
    [[ "$icon_entries" == *"'stableIdentity': <'$smoke_identity'>"* &&
       "$icon_entries" == *"'iconOverrideApplied': <false>"* &&
       "$icon_entries" == *"'displayName': <'Arch Dock'>"* &&
       "$icon_entries" == *"'id': <'dark-orb'>"* ]] || {
        printf 'The live scene did not return to panel defaults after reset: %s\n' \
            "$icon_entries" >&2
        return 1
    }
    require_no_import_errors

    native_renderer=''
    free_renderer=''
    for ((attempt = 0; attempt < 50; ++attempt)); do
        native_renderer="$(panel_call panelRendererConfiguration bottom)"
        free_renderer="$(panel_call panelRendererConfiguration "$free_panel_id")"
        if [[ "$native_renderer" == *"'effectiveRendererTier': <'skinned2d'>"* &&
              "$native_renderer" == *"'themeProjectionStatus': <'ready'>"* &&
              "$native_renderer" == *"'id': <'energy-frame-cyan'>"* &&
              "$native_renderer" == *"'dynamic-glow'"* &&
              "$native_renderer" == *"'manifestPath': <'$ARCHDOCK_RENDERING_STAGED_THEME_MANIFEST'>"* &&
              "$free_renderer" == *"'effectiveRendererTier': <'skinned2d'>"* &&
              "$free_renderer" == *"'themeProjectionStatus': <'ready'>"* &&
              "$free_renderer" == *"'id': <'energy-frame-cyan'>"* &&
              "$free_renderer" == *"'dynamic-glow'"* &&
              "$free_renderer" == *"'manifestPath': <'$ARCHDOCK_RENDERING_STAGED_THEME_MANIFEST'>"* ]]; then
            break
        fi
        sleep 0.1
    done
    [[ "$native_renderer" == *"'effectiveRendererTier': <'skinned2d'>"* &&
       "$native_renderer" == *"'themeProjectionStatus': <'ready'>"* &&
       "$native_renderer" == *"'id': <'energy-frame-cyan'>"* &&
       "$native_renderer" == *"'dynamic-glow'"* &&
       "$native_renderer" == *"'manifestPath': <'$ARCHDOCK_RENDERING_STAGED_THEME_MANIFEST'>"* ]] || {
        printf 'The native live host did not resolve the cyan energy renderer: %s\n' \
            "$native_renderer" >&2
        return 1
    }
    [[ "$free_renderer" == *"'effectiveRendererTier': <'skinned2d'>"* &&
       "$free_renderer" == *"'themeProjectionStatus': <'ready'>"* &&
       "$free_renderer" == *"'id': <'energy-frame-cyan'>"* &&
       "$free_renderer" == *"'dynamic-glow'"* &&
       "$free_renderer" == *"'manifestPath': <'$ARCHDOCK_RENDERING_STAGED_THEME_MANIFEST'>"* ]] || {
        printf 'The free live host did not resolve the cyan energy renderer: %s\n' \
            "$free_renderer" >&2
        return 1
    }
    sleep 1
    kill -0 "$ARCHDOCK_RENDERING_SERVICE_PID"
    kill -0 "$ARCHDOCK_RENDERING_PLASMASHELL_PID"
    require_no_import_errors

    plasma_script \
        "var desktop = desktopById($desktop_id); var freeWidget = desktop ? desktop.widgetById($free_applet_id) : null; if (freeWidget) { freeWidget.remove(); } var panel = panelById($panel_id); if (panel) { panel.remove(); } print('removed');" \
        >/dev/null

    printf 'Private-Wayland Icon Properties UI interaction plus staged service/Studio and native/free skinned PanelScene hosts succeeded with all energy themes and production icon styles.\n'
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
    [[ -x "$build_dir/panel-window-capability-test" ]] || {
        printf 'Build the Icon Properties interaction target first: %s\n' \
            "$build_dir/panel-window-capability-test" >&2
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
    local theme_root="$stage_root/share/arch-dock/themes"
    local icon_style_root="$stage_root/share/arch-dock/icon-styles"
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
       -r "$module_root/IconStyleResolver.js" &&
       -r "$module_root/IconScene.qml" &&
       -r "$module_root/PanelScene.qml" &&
       -r "$module_root/PanelSurfaceLoader.qml" &&
       -r "$module_root/RunningIndicator.qml" &&
       -r "$module_root/previews/LivePanelPreview.qml" &&
       -r "$module_root/renderers/PanelProcedural2D.qml" &&
       -r "$module_root/renderers/PanelSkin2D.qml" &&
       -r "$module_root/renderers/PanelSkinLayer2D.qml" &&
       -r "$module_root/renderers/IconStyle2D.qml" &&
       -r "$module_root/inputs/AlphaHitMask.qml" ]] || {
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
    [[ -r "$theme_root/builtin-themes.json" ]] || {
        printf 'The staged built-in theme catalog is unavailable.\n' >&2
        return 1
    }
    [[ -r "$icon_style_root/builtin-icon-styles.json" ]] || {
        printf 'The staged built-in icon-style catalog is unavailable.\n' >&2
        return 1
    }
    local icon_style_id
    for icon_style_id in plain-original metallic-blue metallic-red \
                         neon-green neon-orange dark-orb; do
        [[ -r "$icon_style_root/$icon_style_id/archdock-icon-style.json" &&
           -r "$icon_style_root/$icon_style_id/metadata/production-record.json" &&
           -r "$icon_style_root/$icon_style_id/metadata/visual-review.json" ]] || {
            printf 'The staged icon-style package is incomplete: %s\n' \
                "$icon_style_id" >&2
            return 1
        }
    done
    local theme_id
    for theme_id in sci-fi-chassis-dark sci-fi-chassis-red sci-fi-chassis-blue; do
        [[ -r "$theme_root/$theme_id/archdock-theme.json" &&
           -r "$theme_root/$theme_id/assets/surface.svg" &&
           -r "$theme_root/$theme_id/assets/glow.svg" &&
           -r "$theme_root/$theme_id/masks/input.svg" &&
           -r "$theme_root/$theme_id/metadata/production-record.json" &&
           -r "$theme_root/$theme_id/metadata/visual-review.json" ]] || {
            printf 'The staged chassis package is incomplete: %s\n' "$theme_id" >&2
            return 1
        }
    done
    for theme_id in energy-frame-cyan energy-frame-green \
                    energy-frame-orange energy-frame-purple; do
        [[ -r "$theme_root/$theme_id/archdock-theme.json" &&
           -r "$theme_root/$theme_id/assets/surface.svg" &&
           -r "$theme_root/$theme_id/assets/frame-mask.svg" &&
           -r "$theme_root/$theme_id/assets/glow-mask.svg" &&
           -r "$theme_root/$theme_id/assets/energy-overlay-mask.svg" &&
           -r "$theme_root/$theme_id/assets/highlight-mask.svg" &&
           -r "$theme_root/$theme_id/masks/input.svg" &&
           -r "$theme_root/$theme_id/metadata/production-record.json" &&
           -r "$theme_root/$theme_id/metadata/visual-review.json" ]] || {
            printf 'The staged energy package is incomplete: %s\n' "$theme_id" >&2
            return 1
        }
    done
    local forbidden_reference
    forbidden_reference="$(find "$stage_root" -type f \
        \( -iname 'Screenshot_*' -o -path '*/source-samples/*' \) \
        -print -quit)"
    [[ -z "$forbidden_reference" ]] || {
        printf 'A reference-only source asset entered the staged install: %s\n' \
            "$forbidden_reference" >&2
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
        ARCHDOCK_RENDERING_ICON_PROPERTIES_INTERACTION_TEST="$build_dir/panel-window-capability-test" \
        ARCHDOCK_RENDERING_PANEL_SKIN_TEST="$ARCHDOCK_RENDERING_SCRIPT_DIR/tst_PanelSkin2D.qml" \
        ARCHDOCK_RENDERING_QMLTESTRUNNER="$qmltestrunner_binary" \
        ARCHDOCK_RENDERING_SMOKE_DESKTOP_FILE="$stage_root/share/applications/org.archdock.ArchDock.desktop" \
        ARCHDOCK_RENDERING_STAGED_ICON_STYLE_ROOT="$icon_style_root" \
        ARCHDOCK_RENDERING_STAGED_THEME_ROOT="$theme_root" \
        ARCHDOCK_RENDERING_STAGED_THEME_MANIFEST="$theme_root/energy-frame-cyan/archdock-theme.json" \
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
