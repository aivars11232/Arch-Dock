// arch-dock-icon-object: makes icons into the 3D objects Arch Dock's 3D icons
// show (IconObjectMaker), one after another, and one helper at a time for the
// whole session: a second one waits for the first and leaves out what the
// first has made meanwhile. It runs only when the processor and the disk
// have nothing else to do, on two threads, within a memory limit, so the
// desktop and the user's programs never wait for it.
// Usage: arch-dock-icon-object [--threads N] [--triangles N] PICTURE OBJECT [PICTURE OBJECT]...
#include "IconObjectMaker.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QLockFile>
#include <QThread>

#include <algorithm>
#include <cstdio>
#include <sched.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace
{
// The most memory the helper may take (its data, not the files it reads).
// Making one 512 px icon took 3.9 GiB of data at its peak (4.1 GiB in
// memory), measured on 26 November 2026 with two threads and the session
// options in IconObjectMaker; the limit leaves a quarter more, so a fault
// cannot fill the memory and push the desktop into swap.
constexpr rlim_t memoryLimit = rlim_t(5) * 1024 * 1024 * 1024;

// Only what nothing else wants: the idle scheduling class for the processor
// (SCHED_IDLE, below every nice level), and the idle class for the disk
// (ioprio_set, IOPRIO_CLASS_IDLE; glibc has no wrapper for it).
// Whether the computer has the memory the network needs free now (4.1 GiB
// in use for one icon, measured on 26 November 2026; ARCHDOCK_ICON_OBJECT_
// MEMORY_MIN_MB sets another floor, 0 for none): with less, the helper waits
// for another run rather than push the computer into swapping or take
// memory others need.
bool enoughMemory()
{
    bool set = false;
    const long long floorMb = qEnvironmentVariableIntValue("ARCHDOCK_ICON_OBJECT_MEMORY_MIN_MB", &set);
    const long long floorKb = (set ? floorMb : 4608) * 1024;
    if (floorKb <= 0) return true;
    QFile info(QStringLiteral("/proc/meminfo"));
    if (!info.open(QIODevice::ReadOnly)) return true;
    const QList<QByteArray> lines = info.readAll().split('\n');
    for (const QByteArray &line : lines)
        if (line.startsWith("MemAvailable:"))
            return line.simplified().split(' ').value(1).toLongLong() >= floorKb;
    return true;
}

void runWhenIdle()
{
    setpriority(PRIO_PROCESS, 0, 19);
    sched_param parameters{};
    sched_setscheduler(0, SCHED_IDLE, &parameters);
    constexpr int ioprioWhoProcess = 1, ioprioClassIdle = 3, ioprioClassShift = 13;
    syscall(SYS_ioprio_set, ioprioWhoProcess, 0, ioprioClassIdle << ioprioClassShift);
    const rlimit limit{memoryLimit, memoryLimit};
    setrlimit(RLIMIT_DATA, &limit);
    // A helper that fails leaves no core file: one would be gigabytes.
    const rlimit noCore{0, 0};
    setrlimit(RLIMIT_CORE, &noCore);
}
}

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    runWhenIdle();
    QStringList arguments = application.arguments().mid(1);
    // Two threads at most: a quarter of the processor's threads, so the
    // helper never takes every core of a small laptop.
    int threads = std::clamp(QThread::idealThreadCount() / 4, 1, 2);
    int triangles = 0;
    // Only the dock's own pictures, written by it to be written again, are
    // removed when they cannot be read; anyone else's are left alone.
    bool removeUnreadable = false;
    while (!arguments.isEmpty() && arguments.first() == QLatin1String("--remove-unreadable"))
    {
        removeUnreadable = true;
        arguments = arguments.mid(1);
    }
    while (arguments.size() >= 2 && arguments.first().startsWith(QLatin1String("--")))
    {
        const int value = arguments.at(1).toInt();
        if (arguments.first() == QLatin1String("--threads")) threads = std::max(1, value);
        else if (arguments.first() == QLatin1String("--triangles")) triangles = std::max(100, value);
        else break;
        arguments = arguments.mid(2);
    }
    if (arguments.isEmpty() || arguments.size() % 2)
    {
        std::fprintf(stderr, "usage: arch-dock-icon-object [--remove-unreadable] [--threads N] [--triangles N]"
                             " PICTURE OBJECT [PICTURE OBJECT]...\n");
        return 1;
    }
    const QString folder = QFileInfo(arguments.at(1)).absolutePath();
    QDir().mkpath(folder);
    QLockFile making(folder + QStringLiteral("/making.lock"));
    making.setStaleLockTime(0);
    if (!making.lock())
    {
        std::fprintf(stderr, "arch-dock-icon-object: %s cannot be locked\n", qPrintable(making.fileName()));
        return 3;
    }
    // What another program's helper made, or found to give no object, while
    // this one waited is left out; with nothing left the network is not
    // loaded at all.
    const auto failedMarker = [](const QString &object) {
        return object.left(object.lastIndexOf(QLatin1Char('.'))) + QStringLiteral(".failed");
    };
    // A picture that cannot be read now (cut short as it was written by an
    // older version, say) is not marked, and the network is not loaded for
    // it; the dock's own (--remove-unreadable) is removed, so that the dock
    // writes it again and it waits for the next run.
    QStringList left;
    int failed = 0;
    for (int i = 0; i + 1 < arguments.size(); i += 2)
    {
        const QString picture = arguments.at(i), object = arguments.at(i + 1);
        if (QFileInfo::exists(object) || QFileInfo::exists(failedMarker(object)))
            continue;
        if (QImage(picture).isNull())
        {
            if (removeUnreadable)
            {
                std::fprintf(stderr, "arch-dock-icon-object: %s cannot be read; it is made again later\n", qPrintable(picture));
                QFile::remove(picture);
            }
            else
            {
                std::fprintf(stderr, "arch-dock-icon-object: %s: not a picture\n", qPrintable(picture));
                ++failed;
            }
            continue;
        }
        left << picture << object;
    }
    if (left.isEmpty()) return failed ? 4 : 0;
    if (!enoughMemory())
    {
        std::fprintf(stderr, "arch-dock-icon-object: not enough free memory now; tried again later\n");
        return 5;
    }
    ArchDock::IconObjectMaker maker(ArchDock::iconObjectModels(), threads);
    if (triangles > 0) maker.frontTriangles = std::size_t(triangles);
    if (!maker.ready())
    {
        std::fprintf(stderr, "arch-dock-icon-object: %s\n", qPrintable(maker.error()));
        return 2;
    }
    for (int i = 0; i + 1 < left.size(); i += 2)
    {
        const QString picture = left.at(i), object = left.at(i + 1);
        if (QFileInfo::exists(object)) continue;
        const QImage image(picture);
        if (image.isNull())
        {
            // Gone or spoilt since it was checked: tried again later.
            std::fprintf(stderr, "arch-dock-icon-object: %s cannot be read; it is made again later\n", qPrintable(picture));
            ++failed;
            continue;
        }
        const auto mesh = maker.make(image);
        if (!mesh || !ArchDock::saveIconObject(*mesh, object))
        {
            std::fprintf(stderr, "arch-dock-icon-object: %s: %s\n", qPrintable(picture),
                         qPrintable(mesh ? QStringLiteral("the object cannot be written")
                                         : maker.error().isEmpty() ? QStringLiteral("no object") : maker.error()));
            // Kept for good only when the network made nothing of a picture
            // it read; an error of the helper's own (memory, the network, the
            // disk) is tried again later.
            if (!mesh && maker.error().isEmpty())
            {
                QFile marker(failedMarker(object));
                if (!marker.open(QIODevice::WriteOnly))
                    std::fprintf(stderr, "arch-dock-icon-object: %s cannot be written\n", qPrintable(marker.fileName()));
            }
            ++failed;
            // The helper's own failure (memory, the network, the disk) would
            // be the same for every picture left: they wait for the next run.
            if (mesh || !maker.error().isEmpty())
                break;
            continue;
        }
        std::printf("%s: network %.1f s, field %.1f s, the rest %.1f s; %zu triangles\n", qPrintable(QFileInfo(object).fileName()),
                    maker.networkTime / 1000, maker.fieldTime / 1000, maker.meshTime / 1000, mesh->indices.size() / 3);
        std::fflush(stdout);
    }
    return failed ? 4 : 0;
}
