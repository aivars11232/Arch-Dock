# Arch Dock target structure tree — version 2

This is the dependency-aware logical target for the 86-task plan. It is not authorization for a one-step repository rewrite. Codex introduces each branch only in the task that owns it and keeps the application buildable after every meaningful change.

```text
Arch Dock/
├── CMakeLists.txt
├── PKGBUILD                              # Added in packaging task
├── docs/
│   ├── MASTER_ARCHITECTURE_AND_IMPLEMENTATION_PLAN_V2.md
│   ├── CURRENT_STATE.md
│   ├── TARGET_STRUCTURE_TREE_V2.md
│   ├── PRESET_SYSTEM_SPEC.md
│   ├── THEME_PACKAGE_V2.md
│   ├── ICON_STYLE_PACKAGE.md
│   ├── ANIMATION_PROFILE.md
│   ├── PLASMA_LIFECYCLE.md
│   ├── ARCH_KDE_RUNTIME_MATRIX.md
│   └── RELEASE_CHECKLIST.md
│
├── src/
│   ├── main.cpp
│   │
│   ├── model/
│   │   ├── PanelDefinition.*
│   │   ├── PanelRuntimeState.*
│   │   ├── ThemeDefinition.*
│   │   ├── IconStyleDefinition.*
│   │   ├── AnimationProfile.*
│   │   ├── PanelPresetDefinition.*
│   │   ├── IconPresetDefinition.*
│   │   ├── PresetOrigin.*
│   │   └── SettingsMigration.*
│   │
│   ├── panel/
│   │   ├── PanelWindow.*                # Stable D-Bus/UI façade
│   │   ├── PanelManager.*
│   │   ├── NativePanelController.*
│   │   ├── FreePanelController.*
│   │   ├── PanelVisibilityController.*
│   │   └── PanelPresentationPolicy.*
│   │
│   ├── integration/
│   │   ├── PlasmaPanelAdapter.*
│   │   ├── PlasmaDesktopHostAdapter.*
│   │   ├── KWinActionBridge.*
│   │   ├── ScreenIdentity.*
│   │   └── SystemStatus.*
│   │
│   ├── content/
│   │   ├── DockModel.*
│   │   ├── PanelContentModel.*
│   │   ├── WindowModel.*
│   │   ├── WindowWatcher.*
│   │   └── WindowPreviewModel.*
│   │
│   ├── themes/
│   │   ├── ThemeCatalog.*
│   │   ├── ThemePackage.*
│   │   ├── ThemeAssetProcessor.*
│   │   ├── ThemeRenderCache.*
│   │   └── ThemeCapabilityResolver.*
│   │
│   ├── presets/
│   │   ├── PanelPresetCatalog.*
│   │   ├── IconPresetCatalog.*
│   │   ├── BuiltInPresetLoader.*
│   │   ├── UserPresetStore.*
│   │   ├── PresetPackage.*
│   │   ├── PresetCapabilityResolver.*
│   │   ├── PresetApplicationService.*
│   │   ├── PresetPreviewSession.*
│   │   ├── PresetPreviewRecovery.*
│   │   └── PresetDefaultStore.*
│   │
│   └── persistence/
│       ├── PanelRegistry.*
│       ├── DockSettings.*
│       └── ProfileStore.*
│
├── qml/
│   ├── ArchDock/Rendering/
│   │   ├── qmldir
│   │   ├── PanelScene.qml
│   │   ├── PanelSurfaceLoader.qml
│   │   ├── PanelPresentationController.qml
│   │   ├── IconScene.qml
│   │   ├── IconMotionController.qml
│   │   ├── PanelMotionController.qml
│   │   ├── RunningIndicator.qml
│   │   ├── LayoutEngine.js
│   │   ├── renderers/
│   │   │   ├── PanelProcedural2D.qml
│   │   │   ├── PanelSkin2D.qml
│   │   │   ├── PanelBaked25D.qml
│   │   │   ├── PanelScene3D.qml
│   │   │   ├── IconStyle2D.qml
│   │   │   └── IconStyle3D.qml
│   │   ├── effects/
│   │   ├── previews/
│   │   │   ├── LivePanelPreview.qml
│   │   │   ├── PresetCardPreview.qml
│   │   │   └── WindowPreviewPopup.qml
│   │   └── inputs/
│   │
│   └── studio/
│       ├── SettingsPopup.qml
│       ├── StudioForm.qml
│       ├── ThemeBrowser.qml
│       ├── IconStyleBrowser.qml
│       ├── AnimationProfileEditor.qml
│       ├── LivePanelPreview.qml
│       └── presets/
│           ├── PanelPresetBrowser.qml
│           ├── IconPresetBrowser.qml
│           ├── PresetCard.qml
│           ├── PresetDetails.qml
│           ├── PresetActionBar.qml
│           ├── DesktopAuditionStatus.qml
│           └── UserPresetSaveDialog.qml
│
├── plasma-dock-widget/
│   └── contents/
│       ├── config/
│       └── ui/
│           ├── main.qml                 # Plasma host/data bridge
│           ├── DockEntry.qml            # Entry interaction adapter
│           └── native configuration pages
│
├── data/
│   ├── presets/
│   │   ├── panels/                      # Exactly 15 valid built-ins at first release
│   │   └── icons/                       # Exactly 15 valid built-ins at first release
│   ├── themes/
│   ├── icon-styles/
│   ├── dbus/
│   └── systemd/
│
├── assets/
│   ├── source-samples/                  # Not installed by default
│   ├── themes/
│   ├── icon-styles/
│   └── common/
│       ├── masks/
│       ├── shaders/
│       └── materials/
│
└── tests/
    ├── unit/
    │   ├── panel lifecycle
    │   ├── schema/migration
    │   ├── theme/icon style
    │   ├── preset definitions/catalogs
    │   └── preview session/rollback
    ├── qml/
    │   ├── geometry/rendering
    │   ├── preset browsers/cards
    │   └── animation/presentation
    ├── integration/
    │   ├── native/free host lifecycle
    │   ├── profile transactions
    │   └── preset apply/audition/recovery
    ├── visual/
    │   ├── renderer tiers
    │   └── preset preview parity
    └── plasma-session/
        ├── Arch Linux KDE Plasma 6 Wayland matrix
        ├── preset audition matrix
        └── install/upgrade/uninstall matrix
```

## Boundary rules

1. `PanelWindow` remains the stable D-Bus façade while logic is delegated incrementally.
2. A theme is not a panel preset.
3. An icon style is not an icon preset.
4. A panel or icon preset is not a profile.
5. Built-in presets are immutable installed resources.
6. Active panel configuration stores a resolved definition, not a fragile pointer-only reference.
7. Preview state belongs in `PresetPreviewSession`, not durable panel configuration.
8. The live applet, embedded previews and preset cards use the same `PanelScene`/`IconScene` renderer.
9. Real-desktop audition may mutate only a verified Arch Dock-owned host or a temporary preview-owned host.
10. Arch Linux/KDE Plasma 6/Wayland runtime evidence is mandatory where a task claims desktop behavior.
