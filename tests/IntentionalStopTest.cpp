#include "IntentionalStop.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

class IntentionalStopTest final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();
    void recordsAndClearsForThisSession();
    void anotherLoginSessionDoesNotStayStopped();
    void refusesEntriesItDidNotWrite();
    void withoutRuntimeDirectoryNothingIsRecorded();

private:
    QTemporaryDir *m_runtime = nullptr;
    QByteArray m_session;
};

void IntentionalStopTest::init()
{
    m_runtime = new QTemporaryDir;
    QVERIFY(m_runtime->isValid());
    QVERIFY(QFile::setPermissions(m_runtime->path(), QFileDevice::ReadOwner |
        QFileDevice::WriteOwner | QFileDevice::ExeOwner));
    qputenv("XDG_RUNTIME_DIR", m_runtime->path().toLocal8Bit());
    m_session = qgetenv("XDG_SESSION_ID");
    qputenv("XDG_SESSION_ID", "7");
}

void IntentionalStopTest::cleanup()
{
    delete m_runtime;
    m_runtime = nullptr;
    qputenv("XDG_SESSION_ID", m_session);
}

void IntentionalStopTest::recordsAndClearsForThisSession()
{
    using namespace ArchDock::IntentionalStop;
    const QString path = statePath();
    QCOMPARE(QFileInfo(path).dir().path(), m_runtime->path());
    QVERIFY(!active());
    QString error;
    QVERIFY2(record(QStringLiteral("quit"), &error), qPrintable(error));
    QVERIFY(active());
    QCOMPARE(QFileInfo(path).permissions() & (QFileDevice::ReadGroup | QFileDevice::WriteGroup |
        QFileDevice::ReadOther | QFileDevice::WriteOther), QFileDevice::Permissions{});
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QJsonObject object = QJsonDocument::fromJson(file.readAll()).object();
    QCOMPARE(object.value(QStringLiteral("reason")).toString(), QStringLiteral("quit"));
    QCOMPARE(object.value(QStringLiteral("session")).toString(), sessionIdentity());
    QVERIFY(sessionIdentity().endsWith(QStringLiteral("/7")));
    QVERIFY2(record(QStringLiteral("signal"), &error), qPrintable(error));
    QVERIFY(active());
    QVERIFY2(clear(&error), qPrintable(error));
    QVERIFY(!active());
    QVERIFY(!QFileInfo::exists(path));
    QVERIFY(clear(&error));
}

void IntentionalStopTest::anotherLoginSessionDoesNotStayStopped()
{
    using namespace ArchDock::IntentionalStop;
    QVERIFY(record(QStringLiteral("quit")));
    qputenv("XDG_SESSION_ID", "8");
    QVERIFY(!active());
    QVERIFY2(!QFileInfo::exists(statePath()), "a stale request is removed");
    qputenv("XDG_SESSION_ID", "7");
    QVERIFY(!active());
}

void IntentionalStopTest::refusesEntriesItDidNotWrite()
{
    using namespace ArchDock::IntentionalStop;
    const QString path = statePath();
    const QString target = QDir(m_runtime->path()).filePath(QStringLiteral("target.json"));
    QFile victim(target);
    QVERIFY(victim.open(QIODevice::WriteOnly));
    const QByteArray original = QJsonDocument(QJsonObject{
        {QStringLiteral("format"), QStringLiteral("org.archdock.intentional-stop")},
        {QStringLiteral("session"), sessionIdentity()}}).toJson();
    victim.write(original);
    victim.close();
    QVERIFY(QFile::link(target, path));
    QVERIFY2(!active(), "a link is never trusted, even to a valid request");
    QVERIFY(!QFileInfo(path).isSymLink());
    QVERIFY(QFile::link(target, path));
    QVERIFY(record(QStringLiteral("quit")));
    QVERIFY(!QFileInfo(path).isSymLink());
    QVERIFY(active());
    QFile check(target);
    QVERIFY(check.open(QIODevice::ReadOnly));
    QCOMPARE(check.readAll(), original);

    QVERIFY(clear());
    QFile garbage(path);
    QVERIFY(garbage.open(QIODevice::WriteOnly));
    garbage.write("not json");
    garbage.close();
    QVERIFY(!active());
    QVERIFY(!QFileInfo::exists(path));
    QVERIFY(QDir(m_runtime->path()).mkdir(QFileInfo(path).fileName()));
    QVERIFY(!active());
}

void IntentionalStopTest::withoutRuntimeDirectoryNothingIsRecorded()
{
    using namespace ArchDock::IntentionalStop;
    qputenv("XDG_RUNTIME_DIR", "");
    QVERIFY(statePath().isEmpty());
    QString error;
    QVERIFY(!record(QStringLiteral("quit"), &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!active());
    QVERIFY(clear());
    qputenv("XDG_RUNTIME_DIR", "relative/runtime");
    QVERIFY(statePath().isEmpty());
}

QTEST_GUILESS_MAIN(IntentionalStopTest)
#include "IntentionalStopTest.moc"
