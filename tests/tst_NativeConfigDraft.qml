pragma ComponentBehavior: Bound

import QtQuick
import QtTest
import "../plasma-dock-widget/contents/ui" as DockUi

TestCase {
    id: testCase

    name: "NativeConfigDraft"

    property var calls: []
    property var pending: []

    function snapshot(revision, visible, showTooltips) {
        return {
            success: true,
            status: "loaded",
            panelId: "bottom",
            revision: revision,
            consumer: "native",
            panelValues: {
                visible: visible,
                visibilityMode: "always-visible",
                acceptDrops: true
            },
            globalValues: {
                showTooltips: showTooltips
            },
            panelFields: [
                {
                    key: "visible",
                    scope: "panel",
                    control: "switch",
                    defaultValue: true
                },
                {
                    key: "visibilityMode",
                    scope: "panel",
                    control: "combo",
                    defaultValue: "always-visible",
                    options: [
                        {
                            label: "Always visible",
                            value: "always-visible"
                        },
                        {
                            label: "Auto hide",
                            value: "auto-hide"
                        }
                    ]
                },
                {
                    key: "acceptDrops",
                    scope: "panel",
                    control: "switch",
                    defaultValue: true
                }
            ],
            globalFields: [
                {
                    key: "showTooltips",
                    scope: "global",
                    control: "switch",
                    defaultValue: true
                }
            ],
            capabilityResolution: {
                available: true,
                renderer: {
                    effectiveTier: "qml"
                }
            }
        }
    }

    function recordDockCall(member, args, resolved, rejected) {
        calls.push({
            member: member,
            args: args
        })
        pending.push({
            resolved: resolved,
            rejected: rejected
        })
    }

    function createPage() {
        const page = createTemporaryObject(pageComponent, testCase)
        verify(page !== null)
        compare(calls.length, 1)
        compare(calls[0].member, "panelSettingsEditorSnapshot")
        compare(calls[0].args[0], "bottom")
        compare(calls[0].args[1], "native")
        return page
    }

    function loadPage(revision) {
        const page = createPage()
        pending[0].resolved(snapshot(revision, true, true))
        compare(page.draftStatus, "loaded")
        compare(page.editorSnapshot.revision, revision)
        verify(!page.draftDirty)
        return page
    }

    function init() {
        calls = []
        pending = []
    }

    Component {
        id: pageComponent

        DockUi.ConfigPageBase {
            panelIdOverride: "bottom"
            dockCallOverride: function(member, args, resolved, rejected) {
                testCase.recordDockCall(member, args, resolved, rejected)
            }
        }
    }

    function test_editsStayLocalUntilTheLocalApply() {
        const page = loadPage(11)

        verify(page.setValue("visible", false))
        verify(page.setValue("showTooltips", false))
        verify(page.draftDirty)
        compare(page.values.visible, false)
        compare(page.values.showTooltips, false)
        compare(calls.length, 1)
        compare(page.transactionRequestCount, 0)
    }

    function test_cancelDiscardsTheLocalDraft() {
        const page = loadPage(4)
        verify(page.setValue("visibilityMode", "auto-hide"))
        verify(page.draftDirty)

        verify(page.cancelDraft())
        verify(!page.draftDirty)
        compare(page.values.visibilityMode, "always-visible")
        compare(page.draftStatus, "cancelled")
        compare(calls.length, 1)
    }

    function test_closeWithDirtyDraftDoesNotSubmit() {
        const page = loadPage(5)
        verify(page.setValue("acceptDrops", false))
        verify(page.draftDirty)

        page.destroy()
        wait(0)
        compare(calls.length, 1)
    }

    function test_successWaitsForOneTransactionAndFreshRevision() {
        const page = loadPage(12)
        verify(page.setValue("visible", false))
        verify(page.setValue("showTooltips", false))

        verify(page.applyDraft())
        verify(!page.applyDraft())
        compare(page.transactionRequestCount, 1)
        compare(calls.length, 2)
        compare(calls[1].member, "applyPanelSettingsTransaction")
        compare(calls[1].args[0], "bottom")
        compare(calls[1].args[1], 12)
        compare(calls[1].args[2].visible, false)
        compare(calls[1].args[2].visibilityMode, "always-visible")
        compare(calls[1].args[2].acceptDrops, true)
        compare(calls[1].args[3].showTooltips, false)
        verify(!Object.prototype.hasOwnProperty.call(
            calls[1].args[2], "screenId"))
        verify(page.applyInFlight)

        pending[1].resolved({
            success: true,
            status: "succeeded",
            revision: 13
        })
        compare(calls.length, 3)
        compare(calls[2].member, "panelSettingsEditorSnapshot")
        compare(page.draftStatus, "refreshing")
        verify(page.applyInFlight)
        verify(page.draftDirty)

        pending[2].resolved(snapshot(13, false, false))
        compare(page.draftStatus, "loaded")
        compare(page.editorSnapshot.revision, 13)
        verify(!page.applyInFlight)
        verify(!page.draftDirty)
        compare(page.values.visible, false)
        compare(page.values.showTooltips, false)
    }

    function test_failedAndConflictingTransactionsRetainTheDraft() {
        const page = loadPage(21)
        verify(page.setValue("visible", false))
        verify(page.applyDraft())

        pending[1].resolved({
            success: false,
            status: "revision-conflict",
            errorCode: "stale-revision",
            errorMessage: "The panel changed elsewhere."
        })
        compare(page.draftStatus, "conflict")
        compare(page.draftErrorCode, "stale-revision")
        verify(!page.applyInFlight)
        verify(page.draftDirty)
        compare(page.values.visible, false)
        compare(page.editorSnapshot.revision, 21)
        compare(calls.length, 2)
    }

    function test_unknownAndProtectedFieldsCannotEnterCandidates() {
        const page = loadPage(8)

        verify(!page.setValue("screenId", "forged-output"))
        verify(!page.setValue("ownerToken", "forged-token"))
        verify(!page.setValue("hostKind", "free"))
        verify(!page.draftDirty)
        verify(!Object.prototype.hasOwnProperty.call(
            page.panelCandidate(), "screenId"))
        verify(!Object.prototype.hasOwnProperty.call(
            page.panelCandidate(), "ownerToken"))
        verify(!Object.prototype.hasOwnProperty.call(
            page.panelCandidate(), "hostKind"))
        compare(calls.length, 1)
    }

    function test_outerConfigurationChangeDoesNotSubmitTheDraft() {
        const page = loadPage(3)
        page.cfg_panelType = "dock"
        page.cfg_bootstrapFreeDock = true
        wait(0)

        compare(calls.length, 1)
        compare(page.transactionRequestCount, 0)
        verify(!page.draftDirty)
    }
}
