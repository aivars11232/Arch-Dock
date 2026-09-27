import QtQuick
import QtTest
import ArchDock.Rendering 1.0

TestCase {
    id: tests
    name: "ContentOverlays"
    when: windowShown
    visible: true
    width: 240
    height: 160
    Component {
        id: iconComponent
        IconScene { width: 64; height: 64; logicalSize: 64 }
    }
    function test_supportedFeedbackStaysInStatusLayer() {
        const icon = createTemporaryObject(iconComponent, tests, {entry: {
            iconName: "applications-system", badgeText: "12", progress: 0.4,
            temporaryStatus: "Launch failed"}, urgent: true});
        verify(icon);
        verify(icon.badgeItem.visible);
        verify(icon.progressItem.visible);
        verify(icon.attentionItem.visible);
        verify(icon.temporaryStatusItem.visible);
        compare(icon.badgeItem.parent, icon.statusLayerItem);
        compare(icon.progressItem.parent, icon.statusLayerItem);
        compare(icon.attentionItem.parent, icon.statusLayerItem);
        compare(icon.temporaryStatusItem.parent, icon.statusLayerItem);
        const width = icon.width;
        icon.entry = {iconName: "applications-system", badgeText: "", progress: -1};
        icon.urgent = false;
        verify(!icon.badgeItem.visible);
        verify(!icon.progressItem.visible);
        verify(!icon.attentionItem.visible);
        verify(!icon.temporaryStatusItem.visible);
        compare(icon.width, width);
    }
    function test_statusReadingsDoNotInventZero() {
        const icon = createTemporaryObject(iconComponent, tests, {entry: {
            iconName: "cpu", isStatus: true, statusAvailable: true, statusText: "0%"}});
        verify(icon);
        compare(icon.statusTextItem.text, "0%");
        verify(icon.statusTextItem.visible);
        icon.entry = {iconName: "cpu", isStatus: true, statusAvailable: false, statusText: "Unavailable"};
        compare(icon.statusTextItem.text, "Unavailable");
        icon.entry = {iconName: "cpu"};
        verify(!icon.statusTextItem.visible);
    }
    function test_progressRejectsUnavailableAndInvalidRanges_data() {
        return [{tag: "missing", value: -1}, {tag: "overflow", value: 2},
                {tag: "nan", value: NaN}, {tag: "infinite", value: Infinity}];
    }
    function test_progressRejectsUnavailableAndInvalidRanges(data) {
        const icon = createTemporaryObject(iconComponent, tests, {entry: {progress: data.value}});
        verify(icon);
        verify(!icon.progressItem.visible);
    }
}
