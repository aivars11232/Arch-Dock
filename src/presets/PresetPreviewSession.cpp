#include "PresetPreviewSession.h"
#include "../model/PanelSettingsSchema.h"

#include <QUuid>

namespace ArchDock
{
PresetPreviewSession::PresetPreviewSession(Operations operations, QString journalPath,
    QString defaultsPath, QObject *parent)
    : QObject(parent), m_operations(std::move(operations)),
      m_recovery(std::move(journalPath)), m_defaults(std::move(defaultsPath))
{
    QString error;
    const auto pending = m_recovery.load(&error);
    if (pending || !error.isEmpty())
    {
        m_state = State::Blocked;
        m_error = error.isEmpty() ? QStringLiteral("preview-recovery-pending") : error;
    }
}

QString PresetPreviewSession::state() const
{
    switch (m_state)
    {
    case State::Idle: return QStringLiteral("IDLE");
    case State::Preparing: return QStringLiteral("PREPARING");
    case State::Active: return QStringLiteral("ACTIVE");
    case State::Committing: return QStringLiteral("COMMITTING");
    case State::RollingBack: return QStringLiteral("ROLLING_BACK");
    case State::Committed: return QStringLiteral("COMMITTED");
    case State::Blocked: return QStringLiteral("BLOCKED");
    }
    return QStringLiteral("BLOCKED");
}
bool PresetPreviewSession::active() const { return m_state == State::Active; }
int PresetPreviewSession::revision() const { return m_revision; }

QVariantMap PresetPreviewSession::status() const
{
    QVariantMap result{{QStringLiteral("state"), state()}, {QStringLiteral("active"), active()},
        {QStringLiteral("errorCode"), m_error}, {QStringLiteral("committed"), m_committed},
        {QStringLiteral("visibilityDeferred"), true}};
    const auto record = m_prepared ? std::optional(m_prepared->record) : m_recovery.load();
    if (record)
    {
        result.insert(QStringLiteral("sessionId"), record->sessionId);
        result.insert(QStringLiteral("kind"), record->kind);
        result.insert(QStringLiteral("panelId"), record->panelId);
        result.insert(QStringLiteral("temporary"), record->temporary);
    }
    if (m_prepared)
    {
        const QString presetId = m_prepared->panelPreset ? m_prepared->panelPreset->identity.id
            : m_prepared->iconPreset ? m_prepared->iconPreset->identity.id : QString{};
        result.insert(QStringLiteral("presetId"), presetId);
        result.insert(QStringLiteral("draftValues"), PanelSettingsSchema::editorValues(
            PanelSettingsFieldScope::Panel, m_prepared->draft.candidatePanel.toLegacyMap()));
        result.insert(QStringLiteral("fallback"), m_prepared->fallback);
        if (m_operations.editorProjection)
        {
            auto projection = m_operations.editorProjection(m_prepared->draft.candidatePanel);
            if (m_prepared->record.kind == QStringLiteral("icon"))
            {
                QVariantList fields;
                for (const auto &field : projection.value(QStringLiteral("panelFields")).toList())
                    if (IconPresetDefinition::panelValueKeys().contains(field.toMap().value(QStringLiteral("key")).toString()))
                        fields.append(field);
                projection.insert(QStringLiteral("panelFields"), fields);
            }
            result.insert(QStringLiteral("editorProjection"),
                projection);
        }
    }
    return result;
}
QVariantMap PresetPreviewSession::getStatus() const { return status(); }

std::optional<PanelDefinition> PresetPreviewSession::previewDefinition(const QString &panelId) const
{
    if (!m_prepared || m_prepared->record.panelId != panelId ||
        (m_state != State::Preparing && m_state != State::Active && m_state != State::Committing))
        return std::nullopt;
    auto definition = m_prepared->draft.candidatePanel;
    definition.visibility = m_prepared->draft.previousPanel.visibility;
    // An unregistered temporary preview host must be visible to audition it.
    if (m_prepared->record.temporary) definition.visibility.visible = true;
    return definition;
}

void PresetPreviewSession::transition(State next)
{
    m_state = next;
    ++m_revision;
    emit changed();
}
QVariantMap PresetPreviewSession::outcome(bool success, const QString &error)
{
    m_error = error;
    ++m_revision;
    emit changed();
    auto result = status();
    result.insert(QStringLiteral("success"), success);
    return result;
}
QString PresetPreviewSession::guard() const
{
    return m_prepared && m_operations.guard
        ? m_operations.guard(m_prepared->record.panelId) : QString{};
}
bool PresetPreviewSession::journal(const QString &phase, QString *error)
{
    if (!m_prepared) return false;
    m_prepared->record.phase = phase;
    return m_recovery.save(m_prepared->record, error);
}

QVariantMap PresetPreviewSession::beginPreview(const QVariantMap &request)
{
    if (m_state != State::Idle && m_state != State::Active)
        return outcome(false, m_state == State::Blocked ? QStringLiteral("preview-blocked")
                                                       : QStringLiteral("preview-busy"));
    const QString guardError = m_operations.guard
        ? m_operations.guard(request.value(QStringLiteral("panelId")).toString()) : QString{};
    if (!guardError.isEmpty()) return outcome(false, guardError);
    QString error;
    auto prepared = m_operations.prepare ? m_operations.prepare(request, &error) : std::nullopt;
    if (!prepared) return outcome(false, error.isEmpty() ? QStringLiteral("preview-unavailable") : error);
    if (active() && !rollback(&error)) return outcome(false, error);
    m_prepared = std::move(prepared);
    m_customizations.clear();
    m_committed = false;
    m_prepared->record.sessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_prepared->record.snapshot = m_prepared->draft.previousPanel;
    m_error.clear();
    transition(State::Preparing);
    if (m_operations.validateDraft && !m_operations.validateDraft(*m_prepared, &error))
    {
        m_prepared.reset();
        transition(State::Idle);
        return outcome(false, error);
    }
    if (!m_prepared->record.temporary &&
        (!m_operations.captureHost || !m_operations.captureHost(m_prepared->record, &error)))
    {
        m_prepared.reset();
        transition(State::Idle);
        return outcome(false, error.isEmpty() ? QStringLiteral("host-not-verified") : error);
    }
    if (!journal(QStringLiteral("PREPARING"), &error))
    {
        m_prepared.reset();
        transition(State::Blocked);
        return outcome(false, error);
    }
    bool applied = true;
    if (m_prepared->record.temporary)
        applied = m_operations.createHost && m_operations.createHost(
            m_prepared->record, m_prepared->draft.candidatePanel, &error);
    if (applied && m_prepared->record.kind == QStringLiteral("panel"))
        applied = m_operations.applyHost && m_operations.applyHost(
            m_prepared->draft.previousPanel, m_prepared->draft.candidatePanel, &error);
    if (!applied || !journal(QStringLiteral("ACTIVE"), &error))
    {
        const QString failure = error.isEmpty() ? QStringLiteral("preview-host-failed") : error;
        QString rollbackError;
        return outcome(false, rollback(&rollbackError) ? failure : rollbackError);
    }
    transition(State::Active);
    return outcome(true);
}

bool PresetPreviewSession::installDraft(Prepared prepared, QString *error)
{
    prepared.record = m_prepared->record;
    prepared.draft.previousPanel = m_prepared->draft.previousPanel;
    const int screenIndex = prepared.draft.candidatePanel.host.screenIndex;
    const QString screenId = prepared.draft.candidatePanel.host.screenId;
    prepared.draft.candidatePanel.host = m_prepared->draft.candidatePanel.host;
    prepared.draft.candidatePanel.host.screenIndex = screenIndex;
    prepared.draft.candidatePanel.host.screenId = screenId;
    if (m_operations.validateDraft && !m_operations.validateDraft(prepared, error)) return false;
    const auto previous = m_prepared->draft.candidatePanel;
    m_prepared = std::move(prepared);
    ++m_revision;
    emit changed();
    if (m_prepared->record.kind == QStringLiteral("panel") &&
        (!m_operations.applyHost || !m_operations.applyHost(previous, m_prepared->draft.candidatePanel, error)))
    {
        const QString failure = error && !error->isEmpty() ? *error : QStringLiteral("preview-host-failed");
        QString rollbackError;
        const bool restored = rollback(&rollbackError);
        if (error) *error = restored ? failure : rollbackError;
        return false;
    }
    return true;
}

QVariantMap PresetPreviewSession::updateDraft(const QVariantMap &customizations)
{
    if (!active()) return outcome(false, QStringLiteral("preview-not-active"));
    const QString guardError = guard();
    if (!guardError.isEmpty()) return outcome(false, guardError);
    QString error;
    auto draft = m_prepared->record.kind == QStringLiteral("icon") && m_prepared->iconPreset
        ? PresetApplication::prepareIcon(m_prepared->draft.previousPanel, m_prepared->draft.previousGlobals,
              *m_prepared->iconPreset, customizations, &error)
        : m_prepared->panelPreset
            ? PresetApplication::preparePanel(m_prepared->draft.previousPanel, m_prepared->draft.previousGlobals,
                  *m_prepared->panelPreset, customizations, m_prepared->recommendedIcons, &error)
            : std::nullopt;
    if (!draft) return outcome(false, error.isEmpty() ? QStringLiteral("invalid-preview-draft") : error);
    auto prepared = *m_prepared;
    prepared.draft = *draft;
    if (!installDraft(std::move(prepared), &error)) return outcome(false, error);
    m_customizations = customizations;
    return outcome(true);
}

bool PresetPreviewSession::rollback(QString *error)
{
    if (!m_prepared) { if (error) *error = QStringLiteral("preview-recovery-required"); return false; }
    if (m_committed) { if (error) *error = QStringLiteral("committed-preview-recovery-required"); return false; }
    transition(State::RollingBack);
    QString journalError;
    const bool recorded = journal(QStringLiteral("ROLLING_BACK"), &journalError);
    const bool restored = m_prepared->record.temporary
        ? m_operations.removeHost && m_operations.removeHost(m_prepared->record, error)
        : m_prepared->record.kind == QStringLiteral("icon") ||
          (m_operations.restoreHost && m_operations.restoreHost(
              m_prepared->record, m_prepared->draft.candidatePanel, error));
    if (!restored || !m_recovery.clear(error))
    {
        if (error && error->isEmpty()) *error = QStringLiteral("preview-rollback-failed");
        QString ignored;
        const bool saved = journal(QStringLiteral("BLOCKED"), &ignored);
        Q_UNUSED(saved);
        transition(State::Blocked);
        return false;
    }
    Q_UNUSED(recorded);
    m_prepared.reset();
    m_customizations.clear();
    transition(State::Idle);
    return true;
}

QVariantMap PresetPreviewSession::applyAsActive()
{
    if (!active()) return outcome(false, QStringLiteral("preview-not-active"));
    const QString guardError = guard();
    if (!guardError.isEmpty()) return outcome(false, guardError);
    QString error;
    transition(State::Committing);
    if (!journal(QStringLiteral("COMMITTING"), &error))
    {
        QString rollbackError;
        return outcome(false, rollback(&rollbackError) ? error : rollbackError);
    }
    bool committed = false;
    if (m_prepared->record.temporary)
    {
        m_prepared->record.managedToken = QStringLiteral("archdock-managed-") +
            QUuid::createUuid().toString(QUuid::WithoutBraces);
        if (journal(QStringLiteral("CONVERTING"), &error) && m_operations.convertHost &&
            m_operations.convertHost(m_prepared->record, m_prepared->draft.candidatePanel, &error) &&
            journal(QStringLiteral("ADOPTING"), &error))
            committed = m_operations.adoptHost && m_operations.adoptHost(m_prepared->draft.candidatePanel, &error);
    }
    else committed = m_operations.commitExisting && m_operations.commitExisting(m_prepared->draft, &error);
    if (!committed)
    {
        const QString failure = error.isEmpty() ? QStringLiteral("preview-commit-failed") : error;
        QString rollbackError;
        return outcome(false, rollback(&rollbackError) ? failure : rollbackError);
    }
    m_committed = true;
    transition(State::Committed);
    if (!journal(QStringLiteral("COMMITTED"), &error) || !m_recovery.clear(&error))
    {
        transition(State::Blocked);
        return outcome(false, error);
    }
    const QString committedPanelId = m_prepared->record.panelId;
    m_prepared.reset();
    m_customizations.clear();
    transition(State::Idle);
    auto result = outcome(true);
    result.insert(QStringLiteral("panelId"), committedPanelId);
    return result;
}

QVariantMap PresetPreviewSession::saveAsCustomPreset(const QString &name)
{
    if (!active()) return outcome(false, QStringLiteral("preview-not-active"));
    if (name.trimmed().isEmpty()) return outcome(false, QStringLiteral("preset-name-required"));
    if (!m_operations.saveCustom) return outcome(false, QStringLiteral("custom-store-unavailable"));
    const auto saved = m_operations.saveCustom(*m_prepared, name.trimmed());
    const bool success = saved.value(QStringLiteral("success")).toBool();
    if (success) emit customPresetSaved(m_prepared->record.kind, saved.value(QStringLiteral("presetId")).toString());
    auto result = outcome(success, saved.value(QStringLiteral("errorCode")).toString());
    result.insert(QStringLiteral("presetId"), saved.value(QStringLiteral("presetId")));
    return result;
}

QVariantMap PresetPreviewSession::defaultSelection() const
{
    QString error;
    const auto selected = m_defaults.load(&error);
    return {{QStringLiteral("success"), selected.has_value()}, {QStringLiteral("errorCode"), error},
        {QStringLiteral("panelPresetId"), selected ? selected->panelPresetId : QString{}},
        {QStringLiteral("iconPresetId"), selected ? selected->iconPresetId : QString{}}};
}
QVariantMap PresetPreviewSession::setAsDefault(const QString &kind, const QString &presetId, bool remove)
{
    if (m_state != State::Idle && m_state != State::Active) return outcome(false, QStringLiteral("preview-busy"));
    if (!m_operations.validDefault || !m_operations.validDefault(kind, presetId))
        return outcome(false, QStringLiteral("invalid-default-preset"));
    QString error;
    const auto selected = m_defaults.load(&error);
    if (!selected) return outcome(false, error);
    const QString current = kind == QStringLiteral("panel") ? selected->panelPresetId : selected->iconPresetId;
    if (remove && current != presetId) return outcome(false, QStringLiteral("not-selected-default"));
    const bool saved = m_defaults.setDefault(kind, remove ? QString{} : presetId, &error);
    return outcome(saved, error);
}
QVariantMap PresetPreviewSession::cancel()
{
    if (m_state == State::Blocked) return recoverInterruptedPreview();
    if (m_state == State::Idle) return outcome(true);
    if (!active()) return outcome(false, QStringLiteral("preview-busy"));
    QString error;
    const bool restored = rollback(&error);
    return outcome(restored, error);
}
QVariantMap PresetPreviewSession::revert() { return cancel(); }

QVariantMap PresetPreviewSession::restoreBuiltInDefaults()
{
    if (!active()) return outcome(false, QStringLiteral("preview-not-active"));
    const QString guardError = guard();
    if (!guardError.isEmpty()) return outcome(false, guardError);
    QString error;
    auto restored = m_operations.restoreBuiltIn ? m_operations.restoreBuiltIn(*m_prepared, &error) : std::nullopt;
    if (!restored) return outcome(false, error.isEmpty() ? QStringLiteral("no-built-in-source") : error);
    if (!installDraft(std::move(*restored), &error)) return outcome(false, error);
    m_customizations.clear();
    return outcome(true);
}

QVariantMap PresetPreviewSession::recoverInterruptedPreview()
{
    if (m_state != State::Idle && m_state != State::Blocked)
        return cancel();
    QString error;
    const auto record = m_recovery.load(&error);
    if (!error.isEmpty()) { transition(State::Blocked); return outcome(false, error); }
    if (!record)
    {
        m_prepared.reset();
        transition(State::Idle);
        return outcome(true);
    }
    transition(State::RollingBack);
    if (!m_operations.recover || !m_operations.recover(*record, &error) || !m_recovery.clear(&error))
    {
        transition(State::Blocked);
        return outcome(false, error.isEmpty() ? QStringLiteral("preview-recovery-blocked") : error);
    }
    m_prepared.reset();
    m_customizations.clear();
    transition(State::Idle);
    return outcome(true);
}
}
