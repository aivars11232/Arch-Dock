#pragma once

#include <QHash>
#include <QObject>
#include <QByteArray>
#include <QStringList>
#include <QUrl>
#include <QVariant>
#include <QVariantMap>

#include <optional>

#include "model/PanelDefinition.h"
#include "model/PanelCapabilityResolver.h"
#include "animation/AnimationProfileCatalog.h"
#include "iconstyles/IconStyleStore.h"
#include "panel/PanelSettingsTransaction.h"
#include "themes/ThemePackage.h"

class QSettings;

// Every panel Arch Dock manages: their saved definitions, revisions and
// migration, the built-in theme catalogue and theme packages, imported
// artwork, and the capability resolution that says what each panel can be
// (renderer, layouts, controls). Settings change only through its
// revision-checked transactions. It is the model behind Panel Studio and
// both panel hosts.
class PanelRegistry final : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QStringList panelIds READ panelIds NOTIFY panelsChanged)
    Q_PROPERTY(QString activePanelId READ activePanelId WRITE setActivePanelId NOTIFY activePanelIdChanged)
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)
    Q_PROPERTY(QString migrationDiagnostic READ migrationDiagnostic NOTIFY migrationDiagnosticChanged)

public:
    enum class FreeHostState
    {
        Unhosted,
        HostedOwned,
        HostedStale,
        Detached
    };

    struct FreeHostAssociation
    {
        int desktopContainmentId = -1;
        int dockAppletId = -1;
        QString ownershipToken;
        int screenIndex = 0;
        QString screenId;
        QString hostMode = QStringLiteral("desktop");
        FreeHostState state = FreeHostState::Unhosted;
    };

    explicit PanelRegistry(QObject *parent = nullptr);
    explicit PanelRegistry(const QVariantList &themeDefinitions,
                           QObject *parent = nullptr);
    ~PanelRegistry() override;

    [[nodiscard]] QStringList panelIds() const;
    [[nodiscard]] QString activePanelId() const;
    [[nodiscard]] int revision() const;
    [[nodiscard]] QString migrationDiagnostic() const;

    Q_INVOKABLE QVariant panelValue(const QString &panelId, const QString &key) const;
    Q_INVOKABLE QVariantMap panelSnapshot(const QString &panelId) const;
    Q_INVOKABLE QString panelName(const QString &panelId) const;
    Q_INVOKABLE bool isBuiltIn(const QString &panelId) const;
    void setPanelValue(const QString &panelId, const QString &key, const QVariant &value);
    void updatePanel(const QString &panelId, const QVariantMap &values);
    [[nodiscard]] bool updatePanelChecked(const QString &panelId, const QVariantMap &values);
    [[nodiscard]] std::optional<ArchDock::PanelDefinition> panelDefinition(
        const QString &panelId,
        QString *errorMessage = nullptr) const;
    [[nodiscard]] QList<ArchDock::PanelDefinition> panelDefinitions(
        QString *errorMessage = nullptr) const;
    [[nodiscard]] bool panelSetMatches(const QList<ArchDock::PanelDefinition> &expected,
        QString *errorMessage = nullptr) const;
    // One durable full-set write after the caller verifies every host. The
    // caller publishes adoption through notifyPanelSettingsTransactionAdopted.
    [[nodiscard]] bool persistPanelSetTransaction(
        const QList<ArchDock::PanelDefinition> &expected,
        const QList<ArchDock::PanelDefinition> &candidate,
        QString *errorMessage = nullptr);
    [[nodiscard]] std::optional<ArchDock::ThemeCapabilityProfile>
    themeCapabilityProfile(const ArchDock::PanelDefinition &definition,
                           QString *errorCode = nullptr) const;
    [[nodiscard]] std::optional<QVariantMap> themeRuntimeProjection(
        const ArchDock::PanelDefinition &definition,
        QString *errorCode = nullptr) const;
    [[nodiscard]] std::optional<QVariantMap> builtInThemeRuntimeProjection(
        const QString &themeId,
        QString *errorCode = nullptr) const;
    // A free panel whose look ships no 3D scene of its own can still be drawn
    // in 3D when its layout has an exact generated platform (a circle,
    // ellipse, regular polygon or the radial arc): the renderer generates the
    // platform along the layout's path, in the look's own colours (sampled
    // from `lookProjection`'s artwork when it has some). Returns {scene3D,
    // scene3DResources}, or nothing when the panel or its look cannot be
    // adapted. The look keeps its id and its own renderer.
    [[nodiscard]] std::optional<QVariantMap> genericScene3D(
        const ArchDock::PanelDefinition &definition,
        const QVariantMap &look,
        const QVariantMap &lookProjection) const;
    [[nodiscard]] ArchDock::CapabilityResolution resolvePanelCapabilities(
        const ArchDock::PanelDefinition &definition,
        QString *errorCode = nullptr) const;
    [[nodiscard]] bool persistPanelSettingsTransaction(
        const ArchDock::PanelSettingsTransactionDraft &draft,
        QString *errorMessage = nullptr);
    [[nodiscard]] bool persistPanelDefinitionTransaction(
        const ArchDock::PanelDefinition &previousPanel,
        const ArchDock::PanelDefinition &candidatePanel,
        const QVariantMap &globalSettings,
        QString *errorMessage = nullptr);
    // The caller verifies and converts the preview host before this single
    // durable insertion. No provisional registry record is needed.
    [[nodiscard]] bool adoptPreviewPanel(
        const ArchDock::PanelDefinition &definition,
        QString *errorMessage = nullptr);
    [[nodiscard]] bool rollbackPanelSettingsTransaction(
        const ArchDock::PanelSettingsTransactionDraft &draft,
        quint64 committedRevision,
        quint64 *rollbackRevision,
        QString *errorMessage = nullptr);
    void notifyPanelSettingsTransactionAdopted(bool nativeTopologyChanged);
    [[nodiscard]] std::optional<FreeHostAssociation> freeHostAssociation(const QString &panelId) const;
    [[nodiscard]] QString beginFreePanelCreation(const QString &ownershipToken);
    [[nodiscard]] bool commitVerifiedFreeHostAssociation(
        const QString &panelId,
        int desktopContainmentId,
        int dockAppletId,
        const QString &ownershipToken,
        int screenIndex,
        const QString &screenId,
        const QString &hostMode);
    [[nodiscard]] bool completeFreePanelCreation(
        const QString &panelId,
        const QString &ownershipToken);
    [[nodiscard]] bool recordRecoverableFreePanelCreation(
        const QString &panelId,
        int desktopContainmentId,
        int dockAppletId,
        const QString &ownershipToken,
        int screenIndex,
        const QString &screenId,
        const QString &creationError,
        const QString &rollbackError);
    [[nodiscard]] bool discardFreePanelCreation(
        const QString &panelId,
        const QString &ownershipToken);
    [[nodiscard]] bool rebindRecoveredFreeHostAssociation(
        const QString &panelId,
        int desktopContainmentId,
        int dockAppletId,
        const QString &ownershipToken,
        int screenIndex,
        const QString &screenId);
    [[nodiscard]] bool detachFreeHostAssociation(
        const QString &panelId,
        const QString &ownershipToken,
        const QString &recoveryError);
    [[nodiscard]] bool recordFreeHostRecoveryError(
        const QString &panelId,
        const QString &ownershipToken,
        const QString &errorCode);
    [[nodiscard]] bool removeDetachedFreePanel(const QString &panelId);
    [[nodiscard]] bool commitVerifiedNativePanelAssociation(
        const QString &panelId,
        int containmentId,
        int dockAppletId,
        const QString &ownershipToken);
    [[nodiscard]] bool rebindRecoveredNativePanelAssociation(
        const QString &panelId,
        int containmentId,
        int dockAppletId,
        const QString &ownershipToken);
    [[nodiscard]] bool detachMissingNativePanelAssociation(
        const QString &panelId,
        const QString &ownershipToken,
        bool recordVisible);
    [[nodiscard]] bool recordNativePanelRecoveryConflict(
        const QString &panelId,
        const QString &ownershipToken,
        const QString &errorCode);
    [[nodiscard]] bool recordNativePanelRecoveryFailure(
        const QString &panelId,
        const QString &errorCode);
    Q_INVOKABLE QVariantList themeDefinitions() const;
    Q_INVOKABLE QVariantList iconStyleDefinitions() const;
    Q_INVOKABLE QVariantMap iconStyleDefinition(const QString &styleId) const;
    Q_INVOKABLE QVariantList animationProfileDefinitions() const;
    // Resolves a legacy `iconAnimation` value or a profile id to its validated
    // runtime projection, falling back safely when neither matches.
    Q_INVOKABLE QVariantMap animationProfileResolution(
        const QString &requestedProfileId) const;
    // The validated built-in stores, or nullptr when one was rejected at
    // start-up. Presets resolve their references against these instances.
    [[nodiscard]] const ArchDock::IconStyleStore *iconStyleStore() const;
    [[nodiscard]] const ArchDock::AnimationProfileCatalog *
    animationProfileCatalog() const;
    [[nodiscard]] QVariantMap resolveIconEntryOverride(
        const ArchDock::PanelDefinition &definition,
        const QVariantMap &entry) const;
    [[nodiscard]] std::optional<QVariantMap> iconStyleRuntimeProjection(
        const ArchDock::PanelDefinition &definition,
        QString *errorCode = nullptr) const;
    Q_INVOKABLE QVariantMap themeCandidate(const QString &panelId,
                                           const QString &themeId,
                                           const QString &layer) const;
    [[nodiscard]] QVariantMap themeCandidateForDefinition(
        const ArchDock::PanelDefinition &definition, const QString &themeId,
        const QString &layer) const;
    // What one catalogue theme makes of a panel. Theme cards and Load both
    // resolve through here, so a card shows the renderer Load will select.
    // `layer` is already normalised: panel, icon or complete.
    [[nodiscard]] QVariantMap themeCandidateForTheme(
        const ArchDock::PanelDefinition &definition, const QVariantMap &theme,
        const QString &layer) const;
    bool applyTheme(const QString &panelId,
                    const QString &themeId,
                    const QString &layer);
    QString addPanel(const QString &edge, const QString &type);
    QString addFreePanel();
    Q_INVOKABLE void removePanel(const QString &panelId);
    Q_INVOKABLE bool importTheme(const QString &panelId, const QUrl &sourceUrl);
    Q_INVOKABLE bool renderTheme(const QString &panelId,
                                 int width,
                                 int height,
                                 qreal devicePixelRatio = 0,
                                 bool force = false);
    Q_INVOKABLE bool clearTheme(const QString &panelId);

public slots:
    void setActivePanelId(const QString &panelId);

signals:
    void panelsChanged();
    void nativePanelTopologyChanged();
    void activePanelIdChanged();
    void revisionChanged();
    void migrationDiagnosticChanged();

private:
    struct RenderRequest
    {
        QString panelId;
        QString sourcePath;
        QString outputPath;
        QString fit;
        int width = 0;
        int height = 0;
    };

    [[nodiscard]] QVariantMap *record(const QString &panelId);
    [[nodiscard]] const QVariantMap *record(const QString &panelId) const;
    [[nodiscard]] QVariantMap makePanel(const QString &id, const QString &name, const QString &edge, bool builtIn) const;
    [[nodiscard]] QVariant normalizeValue(const QString &key, const QVariant &value) const;
    [[nodiscard]] RenderRequest makeRenderRequest(const QString &panelId, int width, int height, qreal devicePixelRatio) const;
    [[nodiscard]] bool renderWithQt(RenderRequest *request, QString *errorMessage) const;
    void setPanelValues(const QString &panelId, const QVariantMap &values);
    [[nodiscard]] bool setPanelValuesChecked(const QString &panelId, const QVariantMap &values);
    void startRender(const RenderRequest &request);
    void finishRender(const RenderRequest &request, bool success, const QString &message);
    void pruneGeneratedRenders(const QString &currentPath);
    void load();
    void save();
    [[nodiscard]] bool saveChecked(bool migration = false);
    [[nodiscard]] bool ensureLegacyBackupChecked(QSettings &settings);
    [[nodiscard]] QByteArray serializePanels(QString *errorMessage) const;
    [[nodiscard]] QByteArray serializePanels(
        const QList<QVariantMap> &panels,
        QString *errorMessage) const;
    [[nodiscard]] bool persistPanelState(
        const QList<QVariantMap> &panels,
        const QVariantMap &globalSettings,
        QString *errorMessage);
    void setMigrationDiagnostic(const QString &diagnostic);
    void changed(bool nativeTopologyChanged);
    [[nodiscard]] std::optional<ArchDock::ThemeCapabilityProfile> lookCapabilityProfile(
        const ArchDock::PanelDefinition &definition, QString *errorCode) const;
    [[nodiscard]] std::optional<QVariantMap> lookRuntimeProjection(
        const ArchDock::PanelDefinition &definition, QString *errorCode) const;
    [[nodiscard]] QVariantMap catalogTheme(const QString &themeId) const;
    [[nodiscard]] static QString themeIdFor(const ArchDock::PanelDefinition &definition);
    // A theme package, parsed and verified once while its manifest stays the
    // same. Reading one hashes every asset, and the settings path asks for
    // the same package for every field of every edit (ADFIX UF-08).
    [[nodiscard]] ArchDock::ThemePackageLoadResult loadThemePackage(const QString &manifestPath) const;

    QList<QVariantMap> m_panels;
    QVariantList m_themeDefinitions;
    // Artwork palettes of looks drawn in generated 3D, by package digest.
    mutable QHash<QString, QVariantMap> m_lookPalettes;
    // Theme packages read successfully, by manifest path, with the manifest's
    // modification time and size when they were read.
    struct CachedThemePackage
    {
        qint64 modified = -1;
        qint64 size = -1;
        ArchDock::ThemePackageLoadResult result;
    };
    mutable QHash<QString, CachedThemePackage> m_themePackages;
    std::optional<ArchDock::IconStyleStore> m_iconStyleStore;
    QString m_iconStyleStoreError;
    std::optional<ArchDock::AnimationProfileCatalog> m_animationProfileCatalog;
    QString m_animationProfileCatalogError;
    QHash<QString, RenderRequest> m_activeRenders;
    QHash<QString, RenderRequest> m_pendingRenders;
    QString m_activePanelId = QStringLiteral("bottom");
    QString m_migrationDiagnostic;
    QByteArray m_legacySource;
    bool m_legacyRewritePending = false;
    bool m_persistenceBlocked = false;
    int m_revision = 0;
};
