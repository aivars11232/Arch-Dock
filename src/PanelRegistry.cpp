#include "PanelRegistry.h"
#include "DockSettings.h"
#include "model/PanelDefinition.h"
#include "model/PanelSettingsSchema.h"
#include "model/SettingsMigration.h"

#include <QDateTime>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QProcess>
#include <QSaveFile>
#include <QScreen>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>
#include <QtGlobal>

#include <limits>

namespace
{
constexpr int kFreeHostOwnershipTokenMaximumLength = 96;
constexpr auto kFreeCreationPending = "pending";
constexpr auto kFreeCreationComplete = "complete";
constexpr auto kFreeCreationRollbackPending = "rollback-pending";
constexpr auto kPanelRecordsKey = "dock/panels";
constexpr auto kPanelLegacyBackupKey = "dock/panelsLegacyV1Backup";

bool isEdge(const QString &edge)
{
    return edge == QStringLiteral("top") ||
           edge == QStringLiteral("bottom") ||
           edge == QStringLiteral("left") ||
           edge == QStringLiteral("right") ||
           edge == QStringLiteral("free");
}

bool isPanelType(const QString &type)
{
    return type == QStringLiteral("empty") ||
           type == QStringLiteral("launcher") ||
           type == QStringLiteral("tasks") ||
           type == QStringLiteral("hybrid");
}

bool isThemeFit(const QString &fit)
{
    return fit == QStringLiteral("cover") ||
           fit == QStringLiteral("contain") ||
           fit == QStringLiteral("stretch") ||
           fit == QStringLiteral("tile");
}

QVariantList builtInThemes()
{
    static const QVariantList themes = [] {
        QFile file(QStringLiteral(":/archdock/data/themes/builtin-themes.json"));
        if (!file.open(QIODevice::ReadOnly))
            return QVariantList{};
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
        const QJsonObject catalog = document.object();
        if (catalog.value(QStringLiteral("format")).toString() !=
                QStringLiteral("org.archdock.theme-catalog") ||
            catalog.value(QStringLiteral("version")).toInt() != 1)
        {
            return QVariantList{};
        }
        return catalog.value(QStringLiteral("themes")).toArray().toVariantList();
    }();
    return themes;
}

QVariantList validatedThemeDefinitions(const QVariantList &themes)
{
    QVariantList result;
    result.reserve(themes.size());
    for (const QVariant &candidate : themes)
    {
        QString errorCode;
        if (!ArchDock::PanelCapabilityResolver::themeProfileFromVariantMap(
                candidate.toMap(), &errorCode).has_value())
        {
            qWarning().noquote()
                << "Theme catalog rejected:"
                << (errorCode.isEmpty()
                        ? QStringLiteral("invalid-capability-input")
                        : errorCode);
            return {};
        }
        result.append(candidate.toMap());
    }
    return result;
}

constexpr int themePackageVersion = 1;
const QString themePackageFormat = QStringLiteral("org.archdock.theme");
const QString themePackageManifestName = QStringLiteral("archdock-theme.json");

struct ThemePackage
{
    QString id;
    QString name;
    QString author;
    QString sourcePath;
    QString manifestPath;
    QString fit = QStringLiteral("cover");
};

QString normalizedThemePackageId(const QString &value)
{
    QString id;
    for (const QChar character : value.trimmed().toLower())
    {
        if (character.isLetterOrNumber())
        {
            id.append(character);
        }
        else if (!id.isEmpty() && !id.endsWith(QLatin1Char('-')))
        {
            id.append(QLatin1Char('-'));
        }
    }
    while (id.endsWith(QLatin1Char('-')))
    {
        id.chop(1);
    }
    return id.isEmpty() ? QStringLiteral("theme") : id.left(64);
}

bool writeThemePackageManifest(const QString &manifestPath,
                               const ThemePackage &package,
                               const QString &assetName,
                               QString *errorMessage)
{
    QSaveFile file(manifestPath);
    if (!file.open(QIODevice::WriteOnly))
    {
        if (errorMessage)
        {
            *errorMessage = file.errorString();
        }
        return false;
    }

    QJsonObject manifest{
        {QStringLiteral("format"), themePackageFormat},
        {QStringLiteral("version"), themePackageVersion},
        {QStringLiteral("id"), package.id},
        {QStringLiteral("name"), package.name},
        {QStringLiteral("surface"), QJsonObject{
             {QStringLiteral("asset"), assetName},
             {QStringLiteral("fit"), package.fit}}}};
    if (!package.author.isEmpty())
    {
        manifest.insert(QStringLiteral("author"), package.author);
    }
    const QByteArray contents = QJsonDocument(manifest).toJson(QJsonDocument::Indented);
    if (file.write(contents) != contents.size() || !file.commit())
    {
        if (errorMessage)
        {
            *errorMessage = file.errorString();
        }
        return false;
    }
    return true;
}

bool readThemePackageManifest(const QFileInfo &manifestFile,
                              ThemePackage *package,
                              QString *errorMessage)
{
    QFile file(manifestFile.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly))
    {
        if (errorMessage)
        {
            *errorMessage = file.errorString();
        }
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    const QJsonObject manifest = document.object();
    if (parseError.error != QJsonParseError::NoError || !document.isObject() ||
        manifest.value(QStringLiteral("format")).toString() != themePackageFormat ||
        manifest.value(QStringLiteral("version")).toInt(-1) != themePackageVersion)
    {
        if (errorMessage)
        {
            *errorMessage = QObject::tr("This theme package has an unsupported format or version.");
        }
        return false;
    }

    const QJsonObject surface = manifest.value(QStringLiteral("surface")).toObject();
    const QString assetReference = QDir::cleanPath(
        surface.value(QStringLiteral("asset")).toString().trimmed());
    if (assetReference.isEmpty() || assetReference == QStringLiteral(".") ||
        assetReference == QStringLiteral("..") || assetReference.startsWith(QStringLiteral("../")) ||
        assetReference.contains(QLatin1Char('\\')) || QDir::isAbsolutePath(assetReference))
    {
        if (errorMessage)
        {
            *errorMessage = QObject::tr("The package surface must be a file inside the package directory.");
        }
        return false;
    }

    const QString packageDirectory = QDir(manifestFile.absolutePath()).canonicalPath();
    const QFileInfo assetFile(QDir(packageDirectory).filePath(assetReference));
    const QString assetPath = assetFile.canonicalFilePath();
    const QString packagePrefix = packageDirectory.endsWith(QLatin1Char('/'))
        ? packageDirectory
        : packageDirectory + QLatin1Char('/');
    if (packageDirectory.isEmpty() || assetPath.isEmpty() || !assetFile.isFile() ||
        !assetPath.startsWith(packagePrefix))
    {
        if (errorMessage)
        {
            *errorMessage = QObject::tr("The package surface is missing or outside the package directory.");
        }
        return false;
    }

    package->id = normalizedThemePackageId(manifest.value(QStringLiteral("id")).toString());
    package->name = manifest.value(QStringLiteral("name")).toString().trimmed();
    if (package->name.isEmpty())
    {
        package->name = package->id;
    }
    package->author = manifest.value(QStringLiteral("author")).toString().trimmed();
    package->sourcePath = assetPath;
    package->fit = surface.value(QStringLiteral("fit")).toString().trimmed().toLower();
    if (!isThemeFit(package->fit))
    {
        package->fit = QStringLiteral("cover");
    }
    return true;
}

bool materializeThemePackage(const QString &panelId,
                             ThemePackage package,
                             ThemePackage *materialized,
                             QString *errorMessage)
{
    const QFileInfo source(package.sourcePath);
    if (!source.isFile())
    {
        if (errorMessage)
        {
            *errorMessage = QObject::tr("The theme surface could not be read.");
        }
        return false;
    }

    package.id = normalizedThemePackageId(package.id);
    if (package.name.isEmpty())
    {
        package.name = source.completeBaseName();
    }
    if (!isThemeFit(package.fit))
    {
        package.fit = QStringLiteral("cover");
    }

    const QString themesDirectory = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation) + QStringLiteral("/themes/") + panelId + QStringLiteral("/packages");
    const QByteArray identity = (package.id + QLatin1Char('|') + source.absoluteFilePath() + QLatin1Char('|') +
        QString::number(QDateTime::currentMSecsSinceEpoch())).toUtf8();
    const QString token = QString::fromLatin1(
        QCryptographicHash::hash(identity, QCryptographicHash::Sha256).toHex().left(12));
    const QString packageDirectory = themesDirectory + QLatin1Char('/') + package.id + QLatin1Char('-') + token;
    if (!QDir().mkpath(packageDirectory))
    {
        if (errorMessage)
        {
            *errorMessage = QObject::tr("The managed theme package directory could not be created.");
        }
        return false;
    }

    const QString sourceName = QStringLiteral("surface") +
        (source.suffix().isEmpty() ? QString{} : QLatin1Char('.') + source.suffix().toLower());
    const QString copiedSource = packageDirectory + QLatin1Char('/') + sourceName;
    if (!QFile::copy(source.absoluteFilePath(), copiedSource))
    {
        QDir(packageDirectory).removeRecursively();
        if (errorMessage)
        {
            *errorMessage = QObject::tr("The theme surface could not be copied into the managed package.");
        }
        return false;
    }

    package.sourcePath = copiedSource;
    package.manifestPath = packageDirectory + QLatin1Char('/') + themePackageManifestName;
    if (!writeThemePackageManifest(package.manifestPath, package, sourceName, errorMessage))
    {
        QDir(packageDirectory).removeRecursively();
        return false;
    }

    *materialized = package;
    return true;
}

QString themeSourceKind(const QString &format)
{
    if (format == QStringLiteral("blend"))
    {
        return QStringLiteral("scene");
    }
    if (format == QStringLiteral("svg") || format == QStringLiteral("svgz"))
    {
        return QStringLiteral("vector");
    }
    if (format == QStringLiteral("pdf") || format == QStringLiteral("psd") || format == QStringLiteral("xcf"))
    {
        return QStringLiteral("document");
    }
    return QStringLiteral("raster");
}

QString suggestedThemeFit(const QSize &sourceSize, const QSize &targetSize)
{
    if (!sourceSize.isValid() || !targetSize.isValid())
    {
        return QStringLiteral("cover");
    }

    const qreal sourceRatio = static_cast<qreal>(sourceSize.width()) / sourceSize.height();
    const qreal targetRatio = static_cast<qreal>(targetSize.width()) / targetSize.height();
    const qreal ratioDifference = qMax(sourceRatio, targetRatio) / qMin(sourceRatio, targetRatio);
    return ratioDifference >= 2.0 ? QStringLiteral("contain") : QStringLiteral("cover");
}

QVariantMap analyzeThemeSource(const ThemePackage &package, const QSize &targetSize)
{
    const QFileInfo source(package.sourcePath);
    const QString format = source.suffix().toLower();
    QVariantMap analysis{
        {QStringLiteral("themeSourceKind"), themeSourceKind(format)},
        {QStringLiteral("themeSourceFormat"), format},
        {QStringLiteral("themeSourceWidth"), 0},
        {QStringLiteral("themeSourceHeight"), 0},
        {QStringLiteral("themeSourceHasAlpha"), false},
        {QStringLiteral("themeSuggestedFit"), QStringLiteral("cover")},
        {QStringLiteral("themePreview"), QString{}},
        {QStringLiteral("themeAnalysisStatus"), QString{}},
        {QStringLiteral("themeConversionTool"), QString{}},
        {QStringLiteral("themeConversionAvailable"), false}};

    if (format == QStringLiteral("blend"))
    {
        const bool blenderAvailable = !QStandardPaths::findExecutable(QStringLiteral("blender")).isEmpty();
        analysis.insert(QStringLiteral("themeConversionTool"), QStringLiteral("Blender"));
        analysis.insert(QStringLiteral("themeConversionAvailable"), blenderAvailable);
        analysis.insert(
            QStringLiteral("themeAnalysisStatus"),
            blenderAvailable
                ? QObject::tr("3D scene saved; Blender rendering is available.")
                : QObject::tr("3D scene saved; Blender is unavailable, so no conversion preview was created."));
        return analysis;
    }

    QImageReader reader(source.absoluteFilePath());
    reader.setAutoTransform(true);
    if (!reader.canRead())
    {
        analysis.insert(
            QStringLiteral("themeAnalysisStatus"),
            QObject::tr("Source metadata is unavailable; rendering will use available import tools."));
        return analysis;
    }

    const QSize sourceSize = reader.size();
    if (!sourceSize.isValid() || sourceSize.width() < 1 || sourceSize.height() < 1)
    {
        analysis.insert(
            QStringLiteral("themeAnalysisStatus"),
            QObject::tr("Source dimensions are unavailable; a preview was not created."));
        return analysis;
    }

    analysis.insert(QStringLiteral("themeSourceWidth"), sourceSize.width());
    analysis.insert(QStringLiteral("themeSourceHeight"), sourceSize.height());
    analysis.insert(QStringLiteral("themeSuggestedFit"), suggestedThemeFit(sourceSize, targetSize));

    constexpr qint64 previewPixelLimit = 16LL * 1024 * 1024;
    const qint64 sourcePixels = static_cast<qint64>(sourceSize.width()) * sourceSize.height();
    if (sourcePixels > previewPixelLimit)
    {
        analysis.insert(
            QStringLiteral("themeAnalysisStatus"),
            QObject::tr("Source dimensions recorded; preview skipped to limit memory use."));
        return analysis;
    }

    QSize previewSize = sourceSize;
    const QSize previewBounds(320, 180);
    if (previewSize.width() > previewBounds.width() || previewSize.height() > previewBounds.height())
    {
        previewSize = previewSize.scaled(previewBounds, Qt::KeepAspectRatio);
        reader.setScaledSize(previewSize);
    }
    QImage preview = reader.read();
    if (preview.isNull())
    {
        analysis.insert(
            QStringLiteral("themeAnalysisStatus"),
            QObject::tr("Source dimensions recorded; preview decoding was unavailable."));
        return analysis;
    }
    if (preview.size() != previewSize)
    {
        preview = preview.scaled(previewSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    const QString previewPath = QFileInfo(package.manifestPath).absolutePath() + QStringLiteral("/preview.png");
    if (preview.save(previewPath, "PNG"))
    {
        analysis.insert(QStringLiteral("themePreview"), QUrl::fromLocalFile(previewPath).toString());
        analysis.insert(QStringLiteral("themeSourceHasAlpha"), preview.hasAlphaChannel());
        analysis.insert(
            QStringLiteral("themeAnalysisStatus"),
            QObject::tr("Analyzed %1 x %2 source; preview ready.")
                .arg(sourceSize.width())
                .arg(sourceSize.height()));
    }
    else
    {
        analysis.insert(
            QStringLiteral("themeAnalysisStatus"),
            QObject::tr("Source dimensions recorded; preview could not be written."));
    }
    return analysis;
}

bool migrateLegacyThemePackage(QVariantMap *panel)
{
    const QUrl currentManifest(panel->value(QStringLiteral("themePackageManifest")).toString());
    if (panel->value(QStringLiteral("themePackageFormat")).toString() == themePackageFormat &&
        panel->value(QStringLiteral("themePackageVersion")).toInt() == themePackageVersion &&
        currentManifest.isLocalFile() && QFileInfo::exists(currentManifest.toLocalFile()))
    {
        return false;
    }

    const QUrl sourceUrl(panel->value(QStringLiteral("themeSource")).toString());
    const QFileInfo source(sourceUrl.toLocalFile());
    ThemePackage package;
    if (sourceUrl.isLocalFile() && source.isFile())
    {
        const QString fingerprint = QString::fromLatin1(
            QCryptographicHash::hash(source.absoluteFilePath().toUtf8(), QCryptographicHash::Sha256).toHex().left(12));
        package.id = QStringLiteral("legacy-") + fingerprint;
        package.name = source.completeBaseName();
        package.sourcePath = source.absoluteFilePath();
        package.fit = panel->value(QStringLiteral("themeFit"), QStringLiteral("cover")).toString();
        if (!isThemeFit(package.fit))
        {
            package.fit = QStringLiteral("cover");
        }
        package.manifestPath = source.absolutePath() + QLatin1Char('/') + package.id +
            QStringLiteral(".archdock-theme.json");

        QString errorMessage;
        if (writeThemePackageManifest(package.manifestPath, package, source.fileName(), &errorMessage))
        {
            panel->insert(QStringLiteral("themePackageFormat"), themePackageFormat);
            panel->insert(QStringLiteral("themePackageVersion"), themePackageVersion);
            panel->insert(QStringLiteral("themePackageId"), package.id);
            panel->insert(QStringLiteral("themePackageName"), package.name);
            panel->insert(QStringLiteral("themePackageAuthor"), QString{});
            panel->insert(QStringLiteral("themePackageManifest"), QUrl::fromLocalFile(package.manifestPath).toString());
            const QSize targetSize(
                panel->value(QStringLiteral("width"), 720).toInt(),
                panel->value(QStringLiteral("height"), 76).toInt());
            const QVariantMap analysis = analyzeThemeSource(package, targetSize);
            for (auto iterator = analysis.cbegin(); iterator != analysis.cend(); ++iterator)
            {
                panel->insert(iterator.key(), iterator.value());
            }
            return true;
        }
    }

    bool changed = false;
    const auto setDefault = [panel, &changed](const QString &key, const QVariant &value)
    {
        if (!panel->contains(key))
        {
            panel->insert(key, value);
            changed = true;
        }
    };
    setDefault(QStringLiteral("themePackageFormat"), QString{});
    setDefault(QStringLiteral("themePackageVersion"), 0);
    setDefault(QStringLiteral("themePackageId"), QString{});
    setDefault(QStringLiteral("themePackageName"), QString{});
    setDefault(QStringLiteral("themePackageAuthor"), QString{});
    setDefault(QStringLiteral("themePackageManifest"), QString{});
    setDefault(QStringLiteral("themeSourceKind"), QString{});
    setDefault(QStringLiteral("themeSourceFormat"), QString{});
    setDefault(QStringLiteral("themeSourceWidth"), 0);
    setDefault(QStringLiteral("themeSourceHeight"), 0);
    setDefault(QStringLiteral("themeSourceHasAlpha"), false);
    setDefault(QStringLiteral("themeSuggestedFit"), QStringLiteral("cover"));
    setDefault(QStringLiteral("themePreview"), QString{});
    setDefault(QStringLiteral("themeAnalysisStatus"), QObject::tr("No source selected."));
    setDefault(QStringLiteral("themeConversionTool"), QString{});
    setDefault(QStringLiteral("themeConversionAvailable"), false);
    return changed;
}

bool isVisibilityMode(const QString &mode)
{
    return mode == QStringLiteral("always") ||
           mode == QStringLiteral("auto-hide") ||
           mode == QStringLiteral("dodge") ||
           mode == QStringLiteral("cover");
}

int normalizedFreeHostId(const QVariant &value)
{
    bool ok = false;
    const qlonglong candidate = value.toLongLong(&ok);
    return ok && candidate >= 0 && candidate <= std::numeric_limits<int>::max()
        ? static_cast<int>(candidate)
        : -1;
}

bool hasFreeHostIdData(const QVariant &value)
{
    if (!value.isValid() || value.isNull() || value.toString().trimmed().isEmpty())
    {
        return false;
    }

    bool ok = false;
    const qlonglong candidate = value.toLongLong(&ok);
    return !ok || candidate != -1;
}

PanelRegistry::FreeHostState freeHostStateFromString(const QString &value)
{
    if (value == QStringLiteral("unhosted"))
    {
        return PanelRegistry::FreeHostState::Unhosted;
    }
    if (value == QStringLiteral("hosted-owned"))
    {
        return PanelRegistry::FreeHostState::HostedOwned;
    }
    if (value == QStringLiteral("detached"))
    {
        return PanelRegistry::FreeHostState::Detached;
    }
    return PanelRegistry::FreeHostState::HostedStale;
}

bool normalizeFreeHostRecord(QVariantMap *panel)
{
    if (!panel || panel->value(QStringLiteral("edge")).toString() != QStringLiteral("free"))
    {
        return false;
    }

    const QVariant rawContainmentId = panel->value(QStringLiteral("freeDesktopContainmentId"), -1);
    const QVariant rawAppletId = panel->value(QStringLiteral("freeDockAppletId"), -1);
    const int containmentId = normalizedFreeHostId(rawContainmentId);
    const int appletId = normalizedFreeHostId(rawAppletId);
    const QString ownershipToken = panel->value(
        QStringLiteral("freeOwnershipToken")).toString().trimmed();
    const QString requestedHostMode = panel->value(
        QStringLiteral("freeHostMode")).toString().trimmed().toLower();
    const QString hostMode = QStringLiteral("desktop");

    const QString requestedState = panel->value(
        QStringLiteral("freeHostState")).toString().trimmed().toLower();
    const bool declaredDesktopMode = requestedHostMode == QStringLiteral("desktop");
    const bool defaultableDesktopMode = requestedHostMode.isEmpty() || declaredDesktopMode;
    const bool validToken = !ownershipToken.isEmpty() &&
        ownershipToken.size() <= kFreeHostOwnershipTokenMaximumLength;
    const bool completeAssociation = containmentId >= 0 && appletId >= 0 &&
        validToken && declaredDesktopMode;
    const bool hasAssociationData = hasFreeHostIdData(rawContainmentId) ||
        hasFreeHostIdData(rawAppletId) || !ownershipToken.isEmpty() ||
        !defaultableDesktopMode;

    QString normalizedState;
    if (requestedState == QStringLiteral("hosted-owned"))
    {
        normalizedState = completeAssociation
            ? QStringLiteral("hosted-owned")
            : QStringLiteral("hosted-stale");
    }
    else if (requestedState == QStringLiteral("hosted-stale"))
    {
        normalizedState = QStringLiteral("hosted-stale");
    }
    else if (requestedState == QStringLiteral("detached"))
    {
        normalizedState = !hasAssociationData && defaultableDesktopMode
            ? QStringLiteral("detached")
            : QStringLiteral("hosted-stale");
    }
    else if (requestedState.isEmpty() || requestedState == QStringLiteral("unhosted"))
    {
        normalizedState = !hasAssociationData && defaultableDesktopMode
            ? QStringLiteral("unhosted")
            : QStringLiteral("hosted-stale");
    }
    else
    {
        normalizedState = QStringLiteral("hosted-stale");
    }

    bool changed = false;
    const auto setValue = [panel, &changed](const QString &key, const QVariant &value)
    {
        if (!panel->contains(key) || panel->value(key) != value)
        {
            panel->insert(key, value);
            changed = true;
        }
    };
    setValue(QStringLiteral("freeDesktopContainmentId"), containmentId);
    setValue(QStringLiteral("freeDockAppletId"), appletId);
    setValue(QStringLiteral("freeOwnershipToken"), ownershipToken);
    setValue(QStringLiteral("freeHostMode"), hostMode);
    setValue(QStringLiteral("freeHostState"), normalizedState);
    setValue(QStringLiteral("screen"), qMax(0, panel->value(QStringLiteral("screen")).toInt()));
    setValue(QStringLiteral("screenId"), panel->value(QStringLiteral("screenId")).toString().trimmed());
    return changed;
}

}

PanelRegistry::PanelRegistry(QObject *parent)
    : PanelRegistry(builtInThemes(), parent)
{
}

PanelRegistry::PanelRegistry(const QVariantList &themeDefinitions,
                             QObject *parent)
    : QObject(parent),
      m_themeDefinitions(validatedThemeDefinitions(themeDefinitions))
{
    load();
}

PanelRegistry::~PanelRegistry()
{
    const auto processes = m_renderProcesses;
    m_renderProcesses.clear();
    m_activeRenders.clear();
    m_pendingRenders.clear();
    for (const QPointer<QProcess> &process : processes)
    {
        if (!process)
        {
            continue;
        }
        QObject::disconnect(process.data(), nullptr, this, nullptr);
        if (process->state() != QProcess::NotRunning)
        {
            process->kill();
            process->waitForFinished(1000);
        }
    }
}

QStringList PanelRegistry::panelIds() const
{
    QStringList ids;
    ids.reserve(m_panels.size());
    for (const QVariantMap &panel : m_panels)
    {
        ids.append(panel.value(QStringLiteral("id")).toString());
    }
    return ids;
}

QString PanelRegistry::activePanelId() const
{
    return m_activePanelId;
}

int PanelRegistry::revision() const
{
    return m_revision;
}

QString PanelRegistry::migrationDiagnostic() const
{
    return m_migrationDiagnostic;
}

QVariant PanelRegistry::panelValue(const QString &panelId, const QString &key) const
{
    const QVariantMap *panel = record(panelId);
    return panel ? panel->value(key) : QVariant{};
}

QVariantMap PanelRegistry::panelSnapshot(const QString &panelId) const
{
    const QVariantMap *panel = record(panelId);
    return panel ? *panel : QVariantMap{};
}

QString PanelRegistry::panelName(const QString &panelId) const
{
    const QVariantMap *panel = record(panelId);
    return panel ? panel->value(QStringLiteral("name")).toString() : QString{};
}

bool PanelRegistry::isBuiltIn(const QString &panelId) const
{
    const QVariantMap *panel = record(panelId);
    return panel && panel->value(QStringLiteral("builtIn")).toBool();
}

void PanelRegistry::setPanelValue(const QString &panelId, const QString &key, const QVariant &value)
{
    setPanelValues(panelId, {{key, value}});
}

void PanelRegistry::updatePanel(const QString &panelId, const QVariantMap &values)
{
    setPanelValues(panelId, values);
}

bool PanelRegistry::updatePanelChecked(const QString &panelId, const QVariantMap &values)
{
    return setPanelValuesChecked(panelId, values);
}

std::optional<ArchDock::PanelDefinition> PanelRegistry::panelDefinition(
    const QString &panelId,
    QString *errorMessage) const
{
    const QVariantMap *panel = record(panelId);
    if (!panel)
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("panel not found: %1").arg(panelId);
        }
        return std::nullopt;
    }
    return ArchDock::PanelDefinition::fromLegacyMap(*panel, errorMessage);
}

std::optional<ArchDock::ThemeCapabilityProfile>
PanelRegistry::themeCapabilityProfile(
    const ArchDock::PanelDefinition &definition,
    QString *errorCode) const
{
    const bool hasManagedArtwork =
        !definition.surface.themeAsset.trimmed().isEmpty() ||
        !definition.surface.themeSource.trimmed().isEmpty() ||
        !definition.surface.themePackageManifest.trimmed().isEmpty();
    if (hasManagedArtwork)
    {
        if (errorCode)
        {
            errorCode->clear();
        }
        return ArchDock::PanelCapabilityResolver::legacyThemeProfile(definition);
    }

    const QString themeId = !definition.surface.completeThemeId.trimmed().isEmpty()
        ? definition.surface.completeThemeId.trimmed()
        : definition.surface.panelThemeId.trimmed();
    if (themeId.isEmpty())
    {
        if (errorCode)
        {
            errorCode->clear();
        }
        return ArchDock::PanelCapabilityResolver::proceduralThemeProfile();
    }

    for (const QVariant &candidate : m_themeDefinitions)
    {
        const QVariantMap theme = candidate.toMap();
        if (theme.value(QStringLiteral("id")).toString() != themeId)
        {
            continue;
        }
        return ArchDock::PanelCapabilityResolver::themeProfileFromVariantMap(
            theme, errorCode);
    }

    if (errorCode)
    {
        *errorCode = QStringLiteral("theme-capability-undeclared");
    }
    return std::nullopt;
}

ArchDock::CapabilityResolution PanelRegistry::resolvePanelCapabilities(
    const ArchDock::PanelDefinition &definition,
    QString *errorCode) const
{
    const ArchDock::HostCapabilityProfile host =
        ArchDock::PanelCapabilityResolver::productionHostProfile(
            definition.host.kind);
    const std::optional<ArchDock::ThemeCapabilityProfile> theme =
        themeCapabilityProfile(definition, errorCode);
    if (!theme.has_value())
    {
        ArchDock::CapabilityResolution unavailable;
        unavailable.hostProfileId = host.id;
        unavailable.themeId = !definition.surface.completeThemeId.trimmed().isEmpty()
            ? definition.surface.completeThemeId.trimmed()
            : definition.surface.panelThemeId.trimmed();
        unavailable.reason = ArchDock::CapabilityReasonCode::InvalidCapabilityInput;
        return unavailable;
    }

    if (errorCode)
    {
        errorCode->clear();
    }
    return ArchDock::PanelCapabilityResolver::resolve(
        definition,
        host,
        *theme,
        ArchDock::PanelCapabilityResolver::productionRenderers(),
        ArchDock::PanelCapabilityResolver::productionPlatform());
}

bool PanelRegistry::persistPanelSettingsTransaction(
    const ArchDock::PanelSettingsTransactionDraft &draft,
    QString *errorMessage)
{
    qsizetype panelIndex = -1;
    for (qsizetype index = 0; index < m_panels.size(); ++index)
    {
        if (m_panels.at(index).value(QStringLiteral("id")).toString() ==
            draft.previousPanel.identity.id)
        {
            panelIndex = index;
            break;
        }
    }
    if (panelIndex < 0)
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("panel disappeared before transaction commit");
        }
        return false;
    }

    QString currentError;
    const std::optional<ArchDock::PanelDefinition> current =
        ArchDock::PanelDefinition::fromLegacyMap(m_panels.at(panelIndex), &currentError);
    if (!current.has_value() || *current != draft.previousPanel)
    {
        if (errorMessage)
        {
            *errorMessage = current.has_value()
                ? QStringLiteral("panel changed before transaction commit")
                : currentError;
        }
        return false;
    }
    if (draft.candidatePanel.settingsRevision !=
            draft.previousPanel.settingsRevision + 1 ||
        draft.candidatePanel.identity.id != draft.previousPanel.identity.id)
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("candidate revision is not the next panel revision");
        }
        return false;
    }

    QList<QVariantMap> stagedPanels = m_panels;
    stagedPanels[panelIndex] = draft.candidatePanel.toLegacyMap();
    if (!persistPanelState(stagedPanels, draft.candidateGlobals, errorMessage))
    {
        return false;
    }
    m_panels = std::move(stagedPanels);
    return true;
}

bool PanelRegistry::rollbackPanelSettingsTransaction(
    const ArchDock::PanelSettingsTransactionDraft &draft,
    quint64 committedRevision,
    quint64 *rollbackRevision,
    QString *errorMessage)
{
    if (rollbackRevision)
    {
        *rollbackRevision = 0;
    }
    if (committedRevision == std::numeric_limits<quint64>::max())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("panel revision cannot advance for rollback");
        }
        return false;
    }

    qsizetype panelIndex = -1;
    for (qsizetype index = 0; index < m_panels.size(); ++index)
    {
        if (m_panels.at(index).value(QStringLiteral("id")).toString() ==
            draft.previousPanel.identity.id)
        {
            panelIndex = index;
            break;
        }
    }
    if (panelIndex < 0)
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("panel disappeared before transaction rollback");
        }
        return false;
    }

    QString currentError;
    const std::optional<ArchDock::PanelDefinition> current =
        ArchDock::PanelDefinition::fromLegacyMap(m_panels.at(panelIndex), &currentError);
    if (!current.has_value() || current->settingsRevision != committedRevision)
    {
        if (errorMessage)
        {
            *errorMessage = current.has_value()
                ? QStringLiteral("panel changed before transaction rollback")
                : currentError;
        }
        return false;
    }

    ArchDock::PanelDefinition restoredPanel = draft.previousPanel;
    restoredPanel.settingsRevision = committedRevision + 1;
    QList<QVariantMap> stagedPanels = m_panels;
    stagedPanels[panelIndex] = restoredPanel.toLegacyMap();
    if (!persistPanelState(stagedPanels, draft.previousGlobals, errorMessage))
    {
        return false;
    }
    m_panels = std::move(stagedPanels);
    if (rollbackRevision)
    {
        *rollbackRevision = restoredPanel.settingsRevision;
    }
    return true;
}

void PanelRegistry::notifyPanelSettingsTransactionAdopted(
    bool nativeTopologyChanged)
{
    changed(nativeTopologyChanged);
}

std::optional<PanelRegistry::FreeHostAssociation> PanelRegistry::freeHostAssociation(
    const QString &panelId) const
{
    const QVariantMap *panel = record(panelId);
    if (!panel || panel->value(QStringLiteral("edge")).toString() != QStringLiteral("free"))
    {
        return std::nullopt;
    }

    FreeHostAssociation association;
    association.desktopContainmentId = panel->value(
        QStringLiteral("freeDesktopContainmentId"), -1).toInt();
    association.dockAppletId = panel->value(QStringLiteral("freeDockAppletId"), -1).toInt();
    association.ownershipToken = panel->value(
        QStringLiteral("freeOwnershipToken")).toString();
    association.screenIndex = panel->value(QStringLiteral("screen")).toInt();
    association.screenId = panel->value(QStringLiteral("screenId")).toString();
    association.hostMode = panel->value(QStringLiteral("freeHostMode")).toString();
    association.state = freeHostStateFromString(
        panel->value(QStringLiteral("freeHostState")).toString());
    return association;
}

QString PanelRegistry::beginFreePanelCreation(const QString &ownershipToken)
{
    const QString normalizedToken = ownershipToken.trimmed();
    if (normalizedToken.isEmpty() ||
        normalizedToken.size() > kFreeHostOwnershipTokenMaximumLength)
    {
        return {};
    }

    int sequence = 1;
    QString id;
    do
    {
        id = QStringLiteral("free-%1").arg(sequence++);
    } while (record(id));

    QVariantMap panel = makePanel(
        id,
        tr("Free panel %1").arg(sequence - 1),
        QStringLiteral("bottom"),
        false);
    panel.insert(QStringLiteral("edge"), QStringLiteral("free"));
    panel.insert(QStringLiteral("visible"), true);
    panel.insert(QStringLiteral("type"), QStringLiteral("empty"));
    panel.insert(QStringLiteral("contentAppIds"), QStringList{});
    panel.insert(QStringLiteral("contentUrls"), QStringList{});
    panel.insert(QStringLiteral("layout"), QStringLiteral("circular"));
    panel.insert(QStringLiteral("width"), 420);
    panel.insert(QStringLiteral("height"), 420);
    panel.insert(QStringLiteral("layoutRadius"), 145);
    panel.insert(QStringLiteral("x"), 240);
    panel.insert(QStringLiteral("y"), 180);
    panel.insert(QStringLiteral("freeOwnershipToken"), normalizedToken);
    panel.insert(QStringLiteral("freeCreationState"),
                 QString::fromLatin1(kFreeCreationPending));
    panel.insert(QStringLiteral("freeCreationError"), QString{});
    panel.insert(QStringLiteral("freeRollbackError"), QString{});
    (void)normalizeFreeHostRecord(&panel);

    const QString previousActivePanelId = m_activePanelId;
    m_panels.append(panel);
    m_activePanelId = id;
    if (!saveChecked())
    {
        m_panels.removeLast();
        m_activePanelId = previousActivePanelId;
        return {};
    }

    if (m_activePanelId != previousActivePanelId)
    {
        emit activePanelIdChanged();
    }
    changed(false);
    return id;
}

bool PanelRegistry::commitVerifiedFreeHostAssociation(
    const QString &panelId,
    int desktopContainmentId,
    int dockAppletId,
    const QString &ownershipToken,
    int screenIndex,
    const QString &screenId,
    const QString &hostMode)
{
    const QVariantMap *panel = record(panelId);
    const QString normalizedToken = ownershipToken.trimmed();
    const QString normalizedMode = hostMode.trimmed().toLower();
    const QString creationState = panel
        ? panel->value(QStringLiteral("freeCreationState")).toString()
        : QString{};
    const QString reservedToken = panel
        ? panel->value(QStringLiteral("freeOwnershipToken")).toString()
        : QString{};
    if (!panel || panel->value(QStringLiteral("edge")).toString() != QStringLiteral("free") ||
        desktopContainmentId < 0 || dockAppletId < 0 || screenIndex < 0 ||
        normalizedToken.isEmpty() ||
        normalizedToken.size() > kFreeHostOwnershipTokenMaximumLength ||
        normalizedMode != QStringLiteral("desktop") ||
        (!creationState.isEmpty() && creationState != QStringLiteral("idle") &&
         (creationState != QLatin1String(kFreeCreationPending) ||
          reservedToken != normalizedToken)))
    {
        return false;
    }

    return setPanelValuesChecked(
        panelId,
        {{QStringLiteral("freeDesktopContainmentId"), desktopContainmentId},
         {QStringLiteral("freeDockAppletId"), dockAppletId},
         {QStringLiteral("freeOwnershipToken"), normalizedToken},
         {QStringLiteral("screen"), screenIndex},
         {QStringLiteral("screenId"), screenId.trimmed()},
         {QStringLiteral("freeHostMode"), normalizedMode},
         {QStringLiteral("freeHostState"), QStringLiteral("hosted-owned")}});
}

bool PanelRegistry::completeFreePanelCreation(
    const QString &panelId,
    const QString &ownershipToken)
{
    const QVariantMap *panel = record(panelId);
    const QString normalizedToken = ownershipToken.trimmed();
    const auto association = freeHostAssociation(panelId);
    if (!panel || panel->value(QStringLiteral("builtIn")).toBool() ||
        panel->value(QStringLiteral("freeCreationState")).toString() !=
            QLatin1String(kFreeCreationPending) ||
        normalizedToken.isEmpty() ||
        panel->value(QStringLiteral("freeOwnershipToken")).toString() != normalizedToken ||
        !association.has_value() || association->state != FreeHostState::HostedOwned ||
        association->desktopContainmentId < 0 || association->dockAppletId < 0 ||
        association->ownershipToken != normalizedToken)
    {
        return false;
    }

    return setPanelValuesChecked(
        panelId,
        {{QStringLiteral("freeCreationState"),
          QString::fromLatin1(kFreeCreationComplete)},
         {QStringLiteral("freeCreationError"), QString{}},
         {QStringLiteral("freeRollbackError"), QString{}}});
}

bool PanelRegistry::recordRecoverableFreePanelCreation(
    const QString &panelId,
    int desktopContainmentId,
    int dockAppletId,
    const QString &ownershipToken,
    int screenIndex,
    const QString &screenId,
    const QString &creationError,
    const QString &rollbackError)
{
    const QVariantMap *panel = record(panelId);
    const QString normalizedToken = ownershipToken.trimmed();
    const QString creationState = panel
        ? panel->value(QStringLiteral("freeCreationState")).toString()
        : QString{};
    if (!panel || panel->value(QStringLiteral("builtIn")).toBool() ||
        panel->value(QStringLiteral("edge")).toString() != QStringLiteral("free") ||
        (creationState != QLatin1String(kFreeCreationPending) &&
         creationState != QLatin1String(kFreeCreationRollbackPending)) ||
        normalizedToken.isEmpty() ||
        panel->value(QStringLiteral("freeOwnershipToken")).toString() != normalizedToken ||
        creationError.trimmed().isEmpty())
    {
        return false;
    }

    const int retainedContainmentId = desktopContainmentId >= 0
        ? desktopContainmentId
        : panel->value(QStringLiteral("freeDesktopContainmentId"), -1).toInt();
    const int retainedAppletId = dockAppletId >= 0
        ? dockAppletId
        : panel->value(QStringLiteral("freeDockAppletId"), -1).toInt();
    const int retainedScreenIndex = screenIndex >= 0
        ? screenIndex
        : panel->value(QStringLiteral("screen"), 0).toInt();
    const QString retainedScreenId = screenId.trimmed().isEmpty()
        ? panel->value(QStringLiteral("screenId")).toString()
        : screenId.trimmed();

    return setPanelValuesChecked(
        panelId,
        {{QStringLiteral("freeDesktopContainmentId"), retainedContainmentId},
         {QStringLiteral("freeDockAppletId"), retainedAppletId},
         {QStringLiteral("freeOwnershipToken"), normalizedToken},
         {QStringLiteral("screen"), retainedScreenIndex},
         {QStringLiteral("screenId"), retainedScreenId},
         {QStringLiteral("freeHostMode"), QStringLiteral("desktop")},
         {QStringLiteral("freeHostState"), QStringLiteral("hosted-stale")},
         {QStringLiteral("freeCreationState"),
          QString::fromLatin1(kFreeCreationRollbackPending)},
         {QStringLiteral("freeCreationError"), creationError.trimmed()},
         {QStringLiteral("freeRollbackError"), rollbackError.trimmed()}});
}

bool PanelRegistry::discardFreePanelCreation(
    const QString &panelId,
    const QString &ownershipToken)
{
    const QString normalizedToken = ownershipToken.trimmed();
    for (int index = 0; index < m_panels.size(); ++index)
    {
        const QVariantMap panel = m_panels.at(index);
        const QString creationState = panel.value(
            QStringLiteral("freeCreationState")).toString();
        if (panel.value(QStringLiteral("id")).toString() != panelId ||
            panel.value(QStringLiteral("builtIn")).toBool() ||
            panel.value(QStringLiteral("edge")).toString() != QStringLiteral("free") ||
            normalizedToken.isEmpty() ||
            panel.value(QStringLiteral("freeOwnershipToken")).toString() != normalizedToken ||
            (creationState != QLatin1String(kFreeCreationPending) &&
             creationState != QLatin1String(kFreeCreationRollbackPending)))
        {
            continue;
        }

        const QString previousActivePanelId = m_activePanelId;
        m_panels.removeAt(index);
        if (m_activePanelId == panelId)
        {
            m_activePanelId = QStringLiteral("bottom");
        }
        if (!saveChecked())
        {
            m_panels.insert(index, panel);
            m_activePanelId = previousActivePanelId;
            return false;
        }

        if (m_activePanelId != previousActivePanelId)
        {
            emit activePanelIdChanged();
        }
        changed(false);
        return true;
    }
    return false;
}

bool PanelRegistry::rebindRecoveredFreeHostAssociation(
    const QString &panelId,
    int desktopContainmentId,
    int dockAppletId,
    const QString &ownershipToken,
    int screenIndex,
    const QString &screenId)
{
    const QVariantMap *panel = record(panelId);
    const QString normalizedToken = ownershipToken.trimmed();
    if (!panel || panel->value(QStringLiteral("builtIn")).toBool() ||
        panel->value(QStringLiteral("edge")).toString() != QStringLiteral("free") ||
        desktopContainmentId < 0 || dockAppletId < 0 || screenIndex < 0 ||
        normalizedToken.isEmpty() ||
        panel->value(QStringLiteral("freeOwnershipToken")).toString().trimmed() !=
            normalizedToken)
    {
        return false;
    }

    return setPanelValuesChecked(
        panelId,
        {{QStringLiteral("freeDesktopContainmentId"), desktopContainmentId},
         {QStringLiteral("freeDockAppletId"), dockAppletId},
         {QStringLiteral("freeOwnershipToken"), normalizedToken},
         {QStringLiteral("screen"), screenIndex},
         {QStringLiteral("screenId"), screenId.trimmed()},
         {QStringLiteral("freeHostMode"), QStringLiteral("desktop")},
         {QStringLiteral("freeHostState"), QStringLiteral("hosted-owned")},
         {QStringLiteral("freeRecoveryError"), QString{}}});
}

bool PanelRegistry::detachFreeHostAssociation(
    const QString &panelId,
    const QString &ownershipToken,
    const QString &recoveryError)
{
    const QVariantMap *panel = record(panelId);
    const QString normalizedToken = ownershipToken.trimmed();
    if (!panel || panel->value(QStringLiteral("builtIn")).toBool() ||
        panel->value(QStringLiteral("edge")).toString() != QStringLiteral("free") ||
        normalizedToken.isEmpty() ||
        panel->value(QStringLiteral("freeOwnershipToken")).toString().trimmed() !=
            normalizedToken)
    {
        return false;
    }

    return setPanelValuesChecked(
        panelId,
        {{QStringLiteral("freeDesktopContainmentId"), -1},
         {QStringLiteral("freeDockAppletId"), -1},
         {QStringLiteral("freeOwnershipToken"), QString{}},
         {QStringLiteral("freeHostMode"), QStringLiteral("desktop")},
         {QStringLiteral("freeHostState"), QStringLiteral("detached")},
         {QStringLiteral("freeRecoveryError"), recoveryError.trimmed()}});
}

bool PanelRegistry::recordFreeHostRecoveryError(
    const QString &panelId,
    const QString &ownershipToken,
    const QString &errorCode)
{
    const QVariantMap *panel = record(panelId);
    const QString normalizedToken = ownershipToken.trimmed();
    const QString normalizedError = errorCode.trimmed();
    if (!panel || panel->value(QStringLiteral("builtIn")).toBool() ||
        panel->value(QStringLiteral("edge")).toString() != QStringLiteral("free") ||
        normalizedToken.isEmpty() || normalizedError.isEmpty() ||
        panel->value(QStringLiteral("freeOwnershipToken")).toString().trimmed() !=
            normalizedToken)
    {
        return false;
    }

    return setPanelValuesChecked(
        panelId,
        {{QStringLiteral("freeHostState"), QStringLiteral("hosted-stale")},
         {QStringLiteral("freeRecoveryError"), normalizedError}});
}

bool PanelRegistry::removeDetachedFreePanel(const QString &panelId)
{
    for (int index = 0; index < m_panels.size(); ++index)
    {
        const QVariantMap panel = m_panels.at(index);
        if (panel.value(QStringLiteral("id")).toString() != panelId)
        {
            continue;
        }

        const auto association = freeHostAssociation(panelId);
        if (panel.value(QStringLiteral("builtIn")).toBool() ||
            panel.value(QStringLiteral("edge")).toString() != QStringLiteral("free") ||
            !association.has_value() || association->state != FreeHostState::Detached ||
            association->desktopContainmentId >= 0 || association->dockAppletId >= 0 ||
            !association->ownershipToken.trimmed().isEmpty())
        {
            return false;
        }

        const QString previousActivePanelId = m_activePanelId;
        m_panels.removeAt(index);
        if (m_activePanelId == panelId)
        {
            m_activePanelId = QStringLiteral("bottom");
        }
        if (!saveChecked())
        {
            m_panels.insert(index, panel);
            m_activePanelId = previousActivePanelId;
            return false;
        }

        if (m_activePanelId != previousActivePanelId)
        {
            emit activePanelIdChanged();
        }
        changed(false);
        return true;
    }
    return false;
}

bool PanelRegistry::commitVerifiedNativePanelAssociation(
    const QString &panelId,
    int containmentId,
    int dockAppletId,
    const QString &ownershipToken)
{
    const QVariantMap *panel = record(panelId);
    const QString normalizedToken = ownershipToken.trimmed();
    if (!panel || panel->value(QStringLiteral("edge")).toString() == QStringLiteral("free") ||
        containmentId < 0 || normalizedToken.isEmpty())
    {
        return false;
    }

    const QString type = panel->value(QStringLiteral("type")).toString();
    if (!isPanelType(type) ||
        (type == QStringLiteral("empty") ? dockAppletId != -1 : dockAppletId < 0))
    {
        return false;
    }

    return setPanelValuesChecked(
        panelId,
        {{QStringLiteral("nativePanelId"), containmentId},
         {QStringLiteral("nativeControlAppletId"), -1},
         {QStringLiteral("nativeDockAppletId"), dockAppletId},
         {QStringLiteral("nativeOwnershipToken"), normalizedToken},
         {QStringLiteral("nativeRecoveryState"), QStringLiteral("ready")},
         {QStringLiteral("nativeRecoveryError"), QString{}}});
}

bool PanelRegistry::rebindRecoveredNativePanelAssociation(
    const QString &panelId,
    int containmentId,
    int dockAppletId,
    const QString &ownershipToken)
{
    const QVariantMap *panel = record(panelId);
    const QString normalizedToken = ownershipToken.trimmed();
    if (!panel || panel->value(QStringLiteral("edge")).toString() == QStringLiteral("free") ||
        containmentId < 0 || dockAppletId < -1 || normalizedToken.isEmpty() ||
        panel->value(QStringLiteral("nativeOwnershipToken")).toString().trimmed() != normalizedToken)
    {
        return false;
    }

    const QString type = panel->value(QStringLiteral("type")).toString();
    if (!isPanelType(type) || (type == QStringLiteral("empty") && dockAppletId != -1))
    {
        return false;
    }

    const bool rendererReady = type == QStringLiteral("empty") || dockAppletId >= 0;
    return setPanelValuesChecked(
        panelId,
        {{QStringLiteral("nativePanelId"), containmentId},
         {QStringLiteral("nativeControlAppletId"), -1},
         {QStringLiteral("nativeDockAppletId"), dockAppletId},
         {QStringLiteral("nativeOwnershipToken"), normalizedToken},
         {QStringLiteral("nativeRecoveryState"), rendererReady
              ? QStringLiteral("ready")
              : QStringLiteral("recovering")},
         {QStringLiteral("nativeRecoveryError"), QString{}}});
}

bool PanelRegistry::detachMissingNativePanelAssociation(
    const QString &panelId,
    const QString &ownershipToken,
    bool recordVisible)
{
    const QVariantMap *panel = record(panelId);
    const QString normalizedToken = ownershipToken.trimmed();
    if (!panel || panel->value(QStringLiteral("edge")).toString() == QStringLiteral("free") ||
        normalizedToken.isEmpty() ||
        panel->value(QStringLiteral("nativeOwnershipToken")).toString().trimmed() != normalizedToken)
    {
        return false;
    }

    return setPanelValuesChecked(
        panelId,
        {{QStringLiteral("nativePanelId"), -1},
         {QStringLiteral("nativeControlAppletId"), -1},
         {QStringLiteral("nativeDockAppletId"), -1},
         {QStringLiteral("nativeOwnershipToken"), QString{}},
         {QStringLiteral("nativeRecoveryState"), recordVisible
              ? QStringLiteral("recovering")
              : QStringLiteral("detached")},
         {QStringLiteral("nativeRecoveryError"), QStringLiteral("owned-host-not-found")}});
}

bool PanelRegistry::recordNativePanelRecoveryConflict(
    const QString &panelId,
    const QString &ownershipToken,
    const QString &errorCode)
{
    const QVariantMap *panel = record(panelId);
    const QString normalizedToken = ownershipToken.trimmed();
    const QString normalizedError = errorCode.trimmed();
    if (!panel || panel->value(QStringLiteral("edge")).toString() == QStringLiteral("free") ||
        normalizedToken.isEmpty() || normalizedError.isEmpty() ||
        panel->value(QStringLiteral("nativeOwnershipToken")).toString().trimmed() != normalizedToken)
    {
        return false;
    }

    return setPanelValuesChecked(
        panelId,
        {{QStringLiteral("nativeRecoveryState"), QStringLiteral("conflict")},
         {QStringLiteral("nativeRecoveryError"), normalizedError}});
}

bool PanelRegistry::recordNativePanelRecoveryFailure(
    const QString &panelId,
    const QString &errorCode)
{
    const QVariantMap *panel = record(panelId);
    const QString normalizedError = errorCode.trimmed();
    if (!panel || panel->value(QStringLiteral("edge")).toString() == QStringLiteral("free") ||
        normalizedError.isEmpty())
    {
        return false;
    }

    return setPanelValuesChecked(
        panelId,
        {{QStringLiteral("nativePanelId"), -1},
         {QStringLiteral("nativeControlAppletId"), -1},
         {QStringLiteral("nativeDockAppletId"), -1},
         {QStringLiteral("nativeOwnershipToken"), QString{}},
         {QStringLiteral("nativeRecoveryState"), QStringLiteral("recoverable-error")},
         {QStringLiteral("nativeRecoveryError"), normalizedError}});
}

QVariantList PanelRegistry::themeDefinitions() const
{
    return m_themeDefinitions;
}

QVariantMap PanelRegistry::themeCandidate(const QString &panelId,
                                          const QString &themeId,
                                          const QString &layer) const
{
    const QString normalizedLayer = layer.trimmed().toLower();
    if (!record(panelId) || (normalizedLayer != QStringLiteral("panel") &&
                             normalizedLayer != QStringLiteral("icon") &&
                             normalizedLayer != QStringLiteral("complete")))
    {
        return {
            {QStringLiteral("success"), false},
            {QStringLiteral("status"), QStringLiteral("validation-failed")},
            {QStringLiteral("errorCode"), record(panelId)
                 ? QStringLiteral("invalid-theme-layer")
                 : QStringLiteral("panel-not-found")},
            {QStringLiteral("panelId"), panelId},
            {QStringLiteral("themeId"), themeId},
            {QStringLiteral("layer"), normalizedLayer},
            {QStringLiteral("values"), QVariantMap{}},
        };
    }

    for (const QVariant &candidate : m_themeDefinitions)
    {
        const QVariantMap theme = candidate.toMap();
        if (theme.value(QStringLiteral("id")).toString() != themeId)
            continue;

        QVariantMap changes;
        const auto mergeStyle = [&changes](const QVariantMap &style)
        {
            for (auto it = style.cbegin(); it != style.cend(); ++it)
                changes.insert(it.key(), it.value());
        };
        if (normalizedLayer == QStringLiteral("panel") || normalizedLayer == QStringLiteral("complete"))
        {
            mergeStyle(theme.value(QStringLiteral("panelStyle")).toMap());
            changes.insert(QStringLiteral("panelThemeId"), themeId);
        }
        if (normalizedLayer == QStringLiteral("icon") || normalizedLayer == QStringLiteral("complete"))
        {
            mergeStyle(theme.value(QStringLiteral("iconStyle")).toMap());
            changes.insert(QStringLiteral("iconThemeId"), themeId);
        }
        if (normalizedLayer == QStringLiteral("complete"))
        {
            mergeStyle(theme.value(QStringLiteral("tileStyle")).toMap());
            mergeStyle(theme.value(QStringLiteral("indicatorStyle")).toMap());
            mergeStyle(theme.value(QStringLiteral("animationStyle")).toMap());
            mergeStyle(theme.value(QStringLiteral("layoutStyle")).toMap());
            changes.insert(QStringLiteral("completeThemeId"), themeId);
        }

        QVariantMap normalizedChanges;
        for (auto iterator = changes.cbegin(); iterator != changes.cend(); ++iterator)
        {
            const ArchDock::PanelSettingsFieldDescriptor *field =
                ArchDock::PanelSettingsSchema::panelDescriptor(iterator.key());
            if (!field || field->access != ArchDock::PanelSettingsFieldAccess::Editor)
            {
                return {
                    {QStringLiteral("success"), false},
                    {QStringLiteral("status"), QStringLiteral("validation-failed")},
                    {QStringLiteral("errorCode"), QStringLiteral("theme-field-unavailable")},
                    {QStringLiteral("errorMessage"), iterator.key()},
                    {QStringLiteral("panelId"), panelId},
                    {QStringLiteral("themeId"), themeId},
                    {QStringLiteral("layer"), normalizedLayer},
                    {QStringLiteral("values"), QVariantMap{}},
                };
            }
            normalizedChanges.insert(
                iterator.key(),
                ArchDock::PanelSettingsSchema::normalizePanelValue(
                    iterator.key(), iterator.value()));
        }

        QVariantMap candidateRecord = *record(panelId);
        for (auto iterator = normalizedChanges.cbegin();
             iterator != normalizedChanges.cend(); ++iterator)
        {
            candidateRecord.insert(iterator.key(), iterator.value());
        }
        QString definitionError;
        const std::optional<ArchDock::PanelDefinition> definition =
            ArchDock::PanelDefinition::fromLegacyMap(
                candidateRecord, &definitionError);
        QString capabilityError;
        const std::optional<ArchDock::ThemeCapabilityProfile> profile =
            ArchDock::PanelCapabilityResolver::themeProfileFromVariantMap(
                theme, &capabilityError);
        if (!definition.has_value() || !profile.has_value())
        {
            return {
                {QStringLiteral("success"), false},
                {QStringLiteral("status"), QStringLiteral("validation-failed")},
                {QStringLiteral("errorCode"), !definition.has_value()
                     ? QStringLiteral("invalid-theme-candidate")
                     : QStringLiteral("invalid-theme-capabilities")},
                {QStringLiteral("errorMessage"), !definition.has_value()
                     ? definitionError : capabilityError},
                {QStringLiteral("panelId"), panelId},
                {QStringLiteral("themeId"), themeId},
                {QStringLiteral("layer"), normalizedLayer},
                {QStringLiteral("values"), QVariantMap{}},
            };
        }
        const ArchDock::CapabilityResolution resolution =
            ArchDock::PanelCapabilityResolver::resolve(
                *definition,
                ArchDock::PanelCapabilityResolver::productionHostProfile(
                    definition->host.kind),
                *profile,
                ArchDock::PanelCapabilityResolver::productionRenderers(),
                ArchDock::PanelCapabilityResolver::productionPlatform());
        if (!resolution.available)
        {
            return {
                {QStringLiteral("success"), false},
                {QStringLiteral("status"), QStringLiteral("capability-unavailable")},
                {QStringLiteral("errorCode"),
                 ArchDock::capabilityReasonCodeName(resolution.reason)},
                {QStringLiteral("panelId"), panelId},
                {QStringLiteral("themeId"), themeId},
                {QStringLiteral("layer"), normalizedLayer},
                {QStringLiteral("values"), QVariantMap{}},
                {QStringLiteral("capabilityResolution"), resolution.toVariantMap()},
            };
        }
        return {
            {QStringLiteral("success"), true},
            {QStringLiteral("status"), QStringLiteral("resolved")},
            {QStringLiteral("errorCode"), QString{}},
            {QStringLiteral("panelId"), panelId},
            {QStringLiteral("themeId"), themeId},
            {QStringLiteral("layer"), normalizedLayer},
            {QStringLiteral("values"), normalizedChanges},
            {QStringLiteral("capabilityResolution"), resolution.toVariantMap()},
        };
    }
    return {
        {QStringLiteral("success"), false},
        {QStringLiteral("status"), QStringLiteral("validation-failed")},
        {QStringLiteral("errorCode"), QStringLiteral("theme-not-found")},
        {QStringLiteral("panelId"), panelId},
        {QStringLiteral("themeId"), themeId},
        {QStringLiteral("layer"), normalizedLayer},
        {QStringLiteral("values"), QVariantMap{}},
    };
}

bool PanelRegistry::applyTheme(const QString &panelId,
                               const QString &themeId,
                               const QString &layer)
{
    const QVariantMap candidate = themeCandidate(panelId, themeId, layer);
    if (!candidate.value(QStringLiteral("success")).toBool())
    {
        return false;
    }
    setPanelValues(panelId, candidate.value(QStringLiteral("values")).toMap());
    return true;
}

QString PanelRegistry::addPanel(const QString &edge, const QString &type)
{
    const QString normalizedEdge = edge.trimmed().toLower();
    if (!isEdge(normalizedEdge))
    {
        return {};
    }

    int sequence = 1;
    QString id;
    do
    {
        id = QStringLiteral("panel-%1").arg(sequence++);
    } while (record(id));

    QVariantMap panel = makePanel(
        id,
        tr("Panel %1").arg(sequence - 1),
        normalizedEdge,
        false);
    panel.insert(QStringLiteral("visible"), true);
    const QString normalizedType = normalizeValue(QStringLiteral("type"), type).toString();
    panel.insert(QStringLiteral("type"), normalizedType);
    if (normalizedType == QStringLiteral("empty"))
    {
        panel.insert(QStringLiteral("dynamic"), false);
    }
    (void)normalizeFreeHostRecord(&panel);
    m_panels.append(panel);
    m_activePanelId = id;
    save();
    emit activePanelIdChanged();
    changed(true);
    return id;
}

QString PanelRegistry::addFreePanel()
{
    int sequence = 1;
    QString id;
    do
    {
        id = QStringLiteral("free-%1").arg(sequence++);
    } while (record(id));

    QVariantMap panel = makePanel(id, tr("Free panel %1").arg(sequence - 1),
                                  QStringLiteral("bottom"), false);
    panel.insert(QStringLiteral("edge"), QStringLiteral("free"));
    panel.insert(QStringLiteral("visible"), true);
    panel.insert(QStringLiteral("type"), QStringLiteral("empty"));
    panel.insert(QStringLiteral("contentAppIds"), QStringList{});
    panel.insert(QStringLiteral("contentUrls"), QStringList{});
    panel.insert(QStringLiteral("layout"), QStringLiteral("circular"));
    panel.insert(QStringLiteral("width"), 420);
    panel.insert(QStringLiteral("height"), 420);
    panel.insert(QStringLiteral("layoutRadius"), 145);
    panel.insert(QStringLiteral("x"), 240);
    panel.insert(QStringLiteral("y"), 180);
    (void)normalizeFreeHostRecord(&panel);
    const QString previousActivePanelId = m_activePanelId;
    m_panels.append(panel);
    m_activePanelId = id;
    if (!saveChecked())
    {
        m_panels.removeLast();
        m_activePanelId = previousActivePanelId;
        return {};
    }
    if (m_activePanelId != previousActivePanelId)
    {
        emit activePanelIdChanged();
    }
    changed(false);
    return id;
}

void PanelRegistry::removePanel(const QString &panelId)
{
    for (int index = 0; index < m_panels.size(); ++index)
    {
        QVariantMap &panel = m_panels[index];
        if (panel.value(QStringLiteral("id")).toString() != panelId)
        {
            continue;
        }

        const bool nativePanel = panel.value(QStringLiteral("edge")).toString() !=
            QStringLiteral("free");
        if (panel.value(QStringLiteral("builtIn")).toBool())
        {
            panel.insert(QStringLiteral("visible"), false);
        }
        else
        {
            m_panels.removeAt(index);
        }

        if (m_activePanelId == panelId)
        {
            m_activePanelId = QStringLiteral("bottom");
            emit activePanelIdChanged();
        }
        save();
        changed(nativePanel);
        return;
    }
}

bool PanelRegistry::importTheme(const QString &panelId, const QUrl &sourceUrl)
{
    if (!record(panelId) || !sourceUrl.isLocalFile())
    {
        return false;
    }

    const QFileInfo source(sourceUrl.toLocalFile());
    if (!source.exists() || !source.isFile())
    {
        return false;
    }

    ThemePackage imported;
    QString errorMessage;
    if (source.suffix().compare(QStringLiteral("json"), Qt::CaseInsensitive) == 0)
    {
        if (!readThemePackageManifest(source, &imported, &errorMessage))
        {
            setPanelValues(panelId, {{QStringLiteral("themeStatus"), errorMessage}});
            return false;
        }
    }
    else
    {
        imported.id = normalizedThemePackageId(source.completeBaseName());
        imported.name = source.completeBaseName();
        imported.sourcePath = source.absoluteFilePath();
        imported.fit = record(panelId)->value(QStringLiteral("themeFit"), QStringLiteral("cover")).toString();
        if (!isThemeFit(imported.fit))
        {
            imported.fit = QStringLiteral("cover");
        }
    }

    QVariantMap capabilityCandidate = *record(panelId);
    capabilityCandidate.insert(
        QStringLiteral("themeSource"),
        QUrl::fromLocalFile(imported.sourcePath).toString());
    capabilityCandidate.insert(
        QStringLiteral("themePackageFormat"), themePackageFormat);
    capabilityCandidate.insert(
        QStringLiteral("themePackageVersion"), themePackageVersion);
    capabilityCandidate.insert(
        QStringLiteral("themePackageId"), imported.id);
    QString capabilityDefinitionError;
    const std::optional<ArchDock::PanelDefinition> capabilityDefinition =
        ArchDock::PanelDefinition::fromLegacyMap(
            capabilityCandidate, &capabilityDefinitionError);
    if (!capabilityDefinition.has_value() ||
        !resolvePanelCapabilities(*capabilityDefinition).available)
    {
        return false;
    }

    ThemePackage managed;
    if (!materializeThemePackage(panelId, imported, &managed, &errorMessage))
    {
        setPanelValues(panelId, {{QStringLiteral("themeStatus"), errorMessage}});
        return false;
    }

    const QVariantMap *currentPanel = record(panelId);
    const QSize targetSize(
        currentPanel ? currentPanel->value(QStringLiteral("width"), 720).toInt() : 720,
        currentPanel ? currentPanel->value(QStringLiteral("height"), 76).toInt() : 76);
    const QVariantMap analysis = analyzeThemeSource(managed, targetSize);
    QVariantMap values{
        {QStringLiteral("themeSource"), QUrl::fromLocalFile(managed.sourcePath).toString()},
        {QStringLiteral("themeAsset"), QString{}},
        {QStringLiteral("themeFit"), managed.fit},
        {QStringLiteral("themePackageFormat"), themePackageFormat},
        {QStringLiteral("themePackageVersion"), themePackageVersion},
        {QStringLiteral("themePackageId"), managed.id},
        {QStringLiteral("themePackageName"), managed.name},
        {QStringLiteral("themePackageAuthor"), managed.author},
        {QStringLiteral("themePackageManifest"), QUrl::fromLocalFile(managed.manifestPath).toString()},
        {QStringLiteral("themeStatus"), tr("Theme package \"%1\" imported; preparing panel skin.").arg(managed.name)},
        {QStringLiteral("themeRenderWidth"), 0},
        {QStringLiteral("themeRenderHeight"), 0},
        {QStringLiteral("themeRenderFit"), QString{}},
        {QStringLiteral("themeRenderOutcome"), QString{}}};
    for (auto iterator = analysis.cbegin(); iterator != analysis.cend(); ++iterator)
    {
        values.insert(iterator.key(), iterator.value());
    }

    setPanelValues(
        panelId,
        values);

    const QVariantMap *panel = record(panelId);
    if (!panel)
    {
        return false;
    }

    renderTheme(
        panelId,
        panel->value(QStringLiteral("width"), 720).toInt(),
        panel->value(QStringLiteral("height"), 76).toInt());
    return true;
}

bool PanelRegistry::renderTheme(const QString &panelId,
                                int width,
                                int height,
                                qreal devicePixelRatio,
                                bool force)
{
    const RenderRequest request = makeRenderRequest(panelId, width, height, devicePixelRatio);
    if (request.panelId.isEmpty())
    {
        return false;
    }

    const QVariantMap *panel = record(panelId);
    const QUrl currentAsset(panel ? panel->value(QStringLiteral("themeAsset")).toString() : QString{});
    const bool targetMatches = panel &&
        panel->value(QStringLiteral("themeRenderWidth")).toInt() == request.width &&
        panel->value(QStringLiteral("themeRenderHeight")).toInt() == request.height &&
        panel->value(QStringLiteral("themeRenderFit")).toString() == request.fit;
    const QString outcome = panel
        ? panel->value(QStringLiteral("themeRenderOutcome")).toString()
        : QString{};
    const bool assetMatches = outcome == QStringLiteral("ready") &&
        QFileInfo(currentAsset.toLocalFile()).exists();
    if (!force && targetMatches && (assetMatches || outcome == QStringLiteral("error")))
    {
        return true;
    }

    const auto matches = [](const RenderRequest &left, const RenderRequest &right)
    {
        return left.panelId == right.panelId &&
            left.sourcePath == right.sourcePath &&
            left.outputPath == right.outputPath &&
            left.fit == right.fit &&
            left.width == right.width &&
            left.height == right.height;
    };
    const auto active = m_activeRenders.constFind(panelId);
    if (active != m_activeRenders.cend())
    {
        if (!matches(active.value(), request))
        {
            m_pendingRenders.insert(panelId, request);
        }
        return true;
    }

    m_activeRenders.insert(panelId, request);
    startRender(request);
    return true;
}

void PanelRegistry::clearTheme(const QString &panelId)
{
    if (!record(panelId))
    {
        return;
    }

    m_pendingRenders.remove(panelId);
    if (const auto process = m_renderProcesses.value(panelId))
    {
        process->kill();
    }
    setPanelValues(
        panelId,
        {{QStringLiteral("themeSource"), QString{}},
         {QStringLiteral("themeAsset"), QString{}},
         {QStringLiteral("themeSourceKind"), QString{}},
         {QStringLiteral("themeSourceFormat"), QString{}},
         {QStringLiteral("themeSourceWidth"), 0},
         {QStringLiteral("themeSourceHeight"), 0},
         {QStringLiteral("themeSourceHasAlpha"), false},
         {QStringLiteral("themeSuggestedFit"), QStringLiteral("cover")},
         {QStringLiteral("themePreview"), QString{}},
         {QStringLiteral("themeAnalysisStatus"), tr("No source selected.")},
         {QStringLiteral("themeConversionTool"), QString{}},
         {QStringLiteral("themeConversionAvailable"), false},
         {QStringLiteral("themePackageFormat"), QString{}},
         {QStringLiteral("themePackageVersion"), 0},
         {QStringLiteral("themePackageId"), QString{}},
         {QStringLiteral("themePackageName"), QString{}},
         {QStringLiteral("themePackageAuthor"), QString{}},
         {QStringLiteral("themePackageManifest"), QString{}},
         {QStringLiteral("themeStatus"), tr("Preset surface active.")},
         {QStringLiteral("themeRenderWidth"), 0},
         {QStringLiteral("themeRenderHeight"), 0},
         {QStringLiteral("themeRenderFit"), QString{}},
         {QStringLiteral("themeRenderOutcome"), QString{}}});
}

void PanelRegistry::setActivePanelId(const QString &panelId)
{
    if (panelId == m_activePanelId || !record(panelId))
    {
        return;
    }

    m_activePanelId = panelId;
    QSettings settings;
    settings.setValue(QStringLiteral("dock/activePanel"), m_activePanelId);
    settings.sync();
    emit activePanelIdChanged();
}

QVariantMap *PanelRegistry::record(const QString &panelId)
{
    for (QVariantMap &panel : m_panels)
    {
        if (panel.value(QStringLiteral("id")).toString() == panelId)
        {
            return &panel;
        }
    }
    return nullptr;
}

const QVariantMap *PanelRegistry::record(const QString &panelId) const
{
    for (const QVariantMap &panel : m_panels)
    {
        if (panel.value(QStringLiteral("id")).toString() == panelId)
        {
            return &panel;
        }
    }
    return nullptr;
}

QVariantMap PanelRegistry::makePanel(const QString &id,
                                     const QString &name,
                                     const QString &edge,
                                     bool builtIn) const
{
    return ArchDock::PanelDefinition::defaults(id, name, edge, builtIn).toLegacyMap();
}

QVariant PanelRegistry::normalizeValue(const QString &key, const QVariant &value) const
{
    return ArchDock::PanelSettingsSchema::normalizePanelValue(key, value);
}

PanelRegistry::RenderRequest PanelRegistry::makeRenderRequest(const QString &panelId,
                                                               int width,
                                                               int height,
                                                               qreal devicePixelRatio) const
{
    const QVariantMap *panel = record(panelId);
    if (!panel)
    {
        return {};
    }

    const QUrl sourceUrl(panel->value(QStringLiteral("themeSource")).toString());
    const QString sourcePath = sourceUrl.toLocalFile();
    const QFileInfo source(sourcePath);
    if (sourcePath.isEmpty() || !source.exists() || !source.isFile())
    {
        return {};
    }

    qreal scale = devicePixelRatio;
    if (scale <= 0)
    {
        if (QScreen *screen = QGuiApplication::primaryScreen())
        {
            scale = screen->devicePixelRatio();
        }
    }
    scale = qBound<qreal>(1.0, scale, 4.0);

    RenderRequest request;
    request.panelId = panelId;
    request.sourcePath = source.absoluteFilePath();
    request.fit = panel->value(QStringLiteral("themeFit"), QStringLiteral("cover")).toString();
    request.width = qBound(64, qRound(qMax(1, width) * scale), 4096);
    request.height = qBound(64, qRound(qMax(1, height) * scale), 4096);
    const QString themeDirectory = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation) + QStringLiteral("/themes/") + panelId;
    QDir().mkpath(themeDirectory);
    const QByteArray identity = (request.sourcePath + QLatin1Char('|') +
        QString::number(source.lastModified().toMSecsSinceEpoch()) + QLatin1Char('|') +
        request.fit).toUtf8();
    const QString fingerprint = QString::fromLatin1(
        QCryptographicHash::hash(identity, QCryptographicHash::Sha256).toHex().left(12));
    request.outputPath = themeDirectory + QStringLiteral("/skin-%1-%2x%3-%4.png")
        .arg(fingerprint)
        .arg(request.width)
        .arg(request.height)
        .arg(request.fit);
    return request;
}

bool PanelRegistry::renderWithQt(const RenderRequest &request, QString *errorMessage) const
{
    QImageReader reader(request.sourcePath);
    reader.setAutoTransform(true);
    const QImage source = reader.read();
    if (source.isNull())
    {
        if (errorMessage)
        {
            *errorMessage = reader.errorString().isEmpty()
                ? tr("Qt could not decode this design file.")
                : reader.errorString();
        }
        return false;
    }

    QImage rendered;
    const QSize target(request.width, request.height);
    if (request.fit == QStringLiteral("tile"))
    {
        const int tileLimit = qBound(48, qMin(request.width, request.height) / 2, 512);
        const QImage tile = source.scaled(
            QSize(tileLimit, tileLimit),
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation);
        rendered = QImage(target, QImage::Format_ARGB32_Premultiplied);
        rendered.fill(Qt::transparent);
        QPainter painter(&rendered);
        for (int y = 0; y < target.height(); y += tile.height())
        {
            for (int x = 0; x < target.width(); x += tile.width())
            {
                painter.drawImage(x, y, tile);
            }
        }
    }
    else if (request.fit == QStringLiteral("stretch"))
    {
        rendered = source.scaled(target, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    else
    {
        const Qt::AspectRatioMode mode = request.fit == QStringLiteral("contain")
            ? Qt::KeepAspectRatio
            : Qt::KeepAspectRatioByExpanding;
        const QImage scaled = source.scaled(target, mode, Qt::SmoothTransformation);
        rendered = QImage(target, QImage::Format_ARGB32_Premultiplied);
        rendered.fill(Qt::transparent);
        QPainter painter(&rendered);
        const QPoint origin(
            (target.width() - scaled.width()) / 2,
            (target.height() - scaled.height()) / 2);
        painter.drawImage(origin, scaled);
    }

    if (rendered.isNull() || !rendered.save(request.outputPath, "PNG"))
    {
        if (errorMessage)
        {
            *errorMessage = tr("Qt could not write the generated panel skin.");
        }
        return false;
    }
    return true;
}

void PanelRegistry::setPanelValues(const QString &panelId, const QVariantMap &values)
{
    (void)setPanelValuesChecked(panelId, values);
}

bool PanelRegistry::setPanelValuesChecked(const QString &panelId, const QVariantMap &values)
{
    QVariantMap *panel = record(panelId);
    if (!panel)
    {
        return false;
    }

    const QVariantMap previousValues = *panel;
    QVariantMap candidate = previousValues;
    const bool wasNativePanel = previousValues.value(QStringLiteral("edge")).toString() !=
        QStringLiteral("free");
    bool contentOnly = true;
    bool topologyChanged = false;
    static const QSet<QString> topologyKeys{
        QStringLiteral("visible"),
        QStringLiteral("edge"),
        QStringLiteral("alignment"),
        QStringLiteral("screen"),
        QStringLiteral("screenId"),
        QStringLiteral("visibilityMode"),
        QStringLiteral("revealZone"),
        QStringLiteral("dynamic"),
        QStringLiteral("width"),
        QStringLiteral("height"),
        QStringLiteral("type")};
    for (auto iterator = values.cbegin(); iterator != values.cend(); ++iterator)
    {
        if (iterator.key() == QStringLiteral("id") ||
            iterator.key() == QStringLiteral("builtIn") ||
            iterator.key() == QStringLiteral("schemaVersion") ||
            iterator.key() == QStringLiteral("settingsRevision"))
        {
            continue;
        }

        const QVariant normalized = normalizeValue(iterator.key(), iterator.value());
        if (candidate.value(iterator.key()) == normalized)
        {
            continue;
        }
        candidate.insert(iterator.key(), normalized);
        topologyChanged = topologyChanged || topologyKeys.contains(iterator.key());
        if (iterator.key() != QStringLiteral("contentUrls") &&
            iterator.key() != QStringLiteral("contentAppIds"))
        {
            contentOnly = false;
        }
    }
    if (normalizeFreeHostRecord(&candidate))
    {
        contentOnly = false;
    }
    if (values.contains(QStringLiteral("edge")))
    {
        candidate.insert(
            QStringLiteral("hostKind"),
            candidate.value(QStringLiteral("edge")).toString() == QStringLiteral("free")
                ? QStringLiteral("free-desktop")
                : QStringLiteral("native-edge"));
    }

    QString definitionError;
    const std::optional<ArchDock::PanelDefinition> definition =
        ArchDock::PanelDefinition::fromLegacyMap(candidate, &definitionError);
    if (!definition.has_value())
    {
        qWarning().noquote() << "Panel update rejected:" << definitionError;
        return false;
    }

    candidate = definition->toLegacyMap();
    if (candidate == previousValues)
    {
        return true;
    }

    ArchDock::PanelDefinition revisedDefinition = *definition;
    if (revisedDefinition.settingsRevision == std::numeric_limits<quint64>::max())
    {
        qWarning() << "Panel update rejected: settings revision exhausted for" << panelId;
        return false;
    }
    revisedDefinition.settingsRevision += 1;
    candidate = revisedDefinition.toLegacyMap();

    *panel = candidate;
    if (!saveChecked())
    {
        *panel = previousValues;
        return false;
    }
    ++m_revision;
    if (!contentOnly)
    {
        emit panelsChanged();
    }
    const bool isNativePanel = panel->value(QStringLiteral("edge")).toString() !=
        QStringLiteral("free");
    if (topologyChanged && (wasNativePanel || isNativePanel))
    {
        emit nativePanelTopologyChanged();
    }
    emit revisionChanged();
    return true;
}

void PanelRegistry::startRender(const RenderRequest &request)
{
    setPanelValues(
        request.panelId,
        {{QStringLiteral("themeStatus"), tr("Rendering %1 x %2 panel skin...")
            .arg(request.width)
            .arg(request.height)},
         {QStringLiteral("themeRenderOutcome"), QStringLiteral("rendering")}});

    if (QFileInfo(request.sourcePath).suffix().compare(QStringLiteral("blend"), Qt::CaseInsensitive) == 0)
    {
        const QString blender = QStandardPaths::findExecutable(QStringLiteral("blender"));
        if (blender.isEmpty())
        {
            finishRender(
                request,
                false,
                tr("Blender is not installed. The .blend source is saved and can be rendered after Blender is installed."));
            return;
        }

        const QString outputBase = QFileInfo(request.outputPath).absolutePath() +
            QLatin1Char('/') + QFileInfo(request.outputPath).completeBaseName() + QStringLiteral("-blend-");
        const QString renderedFrame = outputBase + QStringLiteral("0001.png");
        QFile::remove(renderedFrame);
        auto *process = new QProcess(this);
        m_renderProcesses.insert(request.panelId, process);
        connect(process,
                qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
                this,
                [this, process, request, renderedFrame](int exitCode, QProcess::ExitStatus exitStatus)
                {
                    const QString error = QString::fromUtf8(process->readAllStandardError()).trimmed();
                    m_renderProcesses.remove(request.panelId);
                    process->deleteLater();
                    if (exitStatus == QProcess::NormalExit && exitCode == 0 && QFileInfo::exists(renderedFrame))
                    {
                        startMagickRender(request, renderedFrame);
                        return;
                    }
                    finishRender(
                        request,
                        false,
                        error.isEmpty() ? tr("Blender could not render frame 1 from this .blend file.") : error.left(220));
                });
        process->start(
            blender,
            {QStringLiteral("--background"), request.sourcePath,
             QStringLiteral("--render-output"), outputBase,
             QStringLiteral("--render-format"), QStringLiteral("PNG"),
             QStringLiteral("--render-frame"), QStringLiteral("1")});
        if (!process->waitForStarted(1500))
        {
            const QString error = process->errorString();
            m_renderProcesses.remove(request.panelId);
            process->deleteLater();
            finishRender(request, false, error.isEmpty() ? tr("Blender did not start.") : error);
        }
        return;
    }

    startMagickRender(request, request.sourcePath);
}

void PanelRegistry::startMagickRender(const RenderRequest &request, const QString &sourcePath)
{
    const QString magick = QStandardPaths::findExecutable(QStringLiteral("magick"));
    if (magick.isEmpty())
    {
        QString error;
        const bool success = renderWithQt(request, &error);
        finishRender(request, success, success ? tr("Rendered with Qt fallback.") : error);
        return;
    }

    QStringList arguments{sourcePath, QStringLiteral("-auto-orient"), QStringLiteral("-colorspace"), QStringLiteral("sRGB")};
    if (request.fit == QStringLiteral("tile"))
    {
        const int tileLimit = qBound(48, qMin(request.width, request.height) / 2, 512);
        arguments << QStringLiteral("-resize")
                  << QStringLiteral("%1x%2>").arg(tileLimit).arg(tileLimit)
                  << QStringLiteral("-write") << QStringLiteral("mpr:tile")
                  << QStringLiteral("+delete")
                  << QStringLiteral("-size")
                  << QStringLiteral("%1x%2").arg(request.width).arg(request.height)
                  << QStringLiteral("tile:mpr:tile");
    }
    else if (request.fit == QStringLiteral("stretch"))
    {
        arguments << QStringLiteral("-resize")
                  << QStringLiteral("%1x%2!").arg(request.width).arg(request.height);
    }
    else if (request.fit == QStringLiteral("contain"))
    {
        arguments << QStringLiteral("-resize")
                  << QStringLiteral("%1x%2").arg(request.width).arg(request.height)
                  << QStringLiteral("-gravity") << QStringLiteral("center")
                  << QStringLiteral("-background") << QStringLiteral("none")
                  << QStringLiteral("-extent")
                  << QStringLiteral("%1x%2").arg(request.width).arg(request.height);
    }
    else
    {
        arguments << QStringLiteral("-resize")
                  << QStringLiteral("%1x%2^").arg(request.width).arg(request.height)
                  << QStringLiteral("-gravity") << QStringLiteral("center")
                  << QStringLiteral("-extent")
                  << QStringLiteral("%1x%2").arg(request.width).arg(request.height);
    }
    arguments << QStringLiteral("-strip") << QStringLiteral("-quality") << QStringLiteral("92")
              << QStringLiteral("PNG32:") + request.outputPath;

    auto *process = new QProcess(this);
    m_renderProcesses.insert(request.panelId, process);
    connect(process,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this,
            [this, process, request](int exitCode, QProcess::ExitStatus exitStatus)
            {
                const QString error = QString::fromUtf8(process->readAllStandardError()).trimmed();
                m_renderProcesses.remove(request.panelId);
                process->deleteLater();
                if (exitStatus == QProcess::NormalExit && exitCode == 0 && QFileInfo::exists(request.outputPath))
                {
                    finishRender(request, true, tr("Panel skin is ready."));
                    return;
                }

                QString fallbackError;
                const bool fallbackSucceeded = renderWithQt(request, &fallbackError);
                finishRender(
                    request,
                    fallbackSucceeded,
                    fallbackSucceeded ? tr("Rendered with Qt fallback.")
                        : (fallbackError.isEmpty() ? error.left(220) : fallbackError));
            });
    process->start(magick, arguments);
    if (!process->waitForStarted(1500))
    {
        const QString error = process->errorString();
        m_renderProcesses.remove(request.panelId);
        process->deleteLater();
        QString fallbackError;
        const bool fallbackSucceeded = renderWithQt(request, &fallbackError);
        finishRender(
            request,
            fallbackSucceeded,
            fallbackSucceeded ? tr("Rendered with Qt fallback.")
                : (fallbackError.isEmpty() ? error : fallbackError));
    }
}

void PanelRegistry::finishRender(const RenderRequest &request, bool success, const QString &message)
{
    m_renderProcesses.remove(request.panelId);
    m_activeRenders.remove(request.panelId);
    const QVariantMap *panel = record(request.panelId);
    const QString currentSource = panel
        ? QUrl(panel->value(QStringLiteral("themeSource")).toString()).toLocalFile()
        : QString{};
    if (currentSource == request.sourcePath)
    {
        if (success)
        {
            setPanelValues(
                request.panelId,
                {{QStringLiteral("themeAsset"), QUrl::fromLocalFile(request.outputPath).toString()},
                 {QStringLiteral("themeStatus"), message},
                 {QStringLiteral("themeRenderWidth"), request.width},
                 {QStringLiteral("themeRenderHeight"), request.height},
                 {QStringLiteral("themeRenderFit"), request.fit},
                 {QStringLiteral("themeRenderOutcome"), QStringLiteral("ready")}});
        }
        else
        {
            setPanelValues(
                request.panelId,
                {{QStringLiteral("themeStatus"), message.isEmpty()
                    ? tr("This design file could not be rendered.") : message},
                 {QStringLiteral("themeRenderWidth"), request.width},
                 {QStringLiteral("themeRenderHeight"), request.height},
                 {QStringLiteral("themeRenderFit"), request.fit},
                 {QStringLiteral("themeRenderOutcome"), QStringLiteral("error")}});
        }
    }

    const auto pending = m_pendingRenders.take(request.panelId);
    if (!pending.panelId.isEmpty() && pending.sourcePath != currentSource)
    {
        return;
    }
    if (!pending.panelId.isEmpty())
    {
        m_activeRenders.insert(pending.panelId, pending);
        startRender(pending);
    }
}

void PanelRegistry::load()
{
    QSettings settings;
    const int legacyScreen = qMax(0, settings.value(QStringLiteral("dock/monitorIndex"), 0).toInt());
    const bool legacyAutoHide = settings.value(QStringLiteral("dock/autoHide"), false).toBool();
    const QString panelRecordsKey = QString::fromLatin1(kPanelRecordsKey);
    const bool hasStoredPanels = settings.contains(panelRecordsKey);
    const QByteArray storedPanels = settings.value(panelRecordsKey).toByteArray();
    bool initializeBuiltIns = !hasStoredPanels;
    bool compatibilityRewriteRequired = false;

    if (hasStoredPanels && storedPanels.isEmpty())
    {
        m_persistenceBlocked = true;
        setMigrationDiagnostic(QStringLiteral(
            "invalid-json: stored panel registry is empty"));
        qWarning().noquote() << "Panel registry load blocked:" << m_migrationDiagnostic;
        return;
    }

    if (hasStoredPanels)
    {
        const ArchDock::PanelMigrationResult migration =
            ArchDock::SettingsMigration::migratePanelRecords(storedPanels);
        if (migration.status != ArchDock::PanelMigrationStatus::Success)
        {
            m_persistenceBlocked = true;
            setMigrationDiagnostic(
                ArchDock::SettingsMigration::statusName(migration.status) +
                QStringLiteral(": ") + migration.diagnostic);
            qWarning().noquote() << "Panel registry load blocked:" << m_migrationDiagnostic;
            return;
        }

        m_legacySource = storedPanels;
        m_legacyRewritePending = migration.rewriteRequired;
        const QJsonArray sourceRecords = QJsonDocument::fromJson(storedPanels).array();
        for (qsizetype index = 0; index < migration.definitions.size(); ++index)
        {
            QVariantMap panel = migration.definitions.at(index).toLegacyMap();
            const QVariantMap sourceRecord = sourceRecords.at(index).toObject().toVariantMap();
            if (!sourceRecord.contains(QStringLiteral("screen")))
            {
                panel.insert(QStringLiteral("screen"), legacyScreen);
                compatibilityRewriteRequired = true;
            }
            if (!sourceRecord.contains(QStringLiteral("visibilityMode")))
            {
                panel.insert(
                    QStringLiteral("visibilityMode"),
                    panel.value(QStringLiteral("id")).toString() == QStringLiteral("bottom") && legacyAutoHide
                        ? QStringLiteral("auto-hide")
                        : QStringLiteral("always"));
                compatibilityRewriteRequired = true;
            }
            compatibilityRewriteRequired = normalizeFreeHostRecord(&panel) ||
                compatibilityRewriteRequired;
            compatibilityRewriteRequired = migrateLegacyThemePackage(&panel) ||
                compatibilityRewriteRequired;

            QString definitionError;
            const std::optional<ArchDock::PanelDefinition> normalized =
                ArchDock::PanelDefinition::fromLegacyMap(panel, &definitionError);
            if (!normalized.has_value())
            {
                m_panels.clear();
                m_persistenceBlocked = true;
                setMigrationDiagnostic(
                    QStringLiteral("invalid-record: compatibility migration failed: ") +
                    definitionError);
                qWarning().noquote() << "Panel registry load blocked:" << m_migrationDiagnostic;
                return;
            }
            m_panels.append(normalized->toLegacyMap());
        }

        if (m_panels.isEmpty())
        {
            initializeBuiltIns = true;
            m_legacyRewritePending = true;
        }
    }

    if (initializeBuiltIns)
    {
        const bool suiteVisible = settings.value(QStringLiteral("dock/desktopSuite"), false).toBool();
        QVariantMap bottom = makePanel(QStringLiteral("bottom"), tr("Bottom panel"), QStringLiteral("bottom"), true);
        bottom.insert(QStringLiteral("visible"), settings.value(QStringLiteral("dock/bottomPanelVisible"), true).toBool());
        bottom.insert(QStringLiteral("edge"), normalizeValue(
            QStringLiteral("edge"), settings.value(QStringLiteral("dock/position"), QStringLiteral("bottom"))));
        bottom.insert(QStringLiteral("alignment"), normalizeValue(
            QStringLiteral("alignment"), settings.value(QStringLiteral("dock/alignment"), QStringLiteral("center"))));
        bottom.insert(QStringLiteral("screen"), legacyScreen);
        bottom.insert(
            QStringLiteral("visibilityMode"),
            legacyAutoHide ? QStringLiteral("auto-hide") : QStringLiteral("always"));
        bottom.insert(QStringLiteral("type"), normalizeValue(
            QStringLiteral("type"), settings.value(QStringLiteral("dock/bottomPanelType"), QStringLiteral("hybrid"))));
        bottom.insert(QStringLiteral("iconSize"), normalizeValue(
            QStringLiteral("iconSize"), settings.value(QStringLiteral("dock/iconSize"), 52)));
        bottom.insert(QStringLiteral("spacing"), normalizeValue(
            QStringLiteral("spacing"), settings.value(QStringLiteral("dock/spacing"), 8.0)));
        bottom.insert(QStringLiteral("appearance"), normalizeValue(
            QStringLiteral("appearance"), settings.value(QStringLiteral("dock/appearancePreset"), QStringLiteral("glass"))));
        bottom.insert(QStringLiteral("opacity"), normalizeValue(
            QStringLiteral("opacity"), settings.value(QStringLiteral("dock/panelOpacity"), 0.9)));
        m_panels.append(bottom);

        QVariantMap top = makePanel(QStringLiteral("top"), tr("Top panel"), QStringLiteral("top"), true);
        top.insert(QStringLiteral("visible"), suiteVisible && settings.value(QStringLiteral("dock/topLauncherVisible"), true).toBool());
        top.insert(QStringLiteral("type"), settings.value(QStringLiteral("dock/topPanelType"), QStringLiteral("hybrid")).toString());
        top.insert(QStringLiteral("screen"), legacyScreen);
        m_panels.append(top);

        QVariantMap side = makePanel(QStringLiteral("side"), tr("Side panel"), QStringLiteral("right"), true);
        side.insert(QStringLiteral("visible"), suiteVisible && settings.value(QStringLiteral("dock/sideRailVisible"), true).toBool());
        side.insert(QStringLiteral("type"), settings.value(QStringLiteral("dock/sidePanelType"), QStringLiteral("hybrid")).toString());
        side.insert(QStringLiteral("screen"), legacyScreen);
        m_panels.append(side);
        compatibilityRewriteRequired = true;
    }

    const QString requestedActive = settings.value(QStringLiteral("dock/activePanel"), m_activePanelId).toString();
    if (record(requestedActive))
    {
        m_activePanelId = requestedActive;
    }

    if (m_legacyRewritePending || compatibilityRewriteRequired)
    {
        (void)saveChecked();
    }
}

void PanelRegistry::save()
{
    (void)saveChecked();
}

bool PanelRegistry::saveChecked()
{
    if (m_persistenceBlocked)
    {
        if (m_migrationDiagnostic.isEmpty())
        {
            setMigrationDiagnostic(QStringLiteral(
                "persistence-blocked: panel registry source requires repair"));
        }
        return false;
    }

    QString serializationError;
    const QByteArray serialized = serializePanels(&serializationError);
    if (!serializationError.isEmpty())
    {
        setMigrationDiagnostic(QStringLiteral("serialization-failed: ") + serializationError);
        qWarning().noquote() << "Panel registry save blocked:" << m_migrationDiagnostic;
        return false;
    }

    QSettings settings;
    if (m_legacyRewritePending && !ensureLegacyBackupChecked(settings))
    {
        return false;
    }

    settings.setValue(QString::fromLatin1(kPanelRecordsKey), serialized);
    settings.sync();
    if (settings.status() != QSettings::NoError)
    {
        setMigrationDiagnostic(QStringLiteral(
            "write-failed: QSettings could not persist panel registry schema v2"));
        qWarning().noquote() << "Panel registry save failed:" << m_migrationDiagnostic;
        return false;
    }

    m_legacySource.clear();
    m_legacyRewritePending = false;
    setMigrationDiagnostic(QString{});
    return true;
}

bool PanelRegistry::ensureLegacyBackupChecked(QSettings &settings)
{
    const QString backupKey = QString::fromLatin1(kPanelLegacyBackupKey);
    if (settings.contains(backupKey))
    {
        if (settings.value(backupKey).toByteArray() == m_legacySource)
        {
            return true;
        }
        setMigrationDiagnostic(QStringLiteral(
            "backup-conflict: existing legacy panel backup does not match the source"));
        qWarning().noquote() << "Panel registry migration blocked:" << m_migrationDiagnostic;
        return false;
    }

    settings.setValue(backupKey, m_legacySource);
    settings.sync();
    if (settings.status() != QSettings::NoError)
    {
        setMigrationDiagnostic(QStringLiteral(
            "backup-write-failed: QSettings could not persist the legacy panel source"));
        qWarning().noquote() << "Panel registry migration blocked:" << m_migrationDiagnostic;
        return false;
    }

    QSettings verifier;
    verifier.sync();
    if (verifier.status() != QSettings::NoError ||
        !verifier.contains(backupKey) ||
        verifier.value(backupKey).toByteArray() != m_legacySource)
    {
        setMigrationDiagnostic(QStringLiteral(
            "backup-verification-failed: legacy panel source readback was not exact"));
        qWarning().noquote() << "Panel registry migration blocked:" << m_migrationDiagnostic;
        return false;
    }
    return true;
}

QByteArray PanelRegistry::serializePanels(QString *errorMessage) const
{
    return serializePanels(m_panels, errorMessage);
}

QByteArray PanelRegistry::serializePanels(
    const QList<QVariantMap> &panels,
    QString *errorMessage) const
{
    QList<ArchDock::PanelDefinition> definitions;
    definitions.reserve(panels.size());
    QSet<QString> panelIds;
    for (qsizetype index = 0; index < panels.size(); ++index)
    {
        QString definitionError;
        const std::optional<ArchDock::PanelDefinition> definition =
            ArchDock::PanelDefinition::fromLegacyMap(panels.at(index), &definitionError);
        if (!definition.has_value())
        {
            if (errorMessage)
            {
                *errorMessage = QStringLiteral("panel %1 is invalid: %2")
                    .arg(index)
                    .arg(definitionError);
            }
            return {};
        }
        if (panelIds.contains(definition->identity.id))
        {
            if (errorMessage)
            {
                *errorMessage = QStringLiteral("panel %1 duplicates id '%2'")
                    .arg(index)
                    .arg(definition->identity.id);
            }
            return {};
        }
        panelIds.insert(definition->identity.id);
        definitions.append(*definition);
    }
    if (errorMessage)
    {
        errorMessage->clear();
    }
    return ArchDock::SettingsMigration::serializeVersionTwo(definitions);
}

bool PanelRegistry::persistPanelState(
    const QList<QVariantMap> &panels,
    const QVariantMap &globalSettings,
    QString *errorMessage)
{
    if (m_persistenceBlocked)
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral(
                "panel registry persistence is blocked pending source repair");
        }
        return false;
    }

    QString serializationError;
    const QByteArray serialized = serializePanels(panels, &serializationError);
    if (!serializationError.isEmpty())
    {
        if (errorMessage)
        {
            *errorMessage = serializationError;
        }
        return false;
    }

    QSettings settings;
    if (m_legacyRewritePending && !ensureLegacyBackupChecked(settings))
    {
        if (errorMessage)
        {
            *errorMessage = m_migrationDiagnostic;
        }
        return false;
    }
    settings.setValue(QString::fromLatin1(kPanelRecordsKey), serialized);
    DockSettings::writeTransaction(settings, globalSettings);
    settings.sync();
    if (settings.status() != QSettings::NoError)
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("QSettings could not persist the settings transaction");
        }
        return false;
    }

    QSettings verifier;
    if (verifier.value(QString::fromLatin1(kPanelRecordsKey)).toByteArray() != serialized)
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("panel settings transaction readback did not match");
        }
        return false;
    }
    for (auto it = globalSettings.cbegin(); it != globalSettings.cend(); ++it)
    {
        if (verifier.value(QStringLiteral("dock/") + it.key()) != it.value())
        {
            if (errorMessage)
            {
                *errorMessage = QStringLiteral(
                    "global settings transaction readback did not match field: %1")
                    .arg(it.key());
            }
            return false;
        }
    }

    m_legacySource.clear();
    m_legacyRewritePending = false;
    setMigrationDiagnostic(QString{});
    if (errorMessage)
    {
        errorMessage->clear();
    }
    return true;
}

void PanelRegistry::setMigrationDiagnostic(const QString &diagnostic)
{
    if (m_migrationDiagnostic == diagnostic)
    {
        return;
    }
    m_migrationDiagnostic = diagnostic;
    emit migrationDiagnosticChanged();
}

void PanelRegistry::changed(bool nativeTopologyChanged)
{
    ++m_revision;
    emit panelsChanged();
    if (nativeTopologyChanged)
    {
        emit nativePanelTopologyChanged();
    }
    emit revisionChanged();
}
