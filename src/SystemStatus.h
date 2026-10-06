#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QThread>
#include <QTimer>
#include <QVariantMap>

// The readings the status modules and status entries show: battery,
// network, CPU, memory, disk and GPU use. They are sampled from /proc and
// /sys on a worker thread while some panel shows them, and offered only when
// this machine actually has the source.
class SystemStatus final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool batteryAvailable READ batteryAvailable NOTIFY statusChanged)
    Q_PROPERTY(int batteryPercent READ batteryPercent NOTIFY statusChanged)
    Q_PROPERTY(bool batteryCharging READ batteryCharging NOTIFY statusChanged)
    Q_PROPERTY(bool networkConnected READ networkConnected NOTIFY statusChanged)
    Q_PROPERTY(QString networkName READ networkName NOTIFY statusChanged)
    Q_PROPERTY(int cpuPercent READ cpuPercent NOTIFY statusChanged)
    Q_PROPERTY(int memoryPercent READ memoryPercent NOTIFY statusChanged)
    Q_PROPERTY(int diskPercent READ diskPercent NOTIFY statusChanged)
    Q_PROPERTY(int gpuPercent READ gpuPercent NOTIFY statusChanged)
public:
    struct Sample {
        QVariantMap metrics;
        quint64 cpuTotal = 0;
        quint64 cpuIdle = 0;
        bool cpuPrimed = false;
    };
    explicit SystemStatus(QObject *parent = nullptr);
    ~SystemStatus() override;
    bool batteryAvailable() const;
    int batteryPercent() const;
    bool batteryCharging() const;
    bool networkConnected() const;
    QString networkName() const;
    int cpuPercent() const;
    int memoryPercent() const;
    int diskPercent() const;
    int gpuPercent() const;
    void setEnabled(bool enabled);
    QStringList availableSources() const;
    QVariantList entries() const;
    quint64 sampleCount() const { return m_sampleCount; }
    static Sample readSample(const QString &procRoot, const QString &sysRoot,
                             const Sample &previous, bool systemDevices = true);

signals:
    void statusChanged();

private:
    void refresh();
    QVariantMap metric(const QString &name) const;
    int percent(const QString &name) const;
    QThread m_thread;
    QObject *m_worker = nullptr;
    QTimer m_timer;
    QElapsedTimer m_age;
    Sample m_sample;
    bool m_enabled = false;
    bool m_inFlight = false;
    quint64 m_sampleCount = 0;
};
