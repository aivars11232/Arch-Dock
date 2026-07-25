import QtQuick

Item {
    id: root

    property string action: ""
    property string token: ""
    property int panelId: -1
    property int maximumAttempts: 8
    property int baseRetryInterval: 150
    property int maximumRetryInterval: 4000
    property bool requestPending: false
    property int attempts: 0
    readonly property bool ready: action === "create-circular-free-panel"
        && panelId >= 0 && token.length > 0

    signal request(int panelId, string token)

    function tryRequest() {
        if (requestPending || !ready)
            return;
        retryTimer.stop();
        requestPending = true;
        ++attempts;
        request(panelId, token);
    }

    function scheduleRetry() {
        requestPending = false;
        if (!ready || attempts >= maximumAttempts)
            return;
        retryTimer.interval = Math.min(
            maximumRetryInterval,
            baseRetryInterval * Math.pow(2, Math.max(0, attempts - 1)));
        retryTimer.restart();
    }

    function resolved(result) {
        if (typeof result === "string" && result.length > 0) {
            retryTimer.stop();
            requestPending = false;
            attempts = 0;
            return;
        }
        scheduleRetry();
    }

    function rejected() {
        scheduleRetry();
    }

    function nudge() {
        if (requestPending)
            return;
        attempts = 0;
        tryRequest();
    }

    function settleConfiguration() {
        retryTimer.stop();
        requestPending = false;
        attempts = 0;
        tryRequest();
    }

    width: 0
    height: 0
    visible: false

    onActionChanged: configurationTimer.restart()
    onTokenChanged: configurationTimer.restart()
    onPanelIdChanged: configurationTimer.restart()
    Component.onCompleted: configurationTimer.restart()

    Timer {
        id: configurationTimer

        interval: 0
        repeat: false
        onTriggered: root.settleConfiguration()
    }

    Timer {
        id: retryTimer

        repeat: false
        onTriggered: root.tryRequest()
    }
}
