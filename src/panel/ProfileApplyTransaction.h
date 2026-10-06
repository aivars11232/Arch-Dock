#pragma once

#include "../persistence/ProfileStore.h"
#include <QObject>
#include <functional>

// Applying a saved profile (a whole set of panels) as one recoverable
// transaction: the configuration is backed up, each host is captured,
// created, applied and verified in turn, and a failure restores every touched
// host. A journal on disk lets an interrupted apply be recovered on the next
// start.
namespace ArchDock
{
struct ProfileHostSnapshot
{
    PanelDefinition definition;
    QVariantMap hostState;
    bool touched = false;
    bool removed = false;
};

class ProfileApplyTransaction final : public QObject
{
    Q_OBJECT
public:
    struct Operations
    {
        std::function<QString()> guard;
        std::function<bool(QString *)> backupConfiguration;
        std::function<QList<PanelDefinition>(QString *)> snapshot;
        std::function<bool(const QList<PanelDefinition> &, QString *)> matches;
        std::function<bool(const QList<PanelDefinition> &, QList<PanelDefinition> &,
            QStringList *, QString *)> prepare;
        std::function<bool(ProfileHostSnapshot &, QString *)> capture;
        std::function<bool(PanelDefinition &, const QString &, QString *)> create;
        std::function<bool(const PanelDefinition &, const PanelDefinition &, QString *)> apply;
        std::function<bool(const PanelDefinition &, QString *)> verify;
        std::function<bool(ProfileHostSnapshot &, QString *)> restore;
        std::function<bool(const PanelDefinition &, QString *)> remove;
        std::function<bool(const QList<PanelDefinition> &, const QList<PanelDefinition> &, QString *)> commit;
        std::function<void()> publish;
    };
    explicit ProfileApplyTransaction(Operations operations, QString journalPath,
        QObject *parent = nullptr);
    [[nodiscard]] static QString defaultJournalPath();
    [[nodiscard]] static bool hosted(const PanelDefinition &panel);
    [[nodiscard]] static QString hostToken(const PanelDefinition &panel);
    [[nodiscard]] bool active() const;
    [[nodiscard]] QVariantMap status() const;
    [[nodiscard]] std::optional<PanelDefinition> previewDefinition(const QString &id) const;
    [[nodiscard]] QVariantMap apply(const ProfileDefinition &profile);
    [[nodiscard]] QVariantMap recover();
signals:
    void changed();
private:
    [[nodiscard]] QVariantMap outcome(bool success, const QString &code = {});
    void transition(const QString &state);
    [[nodiscard]] QVariantMap record() const;
    [[nodiscard]] bool readJournal(QString *code);
    [[nodiscard]] bool writeRecord(const QString &path, QString *code) const;
    [[nodiscard]] bool journal(const QString &state, QString *code);
    [[nodiscard]] bool clearJournal(QString *code);
    [[nodiscard]] bool rollback(QString *code);
    [[nodiscard]] QVariantMap abort(const QString &code);
    Operations m_operations;
    QString m_path;
    QString m_state = QStringLiteral("IDLE");
    QString m_sessionId;
    QString m_profileId;
    QString m_error;
    QStringList m_rollbackErrors;
    QStringList m_diagnostics;
    QList<ProfileHostSnapshot> m_backup;
    QList<PanelDefinition> m_candidates;
    QList<PanelDefinition> m_preview;
    QMap<QString, QString> m_created;
    bool m_pending = false;
    bool m_running = false;
};
}
