// The audition recovery record on disk.
#include "PresetPreviewRecovery.h"
#include "../model/PresetIdentity.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>

#include <cmath>
#include <limits>

namespace ArchDock
{
namespace
{
void error(QString *target, const QString &value) { if (target) *target = value; }
bool validHostId(const QVariant &value)
{
    bool ok = false;
    const double number = value.toDouble(&ok);
    return ok && std::isfinite(number) && std::floor(number) == number &&
        number >= -1 && number <= std::numeric_limits<int>::max();
}
}

QVariantMap PresetPreviewRecord::toVariantMap() const
{
    auto original = snapshot.toLegacyMap();
    original.insert(QStringLiteral("settingsRevision"), QString::number(snapshot.settingsRevision));
    return {{QStringLiteral("sessionId"), sessionId}, {QStringLiteral("kind"), kind},
        {QStringLiteral("panelId"), panelId}, {QStringLiteral("hostKind"), hostKind},
        {QStringLiteral("phase"), phase}, {QStringLiteral("temporary"), temporary},
        {QStringLiteral("previewToken"), previewToken}, {QStringLiteral("managedToken"), managedToken},
        {QStringLiteral("containmentId"), containmentId}, {QStringLiteral("appletId"), appletId},
        {QStringLiteral("snapshot"), original}, {QStringLiteral("hostState"), hostState}};
}

std::optional<PresetPreviewRecord> PresetPreviewRecord::fromVariantMap(const QVariantMap &value)
{
    PresetPreviewRecord result;
    if (value.keys() != result.toVariantMap().keys()) return std::nullopt;
    for (const QString &key : {QStringLiteral("sessionId"), QStringLiteral("kind"),
         QStringLiteral("panelId"), QStringLiteral("hostKind"), QStringLiteral("phase"),
         QStringLiteral("previewToken"), QStringLiteral("managedToken")})
        if (value.value(key).metaType().id() != QMetaType::QString) return std::nullopt;
    if (value.value(QStringLiteral("temporary")).metaType().id() != QMetaType::Bool ||
        value.value(QStringLiteral("snapshot")).metaType().id() != QMetaType::QVariantMap ||
        value.value(QStringLiteral("hostState")).metaType().id() != QMetaType::QVariantMap ||
        !validHostId(value.value(QStringLiteral("containmentId"))) ||
        !validHostId(value.value(QStringLiteral("appletId")))) return std::nullopt;
    result.sessionId = value.value(QStringLiteral("sessionId")).toString();
    result.kind = value.value(QStringLiteral("kind")).toString();
    result.panelId = value.value(QStringLiteral("panelId")).toString();
    result.hostKind = value.value(QStringLiteral("hostKind")).toString();
    result.phase = value.value(QStringLiteral("phase")).toString();
    result.temporary = value.value(QStringLiteral("temporary")).toBool();
    result.previewToken = value.value(QStringLiteral("previewToken")).toString();
    result.managedToken = value.value(QStringLiteral("managedToken")).toString();
    result.containmentId = value.value(QStringLiteral("containmentId")).toInt();
    result.appletId = value.value(QStringLiteral("appletId")).toInt();
    result.hostState = value.value(QStringLiteral("hostState")).toMap();
    const auto original = PanelDefinition::fromLegacyMap(value.value(QStringLiteral("snapshot")).toMap());
    const QStringList phases{QStringLiteral("PREPARING"), QStringLiteral("ACTIVE"),
        QStringLiteral("CONVERTING"), QStringLiteral("ADOPTING"), QStringLiteral("COMMITTING"),
        QStringLiteral("COMMITTED"), QStringLiteral("ROLLING_BACK"), QStringLiteral("BLOCKED")};
    if (!original || QUuid(result.sessionId).isNull() || !PresetIdentity::isValidId(result.panelId) ||
        (result.kind != QStringLiteral("panel") && result.kind != QStringLiteral("icon") &&
         result.kind != QStringLiteral("scene3d")) ||
        !phases.contains(result.phase) || result.hostState.size() > 64 ||
        original->identity.id != result.panelId ||
        PanelDefinition::hostKindName(original->host.kind) != result.hostKind ||
        result.previewToken.isEmpty() || result.previewToken.size() > 128 ||
        result.managedToken.size() > 128 ||
        result.managedToken.startsWith(QStringLiteral("archdock-preview-"))) return std::nullopt;
    if (result.temporary)
    {
        static const QRegularExpression reserved(QStringLiteral("^(free|panel)-x[0-9a-f]{8}$"));
        if (result.kind != QStringLiteral("panel") || !reserved.match(result.panelId).hasMatch() ||
            !result.previewToken.startsWith(QStringLiteral("archdock-preview-"))) return std::nullopt;
    }
    else
    {
        const QString token = original->host.kind == PanelHostKind::FreeDesktop
            ? original->host.freeOwnershipToken : original->host.nativeOwnershipToken;
        if (token != result.previewToken || !result.managedToken.isEmpty()) return std::nullopt;
    }
    result.snapshot = *original;
    return result;
}

QString PresetPreviewRecovery::defaultPath()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
        .filePath(QStringLiteral("preset-preview-journal.json"));
}
PresetPreviewRecovery::PresetPreviewRecovery(QString filePath) : m_filePath(std::move(filePath)) {}
QString PresetPreviewRecovery::filePath() const { return m_filePath; }

std::optional<PresetPreviewRecord> PresetPreviewRecovery::load(QString *errorCode) const
{
    error(errorCode, QString{});
    const QFileInfo info(m_filePath);
    if (info.isSymLink()) { error(errorCode, QStringLiteral("unsafe-preview-journal")); return {}; }
    if (!info.exists()) return {};
    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly))
    { error(errorCode, QStringLiteral("preview-journal-unreadable")); return {}; }
    const QByteArray bytes = file.read(MaximumBytes + 1);
    if (bytes.size() > MaximumBytes)
    { error(errorCode, QStringLiteral("preview-journal-limit-exceeded")); return {}; }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(bytes, &parseError);
    const auto object = document.object();
    if (parseError.error != QJsonParseError::NoError || !document.isObject() ||
        object.keys() != QStringList{QStringLiteral("format"), QStringLiteral("record"), QStringLiteral("version")} ||
        object.value(QStringLiteral("format")).toString() != QStringLiteral("org.archdock.preset-preview-journal") ||
        !object.value(QStringLiteral("version")).isDouble() || object.value(QStringLiteral("version")).toDouble() != 1 ||
        !object.value(QStringLiteral("record")).isObject())
    { error(errorCode, QStringLiteral("invalid-preview-journal")); return {}; }
    auto result = PresetPreviewRecord::fromVariantMap(object.value(QStringLiteral("record")).toObject().toVariantMap());
    if (!result) error(errorCode, QStringLiteral("invalid-preview-journal"));
    return result;
}

bool PresetPreviewRecovery::save(const PresetPreviewRecord &record, QString *errorCode) const
{
    QString previousError;
    const auto previous = load(&previousError);
    if (!previousError.isEmpty()) { error(errorCode, previousError); return false; }
    if (previous && previous->sessionId != record.sessionId)
    { error(errorCode, QStringLiteral("preview-journal-busy")); return false; }
    if (!PresetPreviewRecord::fromVariantMap(record.toVariantMap()))
    { error(errorCode, QStringLiteral("invalid-preview-record")); return false; }
    const QByteArray bytes = QJsonDocument(QJsonObject{
        {QStringLiteral("format"), QStringLiteral("org.archdock.preset-preview-journal")},
        {QStringLiteral("version"), 1},
        {QStringLiteral("record"), QJsonObject::fromVariantMap(record.toVariantMap())}})
        .toJson(QJsonDocument::Compact);
    if (bytes.size() > MaximumBytes)
    { error(errorCode, QStringLiteral("preview-journal-limit-exceeded")); return false; }
    if (!QDir().mkpath(QFileInfo(m_filePath).absolutePath()))
    { error(errorCode, QStringLiteral("preview-journal-unwritable")); return false; }
    QSaveFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly) ||
        !file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner) ||
        file.write(bytes) != bytes.size() || !file.commit())
    { error(errorCode, QStringLiteral("preview-journal-unwritable")); return false; }
    error(errorCode, QString{});
    return true;
}

bool PresetPreviewRecovery::clear(QString *errorCode) const
{
    QString previousError;
    const auto previous = load(&previousError);
    if (!previousError.isEmpty()) { error(errorCode, previousError); return false; }
    if (!previous) { error(errorCode, QString{}); return true; }
    if (!QFile::remove(m_filePath))
    { error(errorCode, QStringLiteral("preview-journal-clear-failed")); return false; }
    error(errorCode, QString{});
    return true;
}
}
