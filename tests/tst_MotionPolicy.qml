import QtQuick 2.15
import QtTest 1.3
import "../qml/runtime/MotionPolicy.js" as MotionPolicy

TestCase {
    name: "MotionPolicy"

    function test_transitionDurationBoundsAndRespectsReducedMotion() {
        compare(MotionPolicy.transitionDuration(20, false), 80);
        compare(MotionPolicy.transitionDuration(2200, false), 1200);
        compare(MotionPolicy.transitionDuration(170, true), 0);
    }

    function test_iconDurationScalesWithSpeedAndRespectsReducedMotion() {
        compare(MotionPolicy.iconDuration(170, 2, false), 85);
        compare(MotionPolicy.iconDuration(170, 0.1, false), 850);
        compare(MotionPolicy.iconDuration(170, 1, true), 0);
    }

    function test_continuousTriggerGating() {
        verify(MotionPolicy.shouldRunContinuous("hover", true, false, false, false, false));
        verify(MotionPolicy.shouldRunContinuous("running", false, true, false, false, false));
        verify(MotionPolicy.shouldRunContinuous("reveal", false, false, false, true, false));
        verify(!MotionPolicy.shouldRunContinuous("idle", false, false, false, false, true));
        verify(!MotionPolicy.shouldRunContinuous("click", true, true, true, true, false));
    }
}