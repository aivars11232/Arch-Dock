// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// What Arch Dock's Update button and its root helper both rely on: reading a
// release's signed update manifest (format 1), checking its signature with
// the one key the build trusts, pacman's order of versions and pacman's form
// of a dependency. Only Qt Core's public API, so a Qt update cannot take it
// away. Every system tool runs by its full path through a CommandRunner, so
// tests can script and record each call.

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <optional>

namespace ArchDock::Updates
{
// Runs a system tool by its full path with a fixed environment (PATH=/usr/bin,
// LC_ALL=C). Its standard input is `input`, or nothing.
class CommandRunner
{
public:
    struct Result
    {
        bool started = false;
        int exitCode = -1;
        QByteArray output;
        QByteArray errors;
    };
    virtual ~CommandRunner() = default;
    virtual Result run(const QString &program, const QStringList &arguments, const QByteArray &input = {}) = 0;
    // The same, with this login's session added to the fixed environment:
    // `session` holds NAME=value entries (XDG_RUNTIME_DIR, the session bus),
    // for a tool that talks to the user's service manager, such as
    // `systemd-run --user`. A runner that has no session refuses.
    virtual Result runInSession(const QString &program, const QStringList &arguments, const QStringList &session)
    {
        Q_UNUSED(program)
        Q_UNUSED(arguments)
        Q_UNUSED(session)
        return {};
    }
};

// The real tools, through QProcess.
class SystemCommandRunner final : public CommandRunner
{
public:
    Result run(const QString &program, const QStringList &arguments, const QByteArray &input = {}) override;
    Result runInSession(const QString &program, const QStringList &arguments, const QStringList &session) override;

private:
    Result start(const QString &program, const QStringList &arguments, const QByteArray &input, const QStringList &session);
};

struct PackageEntry
{
    QString file;
    qint64 size = 0;
    QString sha256;
    QString arch;
    QStringList needs;
    qint64 requiredUpdaterVersion = 0;
};

struct Manifest
{
    QString version;
    QString releaseTag;
    QString sourceCommit;
    QString publishedAt;
    QString releaseNotes;
    QString minimumSupportedVersion;
    QString signingKeyId;
    PackageEntry package;
};

struct ManifestReading
{
    // Why a manifest was refused, which decides the helper's exit code and
    // the page's line.
    enum class Refusal
    {
        None,
        NoKey,         // this build trusts no key
        NotSigned,     // the signature does not verify with the trusted key
        CouldNotCheck, // the check's files could not be written on this PC
        WrongKey,      // signed, but the manifest names another key
        OtherFormat,   // a format this Arch Dock cannot read
        Malformed,     // any other reading rule
    };
    std::optional<Manifest> manifest;
    Refusal refusal = Refusal::None;
    // Why it was refused, in words for Copy Details.
    QString problem;
    // A manifest of another format: the version it names, if it names one,
    // for the line that says it must be installed by hand.
    bool otherFormat = false;
    QString otherFormatVersion;
};

// The limits of format 1.
inline constexpr qint64 MaxManifestBytes = 64 * 1024;
inline constexpr qint64 MaxSignatureBytes = 1024;
inline constexpr qint64 MinPackageBytes = 1024 * 1024;
inline constexpr qint64 MaxPackageBytes = 256 * 1024 * 1024;
inline constexpr int MaxNotes = 20000;
inline constexpr int MaxNeeds = 64;

// A release version: "0.1.1-56"; a TEST build also reads "0.1.1-56.1".
bool isReleaseVersion(const QString &version, bool testBuild);
// pacman's dependency form, as makepkg checks it, without the characters at
// which pacman's own reader would split differently.
bool isPacmanDependency(const QString &need);
// pacman's order of two versions, through vercmp: -1, 0 or 1, or nothing when
// vercmp could not say.
std::optional<int> compareVersions(CommandRunner &runner, const QString &toolFolder, const QString &a,
                                   const QString &b);

// The manifest, once every reading rule holds. Read only after its signature
// verified (signatureVerifies); the minimum version is compared with vercmp.
ManifestReading readManifest(const QByteArray &bytes, bool testBuild, CommandRunner &runner,
                             const QString &toolFolder);

// Whether openssl says `signature` over the exact bytes `manifest` verifies
// with the public key `spki` (the DER of an ECDSA P-256 SubjectPublicKeyInfo).
// The key, manifest and signature are written into `workFolder`, which must
// be private to the caller. When they cannot be written, nothing is checked
// and `couldNotCheck` (if given) is set.
bool signatureVerifies(CommandRunner &runner, const QString &toolFolder, const QByteArray &spki,
                       const QByteArray &manifest, const QByteArray &signature, const QString &workFolder,
                       bool *couldNotCheck = nullptr);

// A release's manifest as the Update button and the helper use it: the
// signature first, with the one key this build trusts (`keyId`, `spki`); only
// then is the manifest read, and it must name that key. A key a manifest names
// is never trusted.
ManifestReading readVerifiedManifest(CommandRunner &runner, const QString &toolFolder, const QString &keyId,
                                     const QByteArray &spki, const QByteArray &manifest,
                                     const QByteArray &signature, bool testBuild, const QString &workFolder);
} // namespace ArchDock::Updates
