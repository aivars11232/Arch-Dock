#pragma once

#include <QObject>

class DockSettings final : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString position READ position WRITE setPosition NOTIFY positionChanged)
    Q_PROPERTY(QString alignment READ alignment WRITE setAlignment NOTIFY alignmentChanged)
    Q_PROPERTY(QString appearancePreset READ appearancePreset WRITE setAppearancePreset NOTIFY appearancePresetChanged)
    Q_PROPERTY(int monitorIndex READ monitorIndex WRITE setMonitorIndex NOTIFY monitorIndexChanged)
    Q_PROPERTY(int screenCount READ screenCount NOTIFY screenCountChanged)
    Q_PROPERTY(int iconSize READ iconSize WRITE setIconSize NOTIFY iconSizeChanged)
    Q_PROPERTY(qreal spacing READ spacing WRITE setSpacing NOTIFY spacingChanged)
    Q_PROPERTY(qreal magnification READ magnification WRITE setMagnification NOTIFY magnificationChanged)
    Q_PROPERTY(bool magnificationEnabled READ magnificationEnabled WRITE setMagnificationEnabled NOTIFY magnificationEnabledChanged)
    Q_PROPERTY(qreal panelOpacity READ panelOpacity WRITE setPanelOpacity NOTIFY panelOpacityChanged)
    Q_PROPERTY(bool showReflections READ showReflections WRITE setShowReflections NOTIFY showReflectionsChanged)
    Q_PROPERTY(bool showIndicators READ showIndicators WRITE setShowIndicators NOTIFY showIndicatorsChanged)
    Q_PROPERTY(bool showTooltips READ showTooltips WRITE setShowTooltips NOTIFY showTooltipsChanged)
    Q_PROPERTY(bool showStatusModule READ showStatusModule WRITE setShowStatusModule NOTIFY showStatusModuleChanged)
    Q_PROPERTY(bool showDate READ showDate WRITE setShowDate NOTIFY showDateChanged)
    Q_PROPERTY(bool showNetworkModule READ showNetworkModule WRITE setShowNetworkModule NOTIFY showNetworkModuleChanged)
    Q_PROPERTY(bool showBatteryModule READ showBatteryModule WRITE setShowBatteryModule NOTIFY showBatteryModuleChanged)
    Q_PROPERTY(bool showPerformanceModule READ showPerformanceModule WRITE setShowPerformanceModule NOTIFY showPerformanceModuleChanged)
    Q_PROPERTY(int animationDuration READ animationDuration WRITE setAnimationDuration NOTIFY animationDurationChanged)
    Q_PROPERTY(bool reducedMotion READ reducedMotion WRITE setReducedMotion NOTIFY reducedMotionChanged)
    Q_PROPERTY(bool autoHide READ autoHide WRITE setAutoHide NOTIFY autoHideChanged)
    Q_PROPERTY(bool desktopSuite READ desktopSuite WRITE setDesktopSuite NOTIFY desktopSuiteChanged)
    Q_PROPERTY(bool topLauncherVisible READ topLauncherVisible WRITE setTopLauncherVisible NOTIFY topLauncherVisibleChanged)
    Q_PROPERTY(bool sideRailVisible READ sideRailVisible WRITE setSideRailVisible NOTIFY sideRailVisibleChanged)
    Q_PROPERTY(bool bottomPanelVisible READ bottomPanelVisible WRITE setBottomPanelVisible NOTIFY bottomPanelVisibleChanged)
    Q_PROPERTY(QString panelShape READ panelShape WRITE setPanelShape NOTIFY panelShapeChanged)
    Q_PROPERTY(QString iconTileShape READ iconTileShape WRITE setIconTileShape NOTIFY iconTileShapeChanged)
    Q_PROPERTY(QString topPanelType READ topPanelType WRITE setTopPanelType NOTIFY topPanelTypeChanged)
    Q_PROPERTY(QString sidePanelType READ sidePanelType WRITE setSidePanelType NOTIFY sidePanelTypeChanged)
    Q_PROPERTY(QString bottomPanelType READ bottomPanelType WRITE setBottomPanelType NOTIFY bottomPanelTypeChanged)

public:
    explicit DockSettings(QObject *parent = nullptr);

    [[nodiscard]] const QString &position() const;
    [[nodiscard]] const QString &alignment() const;
    [[nodiscard]] const QString &appearancePreset() const;
    [[nodiscard]] int monitorIndex() const;
    [[nodiscard]] int screenCount() const;
    [[nodiscard]] int iconSize() const;
    [[nodiscard]] qreal spacing() const;
    [[nodiscard]] qreal magnification() const;
    [[nodiscard]] bool magnificationEnabled() const;
    [[nodiscard]] qreal panelOpacity() const;
    [[nodiscard]] bool showReflections() const;
    [[nodiscard]] bool showIndicators() const;
    [[nodiscard]] bool showTooltips() const;
    [[nodiscard]] bool showStatusModule() const;
    [[nodiscard]] bool showDate() const;
    [[nodiscard]] bool showNetworkModule() const;
    [[nodiscard]] bool showBatteryModule() const;
    [[nodiscard]] bool showPerformanceModule() const;
    [[nodiscard]] int animationDuration() const;
    [[nodiscard]] bool reducedMotion() const;
    [[nodiscard]] bool autoHide() const;
    [[nodiscard]] bool desktopSuite() const;
    [[nodiscard]] bool topLauncherVisible() const;
    [[nodiscard]] bool sideRailVisible() const;
    [[nodiscard]] bool bottomPanelVisible() const;
    [[nodiscard]] const QString &panelShape() const;
    [[nodiscard]] const QString &iconTileShape() const;
    [[nodiscard]] const QString &topPanelType() const;
    [[nodiscard]] const QString &sidePanelType() const;
    [[nodiscard]] const QString &bottomPanelType() const;

public slots:
    void setPosition(const QString &position);
    void setAlignment(const QString &alignment);
    void setAppearancePreset(const QString &appearancePreset);
    void setMonitorIndex(int monitorIndex);
    void setIconSize(int iconSize);
    void setSpacing(qreal spacing);
    void setMagnification(qreal magnification);
    void setMagnificationEnabled(bool magnificationEnabled);
    void setPanelOpacity(qreal panelOpacity);
    void setShowReflections(bool showReflections);
    void setShowIndicators(bool showIndicators);
    void setShowTooltips(bool showTooltips);
    void setShowStatusModule(bool showStatusModule);
    void setShowDate(bool showDate);
    void setShowNetworkModule(bool showNetworkModule);
    void setShowBatteryModule(bool showBatteryModule);
    void setShowPerformanceModule(bool showPerformanceModule);
    void setAnimationDuration(int animationDuration);
    void setReducedMotion(bool reducedMotion);
    void setAutoHide(bool autoHide);
    void setDesktopSuite(bool desktopSuite);
    void setTopLauncherVisible(bool topLauncherVisible);
    void setSideRailVisible(bool sideRailVisible);
    void setBottomPanelVisible(bool bottomPanelVisible);
    void setPanelShape(const QString &panelShape);
    void setIconTileShape(const QString &iconTileShape);
    void setTopPanelType(const QString &topPanelType);
    void setSidePanelType(const QString &sidePanelType);
    void setBottomPanelType(const QString &bottomPanelType);
    void applyProfile(const QString &profileName);
    void reset();

signals:
    void positionChanged();
    void alignmentChanged();
    void appearancePresetChanged();
    void monitorIndexChanged();
    void screenCountChanged();
    void iconSizeChanged();
    void spacingChanged();
    void magnificationChanged();
    void magnificationEnabledChanged();
    void panelOpacityChanged();
    void showReflectionsChanged();
    void showIndicatorsChanged();
    void showTooltipsChanged();
    void showStatusModuleChanged();
    void showDateChanged();
    void showNetworkModuleChanged();
    void showBatteryModuleChanged();
    void showPerformanceModuleChanged();
    void animationDurationChanged();
    void reducedMotionChanged();
    void autoHideChanged();
    void desktopSuiteChanged();
    void topLauncherVisibleChanged();
    void sideRailVisibleChanged();
    void bottomPanelVisibleChanged();
    void panelShapeChanged();
    void iconTileShapeChanged();
    void topPanelTypeChanged();
    void sidePanelTypeChanged();
    void bottomPanelTypeChanged();

private:
    void save() const;
    void clampMonitorIndex();

    QString m_position = QStringLiteral("bottom");
    QString m_alignment = QStringLiteral("center");
    QString m_appearancePreset = QStringLiteral("glass");
    int m_monitorIndex = 0;
    int m_iconSize = 56;
    qreal m_spacing = 10.0;
    qreal m_magnification = 1.65;
    bool m_magnificationEnabled = true;
    qreal m_panelOpacity = 0.88;
    bool m_showReflections = true;
    bool m_showIndicators = true;
    bool m_showTooltips = true;
    bool m_showStatusModule = false;
    bool m_showDate = false;
    bool m_showNetworkModule = true;
    bool m_showBatteryModule = true;
    bool m_showPerformanceModule = false;
    int m_animationDuration = 170;
    bool m_reducedMotion = false;
    bool m_autoHide = false;
    bool m_desktopSuite = false;
    bool m_topLauncherVisible = true;
    bool m_sideRailVisible = true;
    bool m_bottomPanelVisible = true;
    QString m_panelShape = QStringLiteral("pill");
    QString m_iconTileShape = QStringLiteral("rounded");
    QString m_topPanelType = QStringLiteral("hybrid");
    QString m_sidePanelType = QStringLiteral("hybrid");
    QString m_bottomPanelType = QStringLiteral("hybrid");
};