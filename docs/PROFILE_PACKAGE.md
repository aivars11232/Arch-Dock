# Profile storage and transactions

A Profile is a complete managed panel arrangement. It is distinct from a
single-panel Panel Preset, an Icon Preset, a theme, an icon style and an
animation profile. Loading a profile never applies it.

## Version 1 definition

`org.archdock.profile` uses `schemaVersion: 1` with these required fields:

| Field | Contract |
| --- | --- |
| `id` | Stable lowercase identifier, at most 64 characters; generated user IDs begin `profile-`. |
| `name` | Nonempty trimmed name, at most 256 UTF-8 bytes, without control characters. |
| `revision` | Positive revision. Replacing a stored profile requires its current revision and advances it once. |
| `screenPolicy` | `stable-id-then-index`: resolve the stored display identity first, then the existing bounded screen index. |
| `metadata` | Bounded, inert JSON object. |
| `panels` | One to 64 complete persisted panel definitions. |

The definition is limited to 4 MiB. Unknown top-level fields and future profile
versions are rejected. Nested legacy panel records use the existing panel
migration; they become current version-2 definitions. No migration invents an
older profile format that never existed.

Panel content/order, segments, placement, layout, theme and icon references,
entry overrides, visibility/presentation and motion settings are retained.
Optional preset lineage is information, not a dependency on the source preset.
Live containment/applet IDs, ownership tokens, recovery status and settings
revisions are reset in a portable capture. Hover, drag, popup, rotation offset
and transition state are excluded by the existing durable panel serializer;
definitions explicitly supplying runtime state or live host claims are rejected.

## Store and effective references

Profiles live under the application's `AppDataLocation/profiles`, one
`<id>.json` per definition, separate from active registry settings and presets.
The store allows at most 512 profiles. Writes use owner-only permissions,
`QSaveFile`, revision checks and independent parsed readback. Managed symbolic
link paths and files are refused. A malformed or newer-version file remains
untouched and is reported without hiding valid profiles.

Resource resolution works on an effective copy. An unavailable theme uses
procedural 2D; an unavailable icon style uses `plain-original`; missing motion
references use a safe disabled motion. Diagnostics identify the affected panel.
The requested definition remains available for round-trip and later resolution.
Opening or listing the store does not create a directory or an active profile.
An explicit capture of the current registry supplies a real initial arrangement.

## Explicit management and interchange

Panel Studio's Profiles pages expose Create from Current, Save Current to
Profile, Rename, Duplicate, Delete, Import, Export and Apply Profile. Management
is disabled while a settings draft or desktop audition is pending. The service
also rejects Edit Mode, popup and drag conflicts. Selection and import do not
apply an arrangement. The legacy `/Control.applyProfile` appearance selector
keeps its existing contract; full arrangements use `/Profiles`, interface
`org.archdock.Profiles`.

Rename, save, duplicate, delete, export and apply require the displayed profile
revision. Import generates a fresh local profile identity and panel identities;
it never adopts another machine's native associations or shortcut bindings.
Export creates a new JSON file and an adjacent `<filename>.assets` directory,
without overwriting an existing destination. Asset references use
`profile-asset:<relative-path>`. Import resolves only those references under the
adjacent asset directory and copies validated artwork into the managed store's
content-addressed assets directory. Duplicates share immutable managed artwork;
deleting a definition does not remove artwork another profile can reference.

The existing theme package validator checks formats and paths. Transfers also
reject symbolic links, traversal, absolute/remote references, executable file
types and SVG scripts, event handlers, external links, entities, DTDs and
external CSS resources. Transfers are bounded to 32 MiB and 256 unique files.
Profiles are inert configuration; import does not run a script or install code.

## Complete panel-set apply and recovery

`ProfileApplyTransaction` preflights the entire current set and its durable
revisions, resolves effective resources/screens, and captures every owned host
before mutation. Native snapshots include actual placement, renderer and
visibility; free snapshots include actual applet geometry. The coordinator
requires a versioned configuration snapshot after preflight and before its
first journal or host mutation. If capture/retention fails, apply remains idle
and no host or registry change occurs. The shared snapshot covers user profiles,
presets/defaults, managed themes, settings and shortcut data; installed built-ins
are excluded. The commands and limits are documented in
[INSTALL.md](INSTALL.md#configuration-recovery-and-upgrades).

The coordinator
writes an owner-only `profile-apply-journal.json.backup.json` and a recovery
journal under AppDataLocation before its first host mutation.

Host changes are serial. Replacements are created and verified before old
owned hosts are removed. A free panel moved to another display receives a
replacement applet. Native placement and visibility use the existing Plasma
adapter with readback; free geometry uses the verified owned-container
workaround for Plasma 6's no-op scripting geometry setter. Native visibility
does not independently persist a fallback during a profile transaction.

Only one final checked registry write publishes the complete declared set.
Display identity is resolved first, followed by the bounded stored index;
fallbacks are reported. Unsupported placement or visibility is refused before
mutation. Personal Plasma panels and widgets without the exact ownership token
are outside the transaction.

Failure removes provisional owned hosts and restores backed-up hosts where
the full previous registry still matches. Recreated hosts retain the original
token; their verified new physical associations are committed together with
advanced revisions. A refused settings write leaves its previous records
available for rollback. Concurrent changes, uncertain ownership, malformed
records or failed restoration retain a precise BLOCKED journal and backup.
No foreign configuration is overwritten to force recovery.

Restart reads the journal and exposes recovery state without applying a
profile or changing a host. Recover Interrupted Apply explicitly retries the
checked restoration or finalizes an already committed, verified set. The
backup is retained after success; the active recovery journal is removed.
Inspect `getStatus()` for errors, rollback errors, diagnostics and record paths.

Saving an existing profile whose nested legacy panels require a destructive
rewrite captures its original bytes first. Current-schema saves do not create
an unnecessary migration snapshot. Pure loads and future-version rejection
leave the original file unchanged. Configuration restore refuses pending
profile recovery so the two journal owners cannot race.

## KDE profile shortcuts

The Profiles / Shortcuts page provides an explicit enable switch, a portable
Qt key sequence field, Assign Shortcut and Remove Shortcut. One key combination
is supported per assignment, for example `Ctrl+Alt+F9`. Shortcuts are off by
default. Saving an assignment while disabled stores data without registering
a key. Shortcut bindings are separate owner-only version-1 JSON in
`AppDataLocation/profile-shortcuts.json`; they are never imported or exported
with a portable profile.

The service exposes `getShortcutStatus`, `setShortcutsEnabled`,
`setProfileShortcut` and `clearProfileShortcut` on `/Profiles`. Management uses
the same interaction/recovery guards as profile CRUD. Enabling restores only
bindings for valid local profiles. A previously explicit enabled preference
can restore registrations on restart, but it does not apply any arrangement.

The adapter uses [KDE KGlobalAccel](https://api.kde.org/kglobalaccel.html),
component `org.archdock.ArchDock`, and the stable profile ID as each QAction's
object name. Registration uses `NoAutoloading`, checks exact and prefix
conflicts, and verifies the native shortcut readback. It never invokes a
systemwide stealing or whole-component cleanup operation. Conflicts expose
their existing action/owner and leave existing keys intact. Native registration
or persistence failures restore previous owned bindings where possible;
uncertain cleanup is reported distinctly.

Activation resolves the profile's current saved revision and calls the same
full-set apply transaction. Renaming does not retarget an action. An invalid
or missing profile cannot register or apply. A configured profile is deleted
only after its native binding is removed and verified. Disabling the feature
removes active registrations while retaining desired assignments. Normal
application shutdown also unregisters its owned actions; the saved opt-in
allows registration on the next launch. Status includes desired sequences,
native registered keys, conflicts and the last activation result.

The store itself has no desktop mutation or executable path. Its import path
supports literal spaces and Unicode filenames while rejecting encoded paths,
control characters, schemes, traversal and symbolic links.
