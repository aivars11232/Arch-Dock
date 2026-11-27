// SPDX-License-Identifier: GPL-3.0-or-later
// The update manifest and its signature (src/updates/UpdateCore): the shared
// vectors in tests/update-manifest-vectors, which tools/release-files.py must
// judge the same way (tests/test_release_files.py), and the signature cases.
// Every tool runs through a stand-in runner: vercmp answers from the test,
// openssl is the real one (it only computes), and anything else fails.

#include "updates/UpdateCore.h"

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QTest>

using ArchDock::Updates::CommandRunner;

using Refusal = ArchDock::Updates::ManifestReading::Refusal;

namespace
{
const QString ToolFolder = QStringLiteral("/usr/bin");

// Records every call. vercmp compares the test's plain versions (digits,
// dots and one release); openssl runs for real.
class StandInRunner final : public CommandRunner
{
public:
    Result run(const QString &program, const QStringList &arguments, const QByteArray &input = {}) override
    {
        calls.append(program + QLatin1Char(' ') + arguments.join(QLatin1Char(' ')));
        if (program == ToolFolder + QStringLiteral("/vercmp") && arguments.size() == 2)
        {
            Result result;
            result.started = true;
            result.exitCode = 0;
            result.output = QByteArray::number(order(arguments.at(0), arguments.at(1))) + '\n';
            return result;
        }
        if (program == ToolFolder + QStringLiteral("/openssl"))
            return real.run(program, arguments, input);
        unexpected.append(program);
        return {};
    }

    bool called(const QString &tool) const
    {
        for (const QString &call : calls)
            if (call.startsWith(ToolFolder + QLatin1Char('/') + tool + QLatin1Char(' ')))
                return true;
        return false;
    }

    QStringList calls;
    QStringList unexpected;

private:
    static int order(const QString &a, const QString &b)
    {
        const auto parts = [](const QString &version)
        {
            QList<int> numbers;
            for (const QString &part : QString(version).replace(QLatin1Char('-'), QLatin1Char('.')).split(QLatin1Char('.')))
                numbers.append(part.toInt());
            return numbers;
        };
        const QList<int> left = parts(a);
        const QList<int> right = parts(b);
        for (int index = 0; index < qMax(left.size(), right.size()); ++index)
        {
            const int x = index < left.size() ? left.at(index) : 0;
            const int y = index < right.size() ? right.at(index) : 0;
            if (x != y)
                return x < y ? -1 : 1;
        }
        return 0;
    }

    ArchDock::Updates::SystemCommandRunner real;
};

QByteArray readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

QByteArray openssl(const QStringList &arguments)
{
    QProcess process;
    process.setStandardInputFile(QProcess::nullDevice());
    process.start(ToolFolder + QStringLiteral("/openssl"), arguments);
    process.waitForFinished(30000);
    return process.exitCode() == 0 ? process.readAllStandardOutput() : QByteArray();
}

// A raw r||s signature from a DER one: what a careless signer might publish.
QByteArray rawSignature(const QByteArray &der)
{
    QByteArray raw;
    int at = 2;
    for (int part = 0; part < 2; ++part)
    {
        const int length = static_cast<unsigned char>(der.at(at + 1));
        QByteArray number = der.mid(at + 2, length);
        while (number.size() > 32 && number.startsWith('\0'))
            number.remove(0, 1);
        raw += QByteArray(32 - number.size(), '\0') + number;
        at += 2 + length;
    }
    return raw;
}
} // namespace

class UpdateCoreTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_keys;
    QByteArray m_firstKey;
    QByteArray m_secondKey;
    QString m_firstPrivate;

    QByteArray signWithFirst(const QByteArray &manifest)
    {
        const QString path = m_keys.filePath(QStringLiteral("to-sign"));
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
            return {};
        file.write(manifest);
        file.close();
        return openssl({QStringLiteral("dgst"), QStringLiteral("-sha256"), QStringLiteral("-sign"), m_firstPrivate, path});
    }

private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY(m_keys.isValid());
        for (const QString &name : {QStringLiteral("first"), QStringLiteral("second")})
        {
            const QString key = m_keys.filePath(name + QStringLiteral(".key"));
            openssl({QStringLiteral("ecparam"), QStringLiteral("-name"), QStringLiteral("prime256v1"), QStringLiteral("-genkey"),
                     QStringLiteral("-noout"), QStringLiteral("-out"), key});
            const QByteArray spki = openssl({QStringLiteral("pkey"), QStringLiteral("-in"), key, QStringLiteral("-pubout"),
                                             QStringLiteral("-outform"), QStringLiteral("DER")});
            QVERIFY2(!spki.isEmpty(), "openssl made the test key");
            (name == QLatin1String("first") ? m_firstKey : m_secondKey) = spki;
            if (name == QLatin1String("first"))
                m_firstPrivate = key;
        }
    }

    void theSharedVectorsAreJudgedAsListed()
    {
        const QDir vectors(QStringLiteral(UPDATE_VECTORS_DIR));
        QStringList listed;
        int judged = 0;
        for (const QByteArray &line : readFile(vectors.filePath(QStringLiteral("expected.txt"))).split('\n'))
        {
            if (line.isEmpty() || line.startsWith('#'))
                continue;
            const QList<QByteArray> fields = line.split(' ');
            QCOMPARE(fields.size(), 2);
            const QString name = QString::fromLatin1(fields.at(0));
            listed.append(name);
            StandInRunner runner;
            const auto reading = ArchDock::Updates::readManifest(readFile(vectors.filePath(name)), false, runner, ToolFolder);
            const QByteArray verdict = reading.manifest ? "ACCEPT" : "REFUSE";
            QVERIFY2(verdict == fields.at(1), qPrintable(name + QStringLiteral(": ") + reading.problem));
            QVERIFY2(runner.unexpected.isEmpty(), qPrintable(runner.unexpected.join(QLatin1Char(' '))));
            ++judged;
        }
        QVERIFY(judged > 70);
        const QStringList files = vectors.entryList({QStringLiteral("*.json")}, QDir::Files);
        for (const QString &file : files)
            QVERIFY2(listed.contains(file), qPrintable(file + QStringLiteral(" is not listed in expected.txt")));
    }

    void aSubreleaseIsReadOnlyByTestBuilds()
    {
        const QByteArray subrelease = readFile(QStringLiteral(UPDATE_VECTORS_DIR "/subrelease.json"));
        StandInRunner runner;
        QVERIFY(!ArchDock::Updates::readManifest(subrelease, false, runner, ToolFolder).manifest);
        QVERIFY(ArchDock::Updates::readManifest(subrelease, true, runner, ToolFolder).manifest);
    }

    void anotherFormatNamesTheVersionToInstallByHand()
    {
        const QByteArray other = readFile(QStringLiteral(UPDATE_VECTORS_DIR "/wrong-format.json"));
        StandInRunner runner;
        const auto reading = ArchDock::Updates::readManifest(other, false, runner, ToolFolder);
        QVERIFY(!reading.manifest);
        QVERIFY(reading.otherFormat);
        QCOMPARE(reading.otherFormatVersion, QStringLiteral("0.1.1-56"));
    }

    void onlyTheRightSignatureVerifies()
    {
        const QByteArray manifest = readFile(QStringLiteral(UPDATE_VECTORS_DIR "/valid.json"));
        const QByteArray signature = signWithFirst(manifest);
        QVERIFY(!signature.isEmpty());
        QTemporaryDir work;
        const auto verifies = [&](const QByteArray &key, const QByteArray &bytes, const QByteArray &sig)
        {
            StandInRunner runner;
            return ArchDock::Updates::signatureVerifies(runner, ToolFolder, key, bytes, sig, work.path());
        };
        QVERIFY(verifies(m_firstKey, manifest, signature));
        QVERIFY(!verifies(m_secondKey, manifest, signature));
        QVERIFY(!verifies(m_firstKey, manifest + ' ', signature));
        QVERIFY(!verifies(m_firstKey, manifest.left(manifest.size() - 1), signature));
        QVERIFY(!verifies(m_firstKey, manifest, QByteArray(72, 'x')));
        QVERIFY(!verifies(m_firstKey, manifest, signature.toBase64()));
        QVERIFY(!verifies(m_firstKey, manifest,
                          "-----BEGIN SIGNATURE-----\n" + signature.toBase64() + "\n-----END SIGNATURE-----\n"));
        QVERIFY(!verifies(m_firstKey, manifest, rawSignature(signature)));
        QVERIFY(!verifies(m_firstKey, manifest, manifest + signature));
        QVERIFY(!verifies(m_firstKey, manifest + signature, signature));
        QVERIFY(!verifies(QByteArray(), manifest, signature));
        // Empty or oversized signatures are refused before openssl is asked.
        StandInRunner runner;
        QVERIFY(!ArchDock::Updates::signatureVerifies(runner, ToolFolder, m_firstKey, manifest, QByteArray(), work.path()));
        QVERIFY(!ArchDock::Updates::signatureVerifies(runner, ToolFolder, m_firstKey, manifest, QByteArray(1025, 'x'), work.path()));
        QVERIFY(runner.calls.isEmpty());
        // Nothing is left in the work folder.
        QVERIFY(QDir(work.path()).isEmpty());
    }

    void theManifestIsReadOnlyAfterItsSignature()
    {
        const QByteArray manifest = readFile(QStringLiteral(UPDATE_VECTORS_DIR "/valid.json"));
        const QByteArray signature = signWithFirst(manifest);
        QTemporaryDir work;
        {
            StandInRunner runner;
            const auto reading = ArchDock::Updates::readVerifiedManifest(
                runner, ToolFolder, QStringLiteral("arch-dock-update-1"), m_firstKey, manifest, signature, false, work.path());
            QVERIFY2(reading.manifest, qPrintable(reading.problem));
            QCOMPARE(reading.manifest->version, QStringLiteral("0.1.1-56"));
            QCOMPARE(reading.refusal, Refusal::None);
        }
        {
            // A signature that fails: no field is read (vercmp, which only
            // reading uses, is never asked).
            StandInRunner runner;
            const auto reading = ArchDock::Updates::readVerifiedManifest(
                runner, ToolFolder, QStringLiteral("arch-dock-update-1"), m_secondKey, manifest, signature, false, work.path());
            QVERIFY(!reading.manifest);
            QCOMPARE(reading.refusal, Refusal::NotSigned);
            QVERIFY(runner.called(QStringLiteral("openssl")));
            QVERIFY(!runner.called(QStringLiteral("vercmp")));
        }
        {
            // Signed well, but naming another key: a key a manifest names is never trusted.
            const QByteArray other = QByteArray(manifest).replace("\"arch-dock-update-1\"", "\"arch-dock-update-2\"");
            StandInRunner runner;
            const auto reading = ArchDock::Updates::readVerifiedManifest(
                runner, ToolFolder, QStringLiteral("arch-dock-update-1"), m_firstKey, other, signWithFirst(other), false, work.path());
            QVERIFY(!reading.manifest);
            QCOMPARE(reading.refusal, Refusal::WrongKey);
        }
        {
            // Signed well, but a key written twice: a reading rule, not the
            // signature, refuses it (the helper's 12, not 16).
            const QByteArray twice = readFile(QStringLiteral(UPDATE_VECTORS_DIR "/duplicate-key.json"));
            StandInRunner runner;
            const auto reading = ArchDock::Updates::readVerifiedManifest(
                runner, ToolFolder, QStringLiteral("arch-dock-update-1"), m_firstKey, twice, signWithFirst(twice), false, work.path());
            QVERIFY(!reading.manifest);
            QCOMPARE(reading.refusal, Refusal::Malformed);
        }
        {
            // Another format, signed well: it names its version for the line
            // that says to install it by hand.
            const QByteArray other = readFile(QStringLiteral(UPDATE_VECTORS_DIR "/wrong-format.json"));
            StandInRunner runner;
            const auto reading = ArchDock::Updates::readVerifiedManifest(
                runner, ToolFolder, QStringLiteral("arch-dock-update-1"), m_firstKey, other, signWithFirst(other), false, work.path());
            QVERIFY(!reading.manifest);
            QCOMPARE(reading.refusal, Refusal::OtherFormat);
        }
        {
            // The check's files cannot be written: nothing is checked, and
            // it is not taken for a bad signature.
            StandInRunner runner;
            const auto reading = ArchDock::Updates::readVerifiedManifest(
                runner, ToolFolder, QStringLiteral("arch-dock-update-1"), m_firstKey, manifest, signature, false,
                work.filePath(QStringLiteral("gone/deeper")));
            QVERIFY(!reading.manifest);
            QCOMPARE(reading.refusal, Refusal::CouldNotCheck);
            QVERIFY(runner.calls.isEmpty());
        }
        {
            // No key in this build: nothing is checked or read.
            StandInRunner runner;
            const auto reading = ArchDock::Updates::readVerifiedManifest(
                runner, ToolFolder, QString(), QByteArray(), manifest, signature, false, work.path());
            QVERIFY(!reading.manifest);
            QCOMPARE(reading.refusal, Refusal::NoKey);
            QVERIFY(runner.calls.isEmpty());
        }
    }
};

QTEST_GUILESS_MAIN(UpdateCoreTest)
#include "UpdateCoreTest.moc"
