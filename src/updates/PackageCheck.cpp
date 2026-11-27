// SPDX-License-Identifier: GPL-3.0-or-later
#include "PackageCheck.h"

#include <QHash>
#include <QSet>

#include <archive.h>
#include <archive_entry.h>

#include <memory>
#include <sys/stat.h>

namespace ArchDock::Updates
{
namespace
{
struct ArchiveFree
{
    void operator()(struct archive *archive) const
    {
        archive_read_free(archive);
    }
};
using Archive = std::unique_ptr<struct archive, ArchiveFree>;

constexpr la_ssize_t MaxMetadataBytes = 1024 * 1024;

// The parts of a path inside the archive, or nothing when it is absolute,
// starts with "./", or has an empty, "." or ".." part.
std::optional<QStringList> pathParts(const QString &path)
{
    if (path.isEmpty() || path.startsWith(u'/') || path.startsWith(QLatin1String("./")))
        return std::nullopt;
    const QStringList parts = path.split(u'/');
    for (const QString &part : parts)
        if (part.isEmpty() || part == QLatin1String(".") || part == QLatin1String(".."))
            return std::nullopt;
    return parts;
}

// Whether a symlink at `parts` pointing at `target` stays inside usr/.
bool linkStaysInUsr(const QStringList &parts, const QString &target)
{
    if (target.isEmpty())
        return false;
    QStringList resolved = target.startsWith(u'/') ? QStringList{} : parts.mid(0, parts.size() - 1);
    for (const QString &part : target.split(u'/'))
    {
        if (part.isEmpty() || part == QLatin1String("."))
            continue;
        if (part == QLatin1String(".."))
        {
            if (resolved.isEmpty())
                return false;
            resolved.removeLast();
            continue;
        }
        resolved.append(part);
    }
    return !resolved.isEmpty() && resolved.first() == QLatin1String("usr");
}

QByteArray readData(struct archive *archive)
{
    QByteArray data;
    char buffer[16384];
    for (;;)
    {
        const la_ssize_t read = archive_read_data(archive, buffer, sizeof buffer);
        if (read < 0 || data.size() + read > MaxMetadataBytes)
            return QByteArray();
        if (read == 0)
            return data;
        data.append(buffer, read);
    }
}

// The paths .MTREE lists, read with libarchive's mtree reader as libalpm does.
std::optional<QSet<QString>> mtreePaths(const QByteArray &mtree)
{
    Archive archive(archive_read_new());
    archive_read_support_filter_all(archive.get());
    archive_read_support_format_mtree(archive.get());
    if (archive_read_open_memory(archive.get(), mtree.constData(), static_cast<size_t>(mtree.size())) != ARCHIVE_OK)
        return std::nullopt;
    QSet<QString> paths;
    struct archive_entry *entry = nullptr;
    int status;
    while ((status = archive_read_next_header(archive.get(), &entry)) == ARCHIVE_OK)
    {
        QString path = QString::fromUtf8(archive_entry_pathname(entry));
        if (path.startsWith(QLatin1String("./")))
            path.remove(0, 2);
        while (path.endsWith(u'/'))
            path.chop(1);
        paths.insert(path);
    }
    if (status != ARCHIVE_EOF)
        return std::nullopt;
    return paths;
}
} // namespace

PackageCheck checkPackage(int fd)
{
    PackageCheck result;
    const auto refused = [&result](const QString &why)
    {
        result.problem = why;
        result.facts.reset();
        return result;
    };
    Archive archive(archive_read_new());
    archive_read_support_filter_all(archive.get());
    archive_read_support_format_all(archive.get());
    if (archive_read_open_fd(archive.get(), fd, 64 * 1024) != ARCHIVE_OK)
        return refused(QStringLiteral("the package cannot be opened as an archive"));

    QSet<QString> seen;
    QHash<QString, QByteArray> metadata;
    struct archive_entry *entry = nullptr;
    bool first = true;
    int status;
    while ((status = archive_read_next_header(archive.get(), &entry)) == ARCHIVE_OK)
    {
        if (first)
        {
            first = false;
            if (archive_filter_count(archive.get()) != 2 || archive_filter_code(archive.get(), 0) != ARCHIVE_FILTER_ZSTD)
                return refused(QStringLiteral("the package is not compressed with zstd"));
            if ((archive_format(archive.get()) & ARCHIVE_FORMAT_BASE_MASK) != ARCHIVE_FORMAT_TAR)
                return refused(QStringLiteral("the package is not a tar archive"));
        }
        const char *rawPath = archive_entry_pathname(entry);
        QString path = rawPath ? QString::fromUtf8(rawPath) : QString();
        while (path.endsWith(u'/') && archive_entry_filetype(entry) == AE_IFDIR)
            path.chop(1);
        const std::optional<QStringList> parts = pathParts(path);
        if (!parts)
            return refused(QStringLiteral("the package holds the path %1").arg(QString::fromUtf8(rawPath ? rawPath : "")));
        if (seen.contains(path))
            return refused(QStringLiteral("the package lists %1 twice").arg(path));
        seen.insert(path);
        if (archive_entry_hardlink(entry))
            return refused(QStringLiteral("%1 is a hard link").arg(path));
        if (archive_entry_uid(entry) != 0 || archive_entry_gid(entry) != 0)
            return refused(QStringLiteral("%1 is not owned by root").arg(path));
        if (archive_entry_xattr_count(entry) != 0
            || archive_entry_acl_count(entry, ARCHIVE_ENTRY_ACL_TYPE_ACCESS | ARCHIVE_ENTRY_ACL_TYPE_DEFAULT
                                                  | ARCHIVE_ENTRY_ACL_TYPE_NFS4) != 0)
            return refused(QStringLiteral("%1 carries xattrs or ACLs").arg(path));
        const unsigned type = archive_entry_filetype(entry);
        const unsigned permissions = archive_entry_perm(entry);
        if (type != AE_IFREG && type != AE_IFDIR && type != AE_IFLNK)
            return refused(QStringLiteral("%1 is a device, fifo or socket").arg(path));
        if (type != AE_IFLNK && (permissions & 022))
            return refused(QStringLiteral("%1 can be written by group or others").arg(path));
        if (permissions & (S_ISUID | S_ISGID))
            return refused(QStringLiteral("%1 is setuid or setgid").arg(path));

        if (path.startsWith(u'.'))
        {
            if (path == QLatin1String(".INSTALL"))
                return refused(QStringLiteral("the package has an install script (.INSTALL)"));
            if (path != QLatin1String(".PKGINFO") && path != QLatin1String(".BUILDINFO") && path != QLatin1String(".MTREE"))
                return refused(QStringLiteral("the package holds %1 outside usr/").arg(path));
            if (type != AE_IFREG)
                return refused(QStringLiteral("%1 is not a file").arg(path));
            const QByteArray data = readData(archive.get());
            if (data.isEmpty())
                return refused(QStringLiteral("%1 cannot be read").arg(path));
            metadata.insert(path, data);
            continue;
        }
        if (parts->first() != QLatin1String("usr"))
            return refused(QStringLiteral("%1 lies outside usr/").arg(path));
        if (type == AE_IFLNK && !linkStaysInUsr(*parts, QString::fromUtf8(archive_entry_symlink(entry))))
            return refused(QStringLiteral("the link %1 points outside usr/").arg(path));
        archive_read_data_skip(archive.get());
    }
    if (status != ARCHIVE_EOF)
        return refused(QStringLiteral("the package cannot be read to its end"));
    for (const char *name : {".PKGINFO", ".BUILDINFO", ".MTREE"})
        if (!metadata.contains(QString::fromLatin1(name)))
            return refused(QStringLiteral("the package has no %1").arg(QString::fromLatin1(name)));

    // libalpm keeps the last of repeated lines, so each must appear once.
    PackageFacts facts;
    QHash<QString, int> counts;
    for (const QByteArray &rawLine : metadata.value(QStringLiteral(".PKGINFO")).split('\n'))
    {
        const QString line = QString::fromUtf8(rawLine);
        if (line.isEmpty() || line.startsWith(u'#'))
            continue;
        const qsizetype separator = line.indexOf(QLatin1String(" = "));
        if (separator <= 0)
            return refused(QStringLiteral(".PKGINFO has a line that is not key = value"));
        const QString key = line.left(separator);
        const QString value = line.mid(separator + 3);
        ++counts[key];
        if (key == QLatin1String("pkgname"))
            facts.name = value;
        else if (key == QLatin1String("pkgbase"))
            facts.base = value;
        else if (key == QLatin1String("pkgver"))
            facts.version = value;
        else if (key == QLatin1String("arch"))
            facts.arch = value;
        else if (key == QLatin1String("depend"))
            facts.depends.append(value);
    }
    for (const char *key : {"pkgname", "pkgbase", "pkgver", "arch"})
        if (counts.value(QString::fromLatin1(key)) != 1)
            return refused(QStringLiteral(".PKGINFO names %1 %2 times, not once").arg(QString::fromLatin1(key)).arg(counts.value(QString::fromLatin1(key))));

    const std::optional<QSet<QString>> listed = mtreePaths(metadata.value(QStringLiteral(".MTREE")));
    if (!listed)
        return refused(QStringLiteral(".MTREE cannot be read"));
    QSet<QString> entries = seen;
    entries.remove(QStringLiteral(".MTREE"));
    if (*listed != entries)
        return refused(QStringLiteral(".MTREE does not list exactly the package's entries"));
    facts.entries = static_cast<int>(seen.size());
    result.facts = facts;
    return result;
}

QString packageMismatch(const PackageFacts &facts, const Manifest &manifest)
{
    if (facts.name != QLatin1String("arch-dock"))
        return QStringLiteral("the package is %1, not arch-dock").arg(facts.name);
    if (facts.version != manifest.version)
        return QStringLiteral("the package is version %1, not %2").arg(facts.version, manifest.version);
    if (facts.arch != QLatin1String("x86_64"))
        return QStringLiteral("the package is for %1, not x86_64").arg(facts.arch);
    if (facts.depends != manifest.package.needs)
        return QStringLiteral("the package's dependencies are not the manifest's needs");
    return {};
}
} // namespace ArchDock::Updates
