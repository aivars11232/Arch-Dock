#include "PanelVisibility.h"

#include <QtTest>

namespace
{
ArchDock::PanelVisibilityInput validInput(ArchDock::PanelVisibilityMode mode)
{
    ArchDock::PanelVisibilityInput input;
    input.mode = mode;
    input.panelGeometry = QRect(0, 1040, 1920, 40);
    input.panelScreenIndex = 0;
    return input;
}

ArchDock::WindowOcclusion activeWindow(
    const QRect &geometry = QRect(0, 0, 1920, 1080),
    int screenIndex = 0)
{
    return {geometry, screenIndex, true, false, false, false};
}
}

class PanelVisibilityTest final : public QObject
{
    Q_OBJECT

private slots:
    void normalizesStoredModes();
    void keepsAlwaysVisibleWithoutManualRequest();
    void autoHideConcealsWhenUnlocked();
    void manualHideIsExplicitAndGuarded();
    void concealmentLocksPreventConcealment_data();
    void concealmentLocksPreventConcealment();
    void dodgeRequiresRelevantOverlap();
    void maximizedModeRequiresRelevantWindowState();
    void invalidTargetsFailVisible();
    void decisionsAreDeterministic();
};

void PanelVisibilityTest::normalizesStoredModes()
{
    using ArchDock::PanelVisibilityMode;

    QVERIFY(ArchDock::panelVisibilityModeFromString(QStringLiteral("always")) ==
            PanelVisibilityMode::AlwaysVisible);
    QVERIFY(ArchDock::panelVisibilityModeFromString(QStringLiteral("none")) ==
            PanelVisibilityMode::AlwaysVisible);
    QVERIFY(ArchDock::panelVisibilityModeFromString(QStringLiteral("auto-hide")) ==
            PanelVisibilityMode::AutoHide);
    QVERIFY(ArchDock::panelVisibilityModeFromString(QStringLiteral("Dodge Windows")) ==
            PanelVisibilityMode::DodgeActiveWindow);
    QVERIFY(ArchDock::panelVisibilityModeFromString(QStringLiteral("cover")) ==
            PanelVisibilityMode::HideForMaximizedOrFullscreen);
    QVERIFY(ArchDock::panelVisibilityModeFromString(QStringLiteral("unknown")) ==
            PanelVisibilityMode::AlwaysVisible);
}

void PanelVisibilityTest::keepsAlwaysVisibleWithoutManualRequest()
{
    using ArchDock::PanelVisibilityDecision;
    using ArchDock::PanelVisibilityMode;

    ArchDock::PanelVisibilityInput input = validInput(PanelVisibilityMode::AlwaysVisible);
    ArchDock::WindowOcclusion window = activeWindow();
    window.maximized = true;
    input.windows = {window};

    QVERIFY(ArchDock::decidePanelVisibility(input) == PanelVisibilityDecision::Reveal);
    QVERIFY(!ArchDock::shouldConcealPanel(input));
}

void PanelVisibilityTest::autoHideConcealsWhenUnlocked()
{
    using ArchDock::PanelVisibilityDecision;
    using ArchDock::PanelVisibilityMode;

    const ArchDock::PanelVisibilityInput input = validInput(PanelVisibilityMode::AutoHide);
    QVERIFY(ArchDock::decidePanelVisibility(input) == PanelVisibilityDecision::Conceal);
    QVERIFY(ArchDock::shouldConcealPanel(input));
}

void PanelVisibilityTest::manualHideIsExplicitAndGuarded()
{
    using ArchDock::PanelVisibilityDecision;
    using ArchDock::PanelVisibilityMode;

    ArchDock::PanelVisibilityInput input = validInput(PanelVisibilityMode::AlwaysVisible);
    QVERIFY(ArchDock::decidePanelVisibility(input) == PanelVisibilityDecision::Reveal);

    input.manualHideRequested = true;
    QVERIFY(ArchDock::decidePanelVisibility(input) == PanelVisibilityDecision::Conceal);

    input.locks.popupOpen = true;
    QVERIFY(ArchDock::decidePanelVisibility(input) == PanelVisibilityDecision::Reveal);
}

void PanelVisibilityTest::concealmentLocksPreventConcealment_data()
{
    QTest::addColumn<int>("lockIndex");

    QTest::newRow("pointer-inside") << 0;
    QTest::newRow("reveal-zone") << 1;
    QTest::newRow("popup-open") << 2;
    QTest::newRow("drag-active") << 3;
    QTest::newRow("keyboard-focus") << 4;
    QTest::newRow("edit-mode") << 5;
}

void PanelVisibilityTest::concealmentLocksPreventConcealment()
{
    using ArchDock::PanelVisibilityDecision;
    using ArchDock::PanelVisibilityMode;

    QFETCH(int, lockIndex);
    ArchDock::PanelVisibilityInput input = validInput(PanelVisibilityMode::AutoHide);
    input.manualHideRequested = true;
    switch (lockIndex)
    {
    case 0:
        input.locks.pointerInside = true;
        break;
    case 1:
        input.locks.revealZoneActive = true;
        break;
    case 2:
        input.locks.popupOpen = true;
        break;
    case 3:
        input.locks.dragActive = true;
        break;
    case 4:
        input.locks.keyboardFocus = true;
        break;
    case 5:
        input.locks.editMode = true;
        break;
    default:
        QFAIL("Unhandled visibility lock test row");
    }

    QVERIFY(ArchDock::decidePanelVisibility(input) == PanelVisibilityDecision::Reveal);
}

void PanelVisibilityTest::dodgeRequiresRelevantOverlap()
{
    using ArchDock::PanelVisibilityDecision;
    using ArchDock::PanelVisibilityMode;

    ArchDock::PanelVisibilityInput input = validInput(PanelVisibilityMode::DodgeActiveWindow);
    input.windows = {activeWindow()};
    QVERIFY(ArchDock::decidePanelVisibility(input) == PanelVisibilityDecision::Conceal);

    input.windows = {activeWindow(QRect(0, 0, 1920, 1000))};
    QVERIFY(ArchDock::decidePanelVisibility(input) == PanelVisibilityDecision::Reveal);

    input.windows = {activeWindow(QRect(0, 0, 1920, 1080), 1)};
    QVERIFY(ArchDock::decidePanelVisibility(input) == PanelVisibilityDecision::Reveal);

    ArchDock::WindowOcclusion inactive = activeWindow();
    inactive.active = false;
    input.windows = {inactive};
    QVERIFY(ArchDock::decidePanelVisibility(input) == PanelVisibilityDecision::Reveal);

    ArchDock::WindowOcclusion minimized = activeWindow();
    minimized.minimized = true;
    input.windows = {minimized};
    QVERIFY(ArchDock::decidePanelVisibility(input) == PanelVisibilityDecision::Reveal);
}

void PanelVisibilityTest::maximizedModeRequiresRelevantWindowState()
{
    using ArchDock::PanelVisibilityDecision;
    using ArchDock::PanelVisibilityMode;

    ArchDock::PanelVisibilityInput input = validInput(
        PanelVisibilityMode::HideForMaximizedOrFullscreen);
    input.windows = {activeWindow()};
    QVERIFY(ArchDock::decidePanelVisibility(input) == PanelVisibilityDecision::Reveal);

    ArchDock::WindowOcclusion maximized = activeWindow(QRect(0, 0, 1920, 1000));
    maximized.maximized = true;
    input.windows = {maximized};
    QVERIFY(ArchDock::decidePanelVisibility(input) == PanelVisibilityDecision::Conceal);

    ArchDock::WindowOcclusion fullScreen = activeWindow();
    fullScreen.fullScreen = true;
    input.windows = {fullScreen};
    QVERIFY(ArchDock::decidePanelVisibility(input) == PanelVisibilityDecision::Conceal);

    maximized.screenIndex = 1;
    input.windows = {maximized};
    QVERIFY(ArchDock::decidePanelVisibility(input) == PanelVisibilityDecision::Reveal);

    maximized.screenIndex = 0;
    maximized.minimized = true;
    input.windows = {maximized};
    QVERIFY(ArchDock::decidePanelVisibility(input) == PanelVisibilityDecision::Reveal);
}

void PanelVisibilityTest::invalidTargetsFailVisible()
{
    using ArchDock::PanelVisibilityDecision;
    using ArchDock::PanelVisibilityMode;

    ArchDock::PanelVisibilityInput input = validInput(PanelVisibilityMode::AutoHide);
    input.manualHideRequested = true;
    input.panelGeometry = {};
    QVERIFY(ArchDock::decidePanelVisibility(input) == PanelVisibilityDecision::Reveal);

    input.panelGeometry = QRect(0, 1040, 1920, 40);
    input.panelScreenIndex = -1;
    QVERIFY(ArchDock::decidePanelVisibility(input) == PanelVisibilityDecision::Reveal);
}

void PanelVisibilityTest::decisionsAreDeterministic()
{
    using ArchDock::PanelVisibilityDecision;
    using ArchDock::PanelVisibilityMode;

    ArchDock::PanelVisibilityInput input = validInput(PanelVisibilityMode::DodgeActiveWindow);
    input.windows = {activeWindow()};
    for (int iteration = 0; iteration < 32; ++iteration)
    {
        QVERIFY(ArchDock::decidePanelVisibility(input) == PanelVisibilityDecision::Conceal);
    }
}

QTEST_APPLESS_MAIN(PanelVisibilityTest)

#include "PanelVisibilityTest.moc"
