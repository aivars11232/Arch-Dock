#pragma once

#include <QObject>

class QTimer;

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
    explicit SystemStatus(QObject *parent = nullptr);

    [[nodiscard]] bool batteryAvailable() const;
    [[nodiscard]] int batteryPercent() const;
    [[nodiscard]] bool batteryCharging() const;
    [[nodiscard]] bool networkConnected() const;
    [[nodiscard]] const QString &networkName() const;
    [[nodiscard]] int cpuPercent() const;
    [[nodiscard]] int memoryPercent() const;
    [[nodiscard]] int diskPercent() const;
    [[nodiscard]] int gpuPercent() const;

signals:
    void statusChanged();

private:
    void refresh();
    void refreshBattery();
    void refreshNetwork();
    void refreshProcessor();
    void refreshMemory();
    void refreshStorage();
    void refreshGraphics();

    QTimer *m_refreshTimer = nullptr;
    bool m_batteryAvailable = false;
    int m_batteryPercent = 0;
    bool m_batteryCharging = false;
    bool m_networkConnected = false;
    QString m_networkName;
    int m_cpuPercent = 0;
    int m_memoryPercent = 0;
    int m_diskPercent = 0;
    int m_gpuPercent = 0;
    quint64 m_previousCpuTotal = 0;
    quint64 m_previousCpuIdle = 0;
};