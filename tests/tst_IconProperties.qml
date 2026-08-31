pragma ComponentBehavior: Bound

import QtQuick
import QtTest
import "../qml/runtime" as Runtime

TestCase {
    id: testCase

    name: "IconProperties"
    when: windowShown
    width: 720
    height: 680

    property var closeReasons: []

    QtObject {
        id: transactionController

        property var calls: []
        property var nextResult: ({
            success: true,
            status: "succeeded",
            revision: 18
        })

        function applyIconOverrideTransaction(panelId, revision,
                                              entryIdentity, values) {
            calls.push({
                method: "apply",
                panelId: panelId,
                revision: revision,
                entryIdentity: entryIdentity,
                values: values
            })
            return nextResult
        }

        function resetIconOverrideTransaction(panelId, revision,
                                              entryIdentity) {
            calls.push({
                method: "reset",
                panelId: panelId,
                revision: revision,
                entryIdentity: entryIdentity
            })
            return nextResult
        }
    }

    Component {
        id: editorComponent

        Runtime.IconProperties {
            width: 620
            height: 590
            transactionController: transactionController
        }
    }

    function styles() {
        return [
            { id: "plain-original", name: "Plain Original" },
            { id: "metallic-blue", name: "Metallic Blue" },
            { id: "metallic-red", name: "Metallic Red" },
            { id: "neon-green", name: "Neon Green" },
            { id: "neon-orange", name: "Neon Orange" },
            { id: "dark-orb", name: "Dark Orb" }
        ]
    }

    function snapshot(overrideValues) {
        return {
            success: true,
            status: "loaded",
            panelId: "bottom",
            revision: 17,
            entryIdentity: "desktop.org.example.editor.desktop",
            baseGlyph: "applications-development",
            baseLabel: "Example Editor",
            override: overrideValues || ({}),
            resolution: {
                resolvedGlyph: "applications-development",
                resolvedLabel: "Example Editor"
            },
            iconStyles: styles()
        }
    }

    function createEditor(overrideValues) {
        const editor = createTemporaryObject(editorComponent, testCase, {
            editorSnapshot: snapshot(overrideValues)
        })
        verify(editor !== null)
        editor.closeRequested.connect(function(reason) {
            testCase.closeReasons.push(reason)
        })
        wait(0)
        return editor
    }

    function init() {
        closeReasons = []
        transactionController.calls = []
        transactionController.nextResult = {
            success: true,
            status: "succeeded",
            revision: 18
        }
    }

    function test_snapshotLoadsStableIdentityAndEveryStyleChoice() {
        const editor = createEditor({
            customGlyph: "utilities-terminal",
            customLabel: "Terminal override",
            tileEnabled: false,
            styleReference: "dark-orb"
        })

        verify(editor.snapshotLoaded)
        compare(editor.editorSnapshot.entryIdentity,
                "desktop.org.example.editor.desktop")
        compare(editor.customGlyph, "utilities-terminal")
        compare(editor.customLabel, "Terminal override")
        compare(editor.tileMode, "disabled")
        compare(editor.styleReference, "dark-orb")
        compare(editor.styleChoices.length, 7)
        compare(editor.styleChoices[0].id, "")
        verify(!editor.draftDirty)
        verify(editor.hasStoredOverride)
    }

    function test_applySubmitsOneAtomicStableIdentityTransaction() {
        const editor = createEditor({
            customLabel: "Old label",
            animationProfileReference: "future-orbit",
            extensions: { futureFlag: true }
        })
        verify(editor.setCustomGlyph("file:///tmp/custom-icon.svg"))
        verify(editor.setCustomLabel("New label"))
        verify(editor.setTileMode("disabled"))
        verify(editor.setStyleReference("metallic-blue"))
        verify(editor.draftDirty)

        verify(editor.applyDraft())
        compare(editor.transactionRequestCount, 1)
        compare(transactionController.calls.length, 1)
        const call = transactionController.calls[0]
        compare(call.method, "apply")
        compare(call.panelId, "bottom")
        compare(call.revision, 17)
        compare(call.entryIdentity,
                "desktop.org.example.editor.desktop")
        compare(call.values.customGlyph, "file:///tmp/custom-icon.svg")
        compare(call.values.customLabel, "New label")
        compare(call.values.tileEnabled, false)
        compare(call.values.styleReference, "metallic-blue")
        compare(call.values.animationProfileReference, "future-orbit")
        compare(call.values.extensions.futureFlag, true)
        compare(closeReasons, ["apply"])
    }

    function test_cancelDiscardsDraftWithoutAnyBackendWrite() {
        const editor = createEditor({ customLabel: "Stored label" })
        verify(editor.setCustomLabel("Unsaved label"))
        verify(editor.draftDirty)

        verify(editor.cancelDraft())
        compare(transactionController.calls.length, 0)
        compare(editor.customLabel, "Stored label")
        verify(!editor.draftDirty)
        compare(editor.draftStatus, "cancelled")
        compare(closeReasons, ["cancel"])
    }

    function test_resetUsesOneDedicatedTransaction() {
        const editor = createEditor({
            customGlyph: "utilities-terminal",
            tileEnabled: true
        })
        verify(editor.canReset)

        verify(editor.resetOverride())
        compare(transactionController.calls.length, 1)
        compare(transactionController.calls[0].method, "reset")
        compare(transactionController.calls[0].panelId, "bottom")
        compare(transactionController.calls[0].revision, 17)
        compare(transactionController.calls[0].entryIdentity,
                "desktop.org.example.editor.desktop")
        compare(closeReasons, ["reset"])
    }

    function test_failedApplyRetainsDraftAndReportsConflict() {
        transactionController.nextResult = {
            success: false,
            status: "revision-conflict",
            errorCode: "stale-revision",
            errorMessage: "The panel changed elsewhere."
        }
        const editor = createEditor({ customLabel: "Stored label" })
        verify(editor.setCustomLabel("Keep this draft"))

        verify(editor.applyDraft())
        compare(transactionController.calls.length, 1)
        compare(closeReasons.length, 0)
        compare(editor.customLabel, "Keep this draft")
        verify(editor.draftDirty)
        compare(editor.draftStatus, "conflict")
        compare(editor.draftErrorCode, "stale-revision")
        compare(editor.draftErrorMessage, "The panel changed elsewhere.")
    }

    function test_unknownStylesAndEmptyNoopCannotBeSubmitted() {
        const editor = createEditor({})
        verify(!editor.setStyleReference("not-shipped"))
        verify(!editor.draftDirty)
        verify(!editor.applyDraft())
        verify(!editor.resetOverride())
        compare(transactionController.calls.length, 0)
        compare(closeReasons.length, 0)
    }
}
