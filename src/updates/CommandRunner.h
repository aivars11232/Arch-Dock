// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// The commands Arch Dock's backend runs for its Update button, with a fence
// in the production code itself: a build without a release and a key, and a
// TEST build other than the rehearsal build (the verification packages),
// refuses pkexec, systemd-run and `pacman -U`, and the network layer refuses
// every request. So no test that builds or installs a real backend can reach
// the owner's password window or GitHub.

#include "updates/UpdateCore.h"

namespace ArchDock::Updates
{
class BackendCommandRunner final : public CommandRunner
{
public:
    Result run(const QString &program, const QStringList &arguments, const QByteArray &input = {}) override;
    Result runInSession(const QString &program, const QStringList &arguments, const QStringList &session) override;

    // Whether this command may run in this build.
    static bool allowed(const QString &program, const QStringList &arguments);

private:
    SystemCommandRunner m_system;
};

// Whether this build installs updates: a release build, or the rehearsal
// build with its own stand-ins. Every other build, the TEST builds of the
// verification packages included, is fenced.
bool installsUpdates();

// Whether this build may use the network for updates, and for which address:
// a release build only https, a rehearsal build only http://127.0.0.1.
bool networkAllowed(const QString &address);
} // namespace ArchDock::Updates
