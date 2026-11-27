// SPDX-License-Identifier: GPL-3.0-or-later
// The Update button's download (src/updates/UpdateDownload) from a local
// test server: a redirect then the file; too many hops; a hop to plain http
// elsewhere; a cut body; the wrong bytes; a stall; more bytes than signed;
// plain http refused outside tests. No failure leaves a file.

#include "updates/UpdateDownload.h"

#include <QCryptographicHash>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>

using ArchDock::Updates::UpdateDownload;

namespace
{
const QByteArray Package = QByteArray(256 * 1024, 'p') + "the end";

// Answers each path as the test says, one request per connection.
class TestServer final : public QTcpServer
{
public:
    TestServer()
    {
        connect(this, &QTcpServer::newConnection, this, [this]
        {
            while (QTcpSocket *socket = nextPendingConnection())
                connect(socket, &QTcpSocket::readyRead, this, [this, socket]
                {
                    const QByteArray request = socket->readAll();
                    const QByteArray path = request.split(' ').value(1);
                    answer(socket, path);
                });
        });
        listen(QHostAddress::LocalHost);
    }

    QString address(const QString &path) const
    {
        return QStringLiteral("http://127.0.0.1:%1%2").arg(serverPort()).arg(path);
    }

private:
    void send(QTcpSocket *socket, const QByteArray &status, const QByteArray &headers, const QByteArray &body, bool close = true)
    {
        socket->write("HTTP/1.1 " + status + "\r\n" + headers + "Connection: close\r\n\r\n" + body);
        socket->flush();
        if (close)
            socket->disconnectFromHost();
    }

    void answer(QTcpSocket *socket, const QByteArray &path)
    {
        const QByteArray length = "Content-Length: " + QByteArray::number(Package.size()) + "\r\n";
        if (path == "/ok")
            return send(socket, "200 OK", length, Package);
        if (path == "/redirect")
            return send(socket, "302 Found", "Location: /ok\r\nContent-Length: 0\r\n", {});
        if (path.startsWith("/chain"))
        {
            // /chain<N>-<k>: N redirects in all, then the file.
            const QList<QByteArray> numbers = path.mid(6).split('-');
            const int total = numbers.value(0).toInt();
            const int hop = numbers.value(1).toInt();
            if (hop < total)
                return send(socket, "302 Found",
                            "Location: /chain" + QByteArray::number(total) + '-' + QByteArray::number(hop + 1)
                                + "\r\nContent-Length: 0\r\n", {});
            return send(socket, "200 OK", length, Package);
        }
        if (path == "/http-elsewhere")
            return send(socket, "302 Found", "Location: http://localhost:" + QByteArray::number(serverPort()) + "/ok\r\nContent-Length: 0\r\n", {});
        if (path == "/cut")
            return send(socket, "200 OK", length, Package.left(1000));
        if (path == "/wrong")
            return send(socket, "200 OK", length, QByteArray(Package.size(), 'x'));
        if (path == "/more")
            return send(socket, "200 OK", "Content-Length: " + QByteArray::number(Package.size() + 10) + "\r\n", Package + "0123456789");
        if (path == "/stall")
            return send(socket, "200 OK", length, Package.left(1000), false);
        send(socket, "404 Not Found", "Content-Length: 0\r\n", {});
    }
};
} // namespace

class UpdateDownloadTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_folder;
    QNetworkAccessManager m_network;
    TestServer m_server;

    UpdateDownload::Result fetch(const QString &path, bool allowLocalHttp = true, QString *target = nullptr)
    {
        UpdateDownload download(&m_network, nullptr, allowLocalHttp, 1500);
        QSignalSpy finished(&download, &UpdateDownload::finished);
        const QString file = m_folder.filePath(QStringLiteral("arch-dock-0.1.1-56-x86_64.pkg.tar.zst"));
        if (target)
            *target = file;
        download.start(QUrl(m_server.address(path)), file, Package.size(),
                       QString::fromLatin1(QCryptographicHash::hash(Package, QCryptographicHash::Sha256).toHex()), "ArchDock/test");
        if (!finished.wait(15000))
            return UpdateDownload::Result::Stopped;
        return finished.first().first().value<UpdateDownload::Result>();
    }

private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY(m_folder.isValid());
        QVERIFY(m_server.isListening());
    }

    void aRedirectThenTheFile()
    {
        QString file;
        QCOMPARE(fetch(QStringLiteral("/redirect"), true, &file), UpdateDownload::Result::Done);
        QFile saved(file);
        QVERIFY(saved.open(QIODevice::ReadOnly));
        QCOMPARE(saved.readAll(), Package);
        saved.remove();
    }

    // At most 5 redirects: 5 then the file is fine, 6 is refused (the row in
    // failuresLeaveNoFile), so a different limit fails one of the two.
    void fiveHopsThenTheFile()
    {
        QString file;
        QCOMPARE(fetch(QStringLiteral("/chain5-0"), true, &file), UpdateDownload::Result::Done);
        QVERIFY(QFile::remove(file));
    }

    void failuresLeaveNoFile_data()
    {
        QTest::addColumn<QString>("path");
        QTest::addColumn<int>("expected");
        QTest::newRow("more than 5 hops") << QStringLiteral("/chain6-0") << int(UpdateDownload::Result::Refused);
        QTest::newRow("a hop to http elsewhere") << QStringLiteral("/http-elsewhere") << int(UpdateDownload::Result::Refused);
        QTest::newRow("a cut body") << QStringLiteral("/cut") << int(UpdateDownload::Result::Cut);
        QTest::newRow("the wrong bytes") << QStringLiteral("/wrong") << int(UpdateDownload::Result::WrongBytes);
        QTest::newRow("more bytes than signed") << QStringLiteral("/more") << int(UpdateDownload::Result::WrongBytes);
        QTest::newRow("a stall") << QStringLiteral("/stall") << int(UpdateDownload::Result::Cut);
    }

    void failuresLeaveNoFile()
    {
        QFETCH(QString, path);
        QFETCH(int, expected);
        QString file;
        QCOMPARE(int(fetch(path, true, &file)), expected);
        QVERIFY(!QFileInfo::exists(file));
        QCOMPARE(QDir(m_folder.path()).entryList(QDir::Files | QDir::Hidden).size(), 0);
    }

    void plainHttpIsRefusedOutsideTests()
    {
        QString file;
        QCOMPARE(fetch(QStringLiteral("/ok"), false, &file), UpdateDownload::Result::Refused);
        QVERIFY(!QFileInfo::exists(file));
    }
};

QTEST_GUILESS_MAIN(UpdateDownloadTest)
#include "UpdateDownloadTest.moc"
