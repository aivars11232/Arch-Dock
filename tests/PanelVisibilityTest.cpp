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
    void validatesAndSerializesModes();
    void reportsOnlySupportedNativeModes();
    void resolvesNativeHostModes();
    void fallsBackToAlwaysVisibleWhenUnsupported();
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

void PanelVisibilityTest::validatesAndSerializesModes()
{
    using ArchDock::PanelVisibilityMode;

    const auto always = ArchDock::normalizedPanelVisibilityMode(QStringLiteral("Always Visible"));
    const auto autoHide = ArchDock::normalizedPanelVisibilityMode(QStringLiteral("auto_hide"));
    const auto dodge = ArchDock::normalizedPanelVisibilityMode(QStringLiteral("Dodge Windows"));
    const auto cover = ArchDock::normalizedPanelVisibilityMode(
        QStringLiteral("hide-for-maximized-or-fullscreen"));

    QVERIFY(always.has_value());
    QVERIFY(autoHide.has_value());
    QVERIFY(dodge.has_value());
    QVERIFY(cover.has_value());
    QCOMPARE(always.value(), PanelVisibilityMode::AlwaysVisible);
    QCOMPARE(autoHide.value(), PanelVisibilityMode::AutoHide);
    QCOMPARE(dodge.value(), PanelVisibilityMode::DodgeActiveWindow);
    QCOMPARE(cover.value(), PanelVisibilityMode::HideForMaximizedOrFullscreen);
    QVERIFY(!ArchDock::normalizedPanelVisibilityMode(QStringLiteral("sometimes")).has_value());
    QVERIFY(!ArchDock::normalizedPanelVisibilityMode(QString{}).has_value());

    QCOMPARE(ArchDock::panelVisibilityModeToString(PanelVisibilityMode::AlwaysVisible),
             QStringLiteral("always"));
    QCOMPARE(ArchDock::panelVisibilityModeToString(PanelVisibilityMode::AutoHide),
             QStringLiteral("auto-hide"));
    QCOMPARE(ArchDock::panelVisibilityModeToString(PanelVisibilityMode::DodgeActiveWindow),
             QStringLiteral("dodge"));
    QCOMPARE(ArchDock::panelVisibilityModeToString(
                 PanelVisibilityMode::HideForMaximizedOrFullscreen),
             QStringLiteral("cover"));
}

void PanelVisibilityTest::reportsOnlySupportedNativeModes()
{
    ArchDock::NativeVisibilityCapabilities capabilities;
    QCOMPARE(ArchDock::supportedNativeVisibilityModes(capabilities),
             QStringList{QStringLiteral("always")});

    capabilities.autoHide = true;
    QCOMPARE(ArchDock::supportedNativeVisibilityModes(capabilities),
             QStringList({QStringLiteral("always"), QStringLiteral("auto-hide")}));

    capabilities.dodgeWindows = true;
    QCOMPARE(ArchDock::supportedNativeVisibilityModes(capabilities),
             QStringList({QStringLiteral("always"),
                          QStringLiteral("auto-hide"),
                          QStringLiteral("dodge")}));

    capabilities.coverController = true;
    QCOMPARE(ArchDock::supportedNativeVisibilityModes(capabilities),
             QStringList({QStringLiteral("always"),
                          QStringLiteral("auto-hide"),
                          QStringLiteral("dodge"),
                          QStringLiteral("cover")}));

    capabilities.autoHide = false;
    QCOMPARE(ArchDock::supportedNativeVisibilityModes(capabilities),
             QStringList({QStringLiteral("always"), QStringLiteral("dodge")}));
}

void PanelVisibilityTest::resolvesNativeHostModes()
{
    using ArchDock::PanelVisibilityDecision;
    using ArchDock::PanelVisibilityMode;
    using ArchDock::PlasmaPanelHidingMode;

    const ArchDock::NativeVisibilityCapabilities capabilities{true, true, true};

    auto result = ArchDock::resolveNativeVisibility(
        PanelVisibilityMode::AlwaysVisible,
        PanelVisibilityDecision::Reveal,
        false,
        capabilities);
    QVERIFY(result.supported);
    QCOMPARE(result.hostMode, PlasmaPanelHidingMode::None);

    result = ArchDock::resolveNativeVisibility(
        PanelVisibilityMode::AutoHide,
        PanelVisibilityDecision::Reveal,
        false,
        capabilities);
    QCOMPARE(result.hostMode, PlasmaPanelHidingMode::AutoHide);

    result = ArchDock::resolveNativeVisibility(
        PanelVisibilityMode::DodgeActiveWindow,
        PanelVisibilityDecision::Reveal,
        false,
        capabilities);
    QCOMPARE(result.hostMode, PlasmaPanelHidingMode::DodgeWindows);

    result = ArchDock::resolveNativeVisibility(
        PanelVisibilityMode::HideForMaximizedOrFullscreen,
        PanelVisibilityDecision::Reveal,
        false,
        capabilities);
    QCOMPARE(result.hostMode, PlasmaPanelHidingMode::None);

    result = ArchDock::resolveNativeVisibility(
        PanelVisibilityMode::HideForMaximizedOrFullscreen,
        PanelVisibilityDecision::Conceal,
        false,
        capabilities);
    QCOMPARE(result.hostMode, PlasmaPanelHidingMode::AutoHide);

    result = ArchDock::resolveNativeVisibility(
        PanelVisibilityMode::AlwaysVisible,
        PanelVisibilityDecision::Reveal,
        true,
        capabilities);
    QCOMPARE(result.hostMode, PlasmaPanelHidingMode::AutoHide);
    QVERIFY(!result.fallbackApplied);

    QCOMPARE(ArchDock::plasmaPanelHidingModeToString(PlasmaPanelHidingMode::None),
             QStringLiteral("none"));
    QCOMPARE(ArchDock::plasmaPanelHidingModeToString(PlasmaPanelHidingMode::AutoHide),
             QStringLiteral("autohide"));
    QCOMPARE(ArchDock::plasmaPanelHidingModeToString(PlasmaPanelHidingMode::DodgeWindows),
             QStringLiteral("dodgewindows"));
}

void PanelVisibilityTest::fallsBackToAlwaysVisibleWhenUnsupported()
{
    using ArchDock::PanelVisibilityDecision;
    using ArchDock::PanelVisibilityMode;
    using ArchDock::PlasmaPanelHidingMode;

    const ArchDock::NativeVisibilityCapabilities unsupported;
    const QList<PanelVisibilityMode> requestedModes{
        PanelVisibilityMode::AutoHide,
        PanelVisibilityMode::DodgeActiveWindow,
        PanelVisibilityMode::HideForMaximizedOrFullscreen,
    };

    for (const PanelVisibilityMode requestedMode : requestedModes)
    {
        const auto result = ArchDock::resolveNativeVisibility(
            requestedMode,
            PanelVisibilityDecision::Conceal,
            false,
            unsupported);
        QVERIFY(!result.supported);
        QVERIFY(result.fallbackApplied);
        QVERIFY(!result.errorCode.isEmpty());
        QCOMPARE(result.requestedMode, requestedMode);
        QCOMPARE(result.effectiveMode, PanelVisibilityMode::AlwaysVisible);
        QCOMPARE(result.hostMode, PlasmaPanelHidingMode::None);
    }

    const auto manualHide = ArchDock::resolveNativeVisibility(
        PanelVisibilityMode::AlwaysVisible,
        PanelVisibilityDecision::Conceal,
        true,
        unsupported);
    QVERIFY(!manualHide.supported);
    QVERIFY(manualHide.fallbackApplied);
    QCOMPARE(manualHide.errorCode, QStringLiteral("manual-hide-unsupported"));
    QCOMPARE(manualHide.effectiveMode, PanelVisibilityMode::AlwaysVisible);
    QCOMPARE(manualHide.hostMode, PlasmaPanelHidingMode::None);
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
