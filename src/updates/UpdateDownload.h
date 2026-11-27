// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Downloads a release's package for the Update button into its cache folder,
// by name from the release (an address built by the caller), through a
// QSaveFile committed only when the bytes have the signed size and sha256:
// no failure leaves a file. https only, at most 5 redirects, each to https;
// TLS errors are never ignored; more bytes than the signed size stop it at once.

#include <QCryptographicHash>
#include <QObject>
#include <QPointer>
#include <QSaveFile>
#include <QTimer>
#include <QUrl>

#include <memory>

class QNetworkAccessManager;
class QNetworkReply;

namespace ArchDock::Updates
{
class UpdateDownload final : public QObject
{
    Q_OBJECT

public:
    enum class Result
    {
        Done,
        Stopped,
        Cut,          // the transfer ended early, stalled or took too long
        WrongBytes,   // other bytes than the signed ones, or more of them
        NoSpace,
        NoConnection,
        Refused,      // not https (or, in tests, not the local test server)
    };
    Q_ENUM(Result)

    // `allowLocalHttp` is for tests only: then http://127.0.0.1 is allowed
    // too, and the timeouts may be shorter.
    explicit UpdateDownload(QNetworkAccessManager *network, QObject *parent = nullptr, bool allowLocalHttp = false,
                            int stallMs = 30000);
    ~UpdateDownload() override;

    // Starts fetching `url` into `target`, which must end up exactly `size`
    // bytes with the lowercase hex `sha256`. Other packages in its folder go.
    void start(const QUrl &url, const QString &target, qint64 size, const QString &sha256, const QByteArray &userAgent);
    void stop();
    [[nodiscard]] bool running() const { return m_reply != nullptr; }
    [[nodiscard]] qint64 received() const { return m_received; }

Q_SIGNALS:
    void progress(qint64 received, qint64 total);
    void finished(ArchDock::Updates::UpdateDownload::Result result, const QString &details);

private:
    bool hopAllowed(const QUrl &url) const;
    void readSome();
    void end(Result result, const QString &details);

    QNetworkAccessManager *m_network;
    bool m_allowLocalHttp;
    int m_stallMs;
    QPointer<QNetworkReply> m_reply;
    std::unique_ptr<QSaveFile> m_file;
    QCryptographicHash m_hash{QCryptographicHash::Sha256};
    qint64 m_size = 0;
    qint64 m_received = 0;
    QString m_sha256;
    QTimer m_overall;
    bool m_ending = false;
};
} // namespace ArchDock::Updates
