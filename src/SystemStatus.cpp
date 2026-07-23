#include "SystemStatus.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QNetworkInterface>
#include <QStorageInfo>
#include <QTextStream>
#include <QTimer>

SystemStatus::SystemStatus(QObject *parent)
    : QObject(parent), m_refreshTimer(new QTimer(this))
{
    m_refreshTimer->setInterval(2000);
    connect(m_refreshTimer, &QTimer::timeout, this, &SystemStatus::refresh);
    refresh();
    m_refreshTimer->start();
}

bool SystemStatus::batteryAvailable() const
{
    return m_batteryAvailable;
}

int SystemStatus::batteryPercent() const
{
    return m_batteryPercent;
}

bool SystemStatus::batteryCharging() const
{
    return m_batteryCharging;
}

bool SystemStatus::networkConnected() const
{
    return m_networkConnected;
}

const QString &SystemStatus::networkName() const
{
    return m_networkName;
}

int SystemStatus::cpuPercent() const
{
    return m_cpuPercent;
}

int SystemStatus::memoryPercent() const
{
    return m_memoryPercent;
}

int SystemStatus::diskPercent() const
{
    return m_diskPercent;
}

int SystemStatus::gpuPercent() const
{
    return m_gpuPercent;
}

void SystemStatus::refresh()
{
    refreshBattery();
    refreshNetwork();
    refreshProcessor();
    refreshMemory();
    refreshStorage();
    refreshGraphics();
    emit statusChanged();
}

void SystemStatus::refreshBattery()
{
    const QDir powerSupplyDirectory(QStringLiteral("/sys/class/power_supply"));
    const QStringList batteries = powerSupplyDirectory.entryList(
        {QStringLiteral("BAT*")},
        QDir::Dirs | QDir::NoDotAndDotDot);
    if (batteries.isEmpty())
    {
        m_batteryAvailable = false;
        m_batteryPercent = 0;
        m_batteryCharging = false;
        return;
    }

    const QString batteryPath = powerSupplyDirectory.filePath(batteries.constFirst());
    QFile capacityFile(batteryPath + QStringLiteral("/capacity"));
    QFile statusFile(batteryPath + QStringLiteral("/status"));
    if (!capacityFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        m_batteryAvailable = false;
        return;
    }

    m_batteryAvailable = true;
    m_batteryPercent = QString::fromUtf8(capacityFile.readAll()).trimmed().toInt();
    if (statusFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        m_batteryCharging = QString::fromUtf8(statusFile.readAll()).trimmed()
                               .compare(QStringLiteral("Charging"), Qt::CaseInsensitive) == 0;
    }
    else
    {
        m_batteryCharging = false;
    }
}

void SystemStatus::refreshNetwork()
{
    m_networkConnected = false;
    m_networkName.clear();
    for (const QNetworkInterface &interface : QNetworkInterface::allInterfaces())
    {
        const auto flags = interface.flags();
        if (!flags.testFlag(QNetworkInterface::IsUp) ||
            !flags.testFlag(QNetworkInterface::IsRunning) ||
            flags.testFlag(QNetworkInterface::IsLoopBack))
        {
            continue;
        }

        m_networkConnected = true;
        m_networkName = interface.humanReadableName().trimmed();
        if (m_networkName.isEmpty())
        {
            m_networkName = interface.name();
        }
        return;
    }
}

void SystemStatus::refreshProcessor()
{
    QFile statFile(QStringLiteral("/proc/stat"));
    if (!statFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return;
    }

    const QStringList values = QString::fromUtf8(statFile.readLine()).simplified().split(QLatin1Char(' '));
    if (values.size() < 5 || values.constFirst() != QStringLiteral("cpu"))
    {
        return;
    }

    quint64 total = 0;
    for (qsizetype index = 1; index < values.size(); ++index)
    {
        total += values.at(index).toULongLong();
    }
    const quint64 idle = values.at(4).toULongLong() +
                         (values.size() > 5 ? values.at(5).toULongLong() : 0);
    if (m_previousCpuTotal > 0 && total > m_previousCpuTotal)
    {
        const quint64 totalDelta = total - m_previousCpuTotal;
        const quint64 idleDelta = idle - m_previousCpuIdle;
        m_cpuPercent = qBound(0, qRound(
            100.0 * static_cast<double>(totalDelta - qMin(totalDelta, idleDelta)) /
            static_cast<double>(totalDelta)), 100);
    }

    m_previousCpuTotal = total;
    m_previousCpuIdle = idle;
}

void SystemStatus::refreshMemory()
{
    QFile memoryFile(QStringLiteral("/proc/meminfo"));
    if (!memoryFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return;
    }

    quint64 total = 0;
    quint64 available = 0;
    while (!memoryFile.atEnd())
    {
        const QStringList fields = QString::fromUtf8(memoryFile.readLine()).simplified().split(QLatin1Char(' '));
        if (fields.size() < 2)
        {
            continue;
        }

        if (fields.constFirst() == QStringLiteral("MemTotal:"))
        {
            total = fields.at(1).toULongLong();
        }
        else if (fields.constFirst() == QStringLiteral("MemAvailable:"))
        {
            available = fields.at(1).toULongLong();
        }
    }

    if (total > 0)
    {
        m_memoryPercent = qBound(0, qRound(
            100.0 * static_cast<double>(total - qMin(total, available)) /
            static_cast<double>(total)), 100);
    }
}

void SystemStatus::refreshStorage()
{
    const QStorageInfo rootStorage = QStorageInfo::root();
    const qint64 total = rootStorage.bytesTotal();
    const qint64 available = rootStorage.bytesAvailable();
    if (total <= 0 || available < 0)
    {
        m_diskPercent = 0;
        return;
    }

    m_diskPercent = qBound(0, qRound(
        100.0 * static_cast<double>(total - qMin(total, available)) /
        static_cast<double>(total)), 100);
}

void SystemStatus::refreshGraphics()
{
    m_gpuPercent = 0;
    QDirIterator cards(
        QStringLiteral("/sys/class/drm"),
        {QStringLiteral("card*")},
        QDir::Dirs | QDir::NoDotAndDotDot);
    while (cards.hasNext())
    {
        const QString cardPath = cards.next();
        for (const QString &fileName : {QStringLiteral("gpu_busy_percent"), QStringLiteral("gt_busy_percent")})
        {
            QFile usageFile(cardPath + QStringLiteral("/device/") + fileName);
            if (!usageFile.open(QIODevice::ReadOnly | QIODevice::Text))
            {
                continue;
            }

            m_gpuPercent = qBound(0, QString::fromUtf8(usageFile.readAll()).trimmed().toInt(), 100);
            return;
        }
    }
}