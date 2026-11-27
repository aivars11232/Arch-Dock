// SPDX-License-Identifier: GPL-3.0-or-later
// The Update button's look at GitHub (src/updates/ReleaseSource): GitHub's
// answers for the newest release, read from fixtures, and what a verified
// release means against the installed Arch Dock and the system. Releases are
// signed in the test with a key openssl makes in its own folder. The runner
// is a stand-in: pacman answers from the test; vercmp and openssl are the
// real ones (they only compute); every call is recorded.

#include "updates/ReleaseSource.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QProcess>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>

#include <optional>

using namespace ArchDock::Updates;

namespace
{
class StandInRunner final : public CommandRunner
{
public:
    Result run(const QString &program, const QStringList &arguments, const QByteArray &input = {}) override
    {
        calls.append(program + QLatin1Char(' ') + arguments.join(QLatin1Char(' ')));
        Result result;
        result.started = true;
        result.exitCode = 0;
        if (program == QLatin1String("/usr/bin/pacman") && arguments == QStringList{QStringLiteral("-Q"), QStringLiteral("arch-dock")})
        {
            result.output = "arch-dock " + installed.toLatin1() + '\n';
            return result;
        }
        if (program == QLatin1String("/usr/bin/pacman") && arguments.value(0) == QLatin1String("-T"))
        {
            QStringList unmet;
            for (const QString &need : arguments.mid(2))
                if (missing.contains(need) || (arguments.size() == 3 && absent.contains(need)))
                    unmet.append(need);
            result.exitCode = unmet.isEmpty() ? 0 : 127;
            result.output = unmet.join(u'\n').toUtf8() + (unmet.isEmpty() ? "" : "\n");
            return result;
        }
        if (program == QLatin1String("/usr/bin/vercmp") || program == QLatin1String("/usr/bin/openssl"))
            return real.run(program, arguments, input);
        result.started = false;
        unexpected.append(program);
        return result;
    }

    QString installed = QStringLiteral("0.1.1-55");
    QStringList missing; // needs pacman -T reports unmet
    QStringList absent;  // names not installed at all
    QStringList calls;
    QStringList unexpected;

private:
    SystemCommandRunner real;
};

QByteArray openssl(const QStringList &arguments)
{
    QProcess process;
    process.setStandardInputFile(QProcess::nullDevice());
    process.start(QStringLiteral("/usr/bin/openssl"), arguments);
    process.waitForFinished(30000);
    return process.exitCode() == 0 ? process.readAllStandardOutput() : QByteArray();
}

QByteArray body(const QJsonObject &object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

// GitHub on this PC: the newest release, then its update-manifest.json
// answered as each case says, the way a network can fail.
class GitHubStandIn final : public QTcpServer
{
public:
    enum class Manifest
    {
        Stall,                // the headers, then nothing more
        Cut,                  // the headers and part of the body, then the connection closes
        ServerError,          // 503
        RedirectToClosedPort, // a 302 to a place that refuses the connection
        Missing,              // 404
    };

    GitHubStandIn()
    {
        connect(this, &QTcpServer::newConnection, this, [this]
        {
            while (QTcpSocket *socket = nextPendingConnection())
            {
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
                connect(socket, &QTcpSocket::readyRead, this, [this, socket]
                {
                    QByteArray &request = m_requests[socket];
                    request += socket->readAll();
                    if (!request.contains("\r\n\r\n"))
                        return;
                    const QByteArray path = request.split(' ').value(1);
                    m_requests.remove(socket);
                    answer(socket, path);
                });
            }
        });
        listen(QHostAddress::LocalHost);
    }

    QString address() const { return QStringLiteral("http://127.0.0.1:%1").arg(serverPort()); }

    Manifest manifest = Manifest::Stall;
    QByteArray latest;
    quint16 closedPort = 0;

private:
    void answer(QTcpSocket *socket, const QByteArray &path)
    {
        if (path.endsWith("/releases/latest"))
            return send(socket, "200 OK", "Content-Length: " + QByteArray::number(latest.size()) + "\r\n", latest);
        const QByteArray partial = "{\"format\": 1, ";
        switch (manifest)
        {
        case Manifest::Stall:
            socket->write("HTTP/1.1 200 OK\r\nContent-Length: 900\r\n\r\n" + partial);
            m_stalled.append(socket);
            return;
        case Manifest::Cut:
            return send(socket, "200 OK", "Content-Length: 900\r\n", partial);
        case Manifest::ServerError:
            return send(socket, "503 Service Unavailable", "Content-Length: 0\r\n", {});
        case Manifest::RedirectToClosedPort:
            return send(socket, "302 Found",
                        "Location: http://127.0.0.1:" + QByteArray::number(closedPort) + "/gone\r\nContent-Length: 0\r\n", {});
        case Manifest::Missing:
            return send(socket, "404 Not Found", "Content-Length: 0\r\n", {});
        }
    }

    void send(QTcpSocket *socket, const QByteArray &status, const QByteArray &headers, const QByteArray &content)
    {
        socket->write("HTTP/1.1 " + status + "\r\n" + headers + "Connection: close\r\n\r\n" + content);
        socket->flush();
        socket->disconnectFromHost();
    }

    QHash<QTcpSocket *, QByteArray> m_requests;
    QList<QPointer<QTcpSocket>> m_stalled;
};
} // namespace

class ReleaseSourceTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_folder;
    QString m_key;
    QByteArray m_spki;
    QJsonObject m_valid;

    // A release of `version` as the button would see it, signed with the test key.
    std::pair<QByteArray, QByteArray> signedManifest(const QJsonObject &manifest)
    {
        const QByteArray bytes = QJsonDocument(manifest).toJson();
        QFile file(m_folder.filePath(QStringLiteral("manifest")));
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
            return {};
        file.write(bytes);
        file.close();
        return {bytes, openssl({QStringLiteral("dgst"), QStringLiteral("-sha256"), QStringLiteral("-sign"), m_key, file.fileName()})};
    }

    QJsonObject manifest(const QString &version)
    {
        QJsonObject object = m_valid;
        object.insert(QStringLiteral("version"), version);
        object.insert(QStringLiteral("release_tag"), QStringLiteral("v") + version);
        QJsonObject package = object.value(QStringLiteral("package")).toObject();
        package.insert(QStringLiteral("file"), QStringLiteral("arch-dock-%1-x86_64.pkg.tar.zst").arg(version));
        object.insert(QStringLiteral("package"), package);
        return object;
    }

    LatestAnswer latest(const QString &version, qint64 size = 34127137)
    {
        LatestAnswer answer;
        answer.kind = LatestAnswer::Kind::Release;
        answer.tag = QStringLiteral("v") + version;
        answer.version = version;
        answer.assets = {{QStringLiteral("update-manifest.json"), 900}, {QStringLiteral("update-manifest.sig"), 72},
                         {QStringLiteral("arch-dock-%1-x86_64.pkg.tar.zst").arg(version), size}};
        return answer;
    }

    ReleaseSetup setup()
    {
        ReleaseSetup setup;
        setup.repository = QStringLiteral("aivars11232/Arch-Dock");
        setup.keyId = QStringLiteral("arch-dock-update-1");
        setup.keySpki = m_spki;
        setup.workFolder = m_folder.path();
        return setup;
    }

    LookResult judge(StandInRunner &runner, const QJsonObject &object, const LatestAnswer &answer)
    {
        const auto [bytes, signature] = signedManifest(object);
        return judgeRelease(answer, bytes, signature, runner, setup());
    }

private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY(m_folder.isValid());
        m_key = m_folder.filePath(QStringLiteral("test.key"));
        openssl({QStringLiteral("ecparam"), QStringLiteral("-name"), QStringLiteral("prime256v1"), QStringLiteral("-genkey"),
                 QStringLiteral("-noout"), QStringLiteral("-out"), m_key});
        m_spki = openssl({QStringLiteral("pkey"), QStringLiteral("-in"), m_key, QStringLiteral("-pubout"), QStringLiteral("-outform"),
                          QStringLiteral("DER")});
        QVERIFY(!m_spki.isEmpty());
        QFile valid(QStringLiteral(UPDATE_VECTORS_DIR "/valid.json"));
        QVERIFY(valid.open(QIODevice::ReadOnly));
        m_valid = QJsonDocument::fromJson(valid.readAll()).object();
        QVERIFY(!m_valid.isEmpty());
    }

    void noNormalReleaseIsUpToDate()
    {
        const QDateTime now = QDateTime::currentDateTimeUtc();
        QCOMPARE(readLatest(404, {}, {}, now, false).kind, LatestAnswer::Kind::UpToDate);
        // Today's releases are all pre-releases, and a draft is never seen.
        for (const char *flag : {"prerelease", "draft"})
            QCOMPARE(readLatest(200, body({{QStringLiteral("tag_name"), QStringLiteral("v0.1.1-54")}, {QLatin1String(flag), true}}),
                                {}, now, false).kind,
                     LatestAnswer::Kind::UpToDate);
        // An unsafe tag, or one without a package release, is no update release.
        for (const char *tag : {"v0.1.1", "v../../x", "0.1.1-56", "v0.1.1-56;rm"})
            QCOMPARE(readLatest(200, body({{QStringLiteral("tag_name"), QString::fromLatin1(tag)}}), {}, now, false).kind,
                     LatestAnswer::Kind::UpToDate);
    }

    void gitHubsAnswersGiveTheirLines()
    {
        const QDateTime now = QDateTime::fromString(QStringLiteral("2026-11-27T12:00:00Z"), Qt::ISODate);
        const LatestAnswer wait = readLatest(403, {}, {{"x-ratelimit-reset", QByteArray::number(now.toSecsSinceEpoch() + 600)}}, now, false);
        QCOMPARE(wait.line, QStringLiteral("L3"));
        QCOMPARE(wait.retryAfter, now.addSecs(600));
        QCOMPARE(readLatest(429, {}, {{"retry-after", "120"}}, now, false).retryAfter, now.addSecs(120));
        QCOMPARE(readLatest(503, {}, {}, now, false).line, QStringLiteral("L2"));
        QCOMPARE(readLatest(500, {}, {}, now, false).line, QStringLiteral("L2"));
        QCOMPARE(readLatest(410, {}, {}, now, false).line, QStringLiteral("L4"));
    }

    void theAnswersAddressesAreNeverUsed()
    {
        const QByteArray answer = body({{QStringLiteral("tag_name"), QStringLiteral("v0.1.1-56")},
            {QStringLiteral("body"), QStringLiteral("GitHub's text, never shown")},
            {QStringLiteral("assets"), QJsonArray{QJsonObject{{QStringLiteral("name"), QStringLiteral("update-manifest.json")},
                {QStringLiteral("size"), 900}, {QStringLiteral("browser_download_url"), QStringLiteral("https://evil.example/m.json")}}}}});
        const LatestAnswer read = readLatest(200, answer, {}, QDateTime::currentDateTimeUtc(), false);
        QCOMPARE(read.kind, LatestAnswer::Kind::Release);
        QCOMPARE(read.assets.value(QStringLiteral("update-manifest.json")), 900);
        QCOMPARE(assetAddress(QStringLiteral("https://github.com"), QStringLiteral("aivars11232/Arch-Dock"), read.tag,
                              QStringLiteral("update-manifest.json")),
                 QStringLiteral("https://github.com/aivars11232/Arch-Dock/releases/download/v0.1.1-56/update-manifest.json"));
        // A release without its package or signature is incomplete.
        QCOMPARE(missingAsset(read), QStringLiteral("update-manifest.sig"));
    }

    void aVerifiedNewerReleaseIsFoundWithItsOwnNotes()
    {
        StandInRunner runner;
        const LookResult result = judge(runner, manifest(QStringLiteral("0.1.1-56")), latest(QStringLiteral("0.1.1-56")));
        QCOMPARE(result.outcome, LookResult::Outcome::Found);
        QCOMPARE(result.manifest.releaseNotes, m_valid.value(QStringLiteral("release_notes")).toString());
        QVERIFY(runner.unexpected.isEmpty());
        // vercmp gets the two versions alone, never "--".
        for (const QString &call : runner.calls)
            if (call.startsWith(QLatin1String("/usr/bin/vercmp")))
                QVERIFY2(!call.contains(QLatin1String(" --")), qPrintable(call));
    }

    void theInstalledVersionComesFirst()
    {
        // 55 running and the newest signed release 54: up to date.
        StandInRunner runner;
        QJsonObject older = manifest(QStringLiteral("0.1.1-54"));
        older.insert(QStringLiteral("minimum_supported_version"), QStringLiteral("0.1.1-54"));
        QCOMPARE(judge(runner, older, latest(QStringLiteral("0.1.1-54"))).outcome, LookResult::Outcome::UpToDate);
        // pacman's order: 0.1.2-1 is newer than 0.1.1-99, and -100 than -99.
        runner.installed = QStringLiteral("0.1.1-99");
        QCOMPARE(judge(runner, manifest(QStringLiteral("0.1.2-1")), latest(QStringLiteral("0.1.2-1"))).outcome, LookResult::Outcome::Found);
        QCOMPARE(judge(runner, manifest(QStringLiteral("0.1.1-100")), latest(QStringLiteral("0.1.1-100"))).outcome, LookResult::Outcome::Found);
    }

    void aReleaseThatDoesNotMatchIsRefused()
    {
        StandInRunner runner;
        QJsonObject other = manifest(QStringLiteral("0.1.1-56"));
        other.insert(QStringLiteral("release_tag"), QStringLiteral("v0.1.1-57"));
        QCOMPARE(judge(runner, other, latest(QStringLiteral("0.1.1-56"))).line, QStringLiteral("L5"));
        QCOMPARE(judge(runner, manifest(QStringLiteral("0.1.1-56")), latest(QStringLiteral("0.1.1-56"), 999)).line, QStringLiteral("L5"));
        // Signed with another key: L6.
        const auto [bytes, signature] = signedManifest(manifest(QStringLiteral("0.1.1-56")));
        ReleaseSetup wrong = setup();
        wrong.keySpki = openssl({QStringLiteral("pkey"), QStringLiteral("-in"), m_key, QStringLiteral("-pubout"), QStringLiteral("-outform"),
                                 QStringLiteral("DER")}).replace(40, 1, "\x01");
        QCOMPARE(judgeRelease(latest(QStringLiteral("0.1.1-56")), bytes, signature, runner, wrong).line, QStringLiteral("L6"));
    }

    void someReleasesMustBeInstalledByHand()
    {
        StandInRunner runner;
        QJsonObject format = manifest(QStringLiteral("0.1.1-60"));
        format.insert(QStringLiteral("format"), 2);
        const LookResult byFormat = judge(runner, format, latest(QStringLiteral("0.1.1-60")));
        QCOMPARE(byFormat.outcome, LookResult::Outcome::InstallByHand);
        QCOMPARE(byFormat.byHandVersion, QStringLiteral("0.1.1-60"));
        QJsonObject updater = manifest(QStringLiteral("0.1.1-60"));
        QJsonObject package = updater.value(QStringLiteral("package")).toObject();
        package.insert(QStringLiteral("required_updater_version"), 2);
        updater.insert(QStringLiteral("package"), package);
        QCOMPARE(judge(runner, updater, latest(QStringLiteral("0.1.1-60"))).outcome, LookResult::Outcome::InstallByHand);
        QJsonObject minimum = manifest(QStringLiteral("0.1.1-60"));
        minimum.insert(QStringLiteral("minimum_supported_version"), QStringLiteral("0.1.1-58"));
        QCOMPARE(judge(runner, minimum, latest(QStringLiteral("0.1.1-60"))).outcome, LookResult::Outcome::InstallByHand);
    }

    void needsTellTooOldFromNotInstalled()
    {
        StandInRunner runner;
        QJsonObject object = manifest(QStringLiteral("0.1.1-56"));
        QJsonObject package = object.value(QStringLiteral("package")).toObject();
        package.insert(QStringLiteral("needs"), QJsonArray{QStringLiteral("glibc"), QStringLiteral("qt6-base>=6.13"),
            QStringLiteral("qt6-foo"), QStringLiteral("libFLAC.so=14-64"), QStringLiteral("pipewire=1:1.6.11-2"),
            QStringLiteral("Qt6Example")});
        object.insert(QStringLiteral("package"), package);
        runner.missing = {QStringLiteral("qt6-base>=6.13"), QStringLiteral("qt6-foo")};
        runner.absent = {QStringLiteral("qt6-foo")};
        const LookResult result = judge(runner, object, latest(QStringLiteral("0.1.1-56")));
        QCOMPARE(result.outcome, LookResult::Outcome::Found);
        QCOMPARE(result.tooOld, QStringList{QStringLiteral("qt6-base>=6.13")});
        QCOMPARE(result.notInstalled, QStringList{QStringLiteral("qt6-foo")});
        // "--" comes before the needs.
        bool asked = false;
        for (const QString &call : runner.calls)
            if (call.startsWith(QLatin1String("/usr/bin/pacman -T")))
            {
                QVERIFY(call.startsWith(QLatin1String("/usr/bin/pacman -T -- ")));
                asked = true;
            }
        QVERIFY(asked);
        // A need starting with "-" never reaches pacman: the manifest is refused.
        QJsonObject dash = manifest(QStringLiteral("0.1.1-56"));
        QJsonObject dashPackage = dash.value(QStringLiteral("package")).toObject();
        dashPackage.insert(QStringLiteral("needs"), QJsonArray{QStringLiteral("-foo")});
        dash.insert(QStringLiteral("package"), dashPackage);
        StandInRunner fresh;
        QCOMPARE(judge(fresh, dash, latest(QStringLiteral("0.1.1-56"))).line, QStringLiteral("L5"));
        for (const QString &call : fresh.calls)
            QVERIFY(!call.startsWith(QLatin1String("/usr/bin/pacman -T")));
    }

    // A release in another format is installed by hand only when it is
    // newer than the installed Arch Dock; otherwise Arch Dock is up to date.
    void anotherFormatThatIsNotNewerIsUpToDate()
    {
        for (const char *version : {"0.1.1-55", "0.1.1-50"})
        {
            StandInRunner runner;
            QJsonObject format = manifest(QString::fromLatin1(version));
            format.insert(QStringLiteral("format"), 2);
            const LookResult result = judge(runner, format, latest(QString::fromLatin1(version)));
            QCOMPARE(result.outcome, LookResult::Outcome::UpToDate);
            QVERIFY(result.line.isEmpty());
        }
    }

    // The signature check's files could not be written on this PC: a line
    // of its own, never "not signed".
    void aCheckThatCouldNotRunIsNoBadSignature()
    {
        StandInRunner runner;
        const auto [bytes, signature] = signedManifest(manifest(QStringLiteral("0.1.1-56")));
        ReleaseSetup gone = setup();
        gone.workFolder = m_folder.filePath(QStringLiteral("gone/deeper"));
        const LookResult result = judgeRelease(latest(QStringLiteral("0.1.1-56")), bytes, signature, runner, gone);
        QCOMPARE(result.outcome, LookResult::Outcome::Problem);
        QCOMPARE(result.line, QStringLiteral("L34"));
    }

    void filesThatDoNotArriveWholeSayTryAgain_data()
    {
        QTest::addColumn<int>("manifest");
        QTest::addColumn<QString>("line");
        QTest::newRow("a body that stalls after its headers") << int(GitHubStandIn::Manifest::Stall) << QStringLiteral("L2");
        QTest::newRow("a body cut short") << int(GitHubStandIn::Manifest::Cut) << QStringLiteral("L2");
        QTest::newRow("a server error") << int(GitHubStandIn::Manifest::ServerError) << QStringLiteral("L2");
        QTest::newRow("a redirect nobody answers") << int(GitHubStandIn::Manifest::RedirectToClosedPort) << QStringLiteral("L1");
        QTest::newRow("a missing file") << int(GitHubStandIn::Manifest::Missing) << QStringLiteral("L5");
    }

    // Qt keeps the status of the last headers it saw, so a reply that failed
    // after them must still count as no answer: try again (L1, L2), never
    // "not signed" (L6) or "not complete" (L5).
    void filesThatDoNotArriveWholeSayTryAgain()
    {
        QFETCH(int, manifest);
        QFETCH(QString, line);
        GitHubStandIn server;
        QVERIFY(server.isListening());
        server.manifest = GitHubStandIn::Manifest(manifest);
        {
            QTcpServer closed;
            QVERIFY(closed.listen(QHostAddress::LocalHost));
            server.closedPort = closed.serverPort();
        }
        const auto asset = [](const QString &name, qint64 size)
        { return QJsonObject{{QStringLiteral("name"), name}, {QStringLiteral("size"), size}}; };
        server.latest = body({{QStringLiteral("tag_name"), QStringLiteral("v0.1.1-56")}, {QStringLiteral("draft"), false},
                              {QStringLiteral("prerelease"), false},
                              {QStringLiteral("assets"), QJsonArray{asset(QStringLiteral("update-manifest.json"), 900),
                                                                    asset(QStringLiteral("update-manifest.sig"), 72),
                                                                    asset(QStringLiteral("arch-dock-0.1.1-56-x86_64.pkg.tar.zst"), 34127137)}}});
        QNetworkAccessManager network;
        StandInRunner runner;
        ReleaseSetup local = setup();
        local.api = server.address();
        local.downloads = server.address();
        local.testBuild = true;
        local.timeoutMs = 1500;
        ReleaseSource source(&network, runner, local);
        std::optional<LookResult> looked;
        connect(&source, &ReleaseSource::finished, this, [&looked](const LookResult &result) { looked = result; });
        source.look();
        QVERIFY(QTest::qWaitFor([&] { return looked.has_value(); }, 15000));
        QCOMPARE(looked->outcome, LookResult::Outcome::Problem);
        QCOMPARE(looked->line, line);
        QVERIFY(runner.unexpected.isEmpty());
    }
};

QTEST_GUILESS_MAIN(ReleaseSourceTest)
#include "ReleaseSourceTest.moc"
