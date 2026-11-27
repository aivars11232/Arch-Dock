// SPDX-License-Identifier: GPL-3.0-or-later
#include "ReleaseSource.h"

#include "updates/CommandRunner.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTimeZone>
#include <QTimer>

namespace ArchDock::Updates
{
namespace
{
constexpr qint64 MaxAnswerBytes = 1024 * 1024;

LookResult problem(const QString &line, const QString &detail)
{
    LookResult result;
    result.outcome = LookResult::Outcome::Problem;
    result.line = line;
    result.detail = detail;
    return result;
}

// A network error that means GitHub could not be reached at all (L1), as
// against one that came after it was reached (L2).
bool unreachable(int error)
{
    return error == QNetworkReply::HostNotFoundError || error == QNetworkReply::ConnectionRefusedError
        || error == QNetworkReply::NetworkSessionFailedError || error == QNetworkReply::UnknownNetworkError
        || error == QNetworkReply::TemporaryNetworkFailureError;
}

// The line for a release file that did not arrive whole: no answer, or a
// server error, says try again; anything else says the release is not
// complete.
LookResult fileProblem(const QString &name, int status, int error)
{
    if (status == 0)
        return problem(unreachable(error) ? QStringLiteral("L1") : QStringLiteral("L2"),
                       QStringLiteral("%1 did not arrive (network error %2)").arg(name).arg(error));
    return problem(status >= 500 ? QStringLiteral("L2") : QStringLiteral("L5"), QStringLiteral("%1 answered %2").arg(name).arg(status));
}

// The name a need asks for, without its operator and version.
QString needName(const QString &need)
{
    static const QRegularExpression operatorAt(QStringLiteral("[<>=]"));
    const qsizetype at = need.indexOf(operatorAt);
    return at < 0 ? need : need.left(at);
}
} // namespace

LatestAnswer readLatest(int status, const QByteArray &body, const QHash<QByteArray, QByteArray> &headers,
                        const QDateTime &now, bool testBuild)
{
    LatestAnswer answer;
    if (status == 404)
    {
        answer.kind = LatestAnswer::Kind::UpToDate;
        answer.detail = QStringLiteral("the repository has no normal release");
        return answer;
    }
    if (status == 403 || status == 429)
    {
        // GitHub asks to wait: until its reset time, or for the seconds it names.
        answer.line = QStringLiteral("L3");
        QDateTime until;
        bool ok = false;
        const qint64 seconds = headers.value("retry-after").toLongLong(&ok);
        if (ok && seconds > 0)
            until = now.addSecs(qMin<qint64>(seconds, 24 * 3600));
        else if (const qint64 reset = headers.value("x-ratelimit-reset").toLongLong(&ok); ok && reset > 0)
            until = QDateTime::fromSecsSinceEpoch(reset, QTimeZone::UTC);
        if (!until.isValid() || until <= now || until > now.addSecs(24 * 3600))
            until = now.addSecs(3600);
        answer.retryAfter = until;
        answer.detail = QStringLiteral("GitHub answered %1").arg(status);
        return answer;
    }
    if (status == 410)
    {
        answer.line = QStringLiteral("L4");
        answer.detail = QStringLiteral("GitHub answered 410");
        return answer;
    }
    if (status != 200)
    {
        answer.line = QStringLiteral("L2");
        answer.detail = QStringLiteral("GitHub answered %1").arg(status);
        return answer;
    }
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(body, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
    {
        answer.line = QStringLiteral("L2");
        answer.detail = QStringLiteral("GitHub's answer is not JSON");
        return answer;
    }
    const QJsonObject release = document.object();
    if (release.value(QStringLiteral("draft")).toBool() || release.value(QStringLiteral("prerelease")).toBool())
    {
        answer.kind = LatestAnswer::Kind::UpToDate;
        answer.detail = QStringLiteral("the newest release is a draft or a pre-release");
        return answer;
    }
    answer.tag = release.value(QStringLiteral("tag_name")).toString();
    answer.version = answer.tag.mid(1);
    if (!answer.tag.startsWith(u'v') || !isReleaseVersion(answer.version, testBuild))
    {
        // Not an update release (an unsafe tag, or one without a package release).
        answer.kind = LatestAnswer::Kind::UpToDate;
        answer.detail = QStringLiteral("the newest release's tag %1 is not v<version>-<release>").arg(answer.tag);
        return answer;
    }
    for (const QJsonValue &value : release.value(QStringLiteral("assets")).toArray())
    {
        const QJsonObject asset = value.toObject();
        const QString name = asset.value(QStringLiteral("name")).toString();
        if (!name.isEmpty())
            answer.assets.insert(name, asset.value(QStringLiteral("size")).toInteger(-1));
    }
    answer.kind = LatestAnswer::Kind::Release;
    return answer;
}

QString missingAsset(const LatestAnswer &latest)
{
    const QString file = QStringLiteral("arch-dock-%1-x86_64.pkg.tar.zst").arg(latest.version);
    for (const QString &needed : {QStringLiteral("update-manifest.json"), QStringLiteral("update-manifest.sig"), file})
        if (!latest.assets.contains(needed))
            return needed;
    return {};
}

QString assetAddress(const QString &downloads, const QString &repository, const QString &tag, const QString &name)
{
    return QStringLiteral("%1/%2/releases/download/%3/%4").arg(downloads, repository, tag, name);
}

std::optional<QString> installedArchDock(CommandRunner &runner, const QString &toolFolder)
{
    const auto answer = runner.run(toolFolder + QStringLiteral("/pacman"), {QStringLiteral("-Q"), QStringLiteral("arch-dock")});
    if (!answer.started || answer.exitCode != 0)
        return std::nullopt;
    const QStringList fields = QString::fromUtf8(answer.output).trimmed().split(u' ');
    if (fields.size() != 2 || fields.at(0) != QLatin1String("arch-dock") || fields.at(1).contains(u'\n'))
        return std::nullopt;
    return fields.at(1);
}

LookResult judgeRelease(const LatestAnswer &latest, const QByteArray &manifestBytes, const QByteArray &signatureBytes,
                        CommandRunner &runner, const ReleaseSetup &setup)
{
    const ManifestReading reading = readVerifiedManifest(runner, setup.toolFolder, setup.keyId, setup.keySpki,
                                                         manifestBytes, signatureBytes, setup.testBuild, setup.workFolder);
    if (!reading.manifest)
    {
        using Refusal = ManifestReading::Refusal;
        if (reading.refusal == Refusal::OtherFormat)
        {
            // The installed Arch Dock first here too: a release in another
            // format that is not newer is no reason to install by hand.
            const QString named = reading.otherFormatVersion.isEmpty() ? latest.version : reading.otherFormatVersion;
            const std::optional<QString> installed = installedArchDock(runner, setup.toolFolder);
            if (!installed)
                return problem(QStringLiteral("L2"), QStringLiteral("pacman gave no answer for the installed Arch Dock"));
            const std::optional<int> newer = compareVersions(runner, setup.toolFolder, named, *installed);
            LookResult result;
            result.installedVersion = *installed;
            if (newer && *newer <= 0)
            {
                result.outcome = LookResult::Outcome::UpToDate;
                return result;
            }
            result.outcome = LookResult::Outcome::InstallByHand;
            result.line = QStringLiteral("L7");
            result.byHandVersion = named;
            result.detail = reading.problem;
            return result;
        }
        if (reading.refusal == Refusal::CouldNotCheck)
            return problem(QStringLiteral("L34"), reading.problem);
        const bool unsigned_ = reading.refusal == Refusal::NotSigned || reading.refusal == Refusal::WrongKey
            || reading.refusal == Refusal::NoKey;
        return problem(unsigned_ ? QStringLiteral("L6") : QStringLiteral("L5"), reading.problem);
    }
    const Manifest &manifest = *reading.manifest;
    if (manifest.releaseTag != latest.tag)
        return problem(QStringLiteral("L5"), QStringLiteral("the manifest is for %1, the release is %2").arg(manifest.releaseTag, latest.tag));
    if (latest.assets.value(manifest.package.file, -1) != manifest.package.size)
        return problem(QStringLiteral("L5"), QStringLiteral("the release's %1 is not the signed size").arg(manifest.package.file));
    const std::optional<QString> installed = installedArchDock(runner, setup.toolFolder);
    if (!installed)
        return problem(QStringLiteral("L2"), QStringLiteral("pacman gave no answer for the installed Arch Dock"));

    LookResult result;
    result.installedVersion = *installed;
    result.manifest = manifest;
    result.manifestBytes = manifestBytes;
    result.signatureBytes = signatureBytes;
    // The installed Arch Dock first: nothing newer is "up to date", whatever GitHub says.
    const std::optional<int> newer = compareVersions(runner, setup.toolFolder, manifest.version, *installed);
    if (!newer)
        return problem(QStringLiteral("L2"), QStringLiteral("vercmp could not compare the versions"));
    if (*newer <= 0)
    {
        result.outcome = LookResult::Outcome::UpToDate;
        return result;
    }
    const std::optional<int> minimum = compareVersions(runner, setup.toolFolder, *installed, manifest.minimumSupportedVersion);
    if (manifest.package.requiredUpdaterVersion > setup.updaterProtocol || !minimum || *minimum < 0)
    {
        result.outcome = LookResult::Outcome::InstallByHand;
        result.line = QStringLiteral("L7");
        result.byHandVersion = manifest.version;
        return result;
    }
    const QStringList &needs = manifest.package.needs;
    if (!needs.isEmpty())
    {
        const auto test = runner.run(setup.toolFolder + QStringLiteral("/pacman"), QStringList{QStringLiteral("-T"), QStringLiteral("--")} + needs);
        if (test.started && test.exitCode != 0)
        {
            for (const QString &unmet : QString::fromUtf8(test.output).split(u'\n', Qt::SkipEmptyParts))
            {
                // Without its version a need tells "too old" from "not installed".
                const QString name = needName(unmet.trimmed());
                const auto present = runner.run(setup.toolFolder + QStringLiteral("/pacman"),
                                                {QStringLiteral("-T"), QStringLiteral("--"), name});
                if (present.started && present.exitCode == 0)
                    result.tooOld.append(unmet.trimmed());
                else
                    result.notInstalled.append(name);
            }
        }
    }
    result.outcome = LookResult::Outcome::Found;
    return result;
}

ReleaseSource::ReleaseSource(QNetworkAccessManager *network, CommandRunner &runner, ReleaseSetup setup, QObject *parent)
    : QObject(parent)
    , m_network(network)
    , m_runner(runner)
    , m_setup(std::move(setup))
{
}

ReleaseSource::~ReleaseSource()
{
    for (const QPointer<QNetworkReply> &reply : std::as_const(m_replies))
        if (reply)
        {
            reply->disconnect(this);
            reply->abort();
        }
}

void ReleaseSource::get(const QString &address, qint64 limit,
                        std::function<void(int, QByteArray, QHash<QByteArray, QByteArray>, int)> done)
{
    if (!networkAllowed(address) && !(m_setup.testBuild && address.startsWith(QLatin1String("http://127.0.0.1:"))))
    {
        QTimer::singleShot(0, this, [done] { done(-1, {}, {}, QNetworkReply::ProtocolUnknownError); });
        return;
    }
    QNetworkRequest request{QUrl(address)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setMaximumRedirectsAllowed(5);
    request.setTransferTimeout(m_setup.timeoutMs);
    request.setHeader(QNetworkRequest::UserAgentHeader, m_setup.userAgent);
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("X-GitHub-Api-Version", "2026-03-10");
    QNetworkReply *reply = m_network->get(request);
    m_replies.append(reply);
    connect(reply, &QNetworkReply::downloadProgress, this, [reply, limit](qint64 received, qint64)
    {
        if (received > limit)
            reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, limit, done]
    {
        reply->deleteLater();
        m_replies.removeAll(reply);
        QHash<QByteArray, QByteArray> headers;
        for (const auto &[name, value] : reply->rawHeaderPairs())
            headers.insert(name.toLower(), value);
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QByteArray body = reply->readAll();
        if (body.size() > limit)
            return done(0, {}, headers, QNetworkReply::ContentReSendError);
        // Qt keeps the status of the last headers it saw: 200 when a body
        // stalled or was cut after them, 3xx when a redirect's target could
        // not be reached. An error below 400 is therefore no whole answer.
        if (reply->error() != QNetworkReply::NoError && status < 400)
            return done(0, {}, headers, reply->error());
        done(status, body, headers, reply->error());
    });
}

void ReleaseSource::look()
{
    if (m_busy)
        return;
    m_busy = true;
    const QString address = QStringLiteral("%1/repos/%2/releases/latest").arg(m_setup.api, m_setup.repository);
    get(address, MaxAnswerBytes, [this](int status, const QByteArray &body, const QHash<QByteArray, QByteArray> &headers, int error)
    {
        if (status == -1)
            return end(problem(QStringLiteral("L2"), QStringLiteral("this build does not use the network for updates")));
        if (status == 0)
            return end(problem(unreachable(error) ? QStringLiteral("L1") : QStringLiteral("L2"),
                               QStringLiteral("no answer from GitHub (network error %1)").arg(error)));
        m_latest = readLatest(status, body, headers, QDateTime::currentDateTimeUtc(), m_setup.testBuild);
        if (m_latest.kind == LatestAnswer::Kind::UpToDate)
        {
            LookResult result;
            result.outcome = LookResult::Outcome::UpToDate;
            result.detail = m_latest.detail;
            if (const auto installed = installedArchDock(m_runner, m_setup.toolFolder))
                result.installedVersion = *installed;
            return end(result);
        }
        if (m_latest.kind == LatestAnswer::Kind::Problem)
        {
            LookResult result = problem(m_latest.line, m_latest.detail);
            result.retryAfter = m_latest.retryAfter;
            return end(result);
        }
        if (const QString missing = missingAsset(m_latest); !missing.isEmpty())
            return end(problem(QStringLiteral("L5"), QStringLiteral("the release %1 has no %2").arg(m_latest.tag, missing)));
        const QString base = assetAddress(m_setup.downloads, m_setup.repository, m_latest.tag, QString());
        get(base + QStringLiteral("update-manifest.json"), MaxManifestBytes,
            [this, base](int status, const QByteArray &manifest, const QHash<QByteArray, QByteArray> &, int error)
        {
            if (status != 200)
                return end(fileProblem(QStringLiteral("update-manifest.json"), status, error));
            m_manifest = manifest;
            get(base + QStringLiteral("update-manifest.sig"), MaxSignatureBytes,
                [this](int status, const QByteArray &signature, const QHash<QByteArray, QByteArray> &, int error)
            {
                if (status != 200)
                    return end(fileProblem(QStringLiteral("update-manifest.sig"), status, error));
                // The check's own folder, made again should it have gone.
                QDir().mkpath(m_setup.workFolder);
                QFile::setPermissions(m_setup.workFolder, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
                end(judgeRelease(m_latest, m_manifest, signature, m_runner, m_setup));
            });
        });
    });
}

void ReleaseSource::end(const LookResult &result)
{
    m_busy = false;
    Q_EMIT finished(result);
}
} // namespace ArchDock::Updates
