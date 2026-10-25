#pragma once

#include <QByteArray>
#include <QtGlobal>

namespace ArchDock::QmlGarbageCollector
{
// Since Qt 6.8 the QML engine's garbage collector works in slices of a few
// milliseconds between events. When a sliced collection is then forced to
// finish in one go, it can still walk storage an earlier slice freed and the
// process crashes inside the collector. Arch Dock's own tests hit this on
// recent Qt versions, always while a Panel Studio page was being built, and
// other Qt programs report the same crash; Qt's fix for QTBUG-148459 guards
// only one such spot. QV4_GC_TIMELIMIT at 0 makes every collection run to
// completion in one call, as collections did before 6.8: a short pause on
// Arch Dock's small heaps instead of a crash.
//
// Each engine reads the variable once, when it is created, so the policy is
// held around the engine's creation and taken out of the environment again
// when it goes out of scope: programs Arch Dock starts must not inherit it.
// A limit the user set stays as it is.
class WholeCollections
{
public:
    WholeCollections()
    {
        bool userSet = false;
        qEnvironmentVariableIntValue("QV4_GC_TIMELIMIT", &userSet);
        if (!userSet)
        {
            m_set = true;
            qputenv("QV4_GC_TIMELIMIT", QByteArrayLiteral("0"));
        }
    }
    ~WholeCollections()
    {
        if (m_set) qunsetenv("QV4_GC_TIMELIMIT");
    }
    WholeCollections(const WholeCollections &) = delete;
    WholeCollections &operator=(const WholeCollections &) = delete;

private:
    bool m_set = false;
};
}
