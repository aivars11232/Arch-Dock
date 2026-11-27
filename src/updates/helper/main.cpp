// SPDX-License-Identifier: GPL-3.0-or-later
// arch-dock-install-update: the one part of Arch Dock that runs as root. The
// password window (pkexec, through the org.archdock.ArchDock.install-update
// action) starts it; it installs an Arch Dock update only when its signed
// manifest verifies with the key this Arch Dock lists, and the package is
// exactly the file that manifest describes.
//
// Its standard input: line 1 the manifest in base64, line 2 the signature in
// base64, then the package bytes up to the end. It takes no path and no other
// argument, and nothing from the caller's environment.
//
//   arch-dock-install-update               install (as root, through pkexec)
//   arch-dock-install-update --check-only  every check, as the user; installs nothing
//   arch-dock-install-update --about       which source, build and key it trusts
//
// The copy pkexec starts moves the install into a system scope of its own, so
// a logout cannot end pacman halfway; the copy inside that scope does the work.
// The exit codes are a contract with every installed Arch Dock (see Exit).

#include "ArchDockVersion.h"
#include "updates/PackageCheck.h"
#include "updates/UpdateCore.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QString>
#include <QStringList>

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <fcntl.h>
#include <optional>
#include <poll.h>
#include <spawn.h>
#include <sys/file.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace
{
using ArchDock::Updates::CommandRunner;

// The frozen exit codes: every installed Arch Dock has a line on Panel
// Studio's Updates page for each of them, so their meaning never changes.
enum Exit : int
{
    Installed = 0,
    NotThroughPasswordWindow = 2,
    AnotherInstall = 10,
    SystemPartsMissing = 11,
    FailsChecks = 12,
    NotNewer = 13,
    PacmanFailed = 14,
    OtherVersionAfterwards = 15,
    BadSignature = 16,
    NoDiskSpace = 17,
    LeftoverLock = 18,
    NoScopeOrFolder = 19,
    InstallByHand = 20,
    NoInstalledVersion = 21,
    InnerEndedUnexpectedly = 22,
};

bool isTableCode(int code)
{
    return code == 0 || code == 2 || (code >= 10 && code <= 22);
}

constexpr qint64 MaxManifestBase64 = 88 * 1024;
constexpr qint64 MaxSignatureBase64 = 2 * 1024;
constexpr qint64 ExtraRoomBytes = 16 * 1024 * 1024;

// Where everything lives. A real build uses only the fixed paths; the test
// build, never installed, takes a test root from ARCHDOCK_HELPER_TEST_ROOT.
struct Paths
{
    QString tools = QStringLiteral("/usr/bin");
    QString run = QStringLiteral("/run");
    QString proc = QStringLiteral("/proc");
    QString etc = QStringLiteral("/etc");
    QString pacmanLock = QStringLiteral("/var/lib/pacman/db.lck");
    QString self = ArchDock::Version::helperPath();
    QString keyId = ArchDock::Version::keyId();
    QByteArray keySpki = ArchDock::Version::keySpki();
    bool testBuild = ArchDock::Version::isTestBuild();
};

Paths paths;
// The test build's root, passed on to its inner copy (never in a real build).
QByteArray testRoot;

void log(const QString &line)
{
    const QByteArray bytes = (QStringLiteral("arch-dock-install-update: ") + line + QLatin1Char('\n')).toUtf8();
    [[maybe_unused]] const ssize_t written = ::write(STDERR_FILENO, bytes.constData(), static_cast<size_t>(bytes.size()));
}

[[noreturn]] void finish(int code, const QString &why = {})
{
    if (!why.isEmpty())
        log(why);
    std::_Exit(code);
}

// Every child starts with default signal handling, an empty signal mask, the
// fixed environment and /dev/null as input, unless it is given an input.
class SpawnRunner final : public CommandRunner
{
public:
    Result run(const QString &program, const QStringList &arguments, const QByteArray &input = {}) override
    {
        Result result;
        int out[2], err[2], in[2] = {-1, -1};
        if (::pipe2(out, O_CLOEXEC) != 0 || ::pipe2(err, O_CLOEXEC) != 0)
            return result;
        if (!input.isEmpty() && ::pipe2(in, O_CLOEXEC) != 0)
            return result;
        posix_spawn_file_actions_t actions;
        posix_spawn_file_actions_init(&actions);
        if (input.isEmpty())
            posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
        else
            posix_spawn_file_actions_adddup2(&actions, in[0], STDIN_FILENO);
        posix_spawn_file_actions_adddup2(&actions, out[1], STDOUT_FILENO);
        posix_spawn_file_actions_adddup2(&actions, err[1], STDERR_FILENO);
        const pid_t child = spawn(program, arguments, &actions);
        posix_spawn_file_actions_destroy(&actions);
        ::close(out[1]);
        ::close(err[1]);
        if (in[0] >= 0)
            ::close(in[0]);
        if (child < 0)
        {
            ::close(out[0]);
            ::close(err[0]);
            if (in[1] >= 0)
                ::close(in[1]);
            return result;
        }
        result.started = true;
        qsizetype sent = 0;
        if (in[1] >= 0)
            ::fcntl(in[1], F_SETFL, O_NONBLOCK);
        bool outOpen = true, errOpen = true;
        while (outOpen || errOpen || in[1] >= 0)
        {
            pollfd fds[3] = {{out[0], POLLIN, 0}, {err[0], POLLIN, 0}, {in[1], POLLOUT, 0}};
            if (::poll(fds, 3, -1) < 0 && errno != EINTR)
                break;
            char buffer[65536];
            for (int which = 0; which < 2; ++which)
            {
                bool &open = which == 0 ? outOpen : errOpen;
                if (!open || !(fds[which].revents & (POLLIN | POLLHUP | POLLERR)))
                    continue;
                const ssize_t got = ::read(fds[which].fd, buffer, sizeof buffer);
                if (got > 0)
                    (which == 0 ? result.output : result.errors).append(buffer, got);
                else if (got == 0 || errno != EINTR)
                    open = false;
            }
            if (in[1] >= 0 && (fds[2].revents & (POLLOUT | POLLERR | POLLHUP)))
            {
                const ssize_t wrote = ::write(in[1], input.constData() + sent, static_cast<size_t>(input.size() - sent));
                if (wrote > 0)
                    sent += wrote;
                if (wrote < 0 && errno != EAGAIN && errno != EINTR)
                    sent = input.size();
                if (sent >= input.size())
                {
                    ::close(in[1]);
                    in[1] = -1;
                }
            }
        }
        ::close(out[0]);
        ::close(err[0]);
        if (in[1] >= 0)
            ::close(in[1]);
        result.exitCode = waitFor(child);
        return result;
    }

    // Starts `program`; -1 when it could not start.
    static pid_t spawn(const QString &program, const QStringList &arguments, const posix_spawn_file_actions_t *actions,
                       const QList<QByteArray> &extraEnvironment = {})
    {
        posix_spawnattr_t attributes;
        posix_spawnattr_init(&attributes);
        sigset_t all, none;
        sigfillset(&all);
        sigemptyset(&none);
        posix_spawnattr_setsigdefault(&attributes, &all);
        posix_spawnattr_setsigmask(&attributes, &none);
        posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETSIGDEF | POSIX_SPAWN_SETSIGMASK);
        QList<QByteArray> argumentBytes{program.toLocal8Bit()};
        for (const QString &argument : arguments)
            argumentBytes.append(argument.toLocal8Bit());
        std::vector<char *> argv;
        for (QByteArray &argument : argumentBytes)
            argv.push_back(argument.data());
        argv.push_back(nullptr);
        QList<QByteArray> environmentBytes{QByteArrayLiteral("PATH=/usr/bin"), QByteArrayLiteral("LC_ALL=C")};
        for (const QByteArray &entry : extraEnvironment)
            if (!entry.isEmpty())
                environmentBytes.append(entry);
        std::vector<char *> envp;
        for (QByteArray &entry : environmentBytes)
            envp.push_back(entry.data());
        envp.push_back(nullptr);
        pid_t child = -1;
        const QByteArray path = program.toLocal8Bit();
        const int status = ::posix_spawn(&child, path.constData(), actions, &attributes, argv.data(), envp.data());
        posix_spawnattr_destroy(&attributes);
        return status == 0 ? child : -1;
    }

    // The child's exit code, or -1 - signal for a child a signal ended.
    static int waitFor(pid_t child)
    {
        int status = 0;
        while (::waitpid(child, &status, 0) < 0)
            if (errno != EINTR)
                return -1;
        if (WIFEXITED(status))
            return WEXITSTATUS(status);
        return WIFSIGNALED(status) ? -1 - WTERMSIG(status) : -1;
    }
};

SpawnRunner runner;

QString tool(const char *name)
{
    return paths.tools + QLatin1Char('/') + QString::fromLatin1(name);
}

// Before anything else: nothing the caller set may reach the install.
void firstThings()
{
    // pkexec asks the kernel to end it when its parent ends; the install must
    // not depend on pkexec's own group change clearing that again.
    ::prctl(PR_SET_PDEATHSIG, 0);
    // A shutdown stops the scope with SIGINT, which pacman answers, not the helper.
    std::signal(SIGPIPE, SIG_IGN);
    std::signal(SIGHUP, SIG_IGN);
    std::signal(SIGINT, SIG_IGN);
    sigset_t none;
    sigemptyset(&none);
    ::sigprocmask(SIG_SETMASK, &none, nullptr);
    // Limits a program in the account could have lowered (prlimit) would
    // otherwise end pacman halfway with SIGXFSZ or SIGXCPU.
    for (const int resource : {RLIMIT_FSIZE, RLIMIT_CPU, RLIMIT_AS, RLIMIT_DATA, RLIMIT_STACK, RLIMIT_CORE})
    {
        rlimit unlimited{RLIM_INFINITY, RLIM_INFINITY};
        if (::setrlimit(resource, &unlimited) != 0)
        {
            rlimit current{};
            ::getrlimit(resource, &current);
            current.rlim_cur = current.rlim_max;
            ::setrlimit(resource, &current);
        }
    }
    rlimit files{1024, 524288};
    if (::setrlimit(RLIMIT_NOFILE, &files) != 0)
    {
        rlimit current{};
        ::getrlimit(RLIMIT_NOFILE, &current);
        current.rlim_cur = std::min<rlim_t>(1024, current.rlim_max);
        ::setrlimit(RLIMIT_NOFILE, &current);
    }
    ::setpriority(PRIO_PROCESS, 0, 0);
    QFile oom(paths.proc + QStringLiteral("/self/oom_score_adj"));
    if (oom.open(QIODevice::WriteOnly))
        oom.write("0\n");
    oom.close();
    ::closefrom(3);
    ::umask(022);
}

// Replaces the environment with PATH, LC_ALL and PKEXEC_UID alone.
void cleanEnvironment()
{
    const QByteArray pkexecUid = qgetenv("PKEXEC_UID");
    const bool hadUid = qEnvironmentVariableIsSet("PKEXEC_UID");
    ::clearenv();
    ::setenv("PATH", "/usr/bin", 1);
    ::setenv("LC_ALL", "C", 1);
    if (hadUid)
        ::setenv("PKEXEC_UID", pkexecUid.constData(), 1);
}

void say(const QByteArray &line)
{
    [[maybe_unused]] const ssize_t written = ::write(STDOUT_FILENO, line.constData(), static_cast<size_t>(line.size()));
}

// The outer copy starts the inner copy in a system scope of its own and
// passes its exit code on.
[[noreturn]] void runInScope()
{
    const QString unit = QStringLiteral("arch-dock-install-update-%1").arg(::getpid());
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    const pid_t child = SpawnRunner::spawn(
        tool("systemd-run"),
        {QStringLiteral("--system"), QStringLiteral("--scope"), QStringLiteral("--quiet"), QStringLiteral("--collect"),
         QStringLiteral("--unit=") + unit, QStringLiteral("--description=Arch Dock update"),
         QStringLiteral("-p"), QStringLiteral("KillSignal=SIGINT"), QStringLiteral("--"), paths.self},
        &actions, {QByteArrayLiteral("ARCHDOCK_UPDATE_INNER=1"), QByteArray("PKEXEC_UID=") + qgetenv("PKEXEC_UID"),
                   testRoot.isEmpty() ? QByteArray() : QByteArray("ARCHDOCK_HELPER_TEST_ROOT=") + testRoot});
    posix_spawn_file_actions_destroy(&actions);
    if (child < 0)
        finish(NoScopeOrFolder, QStringLiteral("systemd-run could not be started"));
    const int code = SpawnRunner::waitFor(child);
    if (code <= -2)
        finish(InnerEndedUnexpectedly, QStringLiteral("the installer in its scope ended by a signal"));
    if (code >= 0 && isTableCode(code))
        finish(code);
    finish(NoScopeOrFolder, QStringLiteral("systemd-run failed (%1) before the installer started").arg(code));
}

bool inOwnScope()
{
    QFile cgroup(paths.proc + QStringLiteral("/self/cgroup"));
    if (!cgroup.open(QIODevice::ReadOnly))
        return false;
    static const QRegularExpression scope(QRegularExpression::anchoredPattern(
        QStringLiteral(R"(0::/system\.slice/arch-dock-install-update-[0-9]+\.scope\n)")));
    return scope.match(QString::fromLatin1(cgroup.read(4096))).hasMatch();
}

// Whether any process has pacman's lock file open, found by device and inode.
// statx with AT_STATX_DONT_SYNC takes what the kernel already knows, so a
// descriptor on a network mount whose server has gone cannot hang the scan.
bool lockHeld(const struct stat &lock)
{
    DIR *processes = ::opendir(QFile::encodeName(paths.proc).constData());
    if (!processes)
        return true;
    bool held = false;
    while (dirent *process = ::readdir(processes))
    {
        if (!std::all_of(process->d_name, process->d_name + std::strlen(process->d_name), ::isdigit))
            continue;
        const QByteArray fdFolder = QFile::encodeName(paths.proc) + '/' + process->d_name + "/fd";
        DIR *descriptors = ::opendir(fdFolder.constData());
        if (!descriptors)
            continue;
        while (dirent *descriptor = ::readdir(descriptors))
        {
            struct statx open{};
            if (descriptor->d_name[0] != '.'
                && ::statx(AT_FDCWD, (fdFolder + '/' + descriptor->d_name).constData(), AT_STATX_DONT_SYNC | AT_NO_AUTOMOUNT,
                           STATX_INO, &open) == 0
                && makedev(open.stx_dev_major, open.stx_dev_minor) == lock.st_dev && open.stx_ino == lock.st_ino)
                held = true;
        }
        ::closedir(descriptors);
        if (held)
            break;
    }
    ::closedir(processes);
    return held;
}

// 0 when pacman's lock is free, else 10 (held) or 18 (left behind).
// As a user only its presence can be seen.
int lockState(bool asUser)
{
    struct stat lock{};
    if (::stat(QFile::encodeName(paths.pacmanLock).constData(), &lock) != 0)
        return 0;
    if (asUser)
        return AnotherInstall;
    return lockHeld(lock) ? AnotherInstall : LeftoverLock;
}

// The work folder, removed on every exit.
class WorkFolder
{
public:
    explicit WorkFolder(const QString &parent)
    {
        QByteArray name = QFile::encodeName(parent + QStringLiteral("/work.XXXXXX"));
        if (::mkdtemp(name.data()))
            m_path = QFile::decodeName(name);
    }
    ~WorkFolder()
    {
        if (!m_path.isEmpty())
            QDir(m_path).removeRecursively();
    }
    QString path() const { return m_path; }
    QString file(const QString &name) const { return m_path + QLatin1Char('/') + name; }

private:
    QString m_path;
};

WorkFolder *work = nullptr;

[[noreturn]] void done(int code, const QString &why = {})
{
    delete work;
    work = nullptr;
    finish(code, why);
}

// The helper's own folder, root's alone.
QString ownFolder()
{
    const QString folder = paths.run + QStringLiteral("/arch-dock-update");
    const QByteArray name = QFile::encodeName(folder);
    ::mkdir(name.constData(), 0700);
    struct stat info{};
    if (::lstat(name.constData(), &info) != 0 || !S_ISDIR(info.st_mode) || info.st_uid != 0
        || (info.st_mode & 07777) != 0700)
        finish(NoScopeOrFolder, QStringLiteral("%1 is not a folder of root's alone").arg(folder));
    return folder;
}

void removeLeftoverWork(const QString &folder)
{
    static const QRegularExpression leftover(QRegularExpression::anchoredPattern(QStringLiteral("work\\.[A-Za-z0-9]{6}")));
    DIR *entries = ::opendir(QFile::encodeName(folder).constData());
    if (!entries)
        return;
    while (dirent *entry = ::readdir(entries))
    {
        const QString name = QFile::decodeName(entry->d_name);
        if (!leftover.match(name).hasMatch())
            continue;
        const QString path = folder + QLatin1Char('/') + name;
        struct stat info{};
        if (::lstat(QFile::encodeName(path).constData(), &info) == 0 && S_ISDIR(info.st_mode))
            QDir(path).removeRecursively();
    }
    ::closedir(entries);
}

// Reads the input's two lines; the bytes after them start the package.
struct Input
{
    QByteArray manifest;
    QByteArray signature;
    QByteArray rest;
};

std::optional<Input> readInput()
{
    QByteArray buffer;
    QList<QByteArray> lines;
    char chunk[65536];
    while (lines.size() < 2)
    {
        const qsizetype newline = buffer.indexOf('\n');
        if (newline >= 0)
        {
            lines.append(buffer.left(newline));
            buffer.remove(0, newline + 1);
            continue;
        }
        if (buffer.size() > MaxManifestBase64 + 1)
            return std::nullopt;
        const ssize_t got = ::read(STDIN_FILENO, chunk, sizeof chunk);
        if (got < 0 && errno == EINTR)
            continue;
        if (got <= 0)
            return std::nullopt;
        buffer.append(chunk, got);
    }
    if (lines.at(0).size() > MaxManifestBase64 || lines.at(1).size() > MaxSignatureBase64)
        return std::nullopt;
    Input input;
    for (int index = 0; index < 2; ++index)
    {
        const auto decoded = QByteArray::fromBase64Encoding(lines.at(index), QByteArray::AbortOnBase64DecodingErrors);
        // Strict: only the canonical form, so one value has one spelling.
        if (!decoded || decoded.decoded.isEmpty() || decoded.decoded.toBase64() != lines.at(index))
            return std::nullopt;
        (index == 0 ? input.manifest : input.signature) = decoded.decoded;
    }
    input.rest = buffer;
    return input;
}

// pacman -Q arch-dock: the installed version, from exactly one line.
std::optional<QString> installedVersion()
{
    const auto answer = runner.run(tool("pacman"), {QStringLiteral("-Q"), QStringLiteral("arch-dock")});
    if (!answer.started || answer.exitCode != 0)
        return std::nullopt;
    const QList<QByteArray> lines = answer.output.trimmed().split('\n');
    if (lines.size() != 1)
        return std::nullopt;
    const QList<QByteArray> fields = lines.first().split(' ');
    if (fields.size() != 2 || fields.at(0) != "arch-dock")
        return std::nullopt;
    return QString::fromLatin1(fields.at(1));
}

struct Checked
{
    ArchDock::Updates::Manifest manifest;
    QString copy;
    QString installed;
};

// The checks against the system, once when the copy is ready and again right
// before pacman: pacman's lock, then the installed version as pacman shows it
// now (one installed in between is caught, so the update never goes back to
// an older version), the needs, and what pacman would do with the copy.
void checkBeforeInstall(const Checked &checked, bool asUser)
{
    if (const int lock = lockState(asUser); lock != 0)
        done(lock, lock == AnotherInstall ? QStringLiteral("another program is installing software")
                                          : QStringLiteral("an interrupted install left pacman's lock behind"));
    const std::optional<QString> installed = installedVersion();
    if (!installed)
        done(NoInstalledVersion, QStringLiteral("pacman gave no answer for the installed Arch Dock"));
    const auto newer = ArchDock::Updates::compareVersions(runner, paths.tools, checked.manifest.version, *installed);
    if (!newer || *newer <= 0)
        done(NotNewer, QStringLiteral("%1 is not newer than the installed %2").arg(checked.manifest.version, *installed));
    const auto minimum = ArchDock::Updates::compareVersions(runner, paths.tools, *installed,
                                                            checked.manifest.minimumSupportedVersion);
    if (!minimum || *minimum < 0)
        done(InstallByHand, QStringLiteral("the update needs Arch Dock %1 or newer first").arg(checked.manifest.minimumSupportedVersion));
    const QStringList &needs = checked.manifest.package.needs;
    for (const QString &need : needs)
        if (need.startsWith(u'-'))
            done(FailsChecks, QStringLiteral("a need starts with -"));
    if (!needs.isEmpty())
    {
        const auto test = runner.run(tool("pacman"), QStringList{QStringLiteral("-T"), QStringLiteral("--")} + needs);
        if (!test.started || test.exitCode != 0)
            done(SystemPartsMissing, QStringLiteral("missing or too old: %1").arg(QString::fromUtf8(test.output.trimmed())));
    }
    const auto printed = runner.run(tool("pacman"), {QStringLiteral("-U"), QStringLiteral("--print"),
                                                      QStringLiteral("--print-format"), QStringLiteral("%n %v %a"),
                                                      QStringLiteral("--"), checked.copy});
    const QList<QByteArray> lines = printed.output.trimmed().split('\n');
    if (!printed.started || printed.exitCode != 0)
        done(SystemPartsMissing, QStringLiteral("pacman cannot install the update alone: %1").arg(QString::fromUtf8(printed.errors.trimmed())));
    // pacman's own word for an install that is no update, on its errors.
    if (printed.errors.contains("downgrading") || printed.errors.contains("is up to date"))
        done(NotNewer, QStringLiteral("pacman reads the install as %1").arg(QString::fromUtf8(printed.errors.trimmed())));
    if (lines.size() != 1)
        done(SystemPartsMissing, QStringLiteral("the install would change other packages too"));
    if (QString::fromUtf8(lines.first()) != QStringLiteral("arch-dock %1 x86_64").arg(checked.manifest.version))
        done(FailsChecks, QStringLiteral("pacman reads the package as %1").arg(QString::fromUtf8(lines.first())));
    const auto depends = runner.run(tool("pacman"), {QStringLiteral("-U"), QStringLiteral("--print"),
                                                      QStringLiteral("--print-format"), QStringLiteral("%D"),
                                                      QStringLiteral("--"), checked.copy});
    const QStringList read = QString::fromUtf8(depends.output).split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    if (!depends.started || depends.exitCode != 0 || QSet<QString>(read.begin(), read.end()) != QSet<QString>(needs.begin(), needs.end())
        || read.size() != needs.size())
        done(FailsChecks, QStringLiteral("pacman reads other dependencies than the manifest's"));
}

// Every check: the input, the signature, the manifest, the copy and the
// package, ending with a private copy ready for pacman.
Checked checkUpdate(const QString &workParent, bool asUser)
{
    const std::optional<Input> input = readInput();
    if (!input)
        done(FailsChecks, QStringLiteral("the input is not two base64 lines and a package"));
    work = new WorkFolder(workParent);
    if (work->path().isEmpty())
        done(NoScopeOrFolder, QStringLiteral("no work folder could be made"));

    const auto reading = ArchDock::Updates::readVerifiedManifest(runner, paths.tools, paths.keyId, paths.keySpki,
                                                                 input->manifest, input->signature, paths.testBuild,
                                                                 work->path());
    if (!reading.manifest)
    {
        using Refusal = ArchDock::Updates::ManifestReading::Refusal;
        if (reading.refusal == Refusal::OtherFormat)
            done(InstallByHand, reading.problem);
        if (reading.refusal == Refusal::NotSigned || reading.refusal == Refusal::WrongKey || reading.refusal == Refusal::NoKey)
            done(BadSignature, reading.problem);
        if (reading.refusal == Refusal::CouldNotCheck)
            done(NoScopeOrFolder, reading.problem);
        done(FailsChecks, reading.problem);
    }
    Checked checked;
    checked.manifest = *reading.manifest;
    const std::optional<QString> installed = installedVersion();
    if (!installed)
        done(NoInstalledVersion, QStringLiteral("pacman gave no answer for the installed Arch Dock"));
    checked.installed = *installed;
    if (checked.manifest.package.requiredUpdaterVersion != ArchDock::Version::updaterProtocol())
        done(InstallByHand, QStringLiteral("the update needs a newer updater"));
    const auto minimum = ArchDock::Updates::compareVersions(runner, paths.tools, checked.installed,
                                                            checked.manifest.minimumSupportedVersion);
    if (!minimum || *minimum < 0)
        done(InstallByHand, QStringLiteral("the update needs Arch Dock %1 or newer first").arg(checked.manifest.minimumSupportedVersion));

    // The private copy, hashed while it is written and again as read back.
    const qint64 size = checked.manifest.package.size;
    struct statvfs room{};
    if (::statvfs(QFile::encodeName(work->path()).constData(), &room) != 0
        || static_cast<qint64>(room.f_bavail) * static_cast<qint64>(room.f_frsize) < size + ExtraRoomBytes)
        done(NoDiskSpace, QStringLiteral("not enough room for the update"));
    checked.copy = work->file(checked.manifest.package.file);
    const int fd = ::open(QFile::encodeName(checked.copy).constData(), O_RDWR | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (fd < 0)
        done(NoScopeOrFolder, QStringLiteral("the private copy could not be made"));
    QCryptographicHash hash(QCryptographicHash::Sha256);
    qint64 copied = 0;
    const auto store = [&](const char *data, qint64 length)
    {
        hash.addData(QByteArrayView(data, length));
        copied += length;
        qint64 written = 0;
        while (written < length)
        {
            const ssize_t step = ::write(fd, data + written, static_cast<size_t>(length - written));
            if (step < 0 && errno == EINTR)
                continue;
            if (step <= 0)
                done(errno == ENOSPC ? NoDiskSpace : FailsChecks, QStringLiteral("the private copy could not be written"));
            written += step;
        }
    };
    store(input->rest.constData(), input->rest.size());
    char chunk[65536];
    for (;;)
    {
        const ssize_t got = ::read(STDIN_FILENO, chunk, sizeof chunk);
        if (got < 0 && errno == EINTR)
            continue;
        if (got <= 0)
            break;
        if (copied + got > ArchDock::Updates::MaxPackageBytes)
            done(FailsChecks, QStringLiteral("more input than any package"));
        store(chunk, got);
    }
    if (copied != size || QString::fromLatin1(hash.result().toHex()) != checked.manifest.package.sha256)
        done(FailsChecks, QStringLiteral("the package is not the file the manifest describes"));
    QCryptographicHash again(QCryptographicHash::Sha256);
    ::lseek(fd, 0, SEEK_SET);
    for (;;)
    {
        const ssize_t got = ::read(fd, chunk, sizeof chunk);
        if (got <= 0)
            break;
        again.addData(QByteArrayView(chunk, got));
    }
    if (QString::fromLatin1(again.result().toHex()) != checked.manifest.package.sha256)
        done(FailsChecks, QStringLiteral("the private copy does not read back as written"));

    // The package's own content, as libalpm will read it.
    ::lseek(fd, 0, SEEK_SET);
    const auto package = ArchDock::Updates::checkPackage(fd);
    ::close(fd);
    if (!package.facts)
        done(FailsChecks, package.problem);
    if (const QString mismatch = ArchDock::Updates::packageMismatch(*package.facts, checked.manifest); !mismatch.isEmpty())
        done(FailsChecks, mismatch);

    checkBeforeInstall(checked, asUser);
    return checked;
}

// The install: pacman, its output into a root-only file; new lines are passed on
// without waiting, and lines no reader takes are dropped.
int install(const Checked &checked)
{
    checkBeforeInstall(checked, false);
    const QString logPath = work->file(QStringLiteral("pacman.log"));
    const int logFd = ::open(QFile::encodeName(logPath).constData(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    const int readFd = ::open(QFile::encodeName(logPath).constData(), O_RDONLY | O_CLOEXEC);
    if (logFd < 0 || readFd < 0)
        done(NoScopeOrFolder, QStringLiteral("pacman's log could not be made"));
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
    posix_spawn_file_actions_adddup2(&actions, logFd, STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&actions, logFd, STDERR_FILENO);
    const pid_t pacman = SpawnRunner::spawn(tool("pacman"),
        {QStringLiteral("-U"), QStringLiteral("--noconfirm"), QStringLiteral("--noprogressbar"), QStringLiteral("--color"),
         QStringLiteral("never"), QStringLiteral("--"), checked.copy}, &actions);
    posix_spawn_file_actions_destroy(&actions);
    ::close(logFd);
    if (pacman < 0)
        done(PacmanFailed, QStringLiteral("pacman could not be started"));
    ::fcntl(STDOUT_FILENO, F_SETFL, ::fcntl(STDOUT_FILENO, F_GETFL) | O_NONBLOCK);
    QByteArray all;
    int status = 0;
    char chunk[16384];
    for (;;)
    {
        const pid_t ended = ::waitpid(pacman, &status, WNOHANG);
        for (ssize_t got; (got = ::read(readFd, chunk, sizeof chunk)) > 0;)
        {
            all.append(chunk, got);
            [[maybe_unused]] const ssize_t passed = ::write(STDOUT_FILENO, chunk, static_cast<size_t>(got));
        }
        if (ended == pacman)
            break;
        if (ended < 0 && errno != EINTR)
            break;
        const timespec pause{0, 200 * 1000 * 1000};
        ::nanosleep(&pause, nullptr);
    }
    ::close(readFd);
    const bool exitedWell = WIFEXITED(status) && WEXITSTATUS(status) == 0;
    if (all.contains("downgrading"))
        return OtherVersionAfterwards;
    if (!exitedWell)
    {
        if (all.contains("unable to lock database"))
        {
            const int lock = lockState(false);
            return lock == LeftoverLock ? LeftoverLock : AnotherInstall;
        }
        if (all.contains("not enough free disk space"))
            return NoDiskSpace;
        return PacmanFailed;
    }
    // The installed version must now be the new one.
    const std::optional<QString> now = installedVersion();
    return now && *now == checked.manifest.version ? Installed : OtherVersionAfterwards;
}

void about()
{
    QByteArray text = "source: github.com/" + ArchDock::Version::repository().toUtf8() + " releases (pinned)\n";
    text += paths.testBuild ? "build: TEST: accepts a test key and a test server\n" : "build: release\n";
    if (paths.keyId.isEmpty() || paths.keySpki.isEmpty())
        text += "key: none: updates are not configured\n";
    else
        text += "key: " + paths.keyId.toUtf8() + ' '
            + QCryptographicHash::hash(paths.keySpki, QCryptographicHash::Sha256).toHex() + '\n';
    text += "protocol: " + QByteArray::number(ArchDock::Version::updaterProtocol()) + '\n';
    say(text);
}

#ifdef ARCHDOCK_HELPER_TEST_BUILD
// The test build's paths and key, from its test root (never installed).
void useTestRoot()
{
    const QString root = qEnvironmentVariable("ARCHDOCK_HELPER_TEST_ROOT");
    if (root.isEmpty())
        finish(NotThroughPasswordWindow, QStringLiteral("the test build needs ARCHDOCK_HELPER_TEST_ROOT"));
    testRoot = QFile::encodeName(root);
    paths.tools = root + QStringLiteral("/tools");
    paths.run = root + QStringLiteral("/run");
    paths.proc = root + QStringLiteral("/proc");
    paths.etc = root + QStringLiteral("/etc");
    paths.pacmanLock = root + QStringLiteral("/var/lib/pacman/db.lck");
    paths.self = QFile::symLinkTarget(QStringLiteral("/proc/self/exe"));
    paths.testBuild = true;
    QFile keys(root + QStringLiteral("/trusted-key.txt"));
    if (keys.open(QIODevice::ReadOnly))
    {
        const QList<QByteArray> fields = keys.readAll().trimmed().split(' ');
        if (fields.size() == 2)
        {
            paths.keyId = QString::fromLatin1(fields.at(0));
            paths.keySpki = QByteArray::fromHex(fields.at(1));
        }
    }
}
#endif
} // namespace

int main(int argc, char **argv)
{
#ifdef ARCHDOCK_HELPER_TEST_BUILD
    useTestRoot();
#endif
    const QString mode = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QString();
    if (argc > 2 || (argc == 2 && mode != QLatin1String("--check-only") && mode != QLatin1String("--about")))
        finish(NotThroughPasswordWindow, QStringLiteral("unknown arguments"));
    const bool inner = qEnvironmentVariableIsSet("ARCHDOCK_UPDATE_INNER");
    // --check-only works in the caller's own runtime folder, read before the
    // environment is replaced.
    const QString runtime = qEnvironmentVariable("XDG_RUNTIME_DIR");
    firstThings();
    cleanEnvironment();
    const bool asRoot = ::geteuid() == 0;
    const bool throughPkexec = qEnvironmentVariableIsSet("PKEXEC_UID");

    if (mode == QLatin1String("--about") || mode == QLatin1String("--check-only"))
    {
        // As the user only: as root the caller's runtime folder is gone.
        if (asRoot || throughPkexec)
            finish(NotThroughPasswordWindow, QStringLiteral("%1 runs as the user, not as root").arg(mode));
        if (mode == QLatin1String("--about"))
        {
            about();
            finish(0);
        }
        struct stat info{};
        if (runtime.isEmpty() || ::lstat(QFile::encodeName(runtime).constData(), &info) != 0 || !S_ISDIR(info.st_mode)
            || info.st_uid != ::getuid() || (info.st_mode & 07777) != 0700)
            finish(NotThroughPasswordWindow, QStringLiteral("--check-only needs a private runtime folder"));
        const Checked checked = checkUpdate(runtime, true);
        say(QStringLiteral("would install arch-dock %1\n").arg(checked.manifest.version).toUtf8());
        done(0);
    }

    // Only as root, started by the password window.
    bool numeric = false;
    qEnvironmentVariable("PKEXEC_UID").toUInt(&numeric);
    if (!asRoot || !numeric)
        finish(NotThroughPasswordWindow, QStringLiteral("not started through the password window"));
    if (paths.testBuild && !QFile::exists(paths.etc + QStringLiteral("/arch-dock-update-test-root")))
        finish(NotThroughPasswordWindow, QStringLiteral("a TEST helper installs only inside a test root"));
    if (!inner)
        say("started\n");

    // A system scope of its own, so a logout cannot end pacman halfway.
    if (QFileInfo::exists(paths.run + QStringLiteral("/systemd/system")))
    {
        if (!inner)
            runInScope();
        if (!inOwnScope())
            finish(NoScopeOrFolder, QStringLiteral("the installer is not in its own system scope"));
    }
    else
        log(QStringLiteral("systemd is not running the system; the install runs without its own scope"));

    // Its own folder and lock: one Arch Dock update at a time.
    const QString folder = ownFolder();
    const int lockFd = ::open(QFile::encodeName(folder + QStringLiteral("/lock")).constData(),
                              O_RDWR | O_CREAT | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (lockFd < 0 || ::flock(lockFd, LOCK_EX | LOCK_NB) != 0)
        finish(AnotherInstall, QStringLiteral("another Arch Dock update is running"));
    // A pacman that outlived a killed helper still holds db.lck, and re-opens
    // its copy at commit: leftovers go only while no process holds the lock.
    if (lockState(false) != AnotherInstall)
        removeLeftoverWork(folder);

    const Checked checked = checkUpdate(folder, false);
    done(install(checked));
}
