#pragma once

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QUrl>
#include <QVariant>
#include <QVariantMap>

class QProcess;

class PanelRegistry final : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QStringList panelIds READ panelIds NOTIFY panelsChanged)
    Q_PROPERTY(QString activePanelId READ activePanelId WRITE setActivePanelId NOTIFY activePanelIdChanged)
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)

public:
    explicit PanelRegistry(QObject *parent = nullptr);
    ~PanelRegistry() override;

    [[nodiscard]] QStringList panelIds() const;
    [[nodiscard]] QString activePanelId() const;
    [[nodiscard]] int revision() const;

    Q_INVOKABLE QVariant panelValue(const QString &panelId, const QString &key) const;
    Q_INVOKABLE QString panelName(const QString &panelId) const;
    Q_INVOKABLE bool isBuiltIn(const QString &panelId) const;
    Q_INVOKABLE void setPanelValue(const QString &panelId, const QString &key, const QVariant &value);
    Q_INVOKABLE void updatePanel(const QString &panelId, const QVariantMap &values);
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
    Q_INVOKABLE bool applyTheme(const QString &panelId,
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
    [[nodiscard]] bool renderWithQt(const RenderRequest &request, QString *errorMessage) const;
    void setPanelValues(const QString &panelId, const QVariantMap &values);
    [[nodiscard]] bool setPanelValuesChecked(const QString &panelId, const QVariantMap &values);
    void startRender(const RenderRequest &request);
    void startMagickRender(const RenderRequest &request, const QString &sourcePath);
    void finishRender(const RenderRequest &request, bool success, const QString &message);
    void load();
    void save() const;
    [[nodiscard]] bool saveChecked() const;
    void changed(bool nativeTopologyChanged);

    QList<QVariantMap> m_panels;
    QHash<QString, RenderRequest> m_activeRenders;
    QHash<QString, RenderRequest> m_pendingRenders;
    QHash<QString, QPointer<QProcess>> m_renderProcesses;
    QString m_activePanelId = QStringLiteral("bottom");
    int m_revision = 0;
};
