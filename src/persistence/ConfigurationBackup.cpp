#include "ConfigurationBackup.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QUuid>
#include <algorithm>

namespace ArchDock
{
namespace
{
constexpr auto privateFile = QFile::ReadOwner | QFile::WriteOwner;
constexpr auto privateDirectory = privateFile | QFile::ExeOwner;
bool fail(QString *error, const QString &code)
{ if (error) *error = code; return false; }
bool safePath(QString path)
{
    path = QFileInfo(path).absoluteFilePath();
    for (;;) {
        const QFileInfo info(path);
        if (info.isSymLink()) return false;
        const QString parent = info.absolutePath();
        if (parent == path) return true;
        path = parent;
    }
}
bool validId(const QString &id)
{
    static const QRegularExpression pattern(QStringLiteral("^[0-9]{17}-[a-f0-9]{32}$"));
    return pattern.match(id).hasMatch();
}
bool dataName(const QString &name)
{
    static const QStringList suffixes = {"json", "conf", "ini", "png", "jpg", "jpeg",
        "webp", "svg", "svgz", "avif", "bmp", "gif", "tif", "tiff", "ico",
        "pdf", "psd", "xcf", "blend", "mesh", "ktx", "ktx2", "hdr"};
    return suffixes.contains(QFileInfo(name).suffix().toLower());
}
bool relativePath(const QString &path)
{
    return !QDir::isAbsolutePath(path) && !path.contains(QLatin1Char('\\')) &&
        !path.split(QLatin1Char('/')).contains(QStringLiteral("..")) &&
        QDir::cleanPath(path) == path;
}
bool makePrivate(const QString &path)
{
    return safePath(path) && QDir().mkpath(path) && QFile::setPermissions(path, privateDirectory);
}
QByteArray digest(const QString &path)
{
    QFile file(path);
    QCryptographicHash hash(QCryptographicHash::Sha256);
    return file.open(QIODevice::ReadOnly) && hash.addData(&file) ? hash.result().toHex() : QByteArray{};
}
bool copy(const QString &source, const QString &destination, QFile::Permissions permissions)
{
    if (!safePath(source) || !safePath(destination) || !QFileInfo(source).isFile() ||
        !QDir().mkpath(QFileInfo(destination).absolutePath())) return false;
    QFile input(source);
    QSaveFile output(destination);
    if (!input.open(QIODevice::ReadOnly) || !output.open(QIODevice::WriteOnly) ||
        !output.setPermissions(permissions)) return false;
    qint64 copied = 0;
    while (!input.atEnd()) {
        const QByteArray bytes = input.read(64 * 1024);
        copied += bytes.size();
        if (bytes.isEmpty() || copied > ConfigurationBackup::MaximumBytes ||
            output.write(bytes) != bytes.size()) return false;
    }
    return input.error() == QFile::NoError && output.commit() && digest(source) == digest(destination);
}
bool writeJson(const QString &path, const QJsonObject &object)
{
    if (!safePath(path)) return false;
    QSaveFile file(path);
    const auto bytes = QJsonDocument(object).toJson(QJsonDocument::Indented);
    return file.open(QIODevice::WriteOnly) && file.setPermissions(privateFile) &&
        file.write(bytes) == bytes.size() && file.commit();
}
QJsonObject readJson(const QString &path)
{
    QFile file(path);
    if (!safePath(path) || !file.open(QIODevice::ReadOnly) || file.size() > 4 * 1024 * 1024) return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}
bool collect(const QString &root, bool directory, QStringList &files, const QString &backupRoot,
    QString *error)
{
    if (!safePath(root)) return fail(error, "backup-unsafe-path");
    const QFileInfo info(root);
    if (!info.exists()) return true;
    if (!directory) {
        if (!info.isFile()) return fail(error, "backup-not-file");
        files.append(root);
        return true;
    }
    if (!info.isDir()) return fail(error, "backup-not-directory");
    QStringList pending{root};
    int entries = 0;
    while (!pending.isEmpty()) {
        const QDir dir(pending.takeLast());
        for (const QFileInfo &entry : dir.entryInfoList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot)) {
            if (++entries > ConfigurationBackup::MaximumFiles * 2) return fail(error, "backup-file-limit");
            const QString name = entry.fileName();
            if (name.startsWith(QLatin1Char('.')) || name == "processed" || name == "processed-renders" ||
                entry.absoluteFilePath() == QFileInfo(backupRoot).absoluteFilePath()) continue;
            if (entry.isSymLink()) return fail(error, "backup-symlink");
            if (entry.isDir()) pending.append(entry.absoluteFilePath());
            else if (entry.isFile() && dataName(name)) {
                if (entry.permissions() & (QFile::ExeOwner | QFile::ExeGroup | QFile::ExeOther))
                    return fail(error, "backup-executable-data");
                files.append(entry.absoluteFilePath());
                if (files.size() > ConfigurationBackup::MaximumFiles) return fail(error, "backup-file-limit");
            }
        }
    }
    std::sort(files.begin(), files.end());
    return true;
}
QJsonObject validate(const QString &root, const QString &id,
    const ConfigurationBackup::Locations &locations, QString *error)
{
    if (!validId(id)) { fail(error, "backup-invalid-id"); return {}; }
    const QString directory = QDir(root).filePath(id);
    const auto manifest = readJson(QDir(directory).filePath("manifest.json"));
    if (manifest["format"] != "archdock.configuration-backup" || manifest["version"].toInt() != 1 ||
        manifest["id"] != id || !manifest["files"].isArray() || !manifest["locations"].isArray()) {
        fail(error, "backup-invalid-manifest"); return {};
    }
    QStringList recorded;
    for (const auto &key : manifest["locations"].toArray()) recorded.append(key.toString());
    if (recorded != locations.keys()) { fail(error, "backup-location-mismatch"); return {}; }
    const auto files = manifest["files"].toArray();
    qint64 total = 0;
    QSet<QString> seen;
    if (files.size() > ConfigurationBackup::MaximumFiles) { fail(error, "backup-file-limit"); return {}; }
    for (const auto &value : files) {
        const auto item = value.toObject();
        const QString key = item["location"].toString(), relative = item["path"].toString();
        const QString identity = key + QLatin1Char('/') + relative;
        const auto permissions = QFile::Permissions(item["permissions"].toInt());
        if (!locations.contains(key) || !relativePath(relative) || seen.contains(identity) ||
            (!locations[key].directory && relative != "settings.conf") ||
            !dataName(relative) || relative.startsWith(QLatin1Char('.')) ||
            (permissions & (QFile::ExeOwner | QFile::ExeGroup | QFile::ExeOther))) {
            fail(error, "backup-unsafe-entry"); return {};
        }
        seen.insert(identity);
        const QString source = QDir(directory).filePath("files/" + identity);
        const QFileInfo info(source);
        total += info.size();
        if (!safePath(source) || !info.isFile() || item["size"].toInteger(-1) != info.size() ||
            total > ConfigurationBackup::MaximumBytes ||
            QString::fromLatin1(digest(source)) != item["sha256"].toString()) {
            fail(error, "backup-integrity-failed"); return {};
        }
    }
    return manifest;
}
}

QString ConfigurationBackup::defaultRoot()
{ return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("config-backups"); }
ConfigurationBackup::Locations ConfigurationBackup::defaultLocations()
{
    const QDir data(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    return {{"settings", {QSettings().fileName(), false}}, {"profiles", {data.filePath("profiles"), true}},
        {"presets", {data.filePath("presets"), true}}, {"themes", {data.filePath("themes"), true}},
        {"shortcuts", {data.filePath("profile-shortcuts.json"), false}}};
}
ConfigurationBackup::ConfigurationBackup(QString root, Locations locations)
    : m_root(QFileInfo(root).absoluteFilePath()), m_locations(std::move(locations)) {}
void ConfigurationBackup::setCheckpoint(std::function<bool(const QString &)> checkpoint)
{ m_checkpoint = std::move(checkpoint); }
bool ConfigurationBackup::recoveryPending() const
{ return QFileInfo::exists(QDir(m_root).filePath("restore.json")); }

QString ConfigurationBackup::capture(const QString &reason, QString *error)
{
    if (error) error->clear();
    if (!makePrivate(m_root)) { fail(error, "backup-root-unwritable"); return {}; }
    QLockFile lock(QDir(m_root).filePath("backup.lock"));
    if (!lock.tryLock(0) || recoveryPending()) { fail(error, "backup-recovery-or-operation-active"); return {}; }
    // Order snapshots by creation even within one millisecond or after a clock
    // adjustment. The lock makes this ordering authoritative for retention.
    QDateTime timestamp = QDateTime::currentDateTimeUtc();
    for (const QString &existing : QDir(m_root).entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (!validId(existing)) continue;
        const auto previous = QDateTime::fromString(existing.left(17), "yyyyMMddHHmmsszzz");
        const auto utc = QDateTime(previous.date(), previous.time(), QTimeZone::UTC);
        if (utc.isValid() && utc >= timestamp) timestamp = utc.addMSecs(1);
    }
    const QString id = timestamp.toString("yyyyMMddHHmmsszzz") + QLatin1Char('-') +
        QUuid::createUuid().toString(QUuid::Id128);
    const QString directory = QDir(m_root).filePath(id);
    if (!makePrivate(directory)) { fail(error, "backup-unwritable"); return {}; }
    const auto abort = [&](const QString &code) { QDir(directory).removeRecursively(); fail(error, code); return QString{}; };
    QJsonArray files;
    qint64 total = 0;
    for (auto it = m_locations.cbegin(); it != m_locations.cend(); ++it) {
        QStringList sources;
        if (!collect(it->path, it->directory, sources, m_root, error)) return abort(error ? *error : "backup-collect-failed");
        for (const QString &source : sources) {
            const QFileInfo info(source);
            total += info.size();
            if (total > MaximumBytes || files.size() >= MaximumFiles) return abort("backup-size-limit");
            if (info.permissions() & (QFile::ExeOwner | QFile::ExeGroup | QFile::ExeOther)) return abort("backup-executable-data");
            const QString relative = it->directory ? QDir(it->path).relativeFilePath(source) : QStringLiteral("settings.conf");
            const QString destination = QDir(directory).filePath("files/" + it.key() + QLatin1Char('/') + relative);
            if ((m_checkpoint && !m_checkpoint("capture-file")) || !copy(source, destination, privateFile))
                return abort("backup-copy-failed");
            files.append(QJsonObject{{"location", it.key()}, {"path", relative}, {"size", info.size()},
                {"permissions", int(info.permissions())}, {"sha256", QString::fromLatin1(digest(destination))}});
        }
    }
    QJsonArray locations;
    for (const auto &key : m_locations.keys()) locations.append(key);
    const QJsonObject manifest{{"format", "archdock.configuration-backup"}, {"version", 1}, {"id", id},
        {"reason", reason.left(128)}, {"createdUtc", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {"locations", locations}, {"files", files}};
    if ((m_checkpoint && !m_checkpoint("capture-manifest")) ||
        !writeJson(QDir(directory).filePath("manifest.json"), manifest) ||
        validate(m_root, id, m_locations, error).isEmpty()) return abort("backup-manifest-failed");
    return id;
}

QStringList ConfigurationBackup::backups(QString *error) const
{
    if (error) error->clear();
    if (!safePath(m_root)) { fail(error, "backup-unsafe-path"); return {}; }
    QStringList result;
    for (const QString &id : QDir(m_root).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name))
        if (validId(id) && !validate(m_root, id, m_locations, nullptr).isEmpty()) result.append(id);
    return result;
}
bool ConfigurationBackup::prune(int retain, QString *error)
{
    if (error) error->clear();
    if (!safePath(m_root)) return fail(error, "backup-unsafe-path");
    if (!QFileInfo::exists(m_root)) return true;
    QLockFile lock(QDir(m_root).filePath("backup.lock"));
    if (!lock.tryLock(0)) return fail(error, "backup-operation-active");
    const auto pending = readJson(QDir(m_root).filePath("restore.json"));
    const QStringList ids = backups(error);
    if (error && !error->isEmpty()) return false;
    const auto keep = ids.mid(qMax(0, int(ids.size()) - qBound(1, retain, 20)));
    for (const QString &id : QDir(m_root).entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (!validId(id) || keep.contains(id) || id == pending["before"].toString() ||
            id == pending["target"].toString()) continue;
        // Only our snapshot-shaped directories are eligible; incomplete or
        // corrupt copies do not accumulate outside the configured bound.
        if (!QDir(QDir(m_root).filePath(id)).removeRecursively()) return fail(error, "backup-prune-failed");
    }
    return true;
}

bool ConfigurationBackup::apply(const QString &id, QString *error)
{
    const auto manifest = validate(m_root, id, m_locations, error);
    if (manifest.isEmpty()) return false;
    // Validate every destination and stage every replacement before mutation.
    const QString stage = QDir(m_root).filePath(".restore-stage");
    if (!safePath(stage) || !QDir(stage).removeRecursively() || !makePrivate(stage))
        return fail(error, "restore-stage-failed");
    QMap<QString, QStringList> expected;
    QMap<QString, QStringList> current;
    for (auto it = m_locations.cbegin(); it != m_locations.cend(); ++it)
        if (!collect(it->path, it->directory, current[it.key()], m_root, error)) return false;
    for (const auto &value : manifest["files"].toArray()) {
        const auto item = value.toObject();
        const QString key = item["location"].toString(), path = item["path"].toString();
        const auto location = m_locations[key];
        const QString destination = location.directory ? QDir(location.path).filePath(path) : location.path;
        const QString identity = key + QLatin1Char('/') + path;
        if (!safePath(destination) || !copy(QDir(m_root).filePath(id + "/files/" + identity),
            QDir(stage).filePath(identity), privateFile)) return fail(error, "restore-stage-failed");
        expected[key].append(destination);
    }
    for (const auto &value : manifest["files"].toArray()) {
        const auto item = value.toObject();
        const QString key = item["location"].toString(), path = item["path"].toString();
        const auto location = m_locations[key];
        const QString destination = location.directory ? QDir(location.path).filePath(path) : location.path;
        if ((m_checkpoint && !m_checkpoint("restore-file")) || !copy(QDir(stage).filePath(key + "/" + path),
            destination, QFile::Permissions(item["permissions"].toInt()))) return fail(error, "restore-write-failed");
    }
    for (auto it = current.cbegin(); it != current.cend(); ++it)
        for (const QString &path : it.value())
            if (!expected[it.key()].contains(path) &&
                ((m_checkpoint && !m_checkpoint("restore-remove")) || !QFile::remove(path)))
                return fail(error, "restore-remove-failed");
    return QDir(stage).removeRecursively() || fail(error, "restore-stage-cleanup-failed");
}
bool ConfigurationBackup::restore(const QString &id, QString *error)
{
    if (error) error->clear();
    if (validate(m_root, id, m_locations, error).isEmpty()) return false;
    const QString before = capture("before-restore", error);
    if (before.isEmpty()) return false;
    QLockFile lock(QDir(m_root).filePath("backup.lock"));
    if (!lock.tryLock(0) || recoveryPending()) return fail(error, "backup-recovery-or-operation-active");
    const QString journal = QDir(m_root).filePath("restore.json");
    if (!writeJson(journal, {{"version", 1}, {"before", before}, {"target", id}}))
        return fail(error, "restore-journal-failed");
    if (apply(id, error)) return QFile::remove(journal) || fail(error, "restore-journal-cleanup-failed");
    const auto checkpoint = std::move(m_checkpoint);
    const bool rolledBack = apply(before, nullptr);
    m_checkpoint = checkpoint;
    if (rolledBack) QFile::remove(journal);
    else fail(error, "restore-rollback-pending");
    return false;
}
bool ConfigurationBackup::recover(QString *error)
{
    if (error) error->clear();
    if (!safePath(m_root)) return fail(error, "backup-unsafe-path");
    if (!recoveryPending()) return true;
    QLockFile lock(QDir(m_root).filePath("backup.lock"));
    if (!lock.tryLock(0)) return fail(error, "backup-operation-active");
    const QString path = QDir(m_root).filePath("restore.json");
    const auto journal = readJson(path);
    if (journal["version"].toInt() != 1 || !validId(journal["target"].toString()) ||
        !validId(journal["before"].toString())) return fail(error, "restore-invalid-journal");
    return apply(journal["before"].toString(), error) &&
        (QFile::remove(path) || fail(error, "restore-journal-cleanup-failed"));
}
}
