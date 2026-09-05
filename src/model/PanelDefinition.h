#pragma once

#include <QMap>
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
    // One ordered list of panel-specific entry ids across both lists above:
    // application ids verbatim and URLs as `free-url:<encoded>`. It is always
    // kept canonical (every known entry exactly once, unknown ids dropped), so
    // reordering a free panel is a permutation of this list and nothing else.
    QStringList entryOrder;
    QStringList kdeWidgets;
    bool acceptDrops = true;
    QString folderLayout = QStringLiteral("fan");
    int folderSpeed = 260;
    QString folderEasing = QStringLiteral("outBack");
    bool folderExpandOnClick = true;

    [[nodiscard]] static QString urlEntryId(const QString &url);
    [[nodiscard]] static bool isUrlEntryId(const QString &entryId);
    [[nodiscard]] static QString urlFromEntryId(const QString &entryId);
    [[nodiscard]] QStringList knownEntryIds() const;
    [[nodiscard]] QStringList canonicalEntryOrder() const;

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
    // These were empty placeholders while nothing could honour them. They
    // carry real defaults now that the panel has a motion engine: an empty
    // mechanism would leave a collapsed panel with no way to be drawn.
    QString mode = QStringLiteral("open");
    QString trigger = QStringLiteral("hover");
    QString collapseAxis = QStringLiteral("horizontal");
    QString collapseMechanism = QStringLiteral("open");
    QString revealHandle = QStringLiteral("edge-strip");

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
    // Whole-scene rotation for free panels: `none`, `clockwise` or
    // `counter-clockwise`, in degrees per second, started by `idle` or
    // `hover`. The running angle offset is runtime state and is never stored.
    QString rotationMode = QStringLiteral("none");
    qreal rotationSpeed = 12.0;
    QString rotationTrigger = QStringLiteral("idle");

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
    QString styleReference = QStringLiteral("plain-original");
    QString themeId;
    QString shape = QStringLiteral("rounded");
    int size = 52;
    qreal spacing = 8.0;
    QVariantMap globalDefaults;
    struct EntryOverride
    {
        QString customGlyph;
        QString customLabel;
        std::optional<bool> tileEnabled;
        QString styleReference;
        QString animationProfileReference;
        QVariantMap extensions;

        [[nodiscard]] bool isEmpty() const;
        [[nodiscard]] QVariantMap toVariantMap() const;
        [[nodiscard]] static std::optional<EntryOverride> fromVariantMap(
            const QVariantMap &record,
            QString *errorMessage = nullptr);

        bool operator==(const EntryOverride &) const = default;
    };
    QMap<QString, EntryOverride> perEntryOverrides;

    bool operator==(const PanelIconStyleDefinition &) const = default;
};

struct PanelMotionDefinition
{
    QString iconProfile = QStringLiteral("scale");
    QString trigger = QStringLiteral("hover");
    qreal speed = 1.0;
    qreal intensity = 1.0;
    // How far the hovered icon's magnification reaches, in entry positions,
    // and the shape of its decay. Visual influence only: the layout and every
    // hit area stay exactly where they were.
    qreal magnifyRadius = 2.4;
    QString magnifyFalloff = QStringLiteral("linear");
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
