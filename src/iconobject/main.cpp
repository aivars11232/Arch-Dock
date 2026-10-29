// arch-dock-icon-object: makes icons into the 3D objects Arch Dock's 3D icons
// show (IconObjectMaker), at the lowest priority, one after another, and
// one helper at a time for the whole session: a second one waits for the
// first and leaves out what the first has made meanwhile.
// Usage: arch-dock-icon-object [--threads N] [--triangles N] PICTURE OBJECT [PICTURE OBJECT]...
#include "IconObjectMaker.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QLockFile>
#include <QThread>

#include <algorithm>
#include <cstdio>
#include <sys/resource.h>

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    setpriority(PRIO_PROCESS, 0, 19);
    QStringList arguments = application.arguments().mid(1);
    // As many threads as the processor has cores, at most four: the
    // network runs slower on a core's second thread than without it.
    int threads = std::clamp(QThread::idealThreadCount() / 2, 1, 4);
    int triangles = 0;
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
        std::fprintf(stderr, "usage: arch-dock-icon-object [--threads N] [--triangles N] PICTURE OBJECT [PICTURE OBJECT]...\n");
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
    ArchDock::IconObjectMaker maker(ArchDock::iconObjectModels(), threads);
    if (triangles > 0) maker.frontTriangles = std::size_t(triangles);
    if (!maker.ready())
    {
        std::fprintf(stderr, "arch-dock-icon-object: %s\n", qPrintable(maker.error()));
        return 2;
    }
    int failed = 0;
    for (int i = 0; i + 1 < arguments.size(); i += 2)
    {
        const QString picture = arguments.at(i), object = arguments.at(i + 1);
        if (QFileInfo::exists(object)) continue;
        const QImage image(picture);
        const auto mesh = image.isNull() ? std::nullopt : maker.make(image);
        if (!mesh || !ArchDock::saveIconObject(*mesh, object))
        {
            std::fprintf(stderr, "arch-dock-icon-object: %s: %s\n", qPrintable(picture),
                         image.isNull() ? "not a picture" : qPrintable(maker.error().isEmpty() ? QStringLiteral("no object") : maker.error()));
            ++failed;
            continue;
        }
        std::printf("%s: network %.1f s, field %.1f s, the rest %.1f s; %zu triangles\n", qPrintable(QFileInfo(object).fileName()),
                    maker.networkTime / 1000, maker.fieldTime / 1000, maker.meshTime / 1000, mesh->indices.size() / 3);
        std::fflush(stdout);
    }
    return failed ? 4 : 0;
}
