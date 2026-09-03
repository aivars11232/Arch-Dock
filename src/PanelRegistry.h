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

class QSettings;

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
    [[nodiscard]] std::optional<ArchDock::ThemeCapabilityProfile>
    themeCapabilityProfile(const ArchDock::PanelDefinition &definition,
                           QString *errorCode = nullptr) const;
    [[nodiscard]] std::optional<QVariantMap> themeRuntimeProjection(
        const ArchDock::PanelDefinition &definition,
        QString *errorCode = nullptr) const;
    [[nodiscard]] std::optional<QVariantMap> builtInThemeRuntimeProjection(
        const QString &themeId,
        QString *errorCode = nullptr) const;
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
    [[nodiscard]] QVariantMap resolveIconEntryOverride(
        const ArchDock::PanelDefinition &definition,
        const QVariantMap &entry) const;
    [[nodiscard]] std::optional<QVariantMap> iconStyleRuntimeProjection(
        const ArchDock::PanelDefinition &definition,
        QString *errorCode = nullptr) const;
    Q_INVOKABLE QVariantMap themeCandidate(const QString &panelId,
                                           const QString &themeId,
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
    Q_INVOKABLE void clearTheme(const QString &panelId);

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
    void load();
    void save();
    [[nodiscard]] bool saveChecked();
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

    QList<QVariantMap> m_panels;
    QVariantList m_themeDefinitions;
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
