import QtQuick
import QtTest
import "../plasma-dock-widget/contents/ui/FreeEntryPolicy.js" as FreeEntryPolicy

TestCase {
    name: "FreeEntryPolicy"

    function test_encodedUrl_data() {
        return [
            {
                tag: "encoded local path",
                appId: "free-url:file:///tmp/A%20B%23C%3F.txt",
                expected: "file:///tmp/A%20B%23C%3F.txt"
            },
            {
                tag: "folder",
                appId: "free-url:file:///tmp/Folder/",
                expected: "file:///tmp/Folder/"
            },
            {
                tag: "regular application",
                appId: "org.kde.dolphin",
                expected: ""
            },
            {
                tag: "empty payload",
                appId: "free-url:",
                expected: ""
            },
            {
                tag: "non-local URL",
                appId: "free-url:https://kde.org/",
                expected: ""
            }
        ];
    }

    function test_encodedUrl(data) {
        compare(FreeEntryPolicy.encodedUrl(data.appId), data.expected);
    }

    function test_interactionDoesNotDependOnServiceWatcher() {
        verify(FreeEntryPolicy.interactionEnabled(false));
        verify(!FreeEntryPolicy.interactionEnabled(true));
    }

    function test_freeEntriesSurviveTransientServiceFailure() {
        verify(FreeEntryPolicy.keepEntriesOnServiceFailure(true));
        verify(!FreeEntryPolicy.keepEntriesOnServiceFailure(false));
    }
}
