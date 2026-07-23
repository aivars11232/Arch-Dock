#include "DockSettings.h"

#include <QGuiApplication>
#include <QSettings>
#include <QtGlobal>

DockSettings::DockSettings(QObject *parent)
    : QObject(parent)
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("dock"));
    m_position = settings.value(QStringLiteral("position"), m_position).toString().toLower();
    m_alignment = settings.value(QStringLiteral("alignment"), m_alignment).toString().toLower();
    m_appearancePreset = settings.value(QStringLiteral("appearancePreset"), m_appearancePreset).toString().toLower();
    m_monitorIndex = settings.value(QStringLiteral("monitorIndex"), m_monitorIndex).toInt();
    m_iconSize = settings.value(QStringLiteral("iconSize"), m_iconSize).toInt();
    m_spacing = settings.value(QStringLiteral("spacing"), m_spacing).toReal();
    m_magnification = settings.value(QStringLiteral("magnification"), m_magnification).toReal();
    m_magnificationEnabled = settings.value(QStringLiteral("magnificationEnabled"), m_magnificationEnabled).toBool();
    m_panelOpacity = settings.value(QStringLiteral("panelOpacity"), m_panelOpacity).toReal();
    m_showReflections = settings.value(QStringLiteral("showReflections"), m_showReflections).toBool();
    m_showIndicators = settings.value(QStringLiteral("showIndicators"), m_showIndicators).toBool();
    m_showTooltips = settings.value(QStringLiteral("showTooltips"), m_showTooltips).toBool();
    m_showStatusModule = settings.value(QStringLiteral("showStatusModule"), m_showStatusModule).toBool();
    m_showDate = settings.value(QStringLiteral("showDate"), m_showDate).toBool();
    m_showNetworkModule = settings.value(QStringLiteral("showNetworkModule"), m_showNetworkModule).toBool();
    m_showBatteryModule = settings.value(QStringLiteral("showBatteryModule"), m_showBatteryModule).toBool();
    m_showPerformanceModule = settings.value(QStringLiteral("showPerformanceModule"), m_showPerformanceModule).toBool();
    m_animationDuration = settings.value(QStringLiteral("animationDuration"), m_animationDuration).toInt();
    m_reducedMotion = settings.value(QStringLiteral("reducedMotion"), m_reducedMotion).toBool();
    m_autoHide = settings.value(QStringLiteral("autoHide"), m_autoHide).toBool();
    m_desktopSuite = settings.value(QStringLiteral("desktopSuite"), m_desktopSuite).toBool();
    m_topLauncherVisible = settings.value(QStringLiteral("topLauncherVisible"), m_topLauncherVisible).toBool();
    m_sideRailVisible = settings.value(QStringLiteral("sideRailVisible"), m_sideRailVisible).toBool();
    m_bottomPanelVisible = settings.value(QStringLiteral("bottomPanelVisible"), m_bottomPanelVisible).toBool();
    m_panelShape = settings.value(QStringLiteral("panelShape"), m_panelShape).toString().toLower();
    m_iconTileShape = settings.value(QStringLiteral("iconTileShape"), m_iconTileShape).toString().toLower();
    m_topPanelType = settings.value(QStringLiteral("topPanelType"), m_topPanelType).toString().toLower();
    m_sidePanelType = settings.value(QStringLiteral("sidePanelType"), m_sidePanelType).toString().toLower();
    m_bottomPanelType = settings.value(QStringLiteral("bottomPanelType"), m_bottomPanelType).toString().toLower();
    settings.endGroup();

    if (m_position != QStringLiteral("top") &&
        m_position != QStringLiteral("bottom") &&
        m_position != QStringLiteral("left") &&
        m_position != QStringLiteral("right"))
    {
        m_position = QStringLiteral("bottom");
    }
    m_iconSize = qBound(32, m_iconSize, 96);
    m_spacing = qBound<qreal>(0.0, m_spacing, 32.0);
    m_magnification = qBound<qreal>(1.0, m_magnification, 2.4);
    m_panelOpacity = qBound<qreal>(0.35, m_panelOpacity, 1.0);
    m_animationDuration = qBound(80, m_animationDuration, 500);
    if (m_alignment != QStringLiteral("start") &&
        m_alignment != QStringLiteral("center") &&
        m_alignment != QStringLiteral("end"))
    {
        m_alignment = QStringLiteral("center");
    }
    if (m_appearancePreset != QStringLiteral("glass") &&
        m_appearancePreset != QStringLiteral("crystal") &&
        m_appearancePreset != QStringLiteral("neon") &&
        m_appearancePreset != QStringLiteral("minimal") &&
        m_appearancePreset != QStringLiteral("plasma") &&
        m_appearancePreset != QStringLiteral("lime"))
    {
        m_appearancePreset = QStringLiteral("glass");
    }
    if (m_panelShape != QStringLiteral("pill") &&
        m_panelShape != QStringLiteral("rounded") &&
        m_panelShape != QStringLiteral("hexagon"))
    {
        m_panelShape = QStringLiteral("pill");
    }
    if (m_iconTileShape != QStringLiteral("rounded") &&
        m_iconTileShape != QStringLiteral("circle") &&
        m_iconTileShape != QStringLiteral("hexagon"))
    {
        m_iconTileShape = QStringLiteral("rounded");
    }
    if (m_topPanelType != QStringLiteral("launcher") &&
        m_topPanelType != QStringLiteral("tasks") &&
        m_topPanelType != QStringLiteral("hybrid"))
    {
        m_topPanelType = QStringLiteral("hybrid");
    }
    const auto normalizePanelType = [](QString &type)
    {
        if (type != QStringLiteral("launcher") &&
            type != QStringLiteral("tasks") &&
            type != QStringLiteral("hybrid"))
        {
            type = QStringLiteral("hybrid");
        }
    };
    normalizePanelType(m_topPanelType);
    normalizePanelType(m_sidePanelType);
    normalizePanelType(m_bottomPanelType);
    clampMonitorIndex();

    if (auto *application = qobject_cast<QGuiApplication *>(QCoreApplication::instance()))
    {
        connect(application, &QGuiApplication::screenAdded, this, [this]
                {
                    clampMonitorIndex();
                    emit screenCountChanged();
                });
        connect(application, &QGuiApplication::screenRemoved, this, [this]
                {
                    clampMonitorIndex();
                    emit screenCountChanged();
                });
    }
}

const QString &DockSettings::position() const
{
    return m_position;
}

const QString &DockSettings::alignment() const
{
    return m_alignment;
}

const QString &DockSettings::appearancePreset() const
{
    return m_appearancePreset;
}

int DockSettings::monitorIndex() const
{
    return m_monitorIndex;
}

int DockSettings::screenCount() const
{
    return QGuiApplication::screens().size();
}

int DockSettings::iconSize() const
{
    return m_iconSize;
}

qreal DockSettings::spacing() const
{
    return m_spacing;
}

qreal DockSettings::magnification() const
{
    return m_magnification;
}

bool DockSettings::magnificationEnabled() const
{
    return m_magnificationEnabled;
}

qreal DockSettings::panelOpacity() const
{
    return m_panelOpacity;
}

bool DockSettings::showReflections() const
{
    return m_showReflections;
}

bool DockSettings::showIndicators() const
{
    return m_showIndicators;
}

bool DockSettings::showTooltips() const
{
    return m_showTooltips;
}

bool DockSettings::showStatusModule() const
{
    return m_showStatusModule;
}

bool DockSettings::showDate() const
{
    return m_showDate;
}

bool DockSettings::showNetworkModule() const
{
    return m_showNetworkModule;
}

bool DockSettings::showBatteryModule() const
{
    return m_showBatteryModule;
}

bool DockSettings::showPerformanceModule() const
{
    return m_showPerformanceModule;
}

int DockSettings::animationDuration() const
{
    return m_animationDuration;
}

bool DockSettings::reducedMotion() const
{
    return m_reducedMotion;
}

bool DockSettings::autoHide() const
{
    return m_autoHide;
}

bool DockSettings::desktopSuite() const
{
    return m_desktopSuite;
}

bool DockSettings::topLauncherVisible() const { return m_topLauncherVisible; }
bool DockSettings::sideRailVisible() const { return m_sideRailVisible; }
bool DockSettings::bottomPanelVisible() const { return m_bottomPanelVisible; }
const QString &DockSettings::panelShape() const { return m_panelShape; }
const QString &DockSettings::iconTileShape() const { return m_iconTileShape; }
const QString &DockSettings::topPanelType() const { return m_topPanelType; }
const QString &DockSettings::sidePanelType() const { return m_sidePanelType; }
const QString &DockSettings::bottomPanelType() const { return m_bottomPanelType; }

void DockSettings::setPosition(const QString &position)
{
    const QString normalized = position.trimmed().toLower();
    if (normalized == m_position ||
        normalized != QStringLiteral("top") &&
            normalized != QStringLiteral("bottom") &&
            normalized != QStringLiteral("left") &&
            normalized != QStringLiteral("right"))
    {
        return;
    }

    m_position = normalized;
    save();
    emit positionChanged();
}

void DockSettings::setAlignment(const QString &alignment)
{
    const QString normalized = alignment.trimmed().toLower();
    if (normalized == m_alignment ||
        normalized != QStringLiteral("start") &&
            normalized != QStringLiteral("center") &&
            normalized != QStringLiteral("end"))
    {
        return;
    }

    m_alignment = normalized;
    save();
    emit alignmentChanged();
}

void DockSettings::setAppearancePreset(const QString &appearancePreset)
{
    const QString normalized = appearancePreset.trimmed().toLower();
    if (normalized == m_appearancePreset ||
        normalized != QStringLiteral("glass") &&
            normalized != QStringLiteral("crystal") &&
            normalized != QStringLiteral("neon") &&
            normalized != QStringLiteral("minimal") &&
            normalized != QStringLiteral("plasma") &&
            normalized != QStringLiteral("lime"))
    {
        return;
    }

    m_appearancePreset = normalized;
    save();
    emit appearancePresetChanged();
}

void DockSettings::setMonitorIndex(int monitorIndex)
{
    const int clamped = qBound(0, monitorIndex, qMax(0, screenCount() - 1));
    if (clamped == m_monitorIndex)
    {
        return;
    }

    m_monitorIndex = clamped;
    save();
    emit monitorIndexChanged();
}

void DockSettings::setIconSize(int iconSize)
{
    const int clamped = qBound(32, iconSize, 96);
    if (clamped == m_iconSize)
    {
        return;
    }

    m_iconSize = clamped;
    save();
    emit iconSizeChanged();
}

void DockSettings::setSpacing(qreal spacing)
{
    const qreal clamped = qBound<qreal>(0.0, spacing, 32.0);
    if (qFuzzyCompare(clamped, m_spacing))
    {
        return;
    }

    m_spacing = clamped;
    save();
    emit spacingChanged();
}

void DockSettings::setMagnification(qreal magnification)
{
    const qreal clamped = qBound<qreal>(1.0, magnification, 2.4);
    if (qFuzzyCompare(clamped, m_magnification))
    {
        return;
    }

    m_magnification = clamped;
    save();
    emit magnificationChanged();
}

void DockSettings::setMagnificationEnabled(bool magnificationEnabled)
{
    if (magnificationEnabled == m_magnificationEnabled)
    {
        return;
    }

    m_magnificationEnabled = magnificationEnabled;
    save();
    emit magnificationEnabledChanged();
}

void DockSettings::setPanelOpacity(qreal panelOpacity)
{
    const qreal clamped = qBound<qreal>(0.35, panelOpacity, 1.0);
    if (qFuzzyCompare(clamped, m_panelOpacity))
    {
        return;
    }

    m_panelOpacity = clamped;
    save();
    emit panelOpacityChanged();
}

void DockSettings::setShowReflections(bool showReflections)
{
    if (showReflections == m_showReflections)
    {
        return;
    }

    m_showReflections = showReflections;
    save();
    emit showReflectionsChanged();
}

void DockSettings::setShowIndicators(bool showIndicators)
{
    if (showIndicators == m_showIndicators)
    {
        return;
    }

    m_showIndicators = showIndicators;
    save();
    emit showIndicatorsChanged();
}

void DockSettings::setShowTooltips(bool showTooltips)
{
    if (showTooltips == m_showTooltips)
    {
        return;
    }

    m_showTooltips = showTooltips;
    save();
    emit showTooltipsChanged();
}

void DockSettings::setShowStatusModule(bool showStatusModule)
{
    if (showStatusModule == m_showStatusModule)
    {
        return;
    }

    m_showStatusModule = showStatusModule;
    save();
    emit showStatusModuleChanged();
}

void DockSettings::setShowDate(bool showDate)
{
    if (showDate == m_showDate)
    {
        return;
    }

    m_showDate = showDate;
    save();
    emit showDateChanged();
}

void DockSettings::setShowNetworkModule(bool showNetworkModule)
{
    if (showNetworkModule == m_showNetworkModule)
    {
        return;
    }

    m_showNetworkModule = showNetworkModule;
    save();
    emit showNetworkModuleChanged();
}

void DockSettings::setShowBatteryModule(bool showBatteryModule)
{
    if (showBatteryModule == m_showBatteryModule)
    {
        return;
    }

    m_showBatteryModule = showBatteryModule;
    save();
    emit showBatteryModuleChanged();
}

void DockSettings::setShowPerformanceModule(bool showPerformanceModule)
{
    if (showPerformanceModule == m_showPerformanceModule)
    {
        return;
    }

    m_showPerformanceModule = showPerformanceModule;
    save();
    emit showPerformanceModuleChanged();
}

void DockSettings::setAnimationDuration(int animationDuration)
{
    const int clamped = qBound(80, animationDuration, 500);
    if (clamped == m_animationDuration)
    {
        return;
    }

    m_animationDuration = clamped;
    save();
    emit animationDurationChanged();
}

void DockSettings::setReducedMotion(bool reducedMotion)
{
    if (reducedMotion == m_reducedMotion)
    {
        return;
    }

    m_reducedMotion = reducedMotion;
    save();
    emit reducedMotionChanged();
}

void DockSettings::setAutoHide(bool autoHide)
{
    if (autoHide == m_autoHide)
    {
        return;
    }

    m_autoHide = autoHide;
    save();
    emit autoHideChanged();
}

void DockSettings::setDesktopSuite(bool desktopSuite)
{
    if (desktopSuite == m_desktopSuite)
    {
        return;
    }

    m_desktopSuite = desktopSuite;
    save();
    emit desktopSuiteChanged();
}

void DockSettings::setTopLauncherVisible(bool value)
{
    if (value == m_topLauncherVisible) return;
    m_topLauncherVisible = value; save(); emit topLauncherVisibleChanged();
}

void DockSettings::setSideRailVisible(bool value)
{
    if (value == m_sideRailVisible) return;
    m_sideRailVisible = value; save(); emit sideRailVisibleChanged();
}

void DockSettings::setBottomPanelVisible(bool value)
{
    if (value == m_bottomPanelVisible) return;
    m_bottomPanelVisible = value; save(); emit bottomPanelVisibleChanged();
}

void DockSettings::setPanelShape(const QString &value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == m_panelShape ||
        normalized != QStringLiteral("pill") && normalized != QStringLiteral("rounded") && normalized != QStringLiteral("hexagon")) return;
    m_panelShape = normalized; save(); emit panelShapeChanged();
}

void DockSettings::setIconTileShape(const QString &value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == m_iconTileShape ||
        normalized != QStringLiteral("rounded") && normalized != QStringLiteral("circle") && normalized != QStringLiteral("hexagon")) return;
    m_iconTileShape = normalized; save(); emit iconTileShapeChanged();
}

void DockSettings::setTopPanelType(const QString &value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == m_topPanelType ||
        normalized != QStringLiteral("launcher") && normalized != QStringLiteral("tasks") && normalized != QStringLiteral("hybrid")) return;
    m_topPanelType = normalized; save(); emit topPanelTypeChanged();
}

void DockSettings::setBottomPanelType(const QString &value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == m_bottomPanelType ||
        normalized != QStringLiteral("launcher") && normalized != QStringLiteral("tasks") && normalized != QStringLiteral("hybrid")) return;
    m_bottomPanelType = normalized; save(); emit bottomPanelTypeChanged();
}

void DockSettings::setSidePanelType(const QString &value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == m_sidePanelType ||
        normalized != QStringLiteral("launcher") && normalized != QStringLiteral("tasks") && normalized != QStringLiteral("hybrid")) return;
    m_sidePanelType = normalized; save(); emit sidePanelTypeChanged();
}

void DockSettings::applyProfile(const QString &profileName)
{
    const QString normalized = profileName.trimmed().toLower();
    if (normalized == QStringLiteral("aurora"))
    {
        setDesktopSuite(true);
        setTopLauncherVisible(true);
        setSideRailVisible(true);
        setBottomPanelVisible(true);
        setPanelShape(QStringLiteral("pill"));
        setIconTileShape(QStringLiteral("rounded"));
        setTopPanelType(QStringLiteral("hybrid"));
        setSidePanelType(QStringLiteral("hybrid"));
        setBottomPanelType(QStringLiteral("hybrid"));
        setPosition(QStringLiteral("bottom"));
        setAlignment(QStringLiteral("center"));
        setAppearancePreset(QStringLiteral("lime"));
        setIconSize(48);
        setSpacing(7.0);
        setMagnificationEnabled(true);
        setMagnification(1.38);
        setPanelOpacity(0.94);
        setShowReflections(false);
        setShowIndicators(true);
        setShowStatusModule(true);
        setShowDate(true);
        setShowNetworkModule(true);
        setShowBatteryModule(true);
        setShowPerformanceModule(true);
        setAnimationDuration(140);
        setAutoHide(false);
    }
    else if (normalized == QStringLiteral("rocket"))
    {
        setAppearancePreset(QStringLiteral("glass"));
        setIconSize(64);
        setSpacing(8.0);
        setMagnificationEnabled(true);
        setMagnification(1.9);
        setPanelOpacity(0.93);
        setShowReflections(true);
        setShowIndicators(true);
        setShowStatusModule(false);
        setShowNetworkModule(true);
        setShowBatteryModule(true);
        setShowPerformanceModule(false);
        setAnimationDuration(180);
    }
    else if (normalized == QStringLiteral("waybar"))
    {
        setAppearancePreset(QStringLiteral("minimal"));
        setIconSize(44);
        setSpacing(6.0);
        setMagnificationEnabled(false);
        setPanelOpacity(0.96);
        setShowReflections(false);
        setShowIndicators(true);
        setShowStatusModule(true);
        setShowDate(true);
        setShowNetworkModule(true);
        setShowBatteryModule(true);
        setShowPerformanceModule(true);
        setAnimationDuration(120);
    }
    else if (normalized == QStringLiteral("neon"))
    {
        setAppearancePreset(QStringLiteral("neon"));
        setIconSize(54);
        setSpacing(10.0);
        setMagnificationEnabled(true);
        setMagnification(1.75);
        setPanelOpacity(0.9);
        setShowReflections(true);
        setShowIndicators(true);
        setShowStatusModule(true);
        setShowNetworkModule(true);
        setShowBatteryModule(true);
        setShowPerformanceModule(false);
        setAnimationDuration(160);
    }
    else if (normalized == QStringLiteral("crystal"))
    {
        setAppearancePreset(QStringLiteral("crystal"));
        setIconSize(56);
        setSpacing(10.0);
        setMagnificationEnabled(true);
        setMagnification(1.65);
        setPanelOpacity(0.82);
        setShowReflections(true);
        setShowIndicators(true);
        setShowStatusModule(false);
        setShowNetworkModule(true);
        setShowBatteryModule(true);
        setShowPerformanceModule(false);
        setAnimationDuration(170);
    }
    else if (normalized == QStringLiteral("plasma"))
    {
        setAppearancePreset(QStringLiteral("plasma"));
        setIconSize(48);
        setSpacing(5.0);
        setMagnificationEnabled(false);
        setPanelOpacity(0.96);
        setShowReflections(false);
        setShowIndicators(true);
        setShowStatusModule(true);
        setShowDate(true);
        setShowNetworkModule(true);
        setShowBatteryModule(true);
        setShowPerformanceModule(false);
        setAnimationDuration(130);
    }
    else if (normalized == QStringLiteral("lime"))
    {
        setAppearancePreset(QStringLiteral("lime"));
        setIconSize(50);
        setSpacing(8.0);
        setMagnificationEnabled(true);
        setMagnification(1.35);
        setPanelOpacity(0.94);
        setShowReflections(false);
        setShowIndicators(true);
        setShowStatusModule(true);
        setShowDate(false);
        setShowNetworkModule(true);
        setShowBatteryModule(true);
        setShowPerformanceModule(false);
        setAnimationDuration(130);
    }
}

void DockSettings::reset()
{
    setPosition(QStringLiteral("bottom"));
    setAlignment(QStringLiteral("center"));
    setAppearancePreset(QStringLiteral("glass"));
    setMonitorIndex(0);
    setIconSize(56);
    setSpacing(10.0);
    setMagnification(1.65);
    setMagnificationEnabled(true);
    setPanelOpacity(0.88);
    setShowReflections(true);
    setShowIndicators(true);
    setShowTooltips(true);
    setShowStatusModule(false);
    setShowDate(false);
    setShowNetworkModule(true);
    setShowBatteryModule(true);
    setShowPerformanceModule(false);
    setAnimationDuration(170);
    setAutoHide(false);
    setDesktopSuite(false);
    setTopLauncherVisible(true);
    setSideRailVisible(true);
    setBottomPanelVisible(true);
    setPanelShape(QStringLiteral("pill"));
    setIconTileShape(QStringLiteral("rounded"));
    setTopPanelType(QStringLiteral("hybrid"));
    setSidePanelType(QStringLiteral("hybrid"));
    setBottomPanelType(QStringLiteral("hybrid"));
}

void DockSettings::save() const
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("dock"));
    settings.setValue(QStringLiteral("position"), m_position);
    settings.setValue(QStringLiteral("alignment"), m_alignment);
    settings.setValue(QStringLiteral("appearancePreset"), m_appearancePreset);
    settings.setValue(QStringLiteral("monitorIndex"), m_monitorIndex);
    settings.setValue(QStringLiteral("iconSize"), m_iconSize);
    settings.setValue(QStringLiteral("spacing"), m_spacing);
    settings.setValue(QStringLiteral("magnification"), m_magnification);
    settings.setValue(QStringLiteral("magnificationEnabled"), m_magnificationEnabled);
    settings.setValue(QStringLiteral("panelOpacity"), m_panelOpacity);
    settings.setValue(QStringLiteral("showReflections"), m_showReflections);
    settings.setValue(QStringLiteral("showIndicators"), m_showIndicators);
    settings.setValue(QStringLiteral("showTooltips"), m_showTooltips);
    settings.setValue(QStringLiteral("showStatusModule"), m_showStatusModule);
    settings.setValue(QStringLiteral("showDate"), m_showDate);
    settings.setValue(QStringLiteral("showNetworkModule"), m_showNetworkModule);
    settings.setValue(QStringLiteral("showBatteryModule"), m_showBatteryModule);
    settings.setValue(QStringLiteral("showPerformanceModule"), m_showPerformanceModule);
    settings.setValue(QStringLiteral("animationDuration"), m_animationDuration);
    settings.setValue(QStringLiteral("reducedMotion"), m_reducedMotion);
    settings.setValue(QStringLiteral("autoHide"), m_autoHide);
    settings.setValue(QStringLiteral("desktopSuite"), m_desktopSuite);
    settings.setValue(QStringLiteral("topLauncherVisible"), m_topLauncherVisible);
    settings.setValue(QStringLiteral("sideRailVisible"), m_sideRailVisible);
    settings.setValue(QStringLiteral("bottomPanelVisible"), m_bottomPanelVisible);
    settings.setValue(QStringLiteral("panelShape"), m_panelShape);
    settings.setValue(QStringLiteral("iconTileShape"), m_iconTileShape);
    settings.setValue(QStringLiteral("topPanelType"), m_topPanelType);
    settings.setValue(QStringLiteral("sidePanelType"), m_sidePanelType);
    settings.setValue(QStringLiteral("bottomPanelType"), m_bottomPanelType);
    settings.remove(QStringLiteral("freePanelVisible"));
    settings.remove(QStringLiteral("freePanelType"));
    settings.remove(QStringLiteral("freePanelX"));
    settings.remove(QStringLiteral("freePanelY"));
    settings.endGroup();
    settings.sync();
}

void DockSettings::clampMonitorIndex()
{
    m_monitorIndex = qBound(0, m_monitorIndex, qMax(0, screenCount() - 1));
}