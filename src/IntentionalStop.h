#pragma once

#include <QString>

namespace ArchDock::IntentionalStop
{
// A user's request to keep Arch Dock stopped. Installed applets and the KWin
// window watcher reach the backend through session D-Bus activation, which
// would otherwise start it again on the next window event. The request lives
// in the user's runtime directory and names the login session it was made in,
// so it ends with that session even where the runtime directory outlives it
// (a lingering user). Starting Arch Dock explicitly clears it.

// The state file, or empty when no safe user runtime directory exists.
QString statePath();
// This boot and login session: the scope of a recorded request.
QString sessionIdentity();
// A valid request for this session exists. A request from another session
// or anything other than a small regular file owned by this user is removed.
bool active();
bool record(const QString &reason, QString *error = nullptr);
bool clear(QString *error = nullptr);
}
