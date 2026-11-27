pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import "StudioThemes.js" as StudioThemes

// Panel Studio's Updates page: which Arch Dock runs, the switch for the
// daily look on GitHub, and one card for where an update stands. Every
// control appears only in its state. Problems are one orange line in plain
// words; their raw details only go to the clipboard (Copy Details).
ColumnLayout {
    id: root

    // Arch Dock's updater (archDockUpdates), or a test's stand-in.
    property var updates: null
    property var theme: StudioThemes.theme("classic")
    // False while Arch Dock's bars cannot hide with the Qt that runs.
    property bool barsCanHide: true
    // True during a preview on the desktop or a profile apply: Update Now waits.
    property bool blocked: false

    // Update Now: Panel Studio first asks about changes not applied yet.
    // A problem whose line says "press Check for Updates" offers that
    // button, by its name, instead of Try Again (both look again).
    readonly property bool checkNamed: state === "problem" && updates !== null
        && ["L8", "L9", "L23"].indexOf(updates.lineCode) >= 0
    signal updateRequested()
    signal restartRequested()

    readonly property string state: updates ? String(updates.state) : "not-looked"
    readonly property var titles: updates ? Array.from(updates.noteTitles || []) : []
    property bool allNotesShown: false

    spacing: 10

    function megabytes(bytes) {
        return (Number(bytes) / 1048576).toFixed(1);
    }

    function copy(text) {
        clipboard.text = String(text || "");
        clipboard.selectAll();
        clipboard.copy();
    }

    TextEdit {
        id: clipboard
        visible: false
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 2

        Label {
            text: StudioThemes.label(root.theme, qsTr("Updates"))
            color: root.theme.heading
            font.pixelSize: 16
            font.weight: root.theme.headingWeight
            font.letterSpacing: root.theme.letterSpacing
        }
        Label {
            objectName: "updates-running-version"
            Layout.fillWidth: true
            text: qsTr("You are using Arch Dock %1.").arg(root.updates ? root.updates.runningVersion : "")
            color: root.theme.description
            font.pixelSize: 11
            wrapMode: Text.Wrap
        }
        Rectangle {
            Layout.fillWidth: true
            Layout.topMargin: 5
            Layout.preferredHeight: 1
            color: root.theme.separator
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 12

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            Label {
                text: qsTr("Look for updates by itself")
                color: root.theme.text
            }
            Label {
                Layout.fillWidth: true
                text: qsTr("Once a day Arch Dock asks GitHub whether a newer version is out. It sends nothing about you or your panels, and installs nothing until you press Update Now.")
                color: root.theme.textMuted
                font.pixelSize: 11
                wrapMode: Text.Wrap
            }
        }
        Switch {
            objectName: "updates-check-automatically"
            checked: root.updates ? root.updates.checkAutomatically : false
            onToggled: if (root.updates) root.updates.checkAutomatically = checked
        }
    }

    StudioFrame {
        objectName: "updates-status-card"
        Layout.fillWidth: true
        Layout.preferredHeight: card.implicitHeight + 24
        color: root.theme.card
        borderColor: root.theme.cardBorder
        borderWidth: 1
        radius: root.theme.cardRadius
        chamfer: root.theme.chamfer

        ColumnLayout {
            id: card
            anchors.fill: parent
            anchors.margins: 12
            spacing: 8

            // The card's first line; not there at all when it has nothing to
            // say (a problem's card starts with its orange line).
            RowLayout {
                objectName: "updates-status-row"
                visible: doneIcon.visible || statusTitle.visible
                Layout.fillWidth: true
                spacing: 8
                Kirigami.Icon {
                    id: doneIcon
                    visible: root.state === "done"
                    Layout.preferredWidth: 20
                    Layout.preferredHeight: 20
                    source: "checkmark"
                    color: root.theme.accent
                }
                Label {
                    id: statusTitle
                    objectName: "updates-status-title"
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    color: root.theme.cardTitle
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                    text: {
                        switch (root.state) {
                        case "not-looked": return qsTr("Not looked for updates yet.");
                        case "looking": return qsTr("Looking on GitHub…");
                        case "up-to-date": return qsTr("Arch Dock is up to date.");
                        case "found": return qsTr("Arch Dock %1 is ready").arg(root.updates.foundVersion);
                        case "updating": return qsTr("Updating to %1").arg(root.updates.foundVersion);
                        case "stopping": return qsTr("Stopping the update…");
                        case "restarting": return qsTr("Restarting Arch Dock and Plasma's panels…");
                        case "done": return root.updates.line;
                        default: return "";
                        }
                    }
                    visible: text.length > 0
                }
            }

            Label {
                visible: root.state === "up-to-date" && root.updates && root.updates.lastLooked.length > 0
                text: qsTr("Last looked: %1").arg(root.updates ? root.updates.lastLooked : "")
                color: root.theme.textMuted
                font.pixelSize: 11
            }

            Label {
                visible: root.state === "found"
                text: qsTr("Released %1 · %2 MB to download").arg(root.updates ? root.updates.releasedOn : "")
                    .arg(root.megabytes(root.updates ? root.updates.downloadSize : 0))
                color: root.theme.textSecondary
                font.pixelSize: 11
            }

            // What a release fixes, from its signed notes, as plain titles.
            ColumnLayout {
                objectName: "updates-notes"
                visible: (root.state === "found" || root.state === "done") && root.titles.length > 0
                Layout.fillWidth: true
                spacing: 3
                Label {
                    text: root.state === "done" ? qsTr("What it fixed") : qsTr("What it fixes")
                    color: root.theme.text
                    font.weight: Font.DemiBold
                }
                Repeater {
                    model: root.allNotesShown ? [] : root.titles.slice(0, 12)
                    delegate: Label {
                        required property string modelData
                        Layout.fillWidth: true
                        text: "• " + modelData
                        textFormat: Text.PlainText
                        color: root.theme.cardText
                        wrapMode: Text.Wrap
                    }
                }
                Label {
                    visible: !root.allNotesShown && root.titles.length > 12
                    text: qsTr("and %1 more").arg(root.titles.length - 12)
                    color: root.theme.textMuted
                }
                Label {
                    objectName: "updates-all-notes"
                    visible: root.allNotesShown
                    Layout.fillWidth: true
                    text: root.updates ? String(root.updates.notes).replace(/\*\*/g, "") : ""
                    textFormat: Text.PlainText
                    color: root.theme.cardText
                    wrapMode: Text.Wrap
                }
            }

            // The steps of an update.
            ColumnLayout {
                visible: root.state === "updating"
                Layout.fillWidth: true
                spacing: 4
                Repeater {
                    model: [
                        { step: "download", label: qsTr("Download") },
                        { step: "check", label: qsTr("Check") },
                        { step: "password", label: qsTr("Install") },
                        { step: "restart", label: qsTr("Restart") }
                    ]
                    // The marker is on every row, shown only on the current
                    // one, so its glyph gives every row the same height.
                    delegate: RowLayout {
                        id: stepRow
                        required property var modelData
                        readonly property bool current: root.updates && (root.updates.step === modelData.step
                            || (modelData.step === "password" && root.updates.step === "installing"))
                        objectName: "updates-step-" + modelData.step
                        spacing: 4
                        Label {
                            text: "▸"
                            opacity: stepRow.current ? 1 : 0
                            color: root.theme.text
                        }
                        Label {
                            text: stepRow.modelData.label
                            color: stepRow.current ? root.theme.text : root.theme.textMuted
                            font.weight: stepRow.current ? Font.DemiBold : Font.Normal
                        }
                    }
                }
                ProgressBar {
                    objectName: "updates-download-progress"
                    visible: root.updates && root.updates.step === "download"
                    Layout.fillWidth: true
                    from: 0
                    to: Math.max(1, root.updates ? root.updates.downloadSize : 1)
                    value: root.updates ? root.updates.received : 0
                }
                Label {
                    visible: root.updates && root.updates.step === "download"
                    text: qsTr("%1 of %2 MB").arg(root.megabytes(root.updates ? root.updates.received : 0))
                        .arg(root.megabytes(root.updates ? root.updates.downloadSize : 0))
                    color: root.theme.textSecondary
                    font.pixelSize: 11
                }
                Label {
                    objectName: "updates-password-hint"
                    visible: root.updates && root.updates.step === "password"
                    Layout.fillWidth: true
                    text: qsTr("Type your password in the password window. If you do not see it, it may be behind Panel Studio.")
                    color: root.theme.textSecondary
                    wrapMode: Text.Wrap
                }
                Label {
                    objectName: "updates-goes-on"
                    visible: root.updates && root.updates.step === "installing"
                    Layout.fillWidth: true
                    text: qsTr("Installing… This goes on even if you close Panel Studio.")
                    color: root.theme.textSecondary
                    wrapMode: Text.Wrap
                }
            }

            // One orange line: a real problem, or what still has to happen.
            RowLayout {
                objectName: "updates-problem"
                visible: root.updates && root.updates.line.length > 0
                    && (root.state === "problem" || root.state === "found" || root.state === "restart-needed" || root.state === "finish")
                Layout.fillWidth: true
                spacing: 6
                Kirigami.Icon {
                    Layout.alignment: Qt.AlignTop
                    Layout.preferredWidth: 16
                    Layout.preferredHeight: 16
                    source: "dialog-warning"
                    color: root.theme.warning
                }
                Label {
                    Layout.fillWidth: true
                    text: root.updates ? root.updates.line : ""
                    textFormat: Text.PlainText
                    color: root.theme.warning
                    wrapMode: Text.Wrap
                }
            }

            Label {
                visible: root.state === "found" && root.updates && root.updates.canUpdate
                Layout.fillWidth: true
                text: qsTr("Asks for your password once (the one sudo asks for), installs the update, then restarts Arch Dock and Plasma's panels: your panels blink and Panel Studio closes for a moment.")
                color: root.theme.textMuted
                font.pixelSize: 11
                wrapMode: Text.Wrap
            }
            Label {
                visible: root.state === "restart-needed"
                Layout.fillWidth: true
                text: qsTr("Arch Dock and Plasma's panels restart. No password.")
                color: root.theme.textMuted
                font.pixelSize: 11
                wrapMode: Text.Wrap
            }

            Flow {
                Layout.fillWidth: true
                spacing: 8

                Button {
                    objectName: "updates-check"
                    visible: root.state === "not-looked" || root.state === "up-to-date" || root.checkNamed
                    text: qsTr("Check for Updates")
                    onClicked: root.updates.check()
                }
                Button {
                    objectName: "updates-update-now"
                    visible: root.state === "found" && root.updates && root.updates.canUpdate
                    enabled: !root.blocked
                    text: qsTr("Update Now")
                    onClicked: root.updateRequested()
                }
                Button {
                    objectName: "updates-show-notes"
                    visible: root.state === "found" && root.titles.length > 0
                    text: root.allNotesShown ? qsTr("Show Titles") : qsTr("Show All Notes")
                    onClicked: root.allNotesShown = !root.allNotesShown
                }
                Button {
                    objectName: "updates-see-on-github"
                    visible: root.state === "found" || (root.state === "problem" && root.updates
                        && (root.updates.lineCode === "L4" || root.updates.lineCode === "L7"))
                    text: qsTr("See It on GitHub")
                    onClicked: Qt.openUrlExternally(root.updates.releasesPage)
                }
                Button {
                    objectName: "updates-copy-install-line"
                    visible: root.updates && root.updates.lineCode === "L9" && root.updates.installLine.length > 0
                    text: qsTr("Copy Install Line")
                    onClicked: root.copy(root.updates.installLine)
                }
                Button {
                    objectName: "updates-stop"
                    visible: root.updates && root.updates.canStop
                    text: qsTr("Stop")
                    onClicked: root.updates.stop()
                }
                Button {
                    objectName: "updates-restart"
                    visible: root.state === "restart-needed" || root.state === "finish"
                    text: qsTr("Restart Arch Dock")
                    onClicked: root.restartRequested()
                }
                Button {
                    objectName: "updates-try-again"
                    visible: root.state === "problem" && !root.checkNamed
                    text: qsTr("Try Again")
                    onClicked: root.updates.check()
                }
                Button {
                    objectName: "updates-copy-details"
                    visible: root.state === "problem" && root.updates && root.updates.details.length > 0
                    text: qsTr("Copy Details")
                    onClicked: root.copy(root.updates.details)
                }
            }
        }
    }

    // The bars line (L33), only while the bars cannot hide.
    RowLayout {
        objectName: "updates-bars-line"
        visible: !root.barsCanHide
        Layout.fillWidth: true
        spacing: 6
        Kirigami.Icon {
            Layout.alignment: Qt.AlignTop
            Layout.preferredWidth: 16
            Layout.preferredHeight: 16
            source: "dialog-warning"
            color: root.theme.warning
        }
        Label {
            Layout.fillWidth: true
            text: qsTr("Arch Dock's bars cannot hide with this Qt version, so they stay shown. Ask for a rebuilt Arch Dock; once it is published, Panel Studio > Updates installs it.")
            color: root.theme.warning
            wrapMode: Text.Wrap
        }
    }

    Item {
        Layout.fillHeight: true
    }
}
