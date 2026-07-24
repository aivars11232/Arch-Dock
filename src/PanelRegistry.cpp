#include "PanelRegistry.h"

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
#include <QSettings>
#include <QStandardPaths>
#include <QtGlobal>

namespace
{
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

bool isPanelShape(const QString &shape)
{
    return shape == QStringLiteral("pill") ||
           shape == QStringLiteral("rounded") ||
           shape == QStringLiteral("hexagon");
}

bool isIconShape(const QString &shape)
{
    return shape == QStringLiteral("rounded") ||
           shape == QStringLiteral("square") ||
           shape == QStringLiteral("squircle") ||
           shape == QStringLiteral("circle") ||
           shape == QStringLiteral("hexagon");
}

bool isAppearance(const QString &appearance)
{
    return appearance == QStringLiteral("glass") ||
           appearance == QStringLiteral("crystal") ||
           appearance == QStringLiteral("neon") ||
           appearance == QStringLiteral("minimal") ||
           appearance == QStringLiteral("plasma") ||
           appearance == QStringLiteral("lime") ||
           appearance == QStringLiteral("floating-glass") ||
           appearance == QStringLiteral("metallic") ||
           appearance == QStringLiteral("futuristic") ||
           appearance == QStringLiteral("organic") ||
           appearance == QStringLiteral("platform") ||
           appearance == QStringLiteral("plate") ||
           appearance == QStringLiteral("pedestal");
}

bool isLayout(const QString &layout)
{
    return layout == QStringLiteral("adaptive") ||
           layout == QStringLiteral("horizontal") ||
           layout == QStringLiteral("vertical") ||
           layout == QStringLiteral("diagonal") ||
           layout == QStringLiteral("circular") ||
           layout == QStringLiteral("ellipse") ||
           layout == QStringLiteral("ring") ||
           layout == QStringLiteral("radial") ||
           layout == QStringLiteral("arc") ||
           layout == QStringLiteral("semicircle") ||
           layout == QStringLiteral("fan") ||
           layout == QStringLiteral("spiral") ||
           layout == QStringLiteral("ribbon") ||
           layout == QStringLiteral("vertical-curve") ||
           layout == QStringLiteral("horizontal-curve") ||
           layout == QStringLiteral("polygon") ||
           layout == QStringLiteral("triangle") ||
           layout == QStringLiteral("square") ||
           layout == QStringLiteral("pentagon") ||
           layout == QStringLiteral("hexagon") ||
           layout == QStringLiteral("octagon") ||
           layout == QStringLiteral("star") ||
           layout == QStringLiteral("grid") ||
           layout == QStringLiteral("floating");
}

bool isPathOrientation(const QString &orientation)
{
    return orientation == QStringLiteral("upright") ||
           orientation == QStringLiteral("tangent") ||
           orientation == QStringLiteral("radial");
}

bool isPathAnchor(const QString &anchor)
{
    return anchor == QStringLiteral("top-left") ||
           anchor == QStringLiteral("top") ||
           anchor == QStringLiteral("top-right") ||
           anchor == QStringLiteral("left") ||
           anchor == QStringLiteral("center") ||
           anchor == QStringLiteral("right") ||
           anchor == QStringLiteral("bottom-left") ||
           anchor == QStringLiteral("bottom") ||
           anchor == QStringLiteral("bottom-right");
}

bool isIconAnimation(const QString &animation)
{
    return animation == QStringLiteral("none") ||
           animation == QStringLiteral("bounce") ||
           animation == QStringLiteral("elastic") ||
           animation == QStringLiteral("pulse") ||
           animation == QStringLiteral("scale") ||
           animation == QStringLiteral("spin") ||
           animation == QStringLiteral("idle-rotate") ||
           animation == QStringLiteral("orbit") ||
           animation == QStringLiteral("swing") ||
           animation == QStringLiteral("wobble") ||
           animation == QStringLiteral("wiggle") ||
           animation == QStringLiteral("shake") ||
           animation == QStringLiteral("glow") ||
           animation == QStringLiteral("breathe") ||
           animation == QStringLiteral("float") ||
           animation == QStringLiteral("wave") ||
           animation == QStringLiteral("ripple") ||
           animation == QStringLiteral("magnetic") ||
           animation == QStringLiteral("spring");
}

bool isAnimationTrigger(const QString &trigger)
{
    return trigger == QStringLiteral("hover") ||
           trigger == QStringLiteral("click") ||
           trigger == QStringLiteral("launch") ||
           trigger == QStringLiteral("running") ||
           trigger == QStringLiteral("drop") ||
           trigger == QStringLiteral("reveal") ||
           trigger == QStringLiteral("idle");
}

bool isFolderLayout(const QString &layout)
{
    return layout == QStringLiteral("fan") ||
           layout == QStringLiteral("grid") ||
           layout == QStringLiteral("stack") ||
           layout == QStringLiteral("arc") ||
           layout == QStringLiteral("spiral") ||
           layout == QStringLiteral("circular") ||
           layout == QStringLiteral("radial") ||
           layout == QStringLiteral("vertical") ||
           layout == QStringLiteral("horizontal") ||
           layout == QStringLiteral("elastic") ||
           layout == QStringLiteral("physics");
}

bool isFolderEasing(const QString &easing)
{
    return easing == QStringLiteral("outCubic") ||
           easing == QStringLiteral("outBack") ||
           easing == QStringLiteral("outElastic") ||
           easing == QStringLiteral("spring");
}

bool isThemeFit(const QString &fit)
{
    return fit == QStringLiteral("cover") ||
           fit == QStringLiteral("contain") ||
           fit == QStringLiteral("stretch") ||
           fit == QStringLiteral("tile");
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

}

PanelRegistry::PanelRegistry(QObject *parent)
    : QObject(parent)
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

QVariant PanelRegistry::panelValue(const QString &panelId, const QString &key) const
{
    const QVariantMap *panel = record(panelId);
    return panel ? panel->value(key) : QVariant{};
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
    m_panels.append(panel);
    m_activePanelId = id;
    save();
    emit activePanelIdChanged();
    changed();
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
    panel.insert(QStringLiteral("type"), QStringLiteral("hybrid"));
    panel.insert(QStringLiteral("layout"), QStringLiteral("circular"));
    panel.insert(QStringLiteral("width"), 420);
    panel.insert(QStringLiteral("height"), 420);
    panel.insert(QStringLiteral("layoutRadius"), 145);
    panel.insert(QStringLiteral("x"), 240);
    panel.insert(QStringLiteral("y"), 180);
    m_panels.append(panel);
    m_activePanelId = id;
    save();
    emit activePanelIdChanged();
    changed();
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
        changed();
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
    const bool vertical = edge == QStringLiteral("left") || edge == QStringLiteral("right");
    return {
        {QStringLiteral("id"), id},
        {QStringLiteral("name"), name},
        {QStringLiteral("builtIn"), builtIn},
        {QStringLiteral("visible"), edge == QStringLiteral("bottom")},
        {QStringLiteral("edge"), edge},
        {QStringLiteral("alignment"), QStringLiteral("center")},
        {QStringLiteral("screen"), 0},
        {QStringLiteral("screenId"), QString{}},
        {QStringLiteral("visibilityMode"), QStringLiteral("always")},
        {QStringLiteral("revealZone"), 10},
        {QStringLiteral("dynamic"), edge != QStringLiteral("bottom")},
        {QStringLiteral("width"), vertical ? 76 : 720},
        {QStringLiteral("height"), vertical ? 420 : 76},
        {QStringLiteral("x"), 180},
        {QStringLiteral("y"), 180},
        {QStringLiteral("type"), QStringLiteral("hybrid")},
        {QStringLiteral("appearance"), QStringLiteral("glass")},
        {QStringLiteral("shape"), QStringLiteral("pill")},
        {QStringLiteral("iconShape"), QStringLiteral("rounded")},
        {QStringLiteral("iconSize"), 52},
        {QStringLiteral("spacing"), 8.0},
        {QStringLiteral("layout"), QStringLiteral("adaptive")},
        {QStringLiteral("layoutScale"), 1.0},
        {QStringLiteral("layoutAngle"), 0.0},
        {QStringLiteral("layoutRadius"), 150},
        {QStringLiteral("layoutRows"), 2},
        {QStringLiteral("layoutPadding"), 18},
        {QStringLiteral("pathSides"), 6},
        {QStringLiteral("pathOrientation"), QStringLiteral("upright")},
        {QStringLiteral("pathAnchor"), QStringLiteral("center")},
        {QStringLiteral("iconAnimation"), QStringLiteral("scale")},
        {QStringLiteral("animationTrigger"), QStringLiteral("hover")},
        {QStringLiteral("animationSpeed"), 1.0},
        {QStringLiteral("animationIntensity"), 1.0},
        {QStringLiteral("physicsEnabled"), false},
        {QStringLiteral("folderLayout"), QStringLiteral("fan")},
        {QStringLiteral("folderSpeed"), 260},
        {QStringLiteral("folderEasing"), QStringLiteral("outBack")},
        {QStringLiteral("folderExpandOnClick"), true},
        {QStringLiteral("opacity"), 0.9},
        {QStringLiteral("color"), QString{}},
        {QStringLiteral("themeAsset"), QString{}},
        {QStringLiteral("themeSource"), QString{}},
        {QStringLiteral("themeFit"), QStringLiteral("cover")},
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
        {QStringLiteral("themeRenderOutcome"), QString{}},
        {QStringLiteral("acceptDrops"), true},
        {QStringLiteral("kdeWidgets"), QStringList{}},
        {QStringLiteral("nativePanelId"), -1},
        {QStringLiteral("nativeControlAppletId"), -1},
        {QStringLiteral("nativeDockAppletId"), -1},
        {QStringLiteral("nativeOwnershipToken"), QString{}}};
}

QVariant PanelRegistry::normalizeValue(const QString &key, const QVariant &value) const
{
    if (key == QStringLiteral("edge"))
    {
        const QString edge = value.toString().trimmed().toLower();
        return isEdge(edge) ? edge : QStringLiteral("bottom");
    }
    if (key == QStringLiteral("alignment"))
    {
        const QString alignment = value.toString().trimmed().toLower();
        return alignment == QStringLiteral("start") ||
                alignment == QStringLiteral("center") ||
                alignment == QStringLiteral("end")
            ? alignment : QStringLiteral("center");
    }
    if (key == QStringLiteral("screen"))
    {
        return qMax(0, value.toInt());
    }
    if (key == QStringLiteral("screenId"))
    {
        return value.toString().trimmed();
    }
    if (key == QStringLiteral("visibilityMode"))
    {
        const QString mode = value.toString().trimmed().toLower();
        return isVisibilityMode(mode) ? mode : QStringLiteral("always");
    }
    if (key == QStringLiteral("revealZone"))
    {
        return qBound(1, value.toInt(), 64);
    }
    if (key == QStringLiteral("type"))
    {
        const QString type = value.toString().trimmed().toLower();
        return isPanelType(type) ? type : QStringLiteral("hybrid");
    }
    if (key == QStringLiteral("shape"))
    {
        const QString shape = value.toString().trimmed().toLower();
        return isPanelShape(shape) ? shape : QStringLiteral("pill");
    }
    if (key == QStringLiteral("iconShape"))
    {
        const QString shape = value.toString().trimmed().toLower();
        return isIconShape(shape) ? shape : QStringLiteral("rounded");
    }
    if (key == QStringLiteral("appearance"))
    {
        const QString appearance = value.toString().trimmed().toLower();
        return isAppearance(appearance) ? appearance : QStringLiteral("glass");
    }
    if (key == QStringLiteral("layout"))
    {
        const QString layout = value.toString().trimmed().toLower();
        return isLayout(layout) ? layout : QStringLiteral("adaptive");
    }
    if (key == QStringLiteral("iconAnimation"))
    {
        const QString animation = value.toString().trimmed().toLower();
        return isIconAnimation(animation) ? animation : QStringLiteral("scale");
    }
    if (key == QStringLiteral("animationTrigger"))
    {
        const QString trigger = value.toString().trimmed().toLower();
        return isAnimationTrigger(trigger) ? trigger : QStringLiteral("hover");
    }
    if (key == QStringLiteral("folderLayout"))
    {
        const QString layout = value.toString().trimmed().toLower();
        return isFolderLayout(layout) ? layout : QStringLiteral("fan");
    }
    if (key == QStringLiteral("folderEasing"))
    {
        const QString easing = value.toString().trimmed();
        return isFolderEasing(easing) ? easing : QStringLiteral("outBack");
    }
    if (key == QStringLiteral("themeFit"))
    {
        const QString fit = value.toString().trimmed().toLower();
        return isThemeFit(fit) ? fit : QStringLiteral("cover");
    }
    if (key == QStringLiteral("opacity"))
    {
        return qBound<qreal>(0.1, value.toReal(), 1.0);
    }
    if (key == QStringLiteral("iconSize"))
    {
        return qBound(24, value.toInt(), 128);
    }
    if (key == QStringLiteral("spacing"))
    {
        return qBound<qreal>(0.0, value.toReal(), 48.0);
    }
    if (key == QStringLiteral("layoutScale"))
    {
        return qBound<qreal>(0.5, value.toReal(), 2.5);
    }
    if (key == QStringLiteral("layoutAngle"))
    {
        return qBound<qreal>(-180.0, value.toReal(), 180.0);
    }
    if (key == QStringLiteral("layoutRadius"))
    {
        return qBound(48, value.toInt(), 2048);
    }
    if (key == QStringLiteral("layoutRows"))
    {
        return qBound(1, value.toInt(), 8);
    }
    if (key == QStringLiteral("layoutPadding"))
    {
        return qBound(0, value.toInt(), 240);
    }
    if (key == QStringLiteral("pathSides"))
    {
        return qBound(3, value.toInt(), 12);
    }
    if (key == QStringLiteral("pathOrientation"))
    {
        const QString orientation = value.toString().trimmed().toLower();
        return isPathOrientation(orientation) ? orientation : QStringLiteral("upright");
    }
    if (key == QStringLiteral("pathAnchor"))
    {
        const QString anchor = value.toString().trimmed().toLower();
        return isPathAnchor(anchor) ? anchor : QStringLiteral("center");
    }
    if (key == QStringLiteral("animationSpeed"))
    {
        return qBound<qreal>(0.2, value.toReal(), 3.0);
    }
    if (key == QStringLiteral("animationIntensity"))
    {
        return qBound<qreal>(0.1, value.toReal(), 2.5);
    }
    if (key == QStringLiteral("folderSpeed"))
    {
        return qBound(80, value.toInt(), 1200);
    }
    if (key == QStringLiteral("width") || key == QStringLiteral("height"))
    {
        return qBound(48, value.toInt(), 4096);
    }
    if (key == QStringLiteral("x") || key == QStringLiteral("y"))
    {
        return qMax(0, value.toInt());
    }
    if (key == QStringLiteral("visible") || key == QStringLiteral("dynamic") || key == QStringLiteral("acceptDrops") ||
        key == QStringLiteral("physicsEnabled") || key == QStringLiteral("folderExpandOnClick"))
    {
        return value.toBool();
    }
    return value;
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
    QVariantMap *panel = record(panelId);
    if (!panel)
    {
        return;
    }

    bool didChange = false;
    for (auto iterator = values.cbegin(); iterator != values.cend(); ++iterator)
    {
        if (iterator.key() == QStringLiteral("id") || iterator.key() == QStringLiteral("builtIn"))
        {
            continue;
        }

        const QVariant normalized = normalizeValue(iterator.key(), iterator.value());
        if (panel->value(iterator.key()) == normalized)
        {
            continue;
        }
        panel->insert(iterator.key(), normalized);
        didChange = true;
    }
    if (didChange)
    {
        save();
        changed();
    }
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
    bool migrated = false;
    const QJsonDocument document = QJsonDocument::fromJson(
        settings.value(QStringLiteral("dock/panels")).toByteArray());
    if (document.isArray())
    {
        for (const QJsonValue &value : document.array())
        {
            if (value.isObject())
            {
                QVariantMap panel = value.toObject().toVariantMap();
                if (!panel.value(QStringLiteral("id")).toString().isEmpty())
                {
                    const QString normalizedEdge = normalizeValue(
                        QStringLiteral("edge"),
                        panel.value(QStringLiteral("edge"))).toString();
                    if (panel.value(QStringLiteral("edge")).toString() != normalizedEdge)
                    {
                        panel.insert(QStringLiteral("edge"), normalizedEdge);
                        migrated = true;
                    }
                    if (!panel.contains(QStringLiteral("screen")))
                    {
                        panel.insert(QStringLiteral("screen"), legacyScreen);
                        migrated = true;
                    }
                    if (!panel.contains(QStringLiteral("screenId")))
                    {
                        panel.insert(QStringLiteral("screenId"), QString{});
                        migrated = true;
                    }
                    if (!panel.contains(QStringLiteral("visibilityMode")))
                    {
                        panel.insert(
                            QStringLiteral("visibilityMode"),
                            panel.value(QStringLiteral("id")).toString() == QStringLiteral("bottom") && legacyAutoHide
                                ? QStringLiteral("auto-hide")
                                : QStringLiteral("always"));
                        migrated = true;
                    }
                    if (!panel.contains(QStringLiteral("revealZone")))
                    {
                        panel.insert(QStringLiteral("revealZone"), 10);
                        migrated = true;
                    }
                    if (!panel.contains(QStringLiteral("pathSides")))
                    {
                        panel.insert(QStringLiteral("pathSides"), 6);
                        migrated = true;
                    }
                    if (!panel.contains(QStringLiteral("pathOrientation")))
                    {
                        panel.insert(QStringLiteral("pathOrientation"), QStringLiteral("upright"));
                        migrated = true;
                    }
                    if (!panel.contains(QStringLiteral("pathAnchor")))
                    {
                        panel.insert(QStringLiteral("pathAnchor"), QStringLiteral("center"));
                        migrated = true;
                    }
                    if (!panel.contains(QStringLiteral("nativeOwnershipToken")))
                    {
                        panel.insert(QStringLiteral("nativeOwnershipToken"), QString{});
                        migrated = true;
                    }
                    if (!panel.contains(QStringLiteral("nativeDockAppletId")))
                    {
                        panel.insert(QStringLiteral("nativeDockAppletId"), -1);
                        migrated = true;
                    }
                    if (migrateLegacyThemePackage(&panel))
                    {
                        migrated = true;
                    }
                    m_panels.append(panel);
                }
            }
        }
    }

    if (m_panels.isEmpty())
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

        save();
    }

    const QString requestedActive = settings.value(QStringLiteral("dock/activePanel"), m_activePanelId).toString();
    if (record(requestedActive))
    {
        m_activePanelId = requestedActive;
    }

    if (migrated)
    {
        save();
    }
}

void PanelRegistry::save() const
{
    QJsonArray panels;
    for (const QVariantMap &panel : m_panels)
    {
        panels.append(QJsonObject::fromVariantMap(panel));
    }

    QSettings settings;
    settings.setValue(
        QStringLiteral("dock/panels"),
        QJsonDocument(panels).toJson(QJsonDocument::Compact));
    settings.sync();
}

void PanelRegistry::changed()
{
    ++m_revision;
    emit panelsChanged();
    emit revisionChanged();
}
