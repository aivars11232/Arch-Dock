// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// The update helper's rules for an Arch Dock package, checked with libarchive
// opened exactly as libalpm opens a package (every filter and format), then
// required to be a tar archive compressed with zstd, so the helper and pacman
// see the same entries. Used by the helper and its tests only.
//
// The rules are broad on purpose, so today's helper accepts future releases:
// .PKGINFO, .BUILDINFO and .MTREE once each, no .INSTALL, no path twice;
// everything else a regular file, directory or symlink under usr/ (symlinks
// pointing inside usr/), owned by root, with no xattrs or ACLs, no group or
// other write bit, no setuid or setgid bit; and .MTREE listing exactly the
// archive's entries, since libalpm installs only the paths it lists.

#include "updates/UpdateCore.h"

#include <QString>
#include <QStringList>

#include <optional>

namespace ArchDock::Updates
{
struct PackageFacts
{
    QString name;
    QString base;
    QString version;
    QString arch;
    QStringList depends;
    int entries = 0;
};

struct PackageCheck
{
    std::optional<PackageFacts> facts;
    QString problem;
};

// The package read from the open descriptor `fd`, from where it stands.
PackageCheck checkPackage(int fd);

// Why the package is not the arch-dock `manifest` describes (its name,
// version, arch and depend lines, which must equal the needs), or empty.
QString packageMismatch(const PackageFacts &facts, const Manifest &manifest);
} // namespace ArchDock::Updates
