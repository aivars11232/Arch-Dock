// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// One look at the pinned repository's newest release, as LCL's source.rs and
// check.rs do: GitHub's /releases/latest (a draft or a pre-release is never
// seen), then the release's update-manifest.json and update-manifest.sig,
// fetched by name from that release at addresses built here, never from
// addresses in GitHub's answer. The signature comes first; only then is the
// manifest read and compared with the installed Arch Dock and the system.

#include "updates/UpdateCore.h"

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QPointer>

#include <functional>

class QNetworkAccessManager;
class QNetworkReply;

namespace ArchDock::Updates
{
// GitHub's answer for the newest release.
struct LatestAnswer
{
    enum class Kind
    {
        UpToDate,   // no normal release, or one that is no update release
        Release,
        Problem,
    };
    Kind kind = Kind::Problem;
    QString tag;
    QString version;
    QHash<QString, qint64> assets; // name -> listed size
    QString line;                  // the problem's line: L2, L3 or L4
    QDateTime retryAfter;          // for L3
    QString detail;
};

// The answer for /releases/latest: its HTTP status, body and headers (names
// in lowercase), read at `now`. Pure, so tests feed it saved answers.
LatestAnswer readLatest(int status, const QByteArray &body, const QHash<QByteArray, QByteArray> &headers,
                        const QDateTime &now, bool testBuild);

// The file of these three that the release lacks, or empty: its manifest,
// signature and package.
QString missingAsset(const LatestAnswer &latest);
// Where a release's file is fetched: built here from the pinned repository,
// never taken from GitHub's answer.
QString assetAddress(const QString &downloads, const QString &repository, const QString &tag, const QString &name);

// The result of one look.
struct LookResult
{
    enum class Outcome
    {
        UpToDate,
        Found,
        InstallByHand, // L7: another format, a newer updater, or a minimum above the installed version
        Problem,       // `line` says which
    };
    Outcome outcome = Outcome::Problem;
    QString line;
    QString detail;
    QDateTime retryAfter;
    QString installedVersion;
    QString byHandVersion;
    Manifest manifest;
    QByteArray manifestBytes;
    QByteArray signatureBytes;
    // Needs the system does not meet: older than needed, or not installed.
    QStringList tooOld;
    QStringList notInstalled;
};

struct ReleaseSetup
{
    QString api = QStringLiteral("https://api.github.com");
    QString downloads = QStringLiteral("https://github.com");
    QString repository;
    QByteArray userAgent;
    QString toolFolder = QStringLiteral("/usr/bin");
    QString keyId;
    QByteArray keySpki;
    bool testBuild = false;
    int updaterProtocol = 1;
    QString workFolder;  // private to this account, for the signature check
    int timeoutMs = 30000;
};

// The installed Arch Dock's version (`pacman -Q arch-dock`), or nothing.
std::optional<QString> installedArchDock(CommandRunner &runner, const QString &toolFolder);

// What a verified release means here: the manifest's checks against the
// listed assets, this updater and the installed version, then its needs.
LookResult judgeRelease(const LatestAnswer &latest, const QByteArray &manifestBytes, const QByteArray &signatureBytes,
                        CommandRunner &runner, const ReleaseSetup &setup);

class ReleaseSource final : public QObject
{
    Q_OBJECT

public:
    ReleaseSource(QNetworkAccessManager *network, CommandRunner &runner, ReleaseSetup setup, QObject *parent = nullptr);
    ~ReleaseSource() override;

    // One look; `finished` follows, once.
    void look();
    [[nodiscard]] bool busy() const { return m_busy; }

Q_SIGNALS:
    void finished(const ArchDock::Updates::LookResult &result);

private:
    void get(const QString &address, qint64 limit, std::function<void(int, QByteArray, QHash<QByteArray, QByteArray>, int)> done);
    void end(const LookResult &result);

    QNetworkAccessManager *m_network;
    CommandRunner &m_runner;
    ReleaseSetup m_setup;
    QList<QPointer<QNetworkReply>> m_replies;
    LatestAnswer m_latest;
    QByteArray m_manifest;
    bool m_busy = false;
};
} // namespace ArchDock::Updates

Q_DECLARE_METATYPE(ArchDock::Updates::LookResult)
