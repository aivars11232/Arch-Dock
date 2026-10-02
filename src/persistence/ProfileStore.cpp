#include "ProfileStore.h"
#include "../model/PanelRuntimeState.h"
#include "../model/SettingsMigration.h"
#include "../themes/ThemePackage.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>
#include <QUrl>
#include <QXmlStreamReader>

#include <algorithm>
#include <cmath>
#include <limits>

namespace ArchDock
{
namespace
{
void error(QString *target, const QString &code)
{
    if (target) *target = code;
}
bool safeAssetRelativePath(const QString &relative)
{
    const auto parts = relative.split(QLatin1Char('/'));
    return !relative.isEmpty() && !QDir::isAbsolutePath(relative) && QDir::cleanPath(relative) == relative &&
        !parts.contains(QStringLiteral("..")) && !parts.contains(QStringLiteral(".")) && !parts.contains(QString{}) &&
        !relative.contains(QLatin1Char('%')) && !relative.contains(QLatin1Char('\\')) && !relative.contains(QLatin1Char(':')) &&
        std::none_of(relative.cbegin(), relative.cend(), [](QChar c) { return c.category() == QChar::Other_Control; });
}

bool boundedJson(const QVariant &value, int depth = 0)
{
    if (depth > 12) return false;
    switch (value.metaType().id())
    {
    case QMetaType::UnknownType:
    case QMetaType::Nullptr:
    case QMetaType::Bool:
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
        return true;
    case QMetaType::Double:
        return std::isfinite(value.toDouble());
    case QMetaType::QString:
        return value.toString().toUtf8().size() <= 16384 &&
            !value.toString().contains(QChar::Null);
    case QMetaType::QStringList:
    {
        const auto list = value.toStringList();
        return list.size() <= 512 &&
            std::all_of(list.cbegin(), list.cend(),
                [](const QString &text) { return text.toUtf8().size() <= 16384; });
    }
    case QMetaType::QVariantList:
    {
        const auto list = value.toList();
        return list.size() <= 512 && std::all_of(list.cbegin(), list.cend(),
            [depth](const QVariant &item) { return boundedJson(item, depth + 1); });
    }
    case QMetaType::QVariantMap:
    {
        const auto map = value.toMap();
        if (map.size() > 512) return false;
        for (auto it = map.cbegin(); it != map.cend(); ++it)
            if (it.key().size() > 256 || !boundedJson(it.value(), depth + 1)) return false;
        return true;
    }
    default:
        return false;
    }
}

bool integer(const QVariant &value, int minimum, int maximum)
{
    const int type = value.metaType().id();
    if (type != QMetaType::Int && type != QMetaType::UInt &&
        type != QMetaType::LongLong && type != QMetaType::ULongLong &&
        type != QMetaType::Double) return false;
    const double number = value.toDouble();
    return std::isfinite(number) && std::floor(number) == number &&
        number >= minimum && number <= maximum;
}

bool safeRoot(const QString &root)
{
    // Reject symbolic links in the managed path, including an absent root's
    // ancestors, before any read or directory creation.
    QString path = QDir::cleanPath(QFileInfo(root).absoluteFilePath());
    while (!path.isEmpty())
    {
        const QFileInfo info(path);
        if (info.isSymLink() || (info.exists() && !info.isDir())) return false;
        const QString parent = info.absolutePath();
        if (parent == path) break;
        path = parent;
    }
    return true;
}

std::optional<ProfileDefinition> readProfile(const QString &path, QString *code)
{
    error(code, {});
    const QFileInfo info(path);
    if (info.isSymLink() || !info.isFile())
    { error(code, QStringLiteral("profile-file-not-regular")); return {}; }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    { error(code, QStringLiteral("profile-unreadable")); return {}; }
    const QByteArray bytes = file.read(ProfileStore::MaximumBytes + 1);
    if (bytes.size() > ProfileStore::MaximumBytes)
    { error(code, QStringLiteral("profile-limit-exceeded")); return {}; }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    { error(code, QStringLiteral("invalid-profile-json")); return {}; }
    return ProfileDefinition::fromVariantMap(document.object().toVariantMap(), code);
}

bool safeSvg(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return false;
    QXmlStreamReader reader(&file);
    bool rootSeen = false;
    static const QSet<QString> forbidden{QStringLiteral("script"), QStringLiteral("foreignobject"),
        QStringLiteral("iframe"), QStringLiteral("object"), QStringLiteral("embed")};
    static const QRegularExpression urls(QStringLiteral("url\\s*\\(([^)]*)\\)"), QRegularExpression::CaseInsensitiveOption);
    const auto safeCss = [](const QString &text) {
        if (text.contains(QLatin1Char('\\')) || text.contains(QStringLiteral("@import"), Qt::CaseInsensitive)) return false;
        auto matches = urls.globalMatch(text);
        while (matches.hasNext())
        {
            QString value = matches.next().captured(1).trimmed();
            value.remove(QLatin1Char('\'')); value.remove(QLatin1Char('"'));
            if (!value.startsWith(QLatin1Char('#'))) return false;
        }
        return true;
    };
    while (!reader.atEnd())
    {
        reader.readNext();
        if (reader.isDTD() || reader.isEntityReference()) return false;
        if (reader.isStartElement())
        {
            const auto name = reader.name().toString().toLower();
            if ((!rootSeen && name != QStringLiteral("svg")) || forbidden.contains(name)) return false;
            rootSeen = true;
            for (const auto &attribute : reader.attributes())
            {
                const auto key = attribute.name().toString().toLower();
                const auto value = attribute.value().toString().trimmed();
                if (key.startsWith(QStringLiteral("on")) ||
                    (key == QStringLiteral("href") && !value.startsWith(QLatin1Char('#'))) || !safeCss(value)) return false;
            }
        }
        if (reader.isCharacters() && !safeCss(reader.text().toString())) return false;
    }
    return rootSeen && !reader.hasError();
}

bool transferAssets(ProfileDefinition &profile, const QString &sourceRoot,
    const QString &destinationRoot, bool importing, QString *code)
{
    constexpr qint64 maximumTransferBytes = 32 * 1024 * 1024;
    qint64 totalBytes = 0;
    QSet<QString> counted;
    const auto fail = [code](const QString &reason) { error(code, reason); return false; };
    if (!safeRoot(destinationRoot) || (importing && !safeRoot(sourceRoot)))
        return fail(QStringLiteral("unsafe-profile-asset-root"));
    const auto transfer = [&](QString &reference, bool manifest) -> bool {
        if (reference.isEmpty()) return true;
        QString source;
        if (importing)
        {
            if (!reference.startsWith(QStringLiteral("profile-asset:")))
                return fail(QStringLiteral("unsafe-profile-asset-reference"));
            const QString relative = reference.mid(14);
            if (!safeAssetRelativePath(relative))
                return fail(QStringLiteral("unsafe-profile-asset-reference"));
            source = QDir(sourceRoot).filePath(relative);
            const QString canonicalRoot = QFileInfo(sourceRoot).canonicalFilePath();
            const QString canonical = QFileInfo(source).canonicalFilePath();
            if (canonicalRoot.isEmpty() || !canonical.startsWith(canonicalRoot + QLatin1Char('/')))
                return fail(QStringLiteral("profile-asset-outside-package"));
        }
        else
        {
            const QUrl url(reference);
            if (!url.isLocalFile()) return fail(QStringLiteral("unsafe-profile-asset-reference"));
            source = url.toLocalFile();
        }
        const QFileInfo info(source);
        if (!info.isFile() || info.isSymLink() || !safeRoot(info.absolutePath()))
            return fail(QStringLiteral("profile-asset-not-regular"));
        ThemePackageLoadResult loaded;
        if (manifest) loaded = ThemePackage::load(source);
        else
        {
            // Reuse the raster/SVG safety validator, limits and materializer;
            // no conversion program or imported executable is invoked.
            const auto bytes = QJsonDocument(QJsonObject{
                {QStringLiteral("format"), QStringLiteral("org.archdock.theme")},
                {QStringLiteral("version"), 1}, {QStringLiteral("id"), QStringLiteral("profile-artwork")},
                {QStringLiteral("name"), QStringLiteral("Profile artwork")},
                {QStringLiteral("surface"), QJsonObject{{QStringLiteral("asset"), info.fileName()},
                    {QStringLiteral("fit"), QStringLiteral("contain")}}}}).toJson();
            loaded = ThemePackage::loadBytes(bytes, info.absolutePath());
        }
        if (!loaded.isValid()) return fail(loaded.primaryCode());
        for (const auto &asset : loaded.package->definition().assets)
        {
            const QString path = loaded.package->assetPath(asset.id);
            const QFileInfo assetInfo(path);
            if (!assetInfo.isFile() || assetInfo.isSymLink() || !safeRoot(assetInfo.absolutePath()))
                return fail(QStringLiteral("profile-asset-not-regular"));
            const QString canonical = assetInfo.canonicalFilePath();
            if (importing && !canonical.startsWith(QFileInfo(sourceRoot).canonicalFilePath() + QLatin1Char('/')))
                return fail(QStringLiteral("profile-asset-outside-package"));
            if (!counted.contains(canonical))
            {
                totalBytes += assetInfo.size();
                counted.insert(canonical);
                if (totalBytes > maximumTransferBytes || counted.size() > 256)
                    return fail(QStringLiteral("profile-assets-limit-exceeded"));
                if (asset.kind == QStringLiteral("vector") &&
                    (assetInfo.suffix().toLower() != QStringLiteral("svg") || !safeSvg(path)))
                    return fail(QStringLiteral("unsafe-profile-svg"));
            }
        }
        const auto copied = loaded.package->materialize(destinationRoot);
        if (!copied.isValid()) return fail(QStringLiteral("profile-asset-copy-failed"));
        const QString path = manifest ? copied.package->manifestPath() : copied.package->primarySurfacePath();
        const QString relative = QDir(destinationRoot).relativeFilePath(path);
        if (!importing && !safeAssetRelativePath(relative))
            return fail(QStringLiteral("unsafe-profile-asset-reference"));
        reference = importing ? QUrl::fromLocalFile(path).toString()
            : QStringLiteral("profile-asset:") + relative;
        return true;
    };
    for (auto &panel : profile.panels)
    {
        auto &surface = panel.surface;
        if (!transfer(surface.themePackageManifest, true) || !transfer(surface.themeAsset, false) ||
            !transfer(surface.themeSource, false) || !transfer(surface.themePreview, false)) return false;
        for (auto it = panel.iconStyle.perEntryOverrides.begin(); it != panel.iconStyle.perEntryOverrides.end(); ++it)
        {
            QString &glyph = it.value().customGlyph;
            if (glyph.startsWith(QStringLiteral("profile-asset:")) || QUrl(glyph).isLocalFile())
            { if (!transfer(glyph, false)) return false; }
            else if (QUrl(glyph).isValid() && !QUrl(glyph).scheme().isEmpty())
                return fail(QStringLiteral("unsafe-profile-glyph"));
            else if (glyph.contains(QLatin1Char('/')) || glyph.contains(QLatin1Char('\\')))
                return fail(QStringLiteral("unsafe-profile-glyph"));
        }
    }
    return true;
}

std::optional<ProfileDefinition> expectedProfile(const ProfileStore &store,
    const QString &id, int revision, QString *code)
{
    const auto profile = store.load(id, code);
    if (profile && profile->revision != revision)
    { error(code, QStringLiteral("stale-profile-revision")); return {}; }
    return profile;
}
}

QString ProfileDefinition::format() { return QStringLiteral("org.archdock.profile"); }

bool ProfileDefinition::validId(const QString &id)
{
    static const QRegularExpression grammar(QStringLiteral("\\A[a-z0-9][a-z0-9-]{0,63}\\z"));
    return grammar.match(id).hasMatch();
}

PanelDefinition ProfileDefinition::portablePanel(const PanelDefinition &panel)
{
    PanelDefinition result = panel;
    result.host = PanelHost{};
    result.host.kind = panel.host.kind;
    result.host.screenIndex = panel.host.screenIndex;
    result.host.screenId = panel.host.screenId;
    result.settingsRevision = 0;
    const auto parsed = PanelDefinition::fromLegacyMap(result.toPersistedMap());
    return parsed.value_or(result).normalized();
}

ProfileDefinition ProfileDefinition::capture(
    const QString &name, const QList<PanelDefinition> &panels)
{
    ProfileDefinition result;
    result.id = QStringLiteral("profile-") + QUuid::createUuid().toString(QUuid::Id128);
    result.name = name.trimmed();
    for (const auto &panel : panels) result.panels.append(portablePanel(panel));
    return result;
}

QVariantMap ProfileDefinition::toVariantMap() const
{
    QVariantList records;
    for (const auto &panel : panels) records.append(portablePanel(panel).toPersistedMap());
    return {{QStringLiteral("format"), format()}, {QStringLiteral("schemaVersion"), SchemaVersion},
        {QStringLiteral("id"), id}, {QStringLiteral("name"), name},
        {QStringLiteral("revision"), revision}, {QStringLiteral("screenPolicy"), screenPolicy},
        {QStringLiteral("metadata"), metadata}, {QStringLiteral("panels"), records}};
}

std::optional<ProfileDefinition> ProfileDefinition::fromVariantMap(
    const QVariantMap &value, QString *errorCode)
{
    error(errorCode, {});
    const auto fail = [errorCode](const QString &code) -> std::optional<ProfileDefinition> {
        error(errorCode, code); return {};
    };
    const QStringList keys{QStringLiteral("format"), QStringLiteral("id"),
        QStringLiteral("metadata"), QStringLiteral("name"), QStringLiteral("panels"),
        QStringLiteral("revision"), QStringLiteral("schemaVersion"), QStringLiteral("screenPolicy")};
    if (value.keys() != keys || !boundedJson(value))
        return fail(QStringLiteral("invalid-profile-shape"));
    if (value.value(QStringLiteral("format")).toString() != format())
        return fail(QStringLiteral("unsupported-profile-format"));
    if (!integer(value.value(QStringLiteral("schemaVersion")), SchemaVersion, SchemaVersion))
        return fail(QStringLiteral("unsupported-profile-version"));
    for (const QString &key : {QStringLiteral("id"), QStringLiteral("name"),
                              QStringLiteral("screenPolicy")})
        if (value.value(key).metaType().id() != QMetaType::QString)
            return fail(QStringLiteral("invalid-profile-type"));
    ProfileDefinition result;
    result.id = value.value(QStringLiteral("id")).toString();
    result.name = value.value(QStringLiteral("name")).toString();
    result.screenPolicy = value.value(QStringLiteral("screenPolicy")).toString();
    if (!validId(result.id) || result.name.isEmpty() || result.name != result.name.trimmed() ||
        result.name.toUtf8().size() > 256 ||
        std::any_of(result.name.cbegin(), result.name.cend(),
            [](QChar character) { return character.category() == QChar::Other_Control; }))
        return fail(QStringLiteral("invalid-profile-identity"));
    if (!integer(value.value(QStringLiteral("revision")), 1, std::numeric_limits<int>::max()))
        return fail(QStringLiteral("invalid-profile-revision"));
    result.revision = value.value(QStringLiteral("revision")).toInt();
    if (result.screenPolicy != QStringLiteral("stable-id-then-index"))
        return fail(QStringLiteral("unsupported-screen-policy"));
    if (value.value(QStringLiteral("metadata")).metaType().id() != QMetaType::QVariantMap ||
        value.value(QStringLiteral("panels")).metaType().id() != QMetaType::QVariantList)
        return fail(QStringLiteral("invalid-profile-type"));
    result.metadata = value.value(QStringLiteral("metadata")).toMap();
    const auto records = value.value(QStringLiteral("panels")).toList();
    if (records.isEmpty() || records.size() > MaximumPanels)
        return fail(QStringLiteral("invalid-profile-panel-count"));
    QSet<QString> ids;
    for (const auto &record : records)
    {
        if (record.metaType().id() != QMetaType::QVariantMap)
            return fail(QStringLiteral("invalid-profile-panel"));
        const auto map = record.toMap();
        const auto extensions = map.value(QStringLiteral("extensions")).toMap();
        for (auto it = map.cbegin(); it != map.cend(); ++it)
            if (PanelRuntimeState::isTransientLegacyKey(it.key()))
                return fail(QStringLiteral("profile-runtime-state"));
        for (auto it = extensions.cbegin(); it != extensions.cend(); ++it)
            if (PanelRuntimeState::isTransientLegacyKey(it.key()))
                return fail(QStringLiteral("profile-runtime-state"));
        const auto bytes = QJsonDocument(QJsonArray{QJsonObject::fromVariantMap(map)})
            .toJson(QJsonDocument::Compact);
        const auto migration = SettingsMigration::migratePanelRecords(bytes);
        if (migration.status != PanelMigrationStatus::Success || migration.definitions.size() != 1)
            return fail(QStringLiteral("invalid-profile-panel"));
        const auto &panel = migration.definitions.first();
        if (panel != portablePanel(panel))
            return fail(QStringLiteral("profile-live-host-association"));
        if (!validId(panel.identity.id) || ids.contains(panel.identity.id))
            return fail(QStringLiteral("duplicate-or-invalid-panel-id"));
        ids.insert(panel.identity.id);
        result.panels.append(panel);
    }
    if (QJsonDocument::fromVariant(result.toVariantMap()).toJson(QJsonDocument::Compact).size() >
        ProfileStore::MaximumBytes) return fail(QStringLiteral("profile-limit-exceeded"));
    return result;
}

QList<PanelDefinition> ProfileDefinition::resolved(
    const ProfileReferences &references, QStringList *diagnostics) const
{
    QList<PanelDefinition> result = panels;
    const auto report = [diagnostics](const QString &id, const QString &code) {
        if (diagnostics) diagnostics->append(id + QLatin1Char(':') + code);
    };
    for (auto &panel : result)
    {
        const QString theme = panel.surface.completeThemeId.isEmpty()
            ? panel.surface.panelThemeId : panel.surface.completeThemeId;
        if (!theme.isEmpty() && !references.themes.contains(theme))
        {
            report(panel.identity.id, QStringLiteral("theme-fallback"));
            panel.surface = PanelSurfaceDefinition{};
            panel.surface.rendererTier = QStringLiteral("procedural2d");
        }
        if (!references.iconStyles.contains(panel.iconStyle.styleReference))
        {
            report(panel.identity.id, QStringLiteral("icon-style-fallback"));
            panel.iconStyle.styleReference = panel.iconStyle.themeId = QStringLiteral("plain-original");
            panel.iconStyle.globalDefaults.remove(QStringLiteral("presetOverrides"));
        }
        const auto motion = [&](QString &id, const QString &fallback) {
            if (!id.isEmpty() && id != QStringLiteral("none") && !references.motions.contains(id))
            { report(panel.identity.id, QStringLiteral("motion-fallback")); id = fallback; }
        };
        motion(panel.motion.iconProfile, QStringLiteral("none"));
        motion(panel.motion.panelProfile, {});
        motion(panel.motion.revealProfile, {});
        for (auto &segment : panel.segments) motion(segment.motionProfile, {});
        for (auto it = panel.iconStyle.perEntryOverrides.begin();
             it != panel.iconStyle.perEntryOverrides.end(); ++it)
        {
            auto &entry = it.value();
            if (!entry.styleReference.isEmpty() && !references.iconStyles.contains(entry.styleReference))
            { report(panel.identity.id, QStringLiteral("entry-style-fallback"));
              entry.styleReference = QStringLiteral("plain-original"); }
            motion(entry.animationProfileReference, QStringLiteral("none"));
        }
        panel = panel.normalized();
    }
    return result;
}

QString ProfileStore::defaultRoot()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
        .filePath(QStringLiteral("profiles"));
}
ProfileStore::ProfileStore(QString root) : m_root(QFileInfo(root).absoluteFilePath()) {}
QString ProfileStore::rootDirectory() const { return m_root; }

QList<ProfileDefinition> ProfileStore::profiles(QStringList *diagnostics) const
{
    QList<ProfileDefinition> result;
    if (!safeRoot(m_root))
    { if (diagnostics) diagnostics->append(QStringLiteral("unsafe-profile-root")); return result; }
    const auto files = QDir(m_root).entryInfoList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
    for (const auto &file : files)
    {
        if (result.size() >= MaximumProfiles)
        { if (diagnostics) diagnostics->append(QStringLiteral("profile-store-limit")); break; }
        QString code;
        const auto profile = load(file.completeBaseName(), &code);
        if (profile) result.append(*profile);
        else if (diagnostics) diagnostics->append(file.fileName() + QLatin1Char(':') + code);
    }
    std::sort(result.begin(), result.end(), [](const auto &left, const auto &right) {
        const int compared = QString::compare(left.name, right.name, Qt::CaseInsensitive);
        return compared == 0 ? left.id < right.id : compared < 0;
    });
    return result;
}

std::optional<ProfileDefinition> ProfileStore::load(const QString &id, QString *errorCode) const
{
    if (!ProfileDefinition::validId(id) || !safeRoot(m_root))
    { error(errorCode, QStringLiteral("unsafe-profile-path")); return {}; }
    const auto result = readProfile(QDir(m_root).filePath(id + QStringLiteral(".json")), errorCode);
    if (result && result->id != id)
    { error(errorCode, QStringLiteral("profile-file-identity-mismatch")); return {}; }
    return result;
}

std::optional<ProfileDefinition> ProfileStore::save(
    const ProfileDefinition &profile, QString *errorCode) const
{
    auto checked = ProfileDefinition::fromVariantMap(profile.toVariantMap(), errorCode);
    if (!checked) return {};
    if (!safeRoot(m_root))
    { error(errorCode, QStringLiteral("unsafe-profile-root")); return {}; }
    const QString path = QDir(m_root).filePath(profile.id + QStringLiteral(".json"));
    const QFileInfo info(path);
    if (info.exists() || info.isSymLink())
    {
        const auto previous = load(profile.id, errorCode);
        if (!previous) return {};
        if (previous->revision != profile.revision || profile.revision == std::numeric_limits<int>::max())
        { error(errorCode, QStringLiteral("stale-profile-revision")); return {}; }
        checked->revision++;
    }
    else if (QDir(m_root).entryList({QStringLiteral("*.json")}, QDir::Files).size() >= MaximumProfiles)
    { error(errorCode, QStringLiteral("profile-store-limit")); return {}; }
    const auto bytes = QJsonDocument::fromVariant(checked->toVariantMap()).toJson(QJsonDocument::Indented);
    if (bytes.size() > MaximumBytes)
    { error(errorCode, QStringLiteral("profile-limit-exceeded")); return {}; }
    if (!QDir().mkpath(m_root))
    { error(errorCode, QStringLiteral("profile-store-unwritable")); return {}; }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) ||
        !file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner) ||
        file.write(bytes) != bytes.size() || !file.commit())
    { error(errorCode, QStringLiteral("profile-store-unwritable")); return {}; }
    const auto stored = load(profile.id, errorCode);
    if (!stored || *stored != *checked)
    { error(errorCode, QStringLiteral("profile-readback-mismatch")); return {}; }
    return stored;
}

std::optional<ProfileDefinition> ProfileStore::create(const QString &name,
    const QList<PanelDefinition> &panels, QString *code) const
{
    return save(ProfileDefinition::capture(name, panels), code);
}

std::optional<ProfileDefinition> ProfileStore::rename(const QString &id,
    int revision, const QString &name, QString *code) const
{
    auto profile = expectedProfile(*this, id, revision, code);
    if (!profile) return {};
    profile->name = name.trimmed();
    return save(*profile, code);
}

std::optional<ProfileDefinition> ProfileStore::duplicate(const QString &id,
    int revision, const QString &name, QString *code) const
{
    auto profile = expectedProfile(*this, id, revision, code);
    if (!profile) return {};
    profile->id = ProfileDefinition::capture(name, {}).id;
    profile->name = name.trimmed();
    profile->revision = 1;
    return save(*profile, code);
}

bool ProfileStore::remove(const QString &id, int revision, QString *code) const
{
    if (!expectedProfile(*this, id, revision, code)) return false;
    if (!QFile::remove(QDir(m_root).filePath(id + QStringLiteral(".json"))))
    { error(code, QStringLiteral("profile-delete-failed")); return false; }
    error(code, {});
    return true;
}

std::optional<ProfileDefinition> ProfileStore::importProfile(const QString &path,
    const QString &name, QString *code) const
{
    if (!safeRoot(QFileInfo(path).absolutePath()))
    { error(code, QStringLiteral("unsafe-profile-import-path")); return {}; }
    auto profile = readProfile(path, code);
    if (!profile) return {};
    if (!transferAssets(*profile, path + QStringLiteral(".assets"),
            QDir(m_root).filePath(QStringLiteral("assets")), true, code)) return {};
    profile->id = ProfileDefinition::capture({}, {}).id;
    profile->revision = 1;
    if (!name.trimmed().isEmpty()) profile->name = name.trimmed();
    for (auto &panel : profile->panels)
    {
        panel.identity.id = (panel.host.kind == PanelHostKind::FreeDesktop
            ? QStringLiteral("free-p") : QStringLiteral("panel-p")) + QUuid::createUuid().toString(QUuid::Id128);
        panel.identity.builtIn = false;
    }
    return save(*profile, code);
}

bool ProfileStore::exportProfile(const QString &id, int revision,
    const QString &path, QString *code) const
{
    auto profile = expectedProfile(*this, id, revision, code);
    if (!profile) return false;
    const QFileInfo info(path);
    const QString assets = path + QStringLiteral(".assets");
    if (path.isEmpty() || info.exists() || info.isSymLink() || QFileInfo::exists(assets) ||
        QFileInfo(assets).isSymLink() || !safeRoot(info.absolutePath()))
    { error(code, QStringLiteral("unsafe-or-existing-profile-export-path")); return false; }
    const auto fail = [&assets] { if (QFileInfo::exists(assets)) QDir(assets).removeRecursively(); return false; };
    if (!transferAssets(*profile, {}, assets, false, code)) return fail();
    QSaveFile file(path);
    const auto bytes = QJsonDocument::fromVariant(profile->toVariantMap()).toJson(QJsonDocument::Indented);
    if (bytes.size() > MaximumBytes || !file.open(QIODevice::WriteOnly) ||
        !file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner) ||
        file.write(bytes) != bytes.size() || !file.commit())
    { error(code, QStringLiteral("profile-export-failed")); return fail(); }
    const auto checked = readProfile(path, code);
    if (!checked || *checked != *profile)
    { error(code, QStringLiteral("profile-export-readback-failed")); QFile::remove(path); return fail(); }
    error(code, {});
    return true;
}
}
