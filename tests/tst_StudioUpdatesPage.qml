import QtQuick
import QtTest
import "../qml/runtime" as Runtime
import "../qml/runtime/StudioThemes.js" as StudioThemes

// Panel Studio's Updates page with a stand-in updater: each state shows only
// its controls, Stop (never Cancel) while an update can still stop, no
// green anywhere, problems in the warning colour, raw details never shown,
// the notes as plain titles, and the bars line only while they cannot hide.
TestCase {
    id: testCase
    name: "StudioUpdatesPage"
    when: windowShown
    width: 820
    height: 760
    visible: true

    readonly property var theme: StudioThemes.theme("classic")
    readonly property var controls: ["updates-check", "updates-update-now", "updates-show-notes",
        "updates-see-on-github", "updates-copy-install-line", "updates-stop", "updates-restart",
        "updates-try-again", "updates-copy-details", "updates-download-progress", "updates-password-hint",
        "updates-goes-on", "updates-problem"]

    Component {
        id: updaterComponent
        QtObject {
            property bool offered: true
            property string runningVersion: "0.1.1-55"
            property string state: "not-looked"
            property string step: ""
            property string line: ""
            property string lineCode: ""
            property string details: ""
            property string lastLooked: "27 Nov at 14:05"
            property string foundVersion: "0.1.1-56"
            property string releasedOn: "3 Dec 2026"
            property real downloadSize: 34127137
            property var noteTitles: ["The bars no longer flicker.", "Panel Studio opens faster."]
            property string notes: "- **The bars no longer flicker.** After a screen change.\n- **Panel Studio opens faster.** See [here](https://example.org)."
            property real received: 8283750
            property bool canUpdate: state === "found" && lineCode === ""
            property bool canStop: state === "updating" && ["download", "check", "password"].indexOf(step) >= 0
            property string installLine: lineCode === "L9" ? "sudo pacman -Syu --needed qt6-foo" : ""
            property string releasesPage: "https://github.com/aivars11232/Arch-Dock/releases/tag/v0.1.1-56"
            property bool holdsSettings: false
            property bool checkAutomatically: true
            property bool attention: false
            property int checks: 0
            property int updates: 0
            property int restarts: 0
            function check() { checks += 1 }
            function update() { updates += 1 }
            function stop() {}
            function restartArchDock() { restarts += 1 }
            function pageOpened() {}
        }
    }
    Component { id: pageComponent; Runtime.StudioUpdatesPage { width: 800; height: 740 } }
    Component { id: spyComponent; SignalSpy {} }

    function page(state, step, line, lineCode, extra) {
        const updater = createTemporaryObject(updaterComponent, testCase,
            Object.assign({ state: state, step: step || "", line: line || "", lineCode: lineCode || "" }, extra || {}))
        const item = createTemporaryObject(pageComponent, testCase, { updates: updater, theme: testCase.theme })
        verify(item !== null)
        verify(waitForRendering(item))
        return item
    }
    function shown(item, name) {
        const child = findChild(item, name)
        return child !== null && child.visible
    }
    function visibleControls(item) {
        return controls.filter(function(name) { return shown(item, name) })
    }
    function visibleTexts(item) {
        const texts = []
        const walk = function(node) {
            if (!node || !node.visible)
                return
            if (node.text !== undefined && typeof node.text === "string")
                texts.push(node.text)
            for (let i = 0; i < node.children.length; ++i)
                walk(node.children[i])
        }
        walk(item)
        return texts
    }
    function colours(item) {
        const found = []
        const walk = function(node) {
            if (!node || !node.visible)
                return
            if (node.color !== undefined)
                found.push(String(node.color))
            for (let i = 0; i < node.children.length; ++i)
                walk(node.children[i])
        }
        walk(item)
        return found
    }

    function test_eachStateShowsOnlyItsControls_data() {
        return [
            { tag: "not looked", state: "not-looked", step: "", line: "", code: "", wanted: ["updates-check"] },
            { tag: "looking", state: "looking", step: "", line: "", code: "", wanted: [] },
            { tag: "up to date", state: "up-to-date", step: "", line: "", code: "", wanted: ["updates-check"] },
            { tag: "found", state: "found", step: "", line: "", code: "",
              wanted: ["updates-update-now", "updates-show-notes", "updates-see-on-github"] },
            // Lines that say "press Check for Updates" offer that button by name.
            { tag: "a part not installed", state: "problem", step: "", code: "L9",
              line: "Arch Dock 0.1.1-56 needs qt6-foo, which is not installed. Install it first, then press Check for Updates again.",
              wanted: ["updates-check", "updates-copy-install-line", "updates-copy-details", "updates-problem"] },
            { tag: "newer system parts", state: "problem", step: "", code: "L8",
              line: "Arch Dock 0.1.1-56 needs newer system parts (qt6-base 6.13 or newer). Update your system first, then press Check for Updates again.",
              wanted: ["updates-check", "updates-copy-details", "updates-problem"] },
            { tag: "download", state: "updating", step: "download", line: "", code: "",
              wanted: ["updates-stop", "updates-download-progress"] },
            { tag: "check", state: "updating", step: "check", line: "", code: "", wanted: ["updates-stop"] },
            { tag: "password", state: "updating", step: "password", line: "", code: "",
              wanted: ["updates-stop", "updates-password-hint"] },
            { tag: "installing", state: "updating", step: "installing", line: "", code: "", wanted: ["updates-goes-on"] },
            { tag: "stopping", state: "stopping", step: "", line: "Stopping the update…", code: "", wanted: [] },
            { tag: "restarting", state: "restarting", step: "", line: "", code: "", wanted: [] },
            { tag: "restart needed", state: "restart-needed", step: "", code: "",
              line: "Arch Dock 0.1.1-56 is installed, but this account still runs 0.1.1-55.",
              wanted: ["updates-restart", "updates-problem"] },
            { tag: "finish", state: "finish", step: "", code: "",
              line: "Arch Dock 0.1.1-56 was installed, but Arch Dock and Plasma's panels were not restarted afterwards.",
              wanted: ["updates-restart", "updates-problem"] },
            { tag: "done", state: "done", step: "", line: "Arch Dock 0.1.1-56 is installed and running.", code: "", wanted: [] },
            { tag: "problem", state: "problem", step: "", line: "GitHub is not answering right now. Try again later.",
              code: "L2", wanted: ["updates-try-again", "updates-copy-details", "updates-problem"] },
            { tag: "install by hand", state: "problem", step: "", code: "L7",
              line: "Arch Dock 0.1.1-60 has to be installed by hand once. The releases page says how.",
              wanted: ["updates-see-on-github", "updates-try-again", "updates-copy-details", "updates-problem"] }
        ]
    }

    function test_eachStateShowsOnlyItsControls(data) {
        const item = page(data.state, data.step, data.line, data.code, { details: "raw trace 0x7f00" })
        compare(JSON.stringify(visibleControls(item).sort()), JSON.stringify(data.wanted.slice().sort()))
    }

    function test_stopIsNeverCancel() {
        for (const step of ["download", "check", "password"]) {
            const item = page("updating", step)
            const stop = findChild(item, "updates-stop")
            compare(stop.text, "Stop")
            verify(visibleTexts(item).indexOf("Cancel") < 0)
        }
        compare(findChild(page("stopping", "", "Stopping the update…"), "updates-status-title").text, "Stopping the update…")
    }

    function test_theInstallGoesOnLine() {
        const item = page("updating", "installing")
        compare(findChild(item, "updates-goes-on").text, "Installing… This goes on even if you close Panel Studio.")
        verify(!shown(item, "updates-stop"))
    }

    function test_noGreenAndProblemsInTheWarningColour() {
        const ready = String(testCase.theme.ready)
        for (const state of ["found", "done", "problem", "restart-needed", "finish", "up-to-date"]) {
            const item = page(state, "", "A line.", state === "problem" ? "L2" : "")
            verify(colours(item).indexOf(ready) < 0, state + " shows the green Ready colour")
        }
        const problem = page("problem", "", "GitHub is not answering right now. Try again later.", "L2")
        const row = findChild(problem, "updates-problem")
        verify(row.visible)
        let warned = false
        for (let i = 0; i < row.children.length; ++i)
            if (String(row.children[i].color) === String(testCase.theme.warning))
                warned = true
        verify(warned)
    }

    function test_rawDetailsAreNeverShown() {
        const item = page("problem", "", "GitHub is not answering right now. Try again later.", "L2",
                          { details: "HTTP 502 raw trace 0x7f00" })
        for (const text of visibleTexts(item))
            verify(text.indexOf("0x7f00") < 0, "raw details shown: " + text)
    }

    function test_theNotesArePlainTitles() {
        const item = page("found")
        const texts = visibleTexts(item)
        verify(texts.indexOf("• The bars no longer flicker.") >= 0)
        verify(texts.indexOf("• Panel Studio opens faster.") >= 0)
        findChild(item, "updates-show-notes").clicked()
        verify(waitForRendering(item))
        const all = findChild(item, "updates-all-notes")
        verify(all.visible)
        compare(all.textFormat, Text.PlainText)
        verify(all.text.indexOf("**") < 0)
        verify(all.text.indexOf("- The bars no longer flicker.") >= 0)
    }

    function test_theBarsLineOnlyWhileTheBarsCannotHide() {
        const item = page("up-to-date")
        verify(!shown(item, "updates-bars-line"))
        item.barsCanHide = false
        verify(waitForRendering(item))
        verify(shown(item, "updates-bars-line"))
    }

    function test_updateNowAsksStudioFirst() {
        const item = page("found")
        const asked = createTemporaryObject(spyComponent, testCase, { target: item, signalName: "updateRequested" })
        findChild(item, "updates-update-now").clicked()
        compare(asked.count, 1)
        compare(item.updates.updates, 0)
    }

    // Restart Arch Dock closes Studio too: Studio asks about unapplied
    // changes first, so the page only asks for it.
    function test_restartAsksStudioFirst() {
        for (const state of ["restart-needed", "finish"]) {
            const item = page(state, "", "A line.")
            const asked = createTemporaryObject(spyComponent, testCase, { target: item, signalName: "restartRequested" })
            findChild(item, "updates-restart").clicked()
            compare(asked.count, 1)
            compare(item.updates.restarts, 0)
        }
    }

    // A card whose first line has nothing to say leaves no room for it: a
    // problem's card starts with its orange line.
    function test_aProblemCardStartsWithItsLine() {
        const problem = page("problem", "", "GitHub is not answering right now. Try again later.", "L2")
        verify(!shown(problem, "updates-status-row"))
        const card = findChild(problem, "updates-status-card")
        const line = findChild(problem, "updates-problem")
        verify(line.mapToItem(card, 0, 0).y < 24, "the orange line starts " + line.mapToItem(card, 0, 0).y + " px down")
        verify(shown(page("found"), "updates-status-row"))
        verify(shown(page("done", "", "Arch Dock 0.1.1-56 is installed and running."), "updates-status-row"))
    }

    // The marker of the current step does not make its row taller, so the
    // steps keep their places as the update moves on.
    function test_theStepsKeepTheirPlaces() {
        let heights = null
        for (const step of ["download", "check", "password", "installing", "restart"]) {
            const item = page("updating", step)
            const rows = ["download", "check", "password", "restart"].map(function(name) {
                return findChild(item, "updates-step-" + name)
            })
            const these = rows.map(function(row) { return row.height })
            for (const height of these)
                compare(height, these[0])
            if (heights === null)
                heights = these
            compare(JSON.stringify(these), JSON.stringify(heights))
        }
    }
}
