#include "ProfileApplyTransaction.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>
#include <algorithm>
#include <limits>

namespace ArchDock
{
namespace
{
constexpr qsizetype maximumJournalBytes = 8 * 1024 * 1024;
void error(QString *target, const QString &code) { if (target) *target = code; }
bool safePath(const QString &path)
{
    if (QFileInfo(path).isSymLink()) return false;
    QString parent = QFileInfo(path).absolutePath();
    for (;;)
    {
        const QFileInfo info(parent);
        if (info.isSymLink() || (info.exists() && !info.isDir())) return false;
        if (info.absolutePath() == parent) return true;
        parent = info.absolutePath();
    }
}
bool sameHost(const PanelDefinition &a, const PanelDefinition &b)
{
    return a.host.kind == b.host.kind && ProfileApplyTransaction::hostToken(a) == ProfileApplyTransaction::hostToken(b) &&
        (a.host.kind == PanelHostKind::FreeDesktop
            ? a.host.freeDesktopContainmentId == b.host.freeDesktopContainmentId && a.host.freeDockAppletId == b.host.freeDockAppletId
            : a.host.nativePanelId == b.host.nativePanelId && a.host.nativeDockAppletId == b.host.nativeDockAppletId);
}
QList<PanelDefinition> definitions(const QList<ProfileHostSnapshot> &snapshots)
{
    QList<PanelDefinition> result;
    for (const auto &snapshot : snapshots) result.append(snapshot.definition);
    return result;
}
}

QString ProfileApplyTransaction::defaultJournalPath()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
        .filePath(QStringLiteral("profile-apply-journal.json"));
}
bool ProfileApplyTransaction::hosted(const PanelDefinition &panel)
{
    return !hostToken(panel).isEmpty() && (panel.host.kind == PanelHostKind::FreeDesktop
        ? panel.host.freeDesktopContainmentId >= 0 && panel.host.freeDockAppletId >= 0
        : panel.host.nativePanelId >= 0);
}
QString ProfileApplyTransaction::hostToken(const PanelDefinition &panel)
{
    return panel.host.kind == PanelHostKind::FreeDesktop ? panel.host.freeOwnershipToken : panel.host.nativeOwnershipToken;
}
ProfileApplyTransaction::ProfileApplyTransaction(Operations operations, QString path, QObject *parent)
    : QObject(parent), m_operations(std::move(operations)), m_path(std::move(path))
{
    QString code;
    if (!readJournal(&code) || m_pending)
    {
        m_error = code.isEmpty() ? QStringLiteral("interrupted-profile-apply:") + m_state : code;
        m_pending = true;
        m_state = QStringLiteral("BLOCKED");
    }
}
bool ProfileApplyTransaction::active() const { return m_running || m_pending; }
QVariantMap ProfileApplyTransaction::status() const
{
    return {{QStringLiteral("state"), m_state}, {QStringLiteral("active"), active()},
        {QStringLiteral("profileId"), m_profileId}, {QStringLiteral("sessionId"), m_sessionId},
        {QStringLiteral("errorCode"), m_error}, {QStringLiteral("rollbackErrors"), m_rollbackErrors},
        {QStringLiteral("diagnostics"), m_diagnostics}, {QStringLiteral("recoveryRequired"), m_pending && !m_running},
        {QStringLiteral("journalPath"), m_path}, {QStringLiteral("backupPath"), m_path + QStringLiteral(".backup.json")}};
}
std::optional<PanelDefinition> ProfileApplyTransaction::previewDefinition(const QString &id) const
{
    for (const auto &panel : m_preview) if (panel.identity.id == id) return panel;
    return {};
}
void ProfileApplyTransaction::transition(const QString &state) { m_state = state; emit changed(); }
QVariantMap ProfileApplyTransaction::outcome(bool success, const QString &code)
{
    if (!code.isEmpty()) m_error = code;
    auto result = status();
    result.insert(QStringLiteral("success"), success);
    emit changed();
    return result;
}
QVariantMap ProfileApplyTransaction::record() const
{
    QVariantList backup, candidates;
    for (const auto &snapshot : m_backup)
        backup.append(QVariantMap{{QStringLiteral("definition"), snapshot.definition.toPersistedMap()},
            {QStringLiteral("hostState"), snapshot.hostState}, {QStringLiteral("touched"), snapshot.touched},
            {QStringLiteral("removed"), snapshot.removed}});
    for (const auto &panel : m_candidates) candidates.append(panel.toPersistedMap());
    QVariantMap created;
    for (auto it = m_created.cbegin(); it != m_created.cend(); ++it) created.insert(it.key(), it.value());
    return {{QStringLiteral("format"), QStringLiteral("org.archdock.profile-apply")}, {QStringLiteral("version"), 1},
        {QStringLiteral("state"), m_state}, {QStringLiteral("sessionId"), m_sessionId},
        {QStringLiteral("profileId"), m_profileId}, {QStringLiteral("backup"), backup},
        {QStringLiteral("candidates"), candidates}, {QStringLiteral("created"), created},
        {QStringLiteral("errorCode"), m_error}, {QStringLiteral("rollbackErrors"), m_rollbackErrors},
        {QStringLiteral("diagnostics"), m_diagnostics}};
}
bool ProfileApplyTransaction::writeRecord(const QString &path, QString *code) const
{
    const auto fail = [code] { error(code, QStringLiteral("profile-journal-unwritable")); return false; };
    if (!safePath(path) || !QDir().mkpath(QFileInfo(path).absolutePath())) return fail();
    const auto bytes = QJsonDocument::fromVariant(record()).toJson(QJsonDocument::Compact);
    if (bytes.size() > maximumJournalBytes) return fail();
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || !file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner) ||
        file.write(bytes) != bytes.size() || !file.commit()) return fail();
    QFile reader(path);
    if (!reader.open(QIODevice::ReadOnly) || reader.read(maximumJournalBytes + 1) != bytes) return fail();
    error(code, {});
    return true;
}
bool ProfileApplyTransaction::journal(const QString &state, QString *code)
{
    transition(state);
    return writeRecord(m_path, code);
}
bool ProfileApplyTransaction::clearJournal(QString *code)
{
    if (!safePath(m_path) || (QFileInfo::exists(m_path) && !QFile::remove(m_path)))
    { error(code, QStringLiteral("profile-journal-clear-failed")); return false; }
    m_pending = false;
    return true;
}
bool ProfileApplyTransaction::readJournal(QString *code)
{
    const auto fail = [code] { error(code, QStringLiteral("invalid-profile-recovery-record")); return false; };
    error(code, {});
    if (!safePath(m_path)) return fail();
    if (!QFileInfo::exists(m_path)) { m_pending = false; return true; }
    QFile file(m_path);
    if (!QFileInfo(m_path).isFile() || !file.open(QIODevice::ReadOnly)) return fail();
    const auto bytes = file.read(maximumJournalBytes + 1);
    QJsonParseError parse;
    const auto document = QJsonDocument::fromJson(bytes, &parse);
    if (bytes.size() > maximumJournalBytes || parse.error != QJsonParseError::NoError || !document.isObject()) return fail();
    const auto map = document.object().toVariantMap();
    if (map.keys() != record().keys() || map.value(QStringLiteral("format")).toString() != QStringLiteral("org.archdock.profile-apply") ||
        !document.object().value(QStringLiteral("version")).isDouble() ||
        map.value(QStringLiteral("version")).toDouble() != 1) return fail();
    for (const QString &key : {QStringLiteral("state"), QStringLiteral("sessionId"), QStringLiteral("profileId"), QStringLiteral("errorCode")})
        if (map.value(key).metaType().id() != QMetaType::QString || map.value(key).toString().size() > 1024) return fail();
    const QString state = map.value(QStringLiteral("state")).toString();
    if (!QStringList{"PREPARING", "APPLYING", "REMOVING", "COMMITTING", "COMMITTED", "ROLLING_BACK", "BLOCKED"}.contains(state) ||
        QUuid(map.value(QStringLiteral("sessionId")).toString()).isNull() ||
        !ProfileDefinition::validId(map.value(QStringLiteral("profileId")).toString())) return fail();
    if (map.value(QStringLiteral("backup")).metaType().id() != QMetaType::QVariantList ||
        map.value(QStringLiteral("candidates")).metaType().id() != QMetaType::QVariantList ||
        map.value(QStringLiteral("created")).metaType().id() != QMetaType::QVariantMap) return fail();
    QList<ProfileHostSnapshot> backup;
    QList<PanelDefinition> candidates;
    QSet<QString> ids;
    const auto backupValues = map.value(QStringLiteral("backup")).toList();
    const auto candidateValues = map.value(QStringLiteral("candidates")).toList();
    if (backupValues.isEmpty() || backupValues.size() > 64 || candidateValues.isEmpty() || candidateValues.size() > 64) return fail();
    for (const auto &value : backupValues)
    {
        const auto item = value.toMap();
        if (item.keys() != QStringList{"definition", "hostState", "removed", "touched"} ||
            item.value(QStringLiteral("definition")).metaType().id() != QMetaType::QVariantMap ||
            item.value(QStringLiteral("hostState")).metaType().id() != QMetaType::QVariantMap ||
            item.value(QStringLiteral("removed")).metaType().id() != QMetaType::Bool ||
            item.value(QStringLiteral("touched")).metaType().id() != QMetaType::Bool) return fail();
        const auto panel = PanelDefinition::fromLegacyMap(item.value(QStringLiteral("definition")).toMap());
        const auto hostState = item.value(QStringLiteral("hostState")).toMap();
        if (!panel || ids.contains(panel->identity.id) || hostState.size() > 64 ||
            QJsonDocument::fromVariant(hostState).toJson().size() > 32768) return fail();
        ids.insert(panel->identity.id);
        backup.append({*panel, hostState, item.value(QStringLiteral("touched")).toBool(), item.value(QStringLiteral("removed")).toBool()});
    }
    ids.clear();
    for (const auto &value : candidateValues)
    {
        if (value.metaType().id() != QMetaType::QVariantMap) return fail();
        const auto panel = PanelDefinition::fromLegacyMap(value.toMap());
        if (!panel || ids.contains(panel->identity.id)) return fail();
        ids.insert(panel->identity.id); candidates.append(*panel);
    }
    QMap<QString, QString> created;
    const auto createdValues = map.value(QStringLiteral("created")).toMap();
    for (auto it = createdValues.cbegin(); it != createdValues.cend(); ++it)
    {
        if (!ids.contains(it.key()) || it.value().metaType().id() != QMetaType::QString ||
            !it.value().toString().startsWith(QStringLiteral("archdock-profile-")) || it.value().toString().size() > 128) return fail();
        created.insert(it.key(), it.value().toString());
    }
    for (const QString &key : {QStringLiteral("diagnostics"), QStringLiteral("rollbackErrors")})
    {
        if (map.value(key).metaType().id() != QMetaType::QVariantList) return fail();
        const auto values = map.value(key).toList();
        if (values.size() > 256) return fail();
        for (const auto &value : values) if (value.metaType().id() != QMetaType::QString || value.toString().size() > 1024) return fail();
    }
    m_backup = backup; m_candidates = candidates; m_created = created; m_state = state;
    m_sessionId = map.value(QStringLiteral("sessionId")).toString();
    m_profileId = map.value(QStringLiteral("profileId")).toString();
    m_error = map.value(QStringLiteral("errorCode")).toString();
    m_diagnostics = map.value(QStringLiteral("diagnostics")).toStringList();
    m_rollbackErrors = map.value(QStringLiteral("rollbackErrors")).toStringList();
    m_pending = true;
    return true;
}

QVariantMap ProfileApplyTransaction::apply(const ProfileDefinition &profile)
{
    if (active()) return outcome(false, QStringLiteral("profile-recovery-or-apply-active"));
    const auto &op = m_operations;
    if (!op.snapshot || !op.matches || !op.prepare || !op.capture || !op.create || !op.apply ||
        !op.verify || !op.restore || !op.remove || !op.commit || !op.publish)
        return outcome(false, QStringLiteral("profile-host-operations-unavailable"));
    if (op.guard) { const auto code = op.guard(); if (!code.isEmpty()) return outcome(false, code); }
    QString code;
    const auto checked = ProfileDefinition::fromVariantMap(profile.toVariantMap(), &code);
    if (!checked || checked->panels != profile.panels) return outcome(false, code.isEmpty() ? QStringLiteral("profile-live-host-association") : code);
    m_running = true; m_error.clear(); m_rollbackErrors.clear(); m_diagnostics.clear(); m_created.clear(); m_backup.clear();
    m_sessionId = QUuid::createUuid().toString(QUuid::WithoutBraces); m_profileId = profile.id;
    transition(QStringLiteral("PREPARING"));
    const auto before = op.snapshot(&code);
    m_candidates = profile.panels;
    if (before.isEmpty() || before.size() > 64 || !op.matches(before, &code) ||
        !op.prepare(before, m_candidates, &m_diagnostics, &code))
    { m_running = false; transition(QStringLiteral("IDLE")); return outcome(false, code.isEmpty() ? QStringLiteral("profile-preflight-failed") : code); }
    QMap<QString, PanelDefinition> previous;
    for (const auto &panel : before)
    {
        ProfileHostSnapshot snapshot{panel, {}, false, false};
        if (hosted(panel) && !op.capture(snapshot, &code))
        { m_running = false; transition(QStringLiteral("IDLE")); return outcome(false, code); }
        m_backup.append(snapshot); previous.insert(panel.identity.id, panel);
    }
    for (auto &panel : m_candidates)
    {
        const auto found = previous.constFind(panel.identity.id);
        if (found != previous.cend() && found->settingsRevision == std::numeric_limits<quint64>::max())
        { m_running = false; transition(QStringLiteral("IDLE")); return outcome(false, QStringLiteral("profile-revision-exhausted")); }
        panel.settingsRevision = found == previous.cend() ? 1 : found->settingsRevision + 1;
        panel.identity.builtIn = found != previous.cend() && found->identity.builtIn;
        const auto requestedHost = panel.host;
        panel.host = ProfileDefinition::portablePanel(panel).host;
        if (found != previous.cend() && hosted(*found) && found->host.kind == panel.host.kind &&
            found->content.type == panel.content.type && (panel.host.kind != PanelHostKind::FreeDesktop ||
                found->host.screenIndex == requestedHost.screenIndex))
            panel.host = found->host;
        panel.host.screenId = requestedHost.screenId; panel.host.screenIndex = requestedHost.screenIndex;
        panel = panel.normalized();
    }
    for (auto &snapshot : m_backup)
        snapshot.touched = hosted(snapshot.definition) && std::any_of(m_candidates.cbegin(), m_candidates.cend(),
            [&](const auto &panel) { return panel.identity.id == snapshot.definition.identity.id; });
    if (!writeRecord(m_path + QStringLiteral(".backup.json"), &code) || !journal(QStringLiteral("PREPARING"), &code))
    { m_running = false; transition(QStringLiteral("IDLE")); return outcome(false, code); }
    m_pending = true; m_preview = m_candidates;
    for (auto &panel : m_candidates)
    {
        if (!op.matches(before, &code)) return abort(code);
        if (!hosted(panel) && panel.visibility.visible)
        {
            const QString token = QStringLiteral("archdock-profile-") + QUuid::createUuid().toString(QUuid::Id128);
            m_created.insert(panel.identity.id, token);
            if (!journal(QStringLiteral("APPLYING"), &code) || !op.create(panel, token, &code)) return abort(code);
            m_preview = m_candidates;
            emit changed();
        }
        if (!hosted(panel)) continue;
        for (auto &snapshot : m_backup)
            if (snapshot.definition.identity.id == panel.identity.id && sameHost(snapshot.definition, panel)) snapshot.touched = true;
        if (!journal(QStringLiteral("APPLYING"), &code)) return abort(code);
        const auto original = previous.value(panel.identity.id, ProfileDefinition::portablePanel(panel));
        if (!op.apply(original, panel, &code) || !op.verify(panel, &code)) return abort(code);
    }
    for (auto &snapshot : m_backup)
    {
        if (!hosted(snapshot.definition)) continue;
        const auto found = std::find_if(m_candidates.cbegin(), m_candidates.cend(), [&](const auto &panel) {
            return panel.identity.id == snapshot.definition.identity.id && sameHost(panel, snapshot.definition);
        });
        if (found != m_candidates.cend()) continue;
        snapshot.removed = true;
        if (!op.matches(before, &code) || !journal(QStringLiteral("REMOVING"), &code) || !op.remove(snapshot.definition, &code)) return abort(code);
    }
    if (!journal(QStringLiteral("COMMITTING"), &code) || !op.commit(before, m_candidates, &code)) return abort(code);
    op.publish();
    if (!journal(QStringLiteral("COMMITTED"), &code) || !clearJournal(&code))
    { m_running = false; m_preview.clear(); transition(QStringLiteral("BLOCKED")); return outcome(false, code); }
    m_running = false; m_preview.clear(); transition(QStringLiteral("APPLIED"));
    return outcome(true);
}

bool ProfileApplyTransaction::rollback(QString *code)
{
    const auto before = definitions(m_backup);
    if (!m_operations.matches || !m_operations.matches(before, code)) return false;
    m_rollbackErrors.clear();
    QString failure;
    if (!journal(QStringLiteral("ROLLING_BACK"), &failure)) m_rollbackErrors.append(failure);
    m_preview = before; emit changed();
    auto keys = m_created.keys();
    std::reverse(keys.begin(), keys.end());
    for (const QString &id : keys)
    {
        const auto found = std::find_if(m_candidates.cbegin(), m_candidates.cend(), [&id](const auto &panel) { return panel.identity.id == id; });
        if (found == m_candidates.cend()) { m_rollbackErrors.append(QStringLiteral("missing-created-profile-host")); continue; }
        auto panel = *found;
        if (panel.host.kind == PanelHostKind::FreeDesktop) panel.host.freeOwnershipToken = m_created.value(id);
        else panel.host.nativeOwnershipToken = m_created.value(id);
        if (!m_operations.remove || !m_operations.remove(panel, &failure)) m_rollbackErrors.append(id + QLatin1Char(':') + failure);
    }
    auto restored = before;
    for (qsizetype index = m_backup.size(); index-- > 0;)
    {
        const auto &snapshot = m_backup.at(index);
        if (!snapshot.touched && !snapshot.removed) continue;
        auto copy = snapshot;
        if (!m_operations.restore || !m_operations.restore(copy, &failure))
            m_rollbackErrors.append(snapshot.definition.identity.id + QLatin1Char(':') + failure);
        else restored[index] = copy.definition;
    }
    if (m_rollbackErrors.isEmpty() && restored != before)
    {
        for (auto &panel : restored) panel.settingsRevision++;
        if (!m_operations.commit || !m_operations.commit(before, restored, &failure)) m_rollbackErrors.append(failure);
        else if (m_operations.publish) m_operations.publish();
    }
    if (m_rollbackErrors.isEmpty() && !clearJournal(&failure)) m_rollbackErrors.append(failure);
    if (!m_rollbackErrors.isEmpty())
    { error(code, QStringLiteral("profile-partial-rollback")); return false; }
    m_preview.clear(); error(code, {}); return true;
}
QVariantMap ProfileApplyTransaction::abort(const QString &code)
{
    m_error = code.isEmpty() ? QStringLiteral("profile-host-change-failed") : code;
    QString rollbackCode;
    const bool restored = rollback(&rollbackCode);
    m_running = false;
    if (!restored)
    {
        if (!rollbackCode.isEmpty() && !m_rollbackErrors.contains(rollbackCode)) m_rollbackErrors.append(rollbackCode);
        QString journalError;
        if (!journal(QStringLiteral("BLOCKED"), &journalError)) m_rollbackErrors.append(journalError);
        return outcome(false);
    }
    transition(QStringLiteral("IDLE"));
    auto result = outcome(false);
    result.insert(QStringLiteral("rollbackStatus"), QStringLiteral("complete"));
    return result;
}
QVariantMap ProfileApplyTransaction::recover()
{
    if (m_running) return outcome(false, QStringLiteral("profile-apply-active"));
    QString code;
    if (!readJournal(&code)) { transition(QStringLiteral("BLOCKED")); return outcome(false, code); }
    if (!m_pending) { transition(QStringLiteral("IDLE")); return outcome(true); }
    if (m_operations.guard) { const auto guard = m_operations.guard(); if (!guard.isEmpty()) return outcome(false, guard); }
    m_running = true;
    if ((m_state == QStringLiteral("COMMITTING") || m_state == QStringLiteral("COMMITTED")) &&
        m_operations.matches && m_operations.matches(m_candidates, &code))
    {
        for (const auto &panel : m_candidates)
            if (hosted(panel) && (!m_operations.verify || !m_operations.verify(panel, &code)))
            { m_running = false; transition(QStringLiteral("BLOCKED")); return outcome(false, code); }
        if (!clearJournal(&code)) { m_running = false; transition(QStringLiteral("BLOCKED")); return outcome(false, code); }
        if (m_operations.publish) m_operations.publish();
        m_running = false; m_error.clear(); m_preview.clear(); transition(QStringLiteral("APPLIED")); return outcome(true);
    }
    if (!rollback(&code))
    { m_running = false; transition(QStringLiteral("BLOCKED")); return outcome(false, code); }
    m_running = false; m_error.clear(); transition(QStringLiteral("IDLE")); return outcome(true);
}
}
