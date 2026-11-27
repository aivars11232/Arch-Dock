// SPDX-License-Identifier: GPL-3.0-or-later
// The update helper's package rules (src/updates/PackageCheck), on packages
// this test writes with libarchive: one good package, and one case for each
// rule, each changing one thing.

#include "updates/PackageCheck.h"

#include <QSet>
#include <QTemporaryFile>
#include <QTest>

#include <archive.h>
#include <archive_entry.h>

#include <functional>
#include <sys/stat.h>

namespace
{
struct Entry
{
    QString path;
    QByteArray data;
    unsigned type = AE_IFREG;
    int permissions = 0644;
    int uid = 0;
    int gid = 0;
    QString link;
    QString hardlink;
    QByteArray xattr;
    bool acl = false;
};

struct Package
{
    QList<Entry> entries;
    QString pkginfo = QStringLiteral("pkgname = arch-dock\npkgbase = arch-dock\npkgver = 0.1.1-56\narch = x86_64\n"
                                     "depend = glibc\ndepend = qt6-base>=6.12\n");
    QSet<QString> mtreeSkip;
    QStringList mtreeExtra;
    int filter = ARCHIVE_FILTER_ZSTD;
    // The archive format: tar (pax), zip or cpio.
    enum class Format { Tar, Zip, Cpio } format = Format::Tar;
    std::function<void(QList<Entry> &)> tweak;
};

void addEntry(struct archive *archive, const Entry &entry)
{
    struct archive_entry *header = archive_entry_new();
    archive_entry_set_pathname_utf8(header, entry.path.toUtf8().constData());
    archive_entry_set_filetype(header, entry.type);
    archive_entry_set_perm(header, static_cast<mode_t>(entry.permissions));
    archive_entry_set_uid(header, entry.uid);
    archive_entry_set_gid(header, entry.gid);
    archive_entry_set_mtime(header, 1764000000, 0);
    if (!entry.link.isEmpty())
        archive_entry_set_symlink_utf8(header, entry.link.toUtf8().constData());
    if (!entry.hardlink.isEmpty())
        archive_entry_set_hardlink_utf8(header, entry.hardlink.toUtf8().constData());
    if (!entry.xattr.isEmpty())
        archive_entry_xattr_add_entry(header, "security.capability", entry.xattr.constData(), static_cast<size_t>(entry.xattr.size()));
    if (entry.acl)
        archive_entry_acl_add_entry(header, ARCHIVE_ENTRY_ACL_TYPE_ACCESS, ARCHIVE_ENTRY_ACL_READ,
                                    ARCHIVE_ENTRY_ACL_USER, 1000, "someone");
    archive_entry_set_size(header, entry.type == AE_IFREG && entry.hardlink.isEmpty() ? entry.data.size() : 0);
    archive_write_header(archive, header);
    if (entry.type == AE_IFREG && !entry.data.isEmpty())
        archive_write_data(archive, entry.data.constData(), static_cast<size_t>(entry.data.size()));
    archive_entry_free(header);
}

QByteArray mtreeOf(const QList<Entry> &entries)
{
    struct archive *archive = archive_write_new();
    archive_write_set_format_mtree(archive);
    archive_write_set_options(archive, "!all,use-set,type,uid,gid,mode,time,size,link");
    archive_write_add_filter_gzip(archive);
    QByteArray buffer(1024 * 1024, '\0');
    size_t used = 0;
    archive_write_open_memory(archive, buffer.data(), static_cast<size_t>(buffer.size()), &used);
    for (const Entry &entry : entries)
    {
        Entry listed = entry;
        listed.path = QStringLiteral("./") + entry.path;
        listed.data.clear();
        listed.xattr.clear();
        listed.acl = false;
        addEntry(archive, listed);
    }
    archive_write_close(archive);
    archive_write_free(archive);
    buffer.truncate(static_cast<qsizetype>(used));
    return buffer;
}

QByteArray build(Package package)
{
    QList<Entry> files = {
        {QStringLiteral("usr"), {}, AE_IFDIR, 0755},
        {QStringLiteral("usr/bin"), {}, AE_IFDIR, 0755},
        {QStringLiteral("usr/bin/arch-dock"), QByteArray(4096, 'x'), AE_IFREG, 0755},
        {QStringLiteral("usr/lib"), {}, AE_IFDIR, 0755},
        {QStringLiteral("usr/lib/arch-dock"), {}, AE_IFDIR, 0755},
        {QStringLiteral("usr/lib/arch-dock/arch-dock-install-update"), QByteArray(2048, 'y'), AE_IFREG, 0755},
        {QStringLiteral("usr/share"), {}, AE_IFDIR, 0755},
        {QStringLiteral("usr/share/arch-dock"), {}, AE_IFDIR, 0755},
        {QStringLiteral("usr/share/arch-dock/notes.txt"), QByteArrayLiteral("notes\n"), AE_IFREG, 0644},
    };
    if (package.tweak)
        package.tweak(files);
    QList<Entry> meta = {{QStringLiteral(".PKGINFO"), package.pkginfo.toUtf8()},
                         {QStringLiteral(".BUILDINFO"), QByteArrayLiteral("format = 2\n")}};
    QList<Entry> listed;
    QSet<QString> listedPaths;
    for (const Entry &entry : meta + files)
    {
        if (package.mtreeSkip.contains(entry.path) || listedPaths.contains(entry.path))
            continue;
        listedPaths.insert(entry.path);
        listed.append(entry);
    }
    for (const QString &extra : package.mtreeExtra)
        listed.append({extra, QByteArrayLiteral("x")});

    struct archive *archive = archive_write_new();
    if (package.format == Package::Format::Zip)
        archive_write_set_format_zip(archive);
    else if (package.format == Package::Format::Cpio)
        archive_write_set_format_cpio_newc(archive);
    else
        archive_write_set_format_pax_restricted(archive);
    if (package.filter == ARCHIVE_FILTER_ZSTD)
        archive_write_add_filter_zstd(archive);
    else if (package.filter == ARCHIVE_FILTER_GZIP)
        archive_write_add_filter_gzip(archive);
    else if (package.filter == ARCHIVE_FILTER_XZ)
        archive_write_add_filter_xz(archive);
    QByteArray buffer(8 * 1024 * 1024, '\0');
    size_t used = 0;
    archive_write_open_memory(archive, buffer.data(), static_cast<size_t>(buffer.size()), &used);
    for (const Entry &entry : meta)
        addEntry(archive, entry);
    addEntry(archive, {QStringLiteral(".MTREE"), mtreeOf(listed)});
    for (const Entry &entry : files)
        addEntry(archive, entry);
    archive_write_close(archive);
    archive_write_free(archive);
    buffer.truncate(static_cast<qsizetype>(used));
    return buffer;
}

ArchDock::Updates::PackageCheck check(const QByteArray &bytes)
{
    QTemporaryFile file;
    if (!file.open())
        return {};
    file.write(bytes);
    file.flush();
    ::lseek(file.handle(), 0, SEEK_SET);
    return ArchDock::Updates::checkPackage(file.handle());
}

ArchDock::Updates::Manifest manifest()
{
    ArchDock::Updates::Manifest manifest;
    manifest.version = QStringLiteral("0.1.1-56");
    manifest.package.needs = {QStringLiteral("glibc"), QStringLiteral("qt6-base>=6.12")};
    return manifest;
}
} // namespace

class UpdatePackageCheckTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void aGoodPackagePasses()
    {
        const auto result = check(build({}));
        QVERIFY2(result.facts, qPrintable(result.problem));
        QCOMPARE(result.facts->name, QStringLiteral("arch-dock"));
        QCOMPARE(result.facts->version, QStringLiteral("0.1.1-56"));
        QVERIFY(ArchDock::Updates::packageMismatch(*result.facts, manifest()).isEmpty());
    }

    void aSymlinkStoredWithMode0777Passes()
    {
        Package package;
        package.tweak = [](QList<Entry> &files)
        { files.append({QStringLiteral("usr/bin/archdock"), {}, AE_IFLNK, 0777, 0, 0, QStringLiteral("arch-dock")}); };
        const auto result = check(build(package));
        QVERIFY2(result.facts, qPrintable(result.problem));
    }

    // Each rule with the reason it must be refused for, so no row passes
    // because some other rule refused it first.
    void theContentRulesRefuse_data()
    {
        QTest::addColumn<QString>("why");
        const QList<std::pair<const char *, const char *>> rules = {
            {"install script", "has an install script (.INSTALL)"},
            {"setuid", "is setuid or setgid"},
            {"device", "is a device, fifo or socket"},
            {"outside usr", "lies outside usr/"},
            {"link leaving usr", "points outside usr/"},
            {"absolute link leaving usr", "points outside usr/"},
            {"pkgname twice", ".PKGINFO names pkgname 2 times"},
            {"pkgbase twice", ".PKGINFO names pkgbase 2 times"},
            {"pkgver twice", ".PKGINFO names pkgver 2 times"},
            {"arch twice", ".PKGINFO names arch 2 times"},
            {"no pkgver", ".PKGINFO names pkgver 0 times"},
            {"mtree lists a missing path", ".MTREE does not list exactly"},
            {"mtree misses a path", ".MTREE does not list exactly"},
            {"uid 1000", "is not owned by root"},
            {"gid 1000", "is not owned by root"},
            {"group-writable file", "can be written by group or others"},
            {"other-writable folder", "can be written by group or others"},
            {"capability xattr", "carries xattrs or ACLs"},
            {"acl", "carries xattrs or ACLs"},
            {"gzip", "is not compressed with zstd"},
            {"xz", "is not compressed with zstd"},
            {"plain tar", "is not compressed with zstd"},
            {"zip", "is not compressed with zstd"},
            {"zip with zstd", "is not a tar archive"},
            {"cpio with zstd", "is not a tar archive"},
            {"absolute path", "holds the path"},
            {"dot-slash prefix", "holds the path"},
            {"dot-dot part", "holds the path"},
            {"hard link", "is a hard link"},
            {"path twice", "lists usr/share/arch-dock/notes.txt twice"},
            {"folder twice with and without its slash", "lists usr/share twice"},
            {"duplicate pkginfo", "lists .PKGINFO twice"},
        };
        for (const auto &[name, why] : rules)
            QTest::newRow(name) << QString::fromLatin1(why);
    }

    void theContentRulesRefuse()
    {
        QFETCH(QString, why);
        const QString rule = QString::fromLatin1(QTest::currentDataTag());
        Package package;
        const auto addFile = [&package](Entry entry)
        { package.tweak = [entry](QList<Entry> &files) { files.append(entry); }; };
        if (rule == QLatin1String("install script"))
            addFile({QStringLiteral(".INSTALL"), QByteArrayLiteral("post_install() { :; }\n")});
        else if (rule == QLatin1String("setuid"))
            addFile({QStringLiteral("usr/bin/setuid"), QByteArrayLiteral("x"), AE_IFREG, 04755});
        else if (rule == QLatin1String("device"))
            addFile({QStringLiteral("usr/share/arch-dock/fifo"), {}, AE_IFIFO, 0644});
        else if (rule == QLatin1String("outside usr"))
            addFile({QStringLiteral("etc/arch-dock.conf"), QByteArrayLiteral("x")});
        else if (rule == QLatin1String("link leaving usr"))
            addFile({QStringLiteral("usr/bin/away"), {}, AE_IFLNK, 0777, 0, 0, QStringLiteral("../../etc/shadow")});
        else if (rule == QLatin1String("absolute link leaving usr"))
            addFile({QStringLiteral("usr/bin/away"), {}, AE_IFLNK, 0777, 0, 0, QStringLiteral("/etc/shadow")});
        else if (rule.endsWith(QLatin1String(" twice")) && !rule.startsWith(QLatin1String("path"))
                 && !rule.startsWith(QLatin1String("folder")))
            package.pkginfo += rule.section(u' ', 0, 0) + QStringLiteral(" = again\n");
        else if (rule == QLatin1String("no pkgver"))
            package.pkginfo.remove(QStringLiteral("pkgver = 0.1.1-56\n"));
        else if (rule == QLatin1String("mtree lists a missing path"))
            package.mtreeExtra.append(QStringLiteral("usr/share/arch-dock/missing.txt"));
        else if (rule == QLatin1String("mtree misses a path"))
            package.mtreeSkip.insert(QStringLiteral("usr/share/arch-dock/notes.txt"));
        else if (rule == QLatin1String("uid 1000"))
            addFile({QStringLiteral("usr/share/arch-dock/theirs"), QByteArrayLiteral("x"), AE_IFREG, 0644, 1000, 0});
        else if (rule == QLatin1String("gid 1000"))
            addFile({QStringLiteral("usr/share/arch-dock/theirs"), QByteArrayLiteral("x"), AE_IFREG, 0644, 0, 1000});
        else if (rule == QLatin1String("group-writable file"))
            addFile({QStringLiteral("usr/share/arch-dock/open"), QByteArrayLiteral("x"), AE_IFREG, 0664});
        else if (rule == QLatin1String("other-writable folder"))
            addFile({QStringLiteral("usr/share/arch-dock/open"), {}, AE_IFDIR, 0757});
        else if (rule == QLatin1String("capability xattr"))
        {
            Entry entry{QStringLiteral("usr/bin/capable"), QByteArrayLiteral("x"), AE_IFREG, 0755};
            entry.xattr = QByteArray("\x01\x00\x00\x02", 4);
            addFile(entry);
        }
        else if (rule == QLatin1String("acl"))
        {
            Entry entry{QStringLiteral("usr/share/arch-dock/shared"), QByteArrayLiteral("x")};
            entry.acl = true;
            addFile(entry);
        }
        else if (rule == QLatin1String("gzip"))
            package.filter = ARCHIVE_FILTER_GZIP;
        else if (rule == QLatin1String("xz"))
            package.filter = ARCHIVE_FILTER_XZ;
        else if (rule == QLatin1String("plain tar"))
            package.filter = ARCHIVE_FILTER_NONE;
        else if (rule == QLatin1String("zip"))
        {
            package.format = Package::Format::Zip;
            package.filter = ARCHIVE_FILTER_NONE;
        }
        else if (rule == QLatin1String("zip with zstd"))
            package.format = Package::Format::Zip;
        else if (rule == QLatin1String("cpio with zstd"))
            package.format = Package::Format::Cpio;
        else if (rule == QLatin1String("absolute path"))
            addFile({QStringLiteral("/usr/share/arch-dock/absolute"), QByteArrayLiteral("x")});
        else if (rule == QLatin1String("dot-slash prefix"))
            addFile({QStringLiteral("./usr/share/arch-dock/dotted"), QByteArrayLiteral("x")});
        else if (rule == QLatin1String("dot-dot part"))
            addFile({QStringLiteral("usr/share/../share/arch-dock/up"), QByteArrayLiteral("x")});
        else if (rule == QLatin1String("hard link"))
            addFile({QStringLiteral("usr/bin/again"), {}, AE_IFREG, 0755, 0, 0, {}, QStringLiteral("usr/bin/arch-dock")});
        else if (rule == QLatin1String("path twice"))
            addFile({QStringLiteral("usr/share/arch-dock/notes.txt"), QByteArrayLiteral("again\n")});
        else if (rule == QLatin1String("folder twice with and without its slash"))
            addFile({QStringLiteral("usr/share/"), {}, AE_IFDIR, 0755});
        else if (rule == QLatin1String("duplicate pkginfo"))
            addFile({QStringLiteral(".PKGINFO"), QByteArrayLiteral("pkgname = arch-dock\n")});
        const auto result = check(build(package));
        QVERIFY2(!result.facts, qPrintable(rule + QStringLiteral(" was accepted")));
        QVERIFY2(result.problem.contains(why), qPrintable(result.problem));
    }

    void aPackageOtherThanTheManifestIsRefused()
    {
        const auto result = check(build({}));
        QVERIFY(result.facts);
        const auto mismatch = [&](std::function<void(ArchDock::Updates::PackageFacts &, ArchDock::Updates::Manifest &)> change)
        {
            ArchDock::Updates::PackageFacts facts = *result.facts;
            ArchDock::Updates::Manifest wanted = manifest();
            change(facts, wanted);
            return !ArchDock::Updates::packageMismatch(facts, wanted).isEmpty();
        };
        QVERIFY(mismatch([](auto &facts, auto &) { facts.name = QStringLiteral("arch-dock-git"); }));
        QVERIFY(mismatch([](auto &facts, auto &) { facts.version = QStringLiteral("0.1.1-57"); }));
        QVERIFY(mismatch([](auto &facts, auto &) { facts.arch = QStringLiteral("aarch64"); }));
        QVERIFY(mismatch([](auto &, auto &wanted) { wanted.package.needs.append(QStringLiteral("polkit")); }));
        QVERIFY(mismatch([](auto &, auto &wanted) { wanted.package.needs = {QStringLiteral("glibc")}; }));
        QVERIFY(!mismatch([](auto &, auto &) {}));
    }
};

QTEST_GUILESS_MAIN(UpdatePackageCheckTest)
#include "UpdatePackageCheckTest.moc"
