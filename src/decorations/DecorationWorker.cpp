#include "DecorationWorker.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <QRegularExpression>
#include <QThread>

#include <atomic>
#include <memory>

namespace ArchDock
{

QJSValue DecorationScripts::module(const QString &file, QString *error)
{
    const QString path = QFileInfo(file).absoluteFilePath();
    const auto known = m_modules.constFind(path);
    if (known != m_modules.constEnd()) return *known;
    QFile source(path);
    if (!source.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        if (error) *error = QStringLiteral("cannot read %1").arg(path);
        return QJSValue(QJSValue::UndefinedValue);
    }
    QString text = QString::fromUtf8(source.readAll());
    // The QML directives become blank lines, so the engine's line numbers
    // still point at the file's own lines (one on, for the wrapper's).
    static const QRegularExpression pragma(QStringLiteral(R"re(^\.pragma[ \t]+library[ \t]*$)re"),
                                           QRegularExpression::MultilineOption);
    static const QRegularExpression import(QStringLiteral(R"re(^\.import[ \t]+"([^"]+)"[ \t]+as[ \t]+(\w+)[ \t]*$)re"),
                                           QRegularExpression::MultilineOption);
    static const QRegularExpression declaration(
        QStringLiteral(R"re(^(?:function[ \t]+(\w+)|var[ \t]+(\w+)|const[ \t]+(\w+)|let[ \t]+(\w+)))re"),
        QRegularExpression::MultilineOption);
    text.replace(pragma, QString());
    QStringList aliases;
    QJSValueList imports;
    for (auto it = import.globalMatch(text); it.hasNext();)
    {
        const QRegularExpressionMatch match = it.next();
        const QJSValue dependency = module(QFileInfo(path).absolutePath() + QLatin1Char('/') + match.captured(1), error);
        if (dependency.isUndefined()) return dependency;
        aliases << match.captured(2);
        imports << dependency;
    }
    text.replace(import, QString());
    QStringList names;
    for (auto it = declaration.globalMatch(text); it.hasNext();)
    {
        const QRegularExpressionMatch match = it.next();
        for (int k = 1; k <= 4; ++k)
            if (!match.captured(k).isEmpty())
            {
                names << match.captured(k);
                break;
            }
    }
    names.removeDuplicates();
    QStringList exports;
    for (const QString &name : std::as_const(names)) exports << name + QStringLiteral(": ") + name;
    const QString wrapped = QStringLiteral("(function(%1) {\n%2\nreturn { %3 };\n})")
                                .arg(aliases.join(QStringLiteral(", ")), text, exports.join(QStringLiteral(", ")));
    const QJSValue factory = m_engine.evaluate(wrapped, path);
    QJSValue result = factory.isError() ? factory : factory.call(imports);
    if (result.isError())
    {
        if (error)
            *error = QStringLiteral("%1:%2: %3")
                         .arg(path, result.property(QStringLiteral("lineNumber")).toString(), result.toString());
        return QJSValue(QJSValue::UndefinedValue);
    }
    m_modules.insert(path, result);
    return result;
}

namespace
{

std::atomic<bool> quitting{false};

// Lives on the worker's thread with the scripts, and is deleted there when
// the thread ends.
class WorkerContext : public QObject
{
public:
    std::unique_ptr<DecorationScripts> scripts;
};

struct Worker
{
    QThread thread;
    WorkerContext *context = new WorkerContext;

    Worker()
    {
        thread.setObjectName(QStringLiteral("ArchDock decorations"));
        context->moveToThread(&thread);
        QObject::connect(&thread, &QThread::finished, context, &QObject::deleteLater);
        thread.start(QThread::LowPriority);
    }

    void stop()
    {
        quitting = true;
        if (thread.isRunning())
        {
            thread.quit();
            thread.wait();
        }
    }
};

// The worker is made on first use and kept to the end; it is stopped as
// the application quits, before the application object goes.
Worker *worker()
{
    static Worker *const instance = []() {
        auto *made = new Worker;
        qAddPostRoutine([]() { worker()->stop(); });
        if (QCoreApplication *application = QCoreApplication::instance())
            QObject::connect(application, &QCoreApplication::aboutToQuit, application, []() { quitting = true; });
        return made;
    }();
    return instance;
}

} // namespace

void DecorationWorker::post(std::function<void(DecorationScripts &)> job)
{
    if (quitting) return;
    WorkerContext *context = worker()->context;
    QMetaObject::invokeMethod(
        context,
        [context, job = std::move(job)]() {
            if (quitting) return;
            if (!context->scripts) context->scripts = std::make_unique<DecorationScripts>();
            job(*context->scripts);
        },
        Qt::QueuedConnection);
}

bool DecorationWorker::stopping()
{
    return quitting;
}

} // namespace ArchDock
