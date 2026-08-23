import QtQuick 2.15
import QtTest 1.3
import "../qml/runtime/VisibilityStatus.js" as VisibilityStatus

TestCase {
    name: "VisibilityStatus"

    readonly property var labels: ({
        "always": "Always visible",
        "auto-hide": "Auto-hide",
        "dodge": "Dodge touching windows",
        "cover": "Hide for maximized/fullscreen"
    })

    function test_nativeOptionsFollowBackendCapabilities() {
        const options = VisibilityStatus.modeOptions({
            supportedModes: ["always", "auto-hide", "dodge", "cover"]
        }, true, labels)

        compare(options.length, 4)
        compare(options[0].value, "always")
        compare(options[1].value, "auto-hide")
        compare(options[2].label, "Dodge touching windows")
        compare(options[3].label, "Hide for maximized/fullscreen")
    }

    function test_unsupportedCoverIsAbsentRatherThanDisabled() {
        const status = {
            supportedModes: ["always", "auto-hide", "dodge"]
        }
        const values = VisibilityStatus.supportedModeValues(status, true)

        compare(values, ["always", "auto-hide", "dodge"])
        verify(!VisibilityStatus.containsMode(status, true, "cover"))
    }

    function test_freeHostHasNoNativeVisibilityOptions() {
        const status = {
            supportedModes: ["always", "auto-hide", "dodge", "cover"]
        }

        compare(VisibilityStatus.modeOptions(status, false, labels), [])
        verify(!VisibilityStatus.containsMode(status, false, "always"))
    }

    function test_missingNativeStatusFailsSafeToAlwaysOnly() {
        const options = VisibilityStatus.modeOptions(null, true, labels)

        compare(options.length, 1)
        compare(options[0].value, "always")
        compare(options[0].label, "Always visible")
    }

    function test_fallbackIsReportedAsAProblemDespiteSafeSuccess() {
        const result = {
            success: true,
            status: "fallback-applied",
            errorCode: "cover-controller-unsupported",
            requestedMode: "cover",
            effectiveMode: "always",
            fallbackApplied: true,
            fallbackReason: "cover-controller-unsupported"
        }

        verify(VisibilityStatus.isProblem(result))
        compare(
            VisibilityStatus.statusLabel(result.status),
            "Fallback applied and verified")
        compare(
            VisibilityStatus.problemText(result),
            "Requested cover could not be applied (cover-controller-unsupported). "
                + "Always visible was applied and verified.")
    }

    function test_failedFallbackIncludesBothBoundaries() {
        const result = {
            success: false,
            status: "fallback-failed",
            errorCode: "readback-mismatch",
            fallbackAttempted: true,
            fallbackApplied: false,
            fallbackErrorCode: "ownership-denied",
            rollbackAttempted: true,
            rollbackSucceeded: false,
            rollbackErrorCode: "script-failure"
        }

        compare(
            VisibilityStatus.problemText(result),
            "readback-mismatch | always-visible fallback=ownership-denied "
                + "| rollback=script-failure")
    }
}
