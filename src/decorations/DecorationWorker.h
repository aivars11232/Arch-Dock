#pragma once

#include <QHash>
#include <QJSEngine>
#include <QJSValue>
#include <QString>

#include <functional>

namespace ArchDock
{

// The decoration scripts (ArchDock.Rendering's DecorationJob.js and what it
// imports) loaded into a JavaScript engine of their own. A script written
// for QML (".pragma library", ".import ... as Name") is loaded the way QML
// loads it: each imported file once, under its name, and every top-level
// function, var and const of a file is what the file gives.
class DecorationScripts
{
public:
    // The module in `file`, with its imports; undefined, with `error` set,
    // when it cannot be loaded.
    QJSValue module(const QString &file, QString *error);

private:
    QJSEngine m_engine;
    QHash<QString, QJSValue> m_modules;
};

// One thread every decoration baker shares, so working out and lighting a
// decoration never holds up a panel, and a desktop of panels does not fill
// the processor: the jobs run one after another, each with the worker's
// scripts at hand.
namespace DecorationWorker
{
// Runs `job` on the worker's thread after the jobs posted before it.
void post(std::function<void(DecorationScripts &)> job);
// True once the application has started to quit: a job then stops.
bool stopping();
} // namespace DecorationWorker

} // namespace ArchDock
