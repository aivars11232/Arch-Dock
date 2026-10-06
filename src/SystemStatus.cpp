// SystemStatus: the sampling thread and the /proc and /sys readers.
#include "SystemStatus.h"

#include <QDir>
#include <QFile>
#include <QNetworkInterface>
#include <QStorageInfo>
#include <limits>

namespace
{
QByteArray read(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.read(65536).trimmed() : QByteArray{};
}
QVariantMap reading(bool available, const QVariant &value = {})
{
    return {{QStringLiteral("available"), available}, {QStringLiteral("value"), value}};
}
QVariantMap percentage(const QByteArray &text)
{
    bool ok = false;
    const int value = text.toInt(&ok);
    return reading(ok && value >= 0 && value <= 100, value);
}
}

SystemStatus::SystemStatus(QObject *parent) : QObject(parent), m_worker(new QObject)
{
    m_worker->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    m_thread.start();
    m_timer.setInterval(2000);
    connect(&m_timer, &QTimer::timeout, this, &SystemStatus::refresh);
    // One discovery sample makes capability filtering possible before opt-in.
    refresh();
}

SystemStatus::~SystemStatus()
{
    m_timer.stop();
    m_thread.quit();
    m_thread.wait();
}

void SystemStatus::setEnabled(bool enabled)
{
    if (m_enabled == enabled) return;
    m_enabled = enabled;
    if (enabled) { m_timer.start(); refresh(); }
    else m_timer.stop();
}

void SystemStatus::refresh()
{
    if (m_inFlight) return;
    m_inFlight = true;
    const Sample previous = m_sample;
    QMetaObject::invokeMethod(m_worker, [this, previous] {
        const Sample next = readSample(QStringLiteral("/proc"), QStringLiteral("/sys"), previous);
        QMetaObject::invokeMethod(this, [this, next] {
            m_inFlight = false;
            ++m_sampleCount;
            m_sample = next;
            m_age.restart();
            emit statusChanged();
            // CPU needs a second sample; discovery remains bounded to two.
            if (!m_enabled && m_sampleCount == 1 && next.cpuPrimed)
                QTimer::singleShot(100, this, &SystemStatus::refresh);
        }, Qt::QueuedConnection);
    }, Qt::QueuedConnection);
}

QVariantMap SystemStatus::metric(const QString &name) const
{
    auto result = m_sample.metrics.value(name).toMap();
    if (m_enabled && (!m_age.isValid() || m_age.elapsed() > 6000))
        result[QStringLiteral("available")] = false;
    return result;
}
int SystemStatus::percent(const QString &name) const
{
    const auto value = metric(name);
    return value.value(QStringLiteral("available")).toBool()
        ? value.value(QStringLiteral("value")).toInt() : -1;
}
bool SystemStatus::batteryAvailable() const { return metric(QStringLiteral("battery")).value(QStringLiteral("available")).toBool(); }
int SystemStatus::batteryPercent() const { return percent(QStringLiteral("battery")); }
bool SystemStatus::batteryCharging() const { return batteryAvailable() && metric(QStringLiteral("battery")).value(QStringLiteral("charging")).toBool(); }
bool SystemStatus::networkConnected() const { return metric(QStringLiteral("network")).value(QStringLiteral("connected")).toBool(); }
QString SystemStatus::networkName() const { return metric(QStringLiteral("network")).value(QStringLiteral("value")).toString(); }
int SystemStatus::cpuPercent() const { return percent(QStringLiteral("cpu")); }
int SystemStatus::memoryPercent() const { return percent(QStringLiteral("memory")); }
int SystemStatus::diskPercent() const { return percent(QStringLiteral("disk")); }
int SystemStatus::gpuPercent() const { return percent(QStringLiteral("gpu")); }

QStringList SystemStatus::availableSources() const
{
    QStringList result;
    for (const auto &name : {"battery", "network", "cpu", "memory", "disk", "gpu"})
        if (metric(QLatin1String(name)).value(QStringLiteral("available")).toBool())
            result.append(QStringLiteral("status:") + QLatin1String(name));
    return result;
}

QVariantList SystemStatus::entries() const
{
    QVariantList result;
    const QStringList names{QStringLiteral("battery"), QStringLiteral("network"), QStringLiteral("cpu"),
        QStringLiteral("memory"), QStringLiteral("disk"), QStringLiteral("gpu")};
    const QStringList labels{tr("Battery"), tr("Network interface"), tr("CPU"), tr("Memory"), tr("Root disk"), tr("GPU")};
    const QStringList icons{QStringLiteral("battery"), QStringLiteral("network-wired"), QStringLiteral("cpu"),
        QStringLiteral("ram"), QStringLiteral("drive-harddisk"), QStringLiteral("video-display")};
    for (int i = 0; i < names.size(); ++i)
    {
        const auto value = metric(names[i]);
        const bool available = value.value(QStringLiteral("available")).toBool()
            && m_age.isValid() && m_age.elapsed() <= 6000;
        const QString text = !available ? tr("Unavailable") : names[i] == QStringLiteral("network")
            ? value.value(QStringLiteral("value")).toString()
            : QString::number(value.value(QStringLiteral("value")).toInt()) + QLatin1Char('%');
        result.append(QVariantMap{{QStringLiteral("appId"), QStringLiteral("status:") + names[i]},
            {QStringLiteral("isStatus"), true}, {QStringLiteral("statusAvailable"), available},
            {QStringLiteral("statusText"), text}, {QStringLiteral("displayName"), labels[i] + QStringLiteral(": ") + text},
            {QStringLiteral("iconName"), icons[i]}});
    }
    return result;
}

SystemStatus::Sample SystemStatus::readSample(const QString &procRoot, const QString &sysRoot,
                                             const Sample &previous, bool systemDevices)
{
    Sample result;
    for (const auto &name : {"battery", "network", "cpu", "memory", "disk", "gpu"})
        result.metrics.insert(QLatin1String(name), reading(false));
    // Linux counts guest time inside user/nice already: only the first eight
    // counters belong in the total. Missing, reset or malformed samples fail closed.
    const auto cpu = read(procRoot + QStringLiteral("/stat")).split('\n').value(0).simplified().split(' ');
    bool valid = cpu.size() >= 5 && cpu.first() == "cpu";
    quint64 total = 0, idle = 0;
    for (int i = 1; valid && i < qMin(cpu.size(), qsizetype(9)); ++i)
    {
        bool ok = false;
        const auto value = cpu[i].toULongLong(&ok);
        valid = ok && !cpu[i].startsWith('-') && total <= std::numeric_limits<quint64>::max() - value;
        total += value;
        if (i == 4 || i == 5) idle += value;
    }
    if (valid && idle <= total)
    {
        result.cpuPrimed = true; result.cpuTotal = total; result.cpuIdle = idle;
        if (previous.cpuPrimed && total > previous.cpuTotal && idle >= previous.cpuIdle
            && idle - previous.cpuIdle <= total - previous.cpuTotal)
            result.metrics[QStringLiteral("cpu")] = reading(true,
                qRound(100.0 * (1.0 - double(idle - previous.cpuIdle) / double(total - previous.cpuTotal))));
    }
    QHash<QByteArray, quint64> memory;
    for (const auto &line : read(procRoot + QStringLiteral("/meminfo")).split('\n'))
    {
        const auto fields = line.simplified().split(' ');
        if (fields.size() < 2) continue;
        bool ok = false;
        const auto value = fields[1].toULongLong(&ok);
        if (ok && !fields[1].startsWith('-')) memory.insert(fields[0], value);
    }
    if (memory.value("MemTotal:") > 0 && memory.contains("MemAvailable:")
        && memory.value("MemAvailable:") <= memory.value("MemTotal:"))
        result.metrics[QStringLiteral("memory")] = reading(true,
            qRound(100.0 * (1.0 - double(memory.value("MemAvailable:")) / double(memory.value("MemTotal:")))));
    const QDir power(sysRoot + QStringLiteral("/class/power_supply"));
    for (const auto &name : power.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name))
    {
        const auto path = power.filePath(name);
        if (read(path + QStringLiteral("/type")) != "Battery") continue;
        auto battery = percentage(read(path + QStringLiteral("/capacity")));
        battery[QStringLiteral("charging")] = read(path + QStringLiteral("/status")) == "Charging";
        result.metrics[QStringLiteral("battery")] = battery;
        break;
    }
    const QDir cards(sysRoot + QStringLiteral("/class/drm"));
    for (const auto &name : cards.entryList({QStringLiteral("card*")}, QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name))
    {
        const auto gpu = percentage(read(cards.filePath(name) + QStringLiteral("/device/gpu_busy_percent")));
        if (gpu.value(QStringLiteral("available")).toBool()) { result.metrics[QStringLiteral("gpu")] = gpu; break; }
    }
    if (systemDevices)
    {
        const QStorageInfo storage = QStorageInfo::root();
        if (storage.isValid() && storage.isReady() && storage.bytesTotal() > 0
            && storage.bytesAvailable() >= 0 && storage.bytesAvailable() <= storage.bytesTotal())
            result.metrics[QStringLiteral("disk")] = reading(true,
                qRound(100.0 * (1.0 - double(storage.bytesAvailable()) / double(storage.bytesTotal()))));
        const auto interfaces = QNetworkInterface::allInterfaces();
        auto network = reading(!interfaces.isEmpty(), tr("No active interface"));
        network[QStringLiteral("connected")] = false;
        for (const auto &interface : interfaces)
        {
            const auto flags = interface.flags();
            if (flags.testFlag(QNetworkInterface::IsUp) && flags.testFlag(QNetworkInterface::IsRunning)
                && !flags.testFlag(QNetworkInterface::IsLoopBack))
            {
                network[QStringLiteral("value")] = interface.humanReadableName();
                network[QStringLiteral("connected")] = true;
                break;
            }
        }
        result.metrics[QStringLiteral("network")] = network;
    }
    return result;
}
