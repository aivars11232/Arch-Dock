# Requirement coverage matrix

This matrix proves that every legacy remaining task from TASK-0022 through TASK-0086 is included exactly once in the 24 active consolidated tasks. The full old phase text is embedded in the corresponding new task specification.

| Legacy task | Legacy title | New task | Implementation bullets | Acceptance criteria | Verification bullets | Exclusion bullets |
|---|---|---:|---:|---:|---:|---:|
| TASK-0022 | Introduce the version-2 PanelDefinition model | TASK-0022 | 5 | 4 | 2 | 3 |
| TASK-0023 | Separate PanelRuntimeState from persisted configuration | TASK-0022 | 4 | 3 | 2 | 2 |
| TASK-0024 | Implement migration from current flat records | TASK-0022 | 5 | 4 | 3 | 2 |
| TASK-0025 | Add atomic panel revisions and transactional updates | TASK-0023 | 5 | 4 | 3 | 2 |
| TASK-0026 | Implement host and theme capability resolution | TASK-0023 | 5 | 4 | 3 | 2 |
| TASK-0027 | Unify schema validation and settings transactions | TASK-0023 | 6 | 4 | 3 | 2 |
| TASK-0028 | Package the shared ArchDock.Rendering QML module | TASK-0024 | 4 | 3 | 4 | 2 |
| TASK-0029 | Consolidate geometry into one LayoutEngine | TASK-0024 | 5 | 4 | 3 | 2 |
| TASK-0030 | Create the shared PanelScene contract | TASK-0024 | 5 | 4 | 3 | 2 |
| TASK-0031 | Bridge the live Plasma applet into PanelScene | TASK-0025 | 4 | 4 | 4 | 3 |
| TASK-0032 | Create the layered shared IconScene and indicator contract | TASK-0025 | 5 | 4 | 3 | 2 |
| TASK-0033 | Replace Panel Studio mock preview with the shared live scene | TASK-0025 | 5 | 4 | 4 | 2 |
| TASK-0034 | Specify Theme Package version 2 | TASK-0026 | 4 | 4 | 3 | 3 |
| TASK-0035 | Implement Theme v2 parsing, validation, and v1 compatibility | TASK-0026 | 5 | 4 | 3 | 2 |
| TASK-0036 | Add the source-asset catalog and provenance model | TASK-0026 | 4 | 4 | 3 | 2 |
| TASK-0037 | Implement the non-destructive asset-processing pipeline | TASK-0026 | 5 | 4 | 4 | 3 |
| TASK-0038 | Classify all panel and icon samples and wire theme capabilities | TASK-0026 | 4 | 4 | 4 | 3 |
| TASK-0039 | Prepare the first chassis skin assets | TASK-0027 | 4 | 4 | 4 | 3 |
| TASK-0040 | Implement the scalable chassis skin renderer and states | TASK-0027 | 5 | 4 | 4 | 3 |
| TASK-0041 | Add chassis variants and close the 2D family | TASK-0027 | 5 | 4 | 4 | 2 |
| TASK-0042 | Prepare layered energy-frame assets | TASK-0028 | 5 | 4 | 3 | 3 |
| TASK-0043 | Implement energy skin rendering, hover glow, and open/close parts | TASK-0028 | 5 | 4 | 4 | 2 |
| TASK-0044 | Package energy variants and verify reduced-motion behavior | TASK-0028 | 6 | 4 | 4 | 2 |
| TASK-0045 | Define and store the icon-style package contract | TASK-0029 | 4 | 4 | 3 | 2 |
| TASK-0046 | Implement layered icon rendering and first style families | TASK-0029 | 5 | 4 | 5 | 3 |
| TASK-0047 | Persist and apply per-icon style overrides | TASK-0029 | 5 | 4 | 3 | 2 |
| TASK-0048 | Connect Icon Properties to the live context menu and verify states | TASK-0029 | 6 | 4 | 3 | 2 |
| TASK-0049 | Define the animation-profile schema and validator | TASK-0030 | 4 | 4 | 2 | 2 |
| TASK-0050 | Implement trigger dispatch and deterministic track composition | TASK-0030 | 5 | 4 | 3 | 2 |
| TASK-0051 | Migrate existing icon effects into profile presets | TASK-0030 | 4 | 4 | 3 | 2 |
| TASK-0052 | Implement slow Y-axis turn, jump, and shake presets | TASK-0031 | 5 | 4 | 4 | 2 |
| TASK-0053 | Implement enlarge, neighbor influence, spiral, and orbit presets | TASK-0031 | 5 | 4 | 4 | 2 |
| TASK-0054 | Complete launch-event truth, reduced motion, and animation safety | TASK-0031 | 7 | 5 | 4 | 2 |
| TASK-0055 | Implement the panel presentation state machine | TASK-0032 | 5 | 4 | 2 | 2 |
| TASK-0056 | Add presentation guards and interaction locks | TASK-0032 | 5 | 3 | 3 | 2 |
| TASK-0057 | Implement horizontal, vertical, radial opening mechanisms and panel glow | TASK-0032 | 6 | 4 | 3 | 2 |
| TASK-0058 | Integrate host visibility with surface presentation and verify closed-shell behavior | TASK-0032 | 6 | 5 | 4 | 2 |
| TASK-0059 | Unify free-panel launcher, tasks, hybrid, and ordering semantics | TASK-0033 | 5 | 4 | 4 | 2 |
| TASK-0060 | Harden ring, arc, polygon, fan, and spiral geometry | TASK-0033 | 5 | 4 | 4 | 2 |
| TASK-0061 | Implement transformed free-panel input regions and whole-panel rotation | TASK-0033 | 5 | 4 | 4 | 2 |
| TASK-0062 | Complete free/multi-shape lifecycle and layout integration tests | TASK-0033 | 5 | 4 | 3 | 2 |
| TASK-0063 | Implement the baked 2.5D renderer contract | TASK-0034 | 5 | 4 | 3 | 2 |
| TASK-0064 | Implement ring, octagonal, and arc 2.5D theme families | TASK-0034 | 5 | 4 | 5 | 3 |
| TASK-0065 | Verify 2.5D depth, rotation, fallback, and performance | TASK-0034 | 5 | 4 | 4 | 2 |
| TASK-0066 | Add optional true-3D build and capability detection | TASK-0035 | 5 | 4 | 4 | 2 |
| TASK-0067 | Implement the true-3D scene contract and base renderer | TASK-0035 | 5 | 4 | 4 | 2 |
| TASK-0068 | Implement true-3D icon and panel motion | TASK-0036 | 5 | 4 | 4 | 2 |
| TASK-0069 | Complete 3D fallback, editor gating, and regression coverage | TASK-0036 | 6 | 4 | 4 | 2 |
| TASK-0070 | Add the grouped-window preview model and popup | TASK-0037 | 5 | 4 | 4 | 2 |
| TASK-0071 | Implement individual-window actions and complete the application context menu | TASK-0037 | 6 | 4 | 4 | 2 |
| TASK-0072 | Complete window preview and context-menu regression coverage | TASK-0037 | 4 | 4 | 3 | 2 |
| TASK-0073 | Implement folder entries and expansion layouts | TASK-0038 | 5 | 4 | 3 | 2 |
| TASK-0074 | Implement panel segments and independent segment surfaces | TASK-0038 | 4 | 4 | 4 | 2 |
| TASK-0075 | Implement badges, progress overlays, notifications, and status modules | TASK-0039 | 5 | 4 | 4 | 3 |
| TASK-0076 | Complete remaining content-system tests and remove placeholders | TASK-0039 | 5 | 4 | 4 | 2 |
| TASK-0077 | Implement Panel/Icon preset definitions, catalogs, browsers, and the 15+15 built-in libraries | TASK-0040 | 9 | 6 | 5 | 4 |
| TASK-0078 | Implement transactional live desktop preset audition and active/custom/default workflows | TASK-0041 | 10 | 8 | 7 | 5 |
| TASK-0079 | Implement the profile store and schema | TASK-0042 | 5 | 4 | 2 | 3 |
| TASK-0080 | Implement profile CRUD, import/export, apply, and rollback | TASK-0042 | 5 | 4 | 4 | 3 |
| TASK-0081 | Add profile global shortcuts with safe conflict handling | TASK-0042 | 5 | 4 | 3 | 2 |
| TASK-0082 | Unify service startup, D-Bus activation, and executable install paths | TASK-0043 | 5 | 4 | 4 | 2 |
| TASK-0083 | Create Arch packaging and verified install/uninstall flow | TASK-0043 | 5 | 5 | 4 | 2 |
| TASK-0084 | Add configuration backup, migration rollback, and upgrade safety | TASK-0044 | 6 | 4 | 3 | 2 |
| TASK-0085 | Complete multi-monitor, scaling, accessibility, diagnostics, and performance hardening | TASK-0044 | 5 | 6 | 4 | 2 |
| TASK-0086 | Run the release regression suite and prepare versioned release evidence | TASK-0045 | 5 | 8 | 5 | 3 |

## Preservation totals

- Legacy tasks mapped: 65 of 65.
- Required implementation bullets preserved under internal phases: 332.
- Acceptance criteria preserved under internal phases: 272.
- Verification bullets preserved under internal phases: 233.
- Explicit exclusions preserved under internal phases: 149.
- Duplicate legacy mappings: 0.
- Missing legacy mappings: 0.
