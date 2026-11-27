// SPDX-License-Identifier: GPL-3.0-or-later
#include "UpdateDownload.h"

#include "CommandRunner.h"

#include <QDir>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

namespace ArchDock::Updates
{
UpdateDownload::UpdateDownload(QNetworkAccessManager *network, QObject *parent, bool allowLocalHttp, int stallMs)
    : QObject(parent)
    , m_network(network)
    , m_allowLocalHttp(allowLocalHttp)
    , m_stallMs(stallMs)
{
    m_overall.setSingleShot(true);
    connect(&m_overall, &QTimer::timeout, this, [this] { end(Result::Cut, QStringLiteral("the download took too long")); });
}

UpdateDownload::~UpdateDownload()
{
    if (m_reply)
    {
        m_reply->disconnect(this);
        m_reply->abort();
    }
    if (m_file)
        m_file->cancelWriting();
}

bool UpdateDownload::hopAllowed(const QUrl &url) const
{
    // Plain http only to this PC, where a test or the rehearsal asks for it;
    // https only in a build that uses the network.
    if (url.scheme() == QLatin1String("http"))
        return m_allowLocalHttp && url.host() == QLatin1String("127.0.0.1");
    return url.scheme() == QLatin1String("https") && networkAllowed(url.toString());
}

void UpdateDownload::start(const QUrl &url, const QString &target, qint64 size, const QString &sha256,
                           const QByteArray &userAgent)
{
    stop();
    m_ending = false;
    m_size = size;
    m_received = 0;
    m_sha256 = sha256;
    m_hash.reset();
    if (!hopAllowed(url))
    {
        QTimer::singleShot(0, this, [this, url] { Q_EMIT finished(Result::Refused, url.toString()); });
        return;
    }
    // At most one package is kept: older downloads go first.
    const QFileInfo targetInfo(target);
    QDir folder = targetInfo.dir();
    folder.mkpath(QStringLiteral("."));
    for (const QString &old : folder.entryList({QStringLiteral("*.pkg.tar.zst")}, QDir::Files))
        if (old != targetInfo.fileName())
            folder.remove(old);
    m_file = std::make_unique<QSaveFile>(target);
    if (!m_file->open(QIODevice::WriteOnly))
    {
        m_file.reset();
        QTimer::singleShot(0, this, [this] { Q_EMIT finished(Result::NoSpace, QStringLiteral("the download cannot be saved")); });
        return;
    }
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setMaximumRedirectsAllowed(5);
    request.setTransferTimeout(m_stallMs);
    request.setHeader(QNetworkRequest::UserAgentHeader, userAgent);
    m_reply = m_network->get(request);
    connect(m_reply, &QNetworkReply::redirected, this, [this](const QUrl &next)
    {
        if (!hopAllowed(next))
            end(Result::Refused, QStringLiteral("a redirect to %1").arg(next.toString()));
    });
    connect(m_reply, &QNetworkReply::readyRead, this, &UpdateDownload::readSome);
    connect(m_reply, &QNetworkReply::finished, this, [this]
    {
        if (!m_reply || m_ending)
            return;
        readSome();
        if (m_ending)
            return;
        const QNetworkReply::NetworkError error = m_reply->error();
        const int status = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const bool encrypted = m_reply->attribute(QNetworkRequest::ConnectionEncryptedAttribute).toBool();
        if (error == QNetworkReply::OperationCanceledError || error == QNetworkReply::TimeoutError)
            return end(Result::Cut, QStringLiteral("the download stopped or stalled"));
        if (error == QNetworkReply::TooManyRedirectsError || error == QNetworkReply::InsecureRedirectError)
            return end(Result::Refused, m_reply->errorString());
        // No connection only before any byte came; a transfer that ended
        // early is a cut download.
        if (error != QNetworkReply::NoError)
            return end(m_received == 0 && error != QNetworkReply::RemoteHostClosedError
                               && error <= QNetworkReply::UnknownNetworkError
                           ? Result::NoConnection : Result::Cut,
                       m_reply->errorString());
        if (status != 200 || (!encrypted && !m_allowLocalHttp))
            return end(Result::Refused, QStringLiteral("answer %1 %2").arg(status).arg(encrypted ? "" : "without TLS"));
        if (m_received != m_size)
            return end(Result::Cut, QStringLiteral("%1 of %2 bytes").arg(m_received).arg(m_size));
        if (QString::fromLatin1(m_hash.result().toHex()) != m_sha256)
            return end(Result::WrongBytes, QStringLiteral("the bytes are not the signed ones"));
        end(Result::Done, {});
    });
    // An overall limit beside the stall limit: the signed size at 32 KiB/s,
    // and at least ten minutes.
    m_overall.start(static_cast<int>(qBound<qint64>(600000, m_size / 32, 6 * 3600 * 1000LL)));
}

void UpdateDownload::readSome()
{
    if (!m_reply || m_ending || !m_file)
        return;
    while (m_reply->bytesAvailable() > 0)
    {
        const QByteArray chunk = m_reply->read(qMin<qint64>(m_reply->bytesAvailable(), 1024 * 1024));
        if (m_received + chunk.size() > m_size)
            return end(Result::WrongBytes, QStringLiteral("more bytes than the signed size"));
        m_hash.addData(chunk);
        if (m_file->write(chunk) != chunk.size())
            return end(Result::NoSpace, QStringLiteral("the download cannot be written"));
        m_received += chunk.size();
    }
    Q_EMIT progress(m_received, m_size);
}

void UpdateDownload::stop()
{
    if (m_reply)
        end(Result::Stopped, {});
}

void UpdateDownload::end(Result result, const QString &details)
{
    if (m_ending)
        return;
    m_ending = true;
    m_overall.stop();
    if (m_reply)
    {
        QNetworkReply *reply = m_reply;
        m_reply = nullptr;
        reply->disconnect(this);
        reply->abort();
        reply->deleteLater();
    }
    if (m_file)
    {
        if (result == Result::Done)
        {
            if (!m_file->commit())
                result = Result::NoSpace;
        }
        else
            m_file->cancelWriting();
        m_file.reset();
    }
    Q_EMIT finished(result, details);
}
} // namespace ArchDock::Updates
