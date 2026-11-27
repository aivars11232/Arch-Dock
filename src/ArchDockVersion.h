// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Arch Dock's release and the constants of its Update button, as this build
// was configured (CMake, cmake/ArchDockVersion.h.in). Shared by the backend,
// the bars and the update helper.

#include <QByteArray>
#include <QString>

namespace ArchDock::Version
{
// The release this build is, "0.1.1-55", or empty for a development build.
QString release();
// What a person reads: the release, or "0.1.1 (development build)".
QString displayVersion();
// "release", "test" (a test key or a rehearsal root) or "development".
QString buildKind();
bool isTestBuild();
// The GitHub repository whose releases the Update button looks at, owner/name.
QString repository();
// The one key the Update button trusts: its id, its public key (the DER of
// its SubjectPublicKeyInfo) and that key's SHA-256 fingerprint in hex. All
// empty when updates are not configured.
QString keyId();
QByteArray keySpki();
QString keyFingerprint();
// Whether this build can check and install updates: it has a release and a key.
bool updatesConfigured();
// The update protocol this build speaks.
int updaterProtocol();
// Where the system tools the updater calls live, and the updater's own parts.
QString toolFolder();
// The installed backend, <prefix>/bin/arch-dock.
QString backendPath();
QString helperPath();
QString finisherPath();
QString policyPath();
// pacman's lock file, as this account sees it.
QString pacmanLock();
// GitHub's API and download addresses; a rehearsal build's test server.
QString apiAddress();
QString downloadsAddress();
// The Qt version this build was compiled against.
QString qtBuildVersion();
// A TEST build's private rehearsal root, or empty.
QString rehearsalRoot();
} // namespace ArchDock::Version
