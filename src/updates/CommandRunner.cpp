// SPDX-License-Identifier: GPL-3.0-or-later
#include "CommandRunner.h"

#include "ArchDockVersion.h"

#include <QDebug>
#include <QUrl>

namespace ArchDock::Updates
{
bool installsUpdates()
{
    return Version::updatesConfigured() && (!Version::isTestBuild() || !Version::rehearsalRoot().isEmpty());
}

bool BackendCommandRunner::allowed(const QString &program, const QStringList &arguments)
{
    if (installsUpdates())
        return true;
    const QString name = program.section(u'/', -1);
    if (name == QLatin1String("pkexec") || name == QLatin1String("systemd-run"))
        return false;
    return !(name == QLatin1String("pacman") && arguments.contains(QStringLiteral("-U")));
}

CommandRunner::Result BackendCommandRunner::run(const QString &program, const QStringList &arguments,
                                                const QByteArray &input)
{
    if (!allowed(program, arguments))
    {
        qWarning().noquote() << "Arch Dock's updater refuses" << program << "in a build that does not install updates";
        return {};
    }
    return m_system.run(program, arguments, input);
}

CommandRunner::Result BackendCommandRunner::runInSession(const QString &program, const QStringList &arguments,
                                                         const QStringList &session)
{
    if (!allowed(program, arguments))
    {
        qWarning().noquote() << "Arch Dock's updater refuses" << program << "in a build that does not install updates";
        return {};
    }
    return m_system.runInSession(program, arguments, session);
}

bool networkAllowed(const QString &address)
{
    const QUrl url(address);
    if (!Version::rehearsalRoot().isEmpty())
        return url.scheme() == QLatin1String("http") && url.host() == QLatin1String("127.0.0.1");
    return installsUpdates() && url.scheme() == QLatin1String("https");
}
} // namespace ArchDock::Updates
