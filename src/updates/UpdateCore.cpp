// SPDX-License-Identifier: GPL-3.0-or-later
#include "UpdateCore.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QList>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSet>
#include <QStringDecoder>

#include <cmath>
#include <limits>

namespace ArchDock::Updates
{
namespace
{
// Every rule matches the whole value (anchoredPattern): a plain ^...$ lets a
// trailing newline through. Qt's \d is 0-9 only, as with Python's re.ASCII.
bool matches(const QRegularExpression &pattern, const QString &value)
{
    return pattern.match(value).hasMatch();
}

QRegularExpression whole(const QString &pattern)
{
    return QRegularExpression(QRegularExpression::anchoredPattern(pattern));
}

const QStringList TopKeys = {
    QStringLiteral("format"), QStringLiteral("product"), QStringLiteral("channel"), QStringLiteral("version"),
    QStringLiteral("release_tag"), QStringLiteral("source_commit"), QStringLiteral("published_at"),
    QStringLiteral("release_notes"), QStringLiteral("minimum_supported_version"),
    QStringLiteral("signing_key_id"), QStringLiteral("package")};
const QStringList PackageKeys = {
    QStringLiteral("file"), QStringLiteral("size"), QStringLiteral("sha256"), QStringLiteral("arch"),
    QStringLiteral("needs"), QStringLiteral("required_updater_version")};

// QJsonDocument keeps one of two equal keys, decodes escapes and lets raw
// control characters through inside strings, so the text itself is scanned
// for an object that names one key twice, a key spelled with a backslash
// escape, and a control character (below U+0020) written raw inside a
// string, which strict JSON and tools/release-files.py refuse.
QString keyProblem(const QString &text)
{
    QList<std::optional<QSet<QString>>> stack;
    bool expectKey = false;
    qsizetype index = 0;
    const qsizetype length = text.size();
    while (index < length)
    {
        const QChar character = text.at(index);
        if (character == u'"')
        {
            qsizetype end = index + 1;
            while (end < length && text.at(end) != u'"')
            {
                const qsizetype step = text.at(end) == u'\\' ? 2 : 1;
                for (qsizetype at = end; at < qMin(end + step, length); ++at)
                    if (text.at(at).unicode() < 0x20)
                        return QStringLiteral("the manifest holds a raw control character inside a text");
                end += step;
            }
            const QString raw = text.mid(index + 1, end - index - 1);
            if (expectKey && !stack.isEmpty() && stack.last().has_value())
            {
                if (raw.contains(u'\\'))
                    return QStringLiteral("the manifest spells a key with an escape: %1").arg(raw);
                if (stack.last()->contains(raw))
                    return QStringLiteral("the manifest names the key %1 twice").arg(raw);
                stack.last()->insert(raw);
                expectKey = false;
            }
            index = end + 1;
            continue;
        }
        if (character == u'{')
        {
            stack.append(QSet<QString>{});
            expectKey = true;
        }
        else if (character == u'[')
        {
            stack.append(std::nullopt);
            expectKey = false;
        }
        else if (character == u'}' || character == u']')
        {
            if (!stack.isEmpty())
                stack.removeLast();
            expectKey = false;
        }
        else if (character == u',')
            expectKey = !stack.isEmpty() && stack.last().has_value();
        else if (character == u':')
            expectKey = false;
        ++index;
    }
    return {};
}

// Every text is valid Unicode: no lone surrogates.
bool textsValid(const QJsonValue &value)
{
    if (value.isString())
        return QStringView(value.toString()).isValidUtf16();
    if (value.isArray())
    {
        for (const QJsonValue &item : value.toArray())
            if (!textsValid(item))
                return false;
    }
    if (value.isObject())
    {
        const QJsonObject object = value.toObject();
        for (auto it = object.begin(); it != object.end(); ++it)
            if (!textsValid(it.value()))
                return false;
    }
    return true;
}

bool hasExactly(const QJsonObject &object, const QStringList &keys)
{
    QStringList present = object.keys();
    QStringList wanted = keys;
    present.sort();
    wanted.sort();
    return present == wanted;
}

// A JSON number read by value (4.0 is the number 4), never true or false.
std::optional<qint64> wholeNumber(const QJsonValue &value, qint64 low, qint64 high)
{
    if (!value.isDouble())
        return std::nullopt;
    const double number = value.toDouble();
    if (!std::isfinite(number) || std::trunc(number) != number)
        return std::nullopt;
    constexpr qint64 none = std::numeric_limits<qint64>::min();
    const qint64 integer = value.toInteger(none);
    if (integer == none || integer < low || integer > high)
        return std::nullopt;
    return integer;
}
} // namespace

CommandRunner::Result SystemCommandRunner::run(const QString &program, const QStringList &arguments,
                                               const QByteArray &input)
{
    return start(program, arguments, input, {});
}

CommandRunner::Result SystemCommandRunner::runInSession(const QString &program, const QStringList &arguments,
                                                        const QStringList &session)
{
    return start(program, arguments, {}, session);
}

CommandRunner::Result SystemCommandRunner::start(const QString &program, const QStringList &arguments,
                                                 const QByteArray &input, const QStringList &session)
{
    Result result;
    QProcess process;
    QProcessEnvironment environment;
    for (const QString &entry : session)
    {
        const qsizetype equals = entry.indexOf(u'=');
        if (equals > 0)
            environment.insert(entry.left(equals), entry.mid(equals + 1));
    }
    // The fixed part last, so no session entry replaces it.
    environment.insert(QStringLiteral("PATH"), QStringLiteral("/usr/bin"));
    environment.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    process.setProcessEnvironment(environment);
    process.setProgram(program);
    process.setArguments(arguments);
    if (input.isEmpty())
        process.setStandardInputFile(QProcess::nullDevice());
    process.start();
    if (!process.waitForStarted(30000))
        return result;
    result.started = true;
    if (!input.isEmpty())
        process.write(input);
    process.closeWriteChannel();
    if (!process.waitForFinished(600000))
    {
        process.kill();
        process.waitForFinished(5000);
    }
    result.output = process.readAllStandardOutput();
    result.errors = process.readAllStandardError();
    result.exitCode = process.exitStatus() == QProcess::NormalExit ? process.exitCode() : -1;
    return result;
}

bool isReleaseVersion(const QString &version, bool testBuild)
{
    static const QRegularExpression release = whole(QStringLiteral(R"(\d+(?:\.\d+)*-\d+)"));
    static const QRegularExpression test = whole(QStringLiteral(R"(\d+(?:\.\d+)*-\d+(?:\.\d+)?)"));
    return matches(testBuild ? test : release, version);
}

bool isPacmanDependency(const QString &need)
{
    // A name of 1-128 of A-Z, a-z, 0-9 and @ . _ + -, not starting with - or
    // .; then optionally an operator and a version of 1-128 characters:
    // [epoch:]version[-release], the version itself printable ASCII except
    // spaces and / : - < > =.
    static const QRegularExpression form = whole(QStringLiteral(
        R"([A-Za-z0-9@_+][A-Za-z0-9@._+-]{0,127})"
        R"((?:(?:<=|>=|<|=|>)(?=.{1,128}\z)(?:\d+:)?[!-,.0-9;?-~]+(?:-\d+(?:\.\d+)?)?)?)"));
    return matches(form, need);
}

std::optional<int> compareVersions(CommandRunner &runner, const QString &toolFolder, const QString &a,
                                   const QString &b)
{
    const CommandRunner::Result result = runner.run(toolFolder + QStringLiteral("/vercmp"), {a, b});
    if (!result.started || result.exitCode != 0)
        return std::nullopt;
    bool ok = false;
    const int order = QString::fromLatin1(result.output).trimmed().toInt(&ok);
    if (!ok)
        return std::nullopt;
    return (order > 0) - (order < 0);
}

ManifestReading readManifest(const QByteArray &bytes, bool testBuild, CommandRunner &runner,
                             const QString &toolFolder)
{
    ManifestReading reading;
    const auto refused = [&reading](const QString &why)
    {
        reading.problem = why;
        reading.manifest.reset();
        reading.refusal = reading.otherFormat ? ManifestReading::Refusal::OtherFormat : ManifestReading::Refusal::Malformed;
        return reading;
    };
    if (bytes.size() > MaxManifestBytes)
        return refused(QStringLiteral("the manifest has %1 bytes, more than %2").arg(bytes.size()).arg(MaxManifestBytes));
    if (bytes.startsWith("\xef\xbb\xbf"))
        return refused(QStringLiteral("the manifest starts with a byte order mark"));
    QStringDecoder decoder(QStringDecoder::Utf8);
    const QString text = decoder(bytes);
    if (decoder.hasError())
        return refused(QStringLiteral("the manifest is not valid UTF-8"));
    if (const QString problem = keyProblem(text); !problem.isEmpty())
        return refused(problem);
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError)
        return refused(QStringLiteral("the manifest is not JSON: %1").arg(error.errorString()));
    if (!document.isObject())
        return refused(QStringLiteral("the manifest is not an object"));
    const QJsonObject top = document.object();
    if (!textsValid(top))
        return refused(QStringLiteral("the manifest holds a text that is not valid Unicode"));

    // Another format first: an Arch Dock that cannot read it says the release
    // must be installed by hand, naming its version when it can.
    const std::optional<qint64> format = wholeNumber(top.value(QStringLiteral("format")), 0,
                                                     std::numeric_limits<int>::max());
    if (format && *format != 1)
    {
        reading.otherFormat = true;
        const QString named = top.value(QStringLiteral("version")).toString();
        if (isReleaseVersion(named, testBuild))
            reading.otherFormatVersion = named;
        return refused(QStringLiteral("the manifest has format %1, not 1").arg(*format));
    }
    if (!hasExactly(top, TopKeys))
        return refused(QStringLiteral("the manifest has missing or unknown keys"));
    const QJsonValue packageValue = top.value(QStringLiteral("package"));
    if (!packageValue.isObject() || !hasExactly(packageValue.toObject(), PackageKeys))
        return refused(QStringLiteral("the package has missing or unknown keys"));
    const QJsonObject package = packageValue.toObject();
    if (!format)
        return refused(QStringLiteral("format is not a whole number"));
    for (const QString &key : {QStringLiteral("product"), QStringLiteral("channel"), QStringLiteral("version"),
                               QStringLiteral("release_tag"), QStringLiteral("source_commit"),
                               QStringLiteral("published_at"), QStringLiteral("release_notes"),
                               QStringLiteral("minimum_supported_version"), QStringLiteral("signing_key_id")})
        if (!top.value(key).isString())
            return refused(QStringLiteral("%1 is not a text").arg(key));
    for (const QString &key : {QStringLiteral("file"), QStringLiteral("sha256"), QStringLiteral("arch")})
        if (!package.value(key).isString())
            return refused(QStringLiteral("%1 is not a text").arg(key));

    Manifest manifest;
    if (top.value(QStringLiteral("product")).toString() != QLatin1String("arch-dock"))
        return refused(QStringLiteral("the manifest is not for arch-dock"));
    if (top.value(QStringLiteral("channel")).toString() != QLatin1String("stable"))
        return refused(QStringLiteral("the manifest is not for the stable channel"));
    manifest.version = top.value(QStringLiteral("version")).toString();
    if (!isReleaseVersion(manifest.version, testBuild))
        return refused(QStringLiteral("the version %1 is not in the form 0.1.1-56").arg(manifest.version));
    manifest.releaseTag = top.value(QStringLiteral("release_tag")).toString();
    if (manifest.releaseTag != QLatin1Char('v') + manifest.version)
        return refused(QStringLiteral("the release tag is not v%1").arg(manifest.version));
    static const QRegularExpression hex40 = whole(QStringLiteral("[0-9a-f]{40}"));
    static const QRegularExpression hex64 = whole(QStringLiteral("[0-9a-f]{64}"));
    static const QRegularExpression timestamp = whole(QStringLiteral(
        R"(\d{4}-(?:0[1-9]|1[0-2])-(?:0[1-9]|[12]\d|3[01])T(?:[01]\d|2[0-3]):[0-5]\d:[0-5]\dZ)"));
    static const QRegularExpression keyId = whole(QStringLiteral("[a-z0-9-]{1,64}"));
    manifest.sourceCommit = top.value(QStringLiteral("source_commit")).toString();
    if (!matches(hex40, manifest.sourceCommit))
        return refused(QStringLiteral("the source commit is not 40 lowercase hex digits"));
    manifest.publishedAt = top.value(QStringLiteral("published_at")).toString();
    if (!matches(timestamp, manifest.publishedAt))
        return refused(QStringLiteral("published_at is not in the form 2026-12-03T12:00:00Z"));
    manifest.releaseNotes = top.value(QStringLiteral("release_notes")).toString();
    if (manifest.releaseNotes.toUcs4().size() > MaxNotes)
        return refused(QStringLiteral("the release notes are longer than %1 characters").arg(MaxNotes));
    manifest.minimumSupportedVersion = top.value(QStringLiteral("minimum_supported_version")).toString();
    if (!isReleaseVersion(manifest.minimumSupportedVersion, testBuild))
        return refused(QStringLiteral("the minimum supported version is not in the form 0.1.1-55"));
    manifest.signingKeyId = top.value(QStringLiteral("signing_key_id")).toString();
    if (!matches(keyId, manifest.signingKeyId))
        return refused(QStringLiteral("the signing key id is not 1 to 64 of a-z, 0-9 and -"));

    PackageEntry &entry = manifest.package;
    entry.file = package.value(QStringLiteral("file")).toString();
    if (entry.file != QStringLiteral("arch-dock-%1-x86_64.pkg.tar.zst").arg(manifest.version))
        return refused(QStringLiteral("the package file does not match the version"));
    const std::optional<qint64> size = wholeNumber(package.value(QStringLiteral("size")), MinPackageBytes, MaxPackageBytes);
    if (!size)
        return refused(QStringLiteral("the package size is not a whole number from 1 MiB to 256 MiB"));
    entry.size = *size;
    entry.sha256 = package.value(QStringLiteral("sha256")).toString();
    if (!matches(hex64, entry.sha256))
        return refused(QStringLiteral("the package's sha256 is not 64 lowercase hex digits"));
    entry.arch = package.value(QStringLiteral("arch")).toString();
    if (entry.arch != QLatin1String("x86_64"))
        return refused(QStringLiteral("the package is not for x86_64"));
    const QJsonValue needs = package.value(QStringLiteral("needs"));
    if (!needs.isArray() || needs.toArray().size() > MaxNeeds)
        return refused(QStringLiteral("needs is not a list of at most %1 entries").arg(MaxNeeds));
    for (const QJsonValue &need : needs.toArray())
    {
        if (!need.isString() || !isPacmanDependency(need.toString()))
            return refused(QStringLiteral("a need is not in pacman's dependency form"));
        entry.needs.append(need.toString());
    }
    const std::optional<qint64> updater = wholeNumber(package.value(QStringLiteral("required_updater_version")),
                                                      1, std::numeric_limits<int>::max());
    if (!updater)
        return refused(QStringLiteral("required_updater_version is not from 1 to 2147483647"));
    entry.requiredUpdaterVersion = *updater;

    // Last, as it runs a tool: the minimum may not be newer than the release.
    const std::optional<int> order = compareVersions(runner, toolFolder, manifest.minimumSupportedVersion, manifest.version);
    if (!order)
        return refused(QStringLiteral("vercmp could not compare the versions"));
    if (*order > 0)
        return refused(QStringLiteral("the minimum supported version is newer than the release"));

    reading.manifest = manifest;
    reading.problem.clear();
    return reading;
}

bool signatureVerifies(CommandRunner &runner, const QString &toolFolder, const QByteArray &spki,
                       const QByteArray &manifest, const QByteArray &signature, const QString &workFolder,
                       bool *couldNotCheck)
{
    if (couldNotCheck)
        *couldNotCheck = false;
    if (spki.isEmpty() || signature.isEmpty() || signature.size() > MaxSignatureBytes
        || manifest.size() > MaxManifestBytes)
        return false;
    const QDir folder(workFolder);
    const QString keyPath = folder.filePath(QStringLiteral("key.pem"));
    const QString manifestPath = folder.filePath(QStringLiteral("manifest"));
    const QString signaturePath = folder.filePath(QStringLiteral("signature"));
    QByteArray pem = "-----BEGIN PUBLIC KEY-----\n";
    const QByteArray body = spki.toBase64();
    for (qsizetype at = 0; at < body.size(); at += 64)
        pem += body.mid(at, 64) + '\n';
    pem += "-----END PUBLIC KEY-----\n";
    // A full disk shows only when the bytes are flushed.
    const auto write = [](const QString &path, const QByteArray &bytes)
    {
        QFile file(path);
        return file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(bytes) == bytes.size() && file.flush();
    };
    bool verified = false;
    if (write(keyPath, pem) && write(manifestPath, manifest) && write(signaturePath, signature))
    {
        const CommandRunner::Result result = runner.run(
            toolFolder + QStringLiteral("/openssl"),
            {QStringLiteral("dgst"), QStringLiteral("-sha256"), QStringLiteral("-verify"), keyPath,
             QStringLiteral("-signature"), signaturePath, manifestPath});
        verified = result.started && result.exitCode == 0 && result.output == "Verified OK\n";
    }
    else if (couldNotCheck)
        *couldNotCheck = true;
    QFile::remove(keyPath);
    QFile::remove(manifestPath);
    QFile::remove(signaturePath);
    return verified;
}

ManifestReading readVerifiedManifest(CommandRunner &runner, const QString &toolFolder, const QString &keyId,
                                     const QByteArray &spki, const QByteArray &manifest,
                                     const QByteArray &signature, bool testBuild, const QString &workFolder)
{
    ManifestReading reading;
    if (keyId.isEmpty() || spki.isEmpty())
    {
        reading.refusal = ManifestReading::Refusal::NoKey;
        reading.problem = QStringLiteral("this build trusts no update key");
        return reading;
    }
    bool couldNotCheck = false;
    if (!signatureVerifies(runner, toolFolder, spki, manifest, signature, workFolder, &couldNotCheck))
    {
        reading.refusal = couldNotCheck ? ManifestReading::Refusal::CouldNotCheck : ManifestReading::Refusal::NotSigned;
        reading.problem = couldNotCheck ? QStringLiteral("the signature check's files could not be written in %1").arg(workFolder)
                                        : QStringLiteral("the release is not signed with the key %1").arg(keyId);
        return reading;
    }
    reading = readManifest(manifest, testBuild, runner, toolFolder);
    if (reading.manifest && reading.manifest->signingKeyId != keyId)
    {
        reading.refusal = ManifestReading::Refusal::WrongKey;
        reading.problem = QStringLiteral("the manifest names the key %1, not %2").arg(reading.manifest->signingKeyId, keyId);
        reading.manifest.reset();
    }
    return reading;
}
} // namespace ArchDock::Updates
