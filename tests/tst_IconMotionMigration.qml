import QtQuick
import QtTest
import ArchDock.Rendering 1.0
import "../plasma-dock-widget/contents/ui" as DockUi

// Verifies that every effect now runs through the animation-profile engine,
// using the real shipped catalog rather than a fixture. It drives the
// production DockEntry so a migrated preset is proved end to end: the profile
// binds, the configured trigger applies, and every declared track claims its
// channel without a conflict.
TestCase {
    id: testCase

    name: "IconMotionMigration"
    when: windowShown
    width: 260
    height: 220

    readonly property var catalog: loadCatalog()

    Component {
        id: dockEntryComponent

        DockUi.DockEntry {}
    }

    function loadCatalog() {
        const request = new XMLHttpRequest()
        request.open("GET", Qt.resolvedUrl(
            "../data/animation-profiles/builtin-animation-profiles.json"),
            false)
        request.send(null)
        return JSON.parse(request.responseText)
    }

    function catalogMap() {
        const result = ({})
        const profiles = catalog.animationProfiles || []
        for (let index = 0; index < profiles.length; ++index)
            result[profiles[index].id] = profiles[index]
        return result
    }

    // Mirrors AnimationProfileCatalog::profileIdForLegacyName.
    function profileFor(name) {
        const profiles = catalog.animationProfiles || []
        for (let index = 0; index < profiles.length; ++index) {
            if (profiles[index].id === name)
                return profiles[index]
        }
        for (let index = 0; index < profiles.length; ++index) {
            const legacy = profiles[index].legacyNames || []
            if (legacy.indexOf(name) >= 0)
                return profiles[index]
        }
        return null
    }

    function createEntry(properties) {
        const values = {
            entry: {
                appId: "org.example.app",
                stableIdentity: "application.org.example.app",
                displayName: "Example",
                iconName: "application-x-executable",
                pinned: true,
                iconPropertiesSupported: true,
                running: false,
                active: false,
                minimized: false,
                attention: false,
                windowCount: 1
            },
            entryIndex: 1,
            vertical: false,
            baseSize: 60,
            magnification: 1.7,
            magnificationEnabled: true,
            hoveredIndex: -1,
            tileShape: "rounded",
            appearance: "glass",
            showReflection: false,
            showIndicator: true,
            showTooltip: false,
            motion: "scale",
            motionTrigger: "hover",
            motionIntensity: 1,
            motionDuration: 170,
            motionSpeed: 1,
            reducedMotion: false,
            inputEnabled: true,
            editMode: false,
            acceptDrops: true,
            invoke: function() {},
            reorder: function() {},
            pinUrls: function() {},
            setHoveredIndex: function() {},
            openPanelStudio: function() {},
            openIconProperties: function() {}
        }
        const additions = properties || ({})
        for (const key of Object.keys(additions))
            values[key] = additions[key]
        const item = createTemporaryObject(
            dockEntryComponent, testCase, values)
        verify(item !== null)
        wait(0)
        return item
    }

    function test_catalogLoadsAndDeclaresAFallback() {
        compare(catalog.format, "org.archdock.animation-profile-catalog")
        compare(catalog.version, 1)
        compare(catalog.fallbackProfileId, "none")
        verify((catalog.animationProfiles || []).length > 0)
    }

    // Every selectable effect name, including legacy aliases.
    function selectableNames() {
        const result = []
        const profiles = catalog.animationProfiles || []
        for (let index = 0; index < profiles.length; ++index) {
            result.push(profiles[index].id)
            const legacy = profiles[index].legacyNames || []
            for (let alias = 0; alias < legacy.length; ++alias)
                result.push(legacy[alias])
        }
        return result
    }

    // Every effect must bind a validated profile and actually claim the
    // properties that profile declares.
    function test_everyMigratedEffectRunsThroughItsProfile() {
        const migrated = selectableNames()
        verify(migrated.length > 0)

        for (let index = 0; index < migrated.length; ++index) {
            const name = migrated[index]
            const profile = profileFor(name)
            verify2(profile !== null, "no profile for " + name)

            const item = createEntry({
                motion: name,
                motionTrigger: "hover",
                animationProfiles: [profile],
                animationCatalog: catalogMap()
            })
            compare(item.boundAnimationProfiles.length, 1)
            // The configured trigger overrides the profile's nominal one.
            compare(item.boundAnimationProfiles[0].trigger, "hover-hold")

            item.motionController.hovered = true
            wait(0)

            const tracks = profile.tracks || []
            compare(item.motionController.conflicts.length, 0)
            compare(item.motionController.activeTracks.length, tracks.length)
            for (let t = 0; t < tracks.length; ++t) {
                const target = tracks[t].target || profile.target
                verify2(item.motionController.hasChannel(
                            target, tracks[t].property),
                        name + " does not claim " + target + "/"
                        + tracks[t].property)
            }
        }
    }

    // Every legacy name the settings schema can still hold must resolve to a
    // profile rather than silently falling back to "none".
    function test_everyLegacySettingValueStillResolves() {
        const legacyValues = [
            "none", "bounce", "elastic", "pulse", "scale", "spin",
            "idle-rotate", "orbit", "swing", "wobble", "wiggle", "shake",
            "glow", "breathe", "float", "wave", "ripple", "magnetic", "spring"
        ]
        for (let index = 0; index < legacyValues.length; ++index) {
            const name = legacyValues[index]
            const profile = profileFor(name)
            verify2(profile !== null, name + " no longer resolves")
        }
        // And the catalog offers nothing the settings schema cannot select.
        const offered = selectableNames().slice().sort()
        compare(offered, legacyValues.slice().sort())
    }

    function test_reducedMotionRestsEveryMigratedEffect() {
        const migrated = selectableNames()
        for (let index = 0; index < migrated.length; ++index) {
            const name = migrated[index]
            const item = createEntry({
                motion: name,
                motionTrigger: "hover",
                reducedMotion: true,
                animationProfiles: [profileFor(name)],
                animationCatalog: catalogMap()
            })
            item.motionController.hovered = true
            wait(0)
            // Current behaviour under reduced motion is a complete rest.
            compare(item.motionController.activeTracks.length, 0)
            compare(item.motionController.channelValue("icon", "translate-y"), 0)
            compare(item.motionController.channelValue("icon", "scale"), 1)
            compare(item.motionController.channelValue("icon", "rotate-z"), 0)
        }
    }

    function test_legacyNamesResolveToTheirMigratedProfile() {
        // `scale` is the one legacy alias in the shipped catalog.
        const resolved = profileFor("scale")
        verify(resolved !== null)
        compare(resolved.id, "pulse")
    }

    function verify2(condition, message) {
        if (!condition)
            fail(message)
        verify(true)
    }
}
