#pragma once

#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <optional>

namespace ArchDock
{

enum class PanelHostKind
{
    NativeEdge,
    FreeDesktop
};

enum class PanelLayoutKind
{
    Adaptive,
    Horizontal,
    Vertical,
    Diagonal,
    Circular,
    Ellipse,
    Ring,
    Radial,
    Arc,
    Semicircle,
    Fan,
    Spiral,
    Ribbon,
    VerticalCurve,
    HorizontalCurve,
    Polygon,
    Triangle,
    Square,
    Pentagon,
    Hexagon,
    Octagon,
    Star,
    Grid,
    Floating,
    Count
};

[[nodiscard]] std::optional<PanelLayoutKind> panelLayoutKindFromName(
    const QString &name);
[[nodiscard]] QString panelLayoutKindName(PanelLayoutKind kind);

struct PanelPresetOrigin
{
    QString panelPresetId;
    int panelPresetRevision = 0;
    QString iconPresetId;
    int iconPresetRevision = 0;
    bool customizedAfterApply = false;
    bool detachedFromPreset = false;

    bool operator==(const PanelPresetOrigin &) const = default;
};

struct PanelIdentity
{
    QString id;
    QString name;
    bool builtIn = false;

    bool operator==(const PanelIdentity &) const = default;
};

struct PanelHost
{
    PanelHostKind kind = PanelHostKind::NativeEdge;
    int screenIndex = 0;
    QString screenId;

    int nativePanelId = -1;
    int nativeControlAppletId = -1;
    int nativeDockAppletId = -1;
    QString nativeOwnershipToken;
    QString nativeRecoveryState = QStringLiteral("idle");
    QString nativeRecoveryError;

    int freeDesktopContainmentId = -1;
    int freeDockAppletId = -1;
    QString freeOwnershipToken;
    QString freeHostMode = QStringLiteral("desktop");
    QString freeHostState = QStringLiteral("unhosted");
    QString freeCreationState = QStringLiteral("idle");
    QString freeCreationError;
    QString freeRollbackError;
    QString freeRecoveryError;

    bool operator==(const PanelHost &) const = default;
};

struct PanelContent
{
    QString type = QStringLiteral("hybrid");
    QStringList applicationIds;
    QStringList urls;
    QStringList kdeWidgets;
    bool acceptDrops = true;
    QString folderLayout = QStringLiteral("fan");
    int folderSpeed = 260;
    QString folderEasing = QStringLiteral("outBack");
    bool folderExpandOnClick = true;

    bool operator==(const PanelContent &) const = default;
};

struct PanelPlacementDefinition
{
    QString edge = QStringLiteral("bottom");
    QString alignment = QStringLiteral("center");
    bool dynamic = false;
    int width = 720;
    int height = 76;
    int x = 180;
    int y = 180;
    std::optional<int> offset;
    std::optional<int> floatingMargin;
    std::optional<int> thickness;
    std::optional<QString> lengthMode;
    std::optional<int> minimumLength;
    std::optional<int> maximumLength;

    bool operator==(const PanelPlacementDefinition &) const = default;
};

struct PanelVisibilityDefinition
{
    bool visible = false;
    QString hostMode = QStringLiteral("always");
    int revealZone = 10;
    int openDelay = 0;
    int closeDelay = 0;
    QString windowOverlapPolicy;

    bool operator==(const PanelVisibilityDefinition &) const = default;
};

struct PanelPresentationDefinition
{
    QString mode;
    QString collapseAxis;
    QString collapseMechanism;
    QString revealHandle;

    bool operator==(const PanelPresentationDefinition &) const = default;
};

struct PanelLayoutDefinition
{
    QString pathType = QStringLiteral("adaptive");
    qreal scale = 1.0;
    qreal angle = 0.0;
    int radius = 150;
    int rows = 2;
    int padding = 18;
    int polygonSides = 6;
    QString orientation = QStringLiteral("upright");
    QString anchor = QStringLiteral("center");

    bool operator==(const PanelLayoutDefinition &) const = default;
};

struct PanelSurfaceDefinition
{
    QString rendererTier;
    QString panelThemeId;
    QString completeThemeId;
    QString appearance = QStringLiteral("glass");
    QString shape = QStringLiteral("pill");
    qreal opacity = 0.9;
    QString color;
    qreal glowIntensity = 1.0;
    QVariantMap border;
    QVariantMap glow;
    QVariantMap shadow;
    QVariantMap blur;
    QVariantMap parameters2D;
    QVariantMap parameters2_5D;
    QVariantMap parameters3D;

    QString themeAsset;
    QString themeSource;
    QString themeFit = QStringLiteral("cover");
    QString themeSourceKind;
    QString themeSourceFormat;
    int themeSourceWidth = 0;
    int themeSourceHeight = 0;
    bool themeSourceHasAlpha = false;
    QString themeSuggestedFit = QStringLiteral("cover");
    QString themePreview;
    QString themeAnalysisStatus;
    QString themeConversionTool;
    bool themeConversionAvailable = false;
    QString themePackageFormat;
    int themePackageVersion = 0;
    QString themePackageId;
    QString themePackageName;
    QString themePackageAuthor;
    QString themePackageManifest;
    QString themeStatus;
    int themeRenderWidth = 0;
    int themeRenderHeight = 0;
    QString themeRenderFit;
    QString themeRenderOutcome;

    bool operator==(const PanelSurfaceDefinition &) const = default;
};

struct PanelIconStyleDefinition
{
    QString styleReference;
    QString themeId;
    QString shape = QStringLiteral("rounded");
    int size = 52;
    qreal spacing = 8.0;
    QVariantMap globalDefaults;
    QVariantMap perEntryOverrides;

    bool operator==(const PanelIconStyleDefinition &) const = default;
};

struct PanelMotionDefinition
{
    QString iconProfile = QStringLiteral("scale");
    QString trigger = QStringLiteral("hover");
    qreal speed = 1.0;
    qreal intensity = 1.0;
    bool physicsEnabled = false;
    QString panelProfile;
    QString revealProfile;
    bool reducedMotion = false;

    bool operator==(const PanelMotionDefinition &) const = default;
};

class PanelDefinition
{
public:
    static constexpr int CurrentSchemaVersion = 2;

    int schemaVersion = CurrentSchemaVersion;
    quint64 settingsRevision = 0;
    PanelIdentity identity;
    PanelHost host;
    PanelContent content;
    PanelPlacementDefinition placement;
    PanelVisibilityDefinition visibility;
    PanelPresentationDefinition presentation;
    PanelLayoutDefinition layout;
    PanelSurfaceDefinition surface;
    PanelIconStyleDefinition iconStyle;
    PanelMotionDefinition motion;
    std::optional<PanelPresetOrigin> presetOrigin;
    QVariantMap extensions;

    [[nodiscard]] static PanelDefinition defaults(const QString &id,
                                                  const QString &name,
                                                  const QString &edge,
                                                  bool builtIn);
    [[nodiscard]] static std::optional<PanelDefinition> fromLegacyMap(
        const QVariantMap &record,
        QString *errorMessage = nullptr);
    [[nodiscard]] PanelDefinition normalized() const;
    [[nodiscard]] bool isValid(QString *errorMessage = nullptr) const;
    [[nodiscard]] QVariantMap toLegacyMap() const;
    [[nodiscard]] QVariantMap toPersistedMap() const;

    [[nodiscard]] static bool isDurableLegacyKey(const QString &key);
    [[nodiscard]] static QVariant normalizeLegacyValue(const QString &key,
                                                       const QVariant &value);
    [[nodiscard]] static QString hostKindName(PanelHostKind kind);
    [[nodiscard]] static PanelHostKind hostKindFromName(const QString &name,
                                                        const QString &edge);

    bool operator==(const PanelDefinition &) const = default;
};

}
