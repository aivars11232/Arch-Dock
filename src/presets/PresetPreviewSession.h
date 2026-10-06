#pragma once

#include "PresetApplication.h"
#include "PresetDefaultStore.h"
#include "PresetPreviewRecovery.h"

#include <QObject>
#include <functional>

// A live audition: a preset, or a desktop 3D edit, tried on a real panel
// before it is kept. The panel's saved state is captured first; Apply
// commits the trial as one transaction, Cancel restores the panel exactly,
// and a recovery record covers a crash in between. Served over D-Bus as
// org.archdock.PresetAudition.
namespace ArchDock
{
class PresetPreviewSession final : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.archdock.PresetAudition")
    Q_PROPERTY(QString state READ state NOTIFY changed)
    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(int revision READ revision NOTIFY changed)
    Q_PROPERTY(QVariantMap status READ status NOTIFY changed)
public:
    enum class State { Idle, Preparing, Active, Committing, RollingBack, Committed, Blocked };
    Q_ENUM(State)
    struct Prepared
    {
        PanelSettingsTransactionDraft draft;
        PresetPreviewRecord record;
        std::optional<PanelPresetDefinition> panelPreset;
        std::optional<IconPresetDefinition> iconPreset;
        std::optional<IconPresetDefinition> recommendedIcons;
        QVariantMap fallback;
    };
    struct Operations
    {
        std::function<std::optional<Prepared>(const QVariantMap &, QString *)> prepare;
        std::function<QString(const QString &)> guard;
        std::function<bool(PresetPreviewRecord &, QString *)> captureHost;
        std::function<bool(PresetPreviewRecord &, PanelDefinition &, QString *)> createHost;
        std::function<bool(const PanelDefinition &, const PanelDefinition &, QString *)> applyHost;
        std::function<bool(const PresetPreviewRecord &, const PanelDefinition &, QString *)> restoreHost;
        std::function<bool(const PanelSettingsTransactionDraft &, QString *)> commitExisting;
        std::function<bool(const PresetPreviewRecord &, PanelDefinition &, QString *)> convertHost;
        std::function<bool(const PanelDefinition &, QString *)> adoptHost;
        std::function<bool(const PresetPreviewRecord &, QString *)> removeHost;
        std::function<bool(const PresetPreviewRecord &, QString *)> recover;
        std::function<QVariantMap(const Prepared &, const QString &)> saveCustom;
        std::function<std::optional<Prepared>(const Prepared &, QString *)> restoreBuiltIn;
        std::function<bool(const QString &, const QString &)> validDefault;
        std::function<bool(Prepared &, QString *)> validateDraft;
        std::function<QVariantMap(const PanelDefinition &)> editorProjection;
    };

    explicit PresetPreviewSession(Operations operations,
        QString journalPath = PresetPreviewRecovery::defaultPath(),
        QString defaultsPath = PresetDefaultStore::defaultPath(), QObject *parent = nullptr);
    [[nodiscard]] QString state() const;
    [[nodiscard]] bool active() const;
    [[nodiscard]] int revision() const;
    [[nodiscard]] QVariantMap status() const;
    [[nodiscard]] std::optional<PanelDefinition> previewDefinition(const QString &panelId) const;

public slots:
    QVariantMap beginPreview(const QVariantMap &request);
    QVariantMap updateDraft(const QVariantMap &customizations);
    QVariantMap applyAsActive();
    QVariantMap saveAsCustomPreset(const QString &name);
    QVariantMap setAsDefault(const QString &kind, const QString &presetId, bool remove);
    QVariantMap cancel();
    QVariantMap revert();
    QVariantMap restoreBuiltInDefaults();
    QVariantMap recoverInterruptedPreview();
    QVariantMap getStatus() const;
    QVariantMap defaultSelection() const;

signals:
    void changed();
    void customPresetSaved(const QString &kind, const QString &presetId);

private:
    void transition(State state);
    [[nodiscard]] QVariantMap outcome(bool success, const QString &error = {});
    [[nodiscard]] QString guard() const;
    [[nodiscard]] bool journal(const QString &phase, QString *error);
    [[nodiscard]] bool rollback(QString *error);
    [[nodiscard]] bool installDraft(Prepared prepared, QString *error);
    Operations m_operations;
    PresetPreviewRecovery m_recovery;
    PresetDefaultStore m_defaults;
    State m_state = State::Idle;
    std::optional<Prepared> m_prepared;
    QVariantMap m_customizations;
    QString m_error;
    int m_revision = 0;
    bool m_committed = false;
};
}
