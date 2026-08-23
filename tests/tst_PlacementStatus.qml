import QtQuick 2.15
import QtTest 1.3
import "../qml/runtime/PlacementStatus.js" as PlacementStatus

TestCase {
    name: "PlacementStatus"

    function test_mapTextIsStableAndPreservesFalsyValues() {
        compare(
            PlacementStatus.mapText({
                width: 720,
                dynamic: false,
                alignment: "center"
            }),
            "alignment=center; dynamic=false; width=720")
        compare(PlacementStatus.mapText({}), "<unavailable>")
        compare(PlacementStatus.mapText(null), "<unavailable>")
    }

    function test_savedIntentAndActualHostRemainDistinct() {
        const result = {
            savedIntent: {
                edge: "bottom",
                width: 720
            },
            hostState: {
                edge: "bottom",
                fixedLength: "704"
            }
        }

        compare(
            PlacementStatus.savedIntentText(result),
            "edge=bottom; width=720")
        compare(
            PlacementStatus.hostStateText(result),
            "edge=bottom; fixedLength=704")
    }

    function test_partialSupportIncludesFieldDiagnostics() {
        const result = {
            success: false,
            status: "unsupported",
            errorCode: "unsupported-capability-unavailable",
            unsupported: [{
                field: "floatingMargin",
                requested: "12",
                errorCode: "property-unsupported"
            }],
            failed: []
        }

        verify(PlacementStatus.isProblem(result))
        compare(PlacementStatus.statusLabel(result.status), "Unsupported")
        compare(
            PlacementStatus.problemText(result),
            "unsupported-capability-unavailable | unsupported "
                + "floatingMargin: requested=12, error=property-unsupported")
    }

    function test_failedFieldReportsObservedValueAndRollback() {
        const result = {
            success: false,
            status: "rollback-failed",
            errorCode: "readback-mismatch",
            unsupported: [],
            failed: [{
                field: "height",
                requested: "76",
                observed: "77",
                errorCode: "readback-mismatch"
            }],
            rollbackAttempted: true,
            rollbackSucceeded: false,
            rollbackErrorCode: "script-failure"
        }

        compare(
            PlacementStatus.statusLabel(result.status),
            "Failed; host restoration failed")
        compare(
            PlacementStatus.problemText(result),
            "readback-mismatch | failed height: requested=76, observed=77, "
                + "error=readback-mismatch | rollback=script-failure")
    }

    function test_successHasNoProblem() {
        const result = {
            success: true,
            status: "applied",
            errorCode: "",
            unsupported: [],
            failed: []
        }

        verify(!PlacementStatus.isProblem(result))
        compare(PlacementStatus.statusLabel(result.status), "Applied and verified")
        compare(PlacementStatus.problemText(result), "")
    }
}
