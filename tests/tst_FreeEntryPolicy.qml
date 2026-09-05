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

    // TASK-0033 Phase A: only free panels that show running applications
    // follow the task model; a launcher stays isolated from task churn.
    function test_onlyTaskBearingContentFollowsTheTaskModel() {
        verify(FreeEntryPolicy.followsTaskModel("tasks"));
        verify(FreeEntryPolicy.followsTaskModel(" Hybrid "));
        verify(!FreeEntryPolicy.followsTaskModel("launcher"));
        verify(!FreeEntryPolicy.followsTaskModel("empty"));
        verify(!FreeEntryPolicy.followsTaskModel(undefined));
    }

    readonly property var mixedEntries: [
        { appId: "free-url:file:///tmp/a.desktop",
          panelEntryId: "free-url:file:///tmp/a.desktop",
          runningAppId: "org.example.a" },
        { appId: "free-url:file:///tmp/Folder/",
          panelEntryId: "free-url:file:///tmp/Folder/" },
        { appId: "org.example.running", panelEntryId: "" }
    ]

    // An owned entry reorders and unpins by its panel id; a running-only entry
    // has none, so the applet must not send it to the panel operations.
    function test_panelEntryIdIsOnlyKnownForOwnedEntries() {
        compare(FreeEntryPolicy.panelEntryId(mixedEntries, "free-url:file:///tmp/Folder/"),
                "free-url:file:///tmp/Folder/");
        compare(FreeEntryPolicy.panelEntryId(mixedEntries, "org.example.running"), "");
        compare(FreeEntryPolicy.panelEntryId(mixedEntries, "unknown"), "");
        compare(FreeEntryPolicy.panelEntryId(null, "unknown"), "");
    }

    // Window actions on a pinned desktop entry with a running instance target
    // that instance; every other entry keeps its own id.
    function test_actionAppIdPrefersTheRunningInstance() {
        compare(FreeEntryPolicy.actionAppId(mixedEntries, "free-url:file:///tmp/a.desktop"),
                "org.example.a");
        compare(FreeEntryPolicy.actionAppId(mixedEntries, "free-url:file:///tmp/Folder/"),
                "free-url:file:///tmp/Folder/");
        compare(FreeEntryPolicy.actionAppId(mixedEntries, "org.example.running"),
                "org.example.running");
        compare(FreeEntryPolicy.actionAppId([], "x"), "x");
    }
}
