import QtQuick 2.15
import QtTest 1.3
import "../qml/runtime/StudioDraft.js" as StudioDraft

TestCase {
    name: "StudioDraft"

    function test_hasValueDistinguishesMissingKeys() {
        const draft = {
            opacity: 0,
            enabled: false,
            optional: null
        }

        verify(StudioDraft.hasValue(draft, "opacity"))
        verify(StudioDraft.hasValue(draft, "enabled"))
        verify(StudioDraft.hasValue(draft, "optional"))
        verify(!StudioDraft.hasValue(draft, "missing"))
        verify(!StudioDraft.hasValue(null, "missing"))
        verify(!StudioDraft.hasValue(undefined, "missing"))
    }

    function test_valuePreservesFalsyValuesAndUsesFallbackForMissingKeys() {
        const draft = {
            opacity: 0,
            enabled: false,
            title: "",
            optional: null
        }

        compare(StudioDraft.value(draft, "opacity", 0.9), 0)
        compare(StudioDraft.value(draft, "enabled", true), false)
        compare(StudioDraft.value(draft, "title", "fallback"), "")
        compare(StudioDraft.value(draft, "optional", "fallback"), null)
        compare(StudioDraft.value(draft, "missing", "fallback"), "fallback")
    }

    function test_setValueReturnsAnIndependentCopy() {
        const original = {
            opacity: 0.9,
            appearance: "glass"
        }
        const updated = StudioDraft.setValue(original, "opacity", 0.5)

        compare(original.opacity, 0.9)
        compare(updated.opacity, 0.5)
        compare(updated.appearance, "glass")
        verify(updated !== original)
    }

    function test_removeValueDoesNotMutateItsInput() {
        const original = {
            opacity: 0.9,
            appearance: "glass"
        }
        const updated = StudioDraft.removeValue(original, "opacity")

        compare(original.opacity, 0.9)
        verify(!StudioDraft.hasValue(updated, "opacity"))
        compare(updated.appearance, "glass")
    }

    function test_setComparedValueRemovesValuesMatchingTheBaseline() {
        const changed = StudioDraft.setComparedValue({}, "opacity", 0.5, 0.9)
        compare(changed.opacity, 0.5)

        const restored = StudioDraft.setComparedValue(changed, "opacity", 0.9, 0.9)
        verify(!StudioDraft.hasValue(restored, "opacity"))
        verify(StudioDraft.equivalent(1.0, 1))
        verify(!StudioDraft.equivalent("1", 1))
    }

    function test_mergeReturnsACopyWithoutMutatingEitherInput() {
        const original = {
            opacity: 0.9,
            appearance: "glass"
        }
        const changes = {
            opacity: 0.4,
            iconSize: 64
        }
        const merged = StudioDraft.merge(original, changes)

        compare(original.opacity, 0.9)
        verify(!StudioDraft.hasValue(original, "iconSize"))
        compare(changes.opacity, 0.4)
        compare(merged.opacity, 0.4)
        compare(merged.appearance, "glass")
        compare(merged.iconSize, 64)
        verify(merged !== original)
        verify(merged !== changes)
    }

    function test_mergeAcceptsEmptyInputs() {
        const fromNull = StudioDraft.merge(null, { opacity: 0.7 })
        const withNull = StudioDraft.merge({ opacity: 0.7 }, null)

        compare(fromNull.opacity, 0.7)
        compare(withNull.opacity, 0.7)
    }

    function test_clearReturnsFreshEmptyMaps() {
        const first = StudioDraft.clear()
        const second = StudioDraft.clear()

        compare(StudioDraft.keyCount(first), 0)
        compare(StudioDraft.keyCount(second), 0)
        verify(first !== second)
    }

    function test_keyCountHandlesDraftMapsAndEmptyValues() {
        compare(StudioDraft.keyCount({}), 0)
        compare(StudioDraft.keyCount({ one: 1, two: false }), 2)
        compare(StudioDraft.keyCount(null), 0)
        compare(StudioDraft.keyCount(undefined), 0)
    }

    function test_nestedMapsTrackIndependentPanelDrafts() {
        let drafts = {}
        drafts = StudioDraft.setNestedMap(
            drafts,
            "free-1",
            StudioDraft.setValue({}, "opacity", 0.5))
        drafts = StudioDraft.setNestedMap(
            drafts,
            "bottom",
            StudioDraft.setValue({}, "iconSize", 64))

        compare(StudioDraft.nestedMap(drafts, "free-1").opacity, 0.5)
        compare(StudioDraft.nestedMap(drafts, "bottom").iconSize, 64)
        compare(StudioDraft.nestedKeyCount(drafts), 2)

        drafts = StudioDraft.setNestedMap(drafts, "free-1", {})
        verify(!StudioDraft.hasValue(drafts, "free-1"))
        compare(StudioDraft.nestedKeyCount(drafts), 1)
    }

    function test_isDirtyCoversEveryDraftChannel() {
        verify(!StudioDraft.isDirty({}, {}, false, ""))
        verify(StudioDraft.isDirty({ opacity: 0.5 }, {}, false, ""))
        verify(StudioDraft.isDirty({}, { reducedMotion: true }, false, ""))
        verify(StudioDraft.isDirty({}, {}, true, ""))
        verify(StudioDraft.isDirty({}, {}, false, "import"))
        verify(StudioDraft.isDirty({}, {}, false, { action: "clear" }))
        verify(!StudioDraft.isDirty({}, {}, false, "   "))
        verify(!StudioDraft.isDirty(null, null, false, null))
    }

    function test_isSessionDirtyCoversAllPanelScopedChannels() {
        verify(!StudioDraft.isSessionDirty({}, {}, {}, {}))
        verify(StudioDraft.isSessionDirty(
            { bottom: { opacity: 0.5 } }, {}, {}, {}))
        verify(StudioDraft.isSessionDirty(
            {}, { showIndicators: false }, {}, {}))
        verify(StudioDraft.isSessionDirty(
            {}, {}, { bottom: { index: 1 } }, {}))
        verify(StudioDraft.isSessionDirty(
            {}, {}, {}, { bottom: { action: "clear" } }))
    }
}
