#!/usr/bin/env bash
# Whether a built package may be published as an Arch Dock update. It checks
# the package's Qt bounds against the Qt it was built with, its dependencies,
# its entries and modes (the root helper's own rules, from
# tools/release-files.py), the Update button's parts and the password
# window's action, that no program or library imports Qt's private interface
# or QML's ahead-of-time helpers, and its root helper: a release build that
# trusts exactly the key data/update-keys.txt lists. With --manifest and
# --signature the packaged helper must also accept the signed release.
#
#   check-package-release.sh <package> [--manifest <file> --signature <file>]
set -euo pipefail
project_root="$(cd -- "$(dirname -- "$0")/.." && pwd)"
exec /usr/bin/python3 -I -B - "$project_root" "$@" <<'PY'
import argparse
import importlib.util
import re
import subprocess
import sys
import tarfile
import tempfile
import xml.etree.ElementTree as ElementTree
from pathlib import Path

root = Path(sys.argv[1])
parser = argparse.ArgumentParser(prog="check-package-release.sh")
parser.add_argument("package")
parser.add_argument("--manifest")
parser.add_argument("--signature")
args = parser.parse_args(sys.argv[2:])
if bool(args.manifest) != bool(args.signature):
    parser.error("--manifest and --signature go together")

spec = importlib.util.spec_from_file_location("release_files", root / "tools/release-files.py")
release = importlib.util.module_from_spec(spec)
spec.loader.exec_module(release)

HELPER = release.HELPER_PATH
POLICY = "usr/share/polkit-1/actions/org.archdock.ArchDock.policy"
# The Update button's parts and their modes.
PARTS = {
    HELPER: 0o755,
    "usr/lib/arch-dock/arch-dock-finish-update": 0o755,
    "usr/share/libalpm/hooks/arch-dock-qt.hook": 0o644,
    "usr/share/libalpm/scripts/arch-dock-check-qt": 0o755,
    "usr/share/knotifications6/arch-dock.notifyrc": 0o644,
    POLICY: 0o644,
}
PRIVATE_IMPORT = re.compile(r"[^ \n]+@Qt_6_PRIVATE_API")
AOT_IMPORT = re.compile(r"_Z(T[VIS])?NK?11QQmlPrivate[0-9]+AOT[^ \n@]*")

package = Path(args.package).resolve()
problems = []


def check(what, test):
    try:
        test()
    except KeyError as missing:
        problems.append(what)
        print(f"FAIL  {what}: the package has no {missing.args[0]}")
        return
    except (release.ReleaseError, AssertionError, OSError, subprocess.SubprocessError,
            ElementTree.ParseError, tarfile.TarError, ValueError) as error:
        problems.append(what)
        print(f"FAIL  {what}: {error}")
        return
    print(f"PASS  {what}")


with tempfile.TemporaryDirectory(prefix="archdock-package-check-") as folder:
    work = Path(folder)
    named = {}
    elves = []
    names = []

    def read_archive():
        # One pass: the files the checks read, and every ELF file to ask readelf.
        with tarfile.open(package, mode="r:zst") as archive:
            for member in archive:
                name = member.name[2:] if member.name.startswith("./") else member.name
                names.append(name.rstrip("/"))
                if not member.isfile():
                    continue
                data = archive.extractfile(member).read()
                if name in PARTS or name in (".PKGINFO", ".BUILDINFO"):
                    named[name] = (data, member.mode & 0o7777)
                if data[:4] == b"\x7fELF":
                    target = work / "elf" / name
                    target.parent.mkdir(parents=True, exist_ok=True)
                    target.write_bytes(data)
                    elves.append((name, target))

    check("the package is a tar archive compressed with zstd", read_archive)
    check("entries, owners, modes, xattrs, .PKGINFO lines and .MTREE (the helper's rules)",
          lambda: release.read_package(package))

    def no_install_script():
        assert ".INSTALL" not in names, "the package has an install script (.INSTALL)"
    check("no install script", no_install_script)

    def qt_bounds():
        info = release._pkginfo(named[".PKGINFO"][0].decode("utf-8"))
        build = release._pkginfo(named[".BUILDINFO"][0].decode("utf-8"))
        installed = {}
        for line in build.get("installed", []):
            match = re.fullmatch(r"(.+)-([^-]+)-([^-]+)-([^-]+)", line)
            if match:
                installed[match.group(1)] = match.group(2)
        for name in ("qt6-base", "qt6-declarative"):
            assert name in installed, f".BUILDINFO names no installed {name}"
            minor = re.match(r"(\d+)\.(\d+)", installed[name].split(":")[-1])
            assert minor, f"{name} {installed[name]} has no major.minor"
            wanted = f"{name}>={minor.group(1)}.{minor.group(2)}"
            bounds = [need for need in info.get("depend", []) if re.split(r"[<>=]", need)[0] == name]
            assert bounds == [wanted], (f"it depends on {', '.join(bounds) or 'no ' + name}, not {wanted}: "
                                        f"{name} {installed[name]} was installed at the build")
    check("Qt bounds equal the Qt it was built with", qt_bounds)

    def dependencies():
        info = release._pkginfo(named[".PKGINFO"][0].decode("utf-8"))
        names_needed = {re.split(r"[<>=]", need)[0] for need in info.get("depend", [])}
        missing = [name for name in ("polkit", "knotifications", "libarchive", "openssl") if name not in names_needed]
        assert not missing, f"it does not depend on {', '.join(missing)}"
    check("it depends on polkit, knotifications, libarchive and openssl", dependencies)

    def parts():
        for name, mode in PARTS.items():
            assert name in named, f"the package has no {name}"
            assert named[name][1] == mode, f"{name} has mode {named[name][1]:o}, not {mode:o}"
    check("the Update button's parts are there with their modes", parts)

    def password_window_action():
        policy = ElementTree.fromstring(named[POLICY][0])
        actions = policy.findall("action")
        assert [action.get("id") for action in actions] == ["org.archdock.ArchDock.install-update"], \
            "the policy holds other actions than org.archdock.ArchDock.install-update"
        exec_path = [note.text for note in actions[0].findall("annotate")
                     if note.get("key") == "org.freedesktop.policykit.exec.path"]
        assert exec_path == ["/" + HELPER], f"exec.path is {exec_path}, not /{HELPER}"
        defaults = actions[0].find("defaults")
        answers = [defaults.findtext(key) for key in ("allow_any", "allow_inactive", "allow_active")]
        assert answers == ["no", "no", "auth_admin"], f"the action's defaults are {answers}"
    check("the password window's action names the helper", password_window_action)

    def no_private_imports():
        assert elves, "the package holds no programs"
        found = []
        for name, path in elves:
            symbols = subprocess.run(("/usr/bin/readelf", "--dyn-syms", "--wide", str(path)), capture_output=True,
                                     text=True, check=True, timeout=120).stdout
            private = PRIVATE_IMPORT.findall(symbols)
            aot = [match.group(0) for match in AOT_IMPORT.finditer(symbols)]
            if private or aot:
                found.append(f"{name}: {len(aot)} ahead-of-time QML and {len(private)} Qt private imports")
        assert not found, "; ".join(found)
    check("no program or library imports Qt's private interface or QML's ahead-of-time helpers", no_private_imports)

    helper = work / "arch-dock-install-update"

    def release_helper():
        helper.write_bytes(named[HELPER][0])
        helper.chmod(0o700)
        answer = subprocess.run((str(helper), "--about"), capture_output=True, text=True, timeout=60,
                                env=release.helper_environment(), stdin=subprocess.DEVNULL)
        assert answer.returncode == 0, f"--about failed: {answer.stderr.strip()}"
        lines = answer.stdout.splitlines()
        builds = [line[len("build: "):] for line in lines if line.startswith("build: ")]
        assert builds == ["release"], f"the helper is a {builds} build, not a release build"
        keys = release.key_list(None)
        assert len(keys) == 1, "data/update-keys.txt lists no key"
        trusted = [line[len("key: "):] for line in lines if line.startswith("key: ")]
        wanted = f"{keys[0][0]} {release.fingerprint(keys[0][1])}"
        assert trusted == [wanted], f"the helper trusts {trusted}, not exactly {wanted}"
    check("its helper is a release build that trusts exactly the listed key", release_helper)

    if args.manifest:
        def accepts_the_release():
            assert helper.exists(), "the helper could not be asked"
            runtime = work / "runtime"
            runtime.mkdir(mode=0o700)
            accepted, why = release.helper_accepts(helper, Path(args.manifest).read_bytes(),
                                                   Path(args.signature).read_bytes(), package, runtime)
            assert accepted, f"--check-only refuses it: {why}"
        check("the packaged helper accepts the signed release", accepts_the_release)

if problems:
    print(f"{package.name} may not be published as an update: {len(problems)} checks failed.")
    sys.exit(1)
print(f"{package.name} may be published as an update.")
PY
