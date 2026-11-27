#!/usr/bin/env python3
"""tools/release-files.py: the update manifest's reading rules (the shared
vectors, which Arch Dock's C++ reader must judge the same way), the package
rules, the key's place, the release history, the three-way signature check
and what check-published refuses.

Releases are made from copies of the tool in synthetic Git repositories the
test makes, with test keys made by openssl in the test's own folder. GitHub
is a stand-in server on 127.0.0.1. Nothing here touches the project's
history, the network or a real key.
"""
import contextlib
import gzip
import hashlib
import http.server
import importlib.util
import io
import json
import os
from pathlib import Path
import random
import shutil
import subprocess
import tarfile
import tempfile
import threading
import unittest

PROJECT = Path(__file__).resolve().parents[1]
TOOL = PROJECT / "tools/release-files.py"
VECTORS = PROJECT / "tests/update-manifest-vectors"
OPENSSL = "/usr/bin/openssl"
GIT = "/usr/bin/git"
VERSION = "0.1.1-56.1"
TEST_BUILD = "TEST: accepts a test key and a test server"


def outside_git():
    """A temporary folder outside every git work tree: the tool refuses a key
    below one, and the suite's own temporary folder lies in the build folder,
    which is inside the repository."""
    for candidate in (tempfile.gettempdir(), "/tmp"):
        folder = Path(candidate).resolve()
        if not any((place / ".git").exists() for place in (folder, *folder.parents)):
            return str(folder)
    raise RuntimeError("no temporary folder outside a git work tree")


def scratch(prefix):
    return tempfile.TemporaryDirectory(prefix=prefix, dir=outside_git())


def load(path, name="release_files"):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


RF = load(TOOL)


def run(*command, cwd=None, env=None, check=True):
    return subprocess.run(command, cwd=cwd, env=env, check=check, capture_output=True, stdin=subprocess.DEVNULL)


def make_key(folder, name):
    key = Path(folder) / f"{name}.key"
    run(OPENSSL, "ecparam", "-name", "prime256v1", "-genkey", "-noout", "-out", str(key))
    key.chmod(0o600)
    spki = run(OPENSSL, "pkey", "-in", str(key), "-pubout", "-outform", "DER").stdout
    return key, spki


def mtree_name(name):
    """A path as bsdtar's mtree writes it: every byte outside plain printable
    ASCII, and space, backslash, # and =, as \\ooo."""
    return "".join(chr(b) if 0x20 < b < 0x7f and b not in b"\\#=" else f"\\{b:03o}" for b in name.encode())


def mtree(entries):
    lines = ["#mtree", "/set type=file uid=0 gid=0 mode=644"]
    lines += [f"./{mtree_name(name)} time=1764000000.0" for name in sorted(entries)]
    return gzip.compress(("\n".join(lines) + "\n").encode())


def package_bytes(version=VERSION, depends=("glibc", "qt6-base>=6.12"), helper=None, tweak=None):
    """A synthetic arch-dock package: .PKGINFO, .BUILDINFO, .MTREE, a stand-in
    helper and one incompressible file, so it is over 1 MiB."""
    files = {
        "usr/lib/arch-dock/arch-dock-install-update": (helper or b"#!/bin/sh\nexit 0\n", 0o755),
        "usr/share/arch-dock/payload.bin": (random.Random(5).randbytes(1100 * 1024), 0o644),
    }
    folders = ["usr", "usr/lib", "usr/lib/arch-dock", "usr/share", "usr/share/arch-dock"]
    pkginfo = [f"pkgname = arch-dock", "pkgbase = arch-dock", f"pkgver = {version}", "arch = x86_64",
               "size = 1"] + [f"depend = {need}" for need in depends]
    meta = {".PKGINFO": "\n".join(pkginfo) + "\n", ".BUILDINFO": "format = 2\npkgbuild_sha256sum = {PKGBUILD}\n"}
    state = {"files": files, "folders": folders, "meta": meta, "extra": [], "mtree_skip": set(),
             "owner": 0, "zstd": True, "pax": {}, "links": [], "symlinks": [], "folder_modes": {}}
    if tweak:
        tweak(state)
    entries = set(state["folders"]) | set(state["files"]) | {".PKGINFO", ".BUILDINFO"}
    entries |= {name for name, _ in state["links"]} | {name for name, _ in state["symlinks"]}
    entries -= state["mtree_skip"]
    buffer = io.BytesIO()
    mode = "w:zst" if state["zstd"] else "w:gz"
    with tarfile.open(fileobj=buffer, mode=mode, format=tarfile.PAX_FORMAT) as archive:
        def add(name, data=None, mode_bits=0o644, folder=False, pax=None):
            info = tarfile.TarInfo(name)
            info.uid = info.gid = state["owner"]
            info.uname = info.gname = "root"
            info.mtime = 1764000000
            info.mode = mode_bits
            if pax:
                info.pax_headers = pax
            if folder:
                info.type = tarfile.DIRTYPE
                info.mode = state["folder_modes"].get(name, 0o755)
                archive.addfile(info)
            else:
                info.size = len(data)
                archive.addfile(info, io.BytesIO(data))
        for name, text in state["meta"].items():
            add(name, text.encode())
        add(".MTREE", mtree(entries))
        for name in state["folders"]:
            add(name, folder=True)
        for name, (data, bits) in state["files"].items():
            add(name, data, bits, pax=state["pax"].get(name))
        for name, data, bits in state["extra"]:
            add(name, data, bits)
        for name, target in state["symlinks"]:
            link = tarfile.TarInfo(name)
            link.type = tarfile.SYMTYPE
            link.linkname = target
            link.mode = 0o777
            link.uid = link.gid = state["owner"]
            link.uname = link.gname = "root"
            link.mtime = 1764000000
            archive.addfile(link)
        for name, target in state["links"]:
            link = tarfile.TarInfo(name)
            link.type = tarfile.LNKTYPE
            link.linkname = target
            link.uid = link.gid = state["owner"]
            link.uname = link.gname = "root"
            link.mtime = 1764000000
            archive.addfile(link)
    return buffer.getvalue()


class StandInGitHub:
    """GitHub's API and downloads, answered from a dict of paths."""

    def __init__(self):
        self.answers = {}
        outer = self

        class Handler(http.server.BaseHTTPRequestHandler):
            def do_GET(self):
                status, body = outer.answers.get(self.path, (404, b""))
                self.send_response(status)
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)

            def log_message(self, *arguments):
                pass

        self.server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        self.address = f"http://127.0.0.1:{self.server.server_address[1]}"

    def close(self):
        self.server.shutdown()
        self.server.server_close()


class ManifestVectors(unittest.TestCase):
    def test_every_vector_is_judged_as_listed(self):
        listed = []
        for line in (VECTORS / "expected.txt").read_text(encoding="utf-8").splitlines():
            if not line or line.startswith("#"):
                continue
            name, verdict = line.split()
            listed.append(name)
            with self.subTest(vector=name):
                try:
                    RF.read_manifest((VECTORS / name).read_bytes())
                    got = "ACCEPT"
                except RF.ReleaseError:
                    got = "REFUSE"
                self.assertEqual(got, verdict)
        self.assertEqual(sorted(listed), sorted(p.name for p in VECTORS.glob("*.json")))

    def test_a_subrelease_is_read_only_for_tests(self):
        data = (VECTORS / "subrelease.json").read_bytes()
        with self.assertRaises(RF.ReleaseError):
            RF.read_manifest(data)
        self.assertEqual(RF.read_manifest(data, test_build=True)["version"], "0.1.1-56.1")

    @unittest.skipUnless(Path("/usr/bin/vercmp").exists(), "pacman's vercmp is not on this system (CI)")
    def test_the_version_order_is_pacmans(self):
        versions = ["0.1.1-55", "0.1.1-56", "0.1.1-56.1", "0.1.1-9", "0.1.1-10", "1:0.1.1-1", "0.1.2-1",
                    "1.0", "1.0a", "1.0.a", "1.0rc1", "6.12.0-5", "6.13.0-1", "1.0+r12.gabc-1.2", "1.002", "a", ""]
        for a in versions:
            for b in versions:
                answer = int(run("/usr/bin/vercmp", a, b).stdout.strip() or 0)
                self.assertEqual(RF.vercmp(a, b), (answer > 0) - (answer < 0), (a, b))


class PackageRules(unittest.TestCase):
    def setUp(self):
        temporary = scratch("archdock-release-package-")
        self.addCleanup(temporary.cleanup)
        self.folder = Path(temporary.name)

    def package(self, tweak=None, **options):
        path = self.folder / f"arch-dock-{VERSION}-x86_64.pkg.tar.zst"
        path.write_bytes(package_bytes(tweak=tweak, **options))
        return path

    def test_a_good_package_reads(self):
        info = RF.read_package(self.package())
        self.assertEqual((info["name"], info["version"], info["arch"]), ("arch-dock", VERSION, "x86_64"))
        self.assertEqual(info["depends"], ["glibc", "qt6-base>=6.12"])

    def test_paths_beyond_ascii_and_links_inside_usr_read(self):
        # .MTREE writes "ö" as two escaped bytes; they are one character
        # again once put back together, as the helper reads them.
        def tweak(state):
            state["files"]["usr/share/arch-dock/Größe ö.txt"] = (b"x", 0o644)
            state["symlinks"].append(("usr/share/arch-dock/here", "payload.bin"))
            state["symlinks"].append(("usr/share/arch-dock/there", "/usr/lib/arch-dock/arch-dock-install-update"))
        info = RF.read_package(self.package(tweak=tweak))
        self.assertIn("usr/share/arch-dock/Größe ö.txt", info["entries"])

    def test_the_content_rules_refuse(self):
        cases = {
            "not root": lambda s: s.update(owner=1000),
            "group write": lambda s: s["files"].update({"usr/share/arch-dock/payload.bin": (b"x" * 10, 0o664)}),
            "setuid": lambda s: s["files"].update({"usr/share/arch-dock/payload.bin": (b"x" * 10, 0o4755)}),
            "xattr": lambda s: s["pax"].update({"usr/share/arch-dock/payload.bin": {"SCHILY.xattr.user.mark": "x"}}),
            "acl": lambda s: s["pax"].update({"usr/share/arch-dock/payload.bin": {"SCHILY.acl.access": "user::rw-"}}),
            "path twice": lambda s: s["extra"].append(("usr/share/arch-dock/payload.bin", b"again", 0o644)),
            "outside usr": lambda s: s["extra"].append(("etc/arch-dock.conf", b"x", 0o644)),
            "install script": lambda s: s["meta"].update({".INSTALL": "post_install() { :; }\n"}),
            "pkgname twice": lambda s: s["meta"].update({".PKGINFO": s["meta"][".PKGINFO"] + "pkgname = arch-dock\n"}),
            "no arch": lambda s: s["meta"].update({".PKGINFO": s["meta"][".PKGINFO"].replace("arch = x86_64\n", "")}),
            "mtree misses an entry": lambda s: s["mtree_skip"].add("usr/share/arch-dock/payload.bin"),
            "not zstd": lambda s: s.update(zstd=False),
            # The helper's path rules: no "./" start, no other "." file, and
            # links that stay inside usr/.
            "dot-slash prefix": lambda s: s["extra"].append(("./usr/share/arch-dock/dotted", b"x", 0o644)),
            "another dot file": lambda s: s["meta"].update({".CHANGELOG": "older\n"}),
            "a link leaving usr": lambda s: s["symlinks"].append(("usr/share/arch-dock/away", "../../../etc/shadow")),
            "an absolute link leaving usr": lambda s: s["symlinks"].append(("usr/share/arch-dock/away", "/etc/shadow")),
        }
        for label, tweak in cases.items():
            with self.subTest(case=label):
                with self.assertRaises(RF.ReleaseError):
                    RF.read_package(self.package(tweak=tweak))


class NotesRules(unittest.TestCase):
    def test_the_notes_are_plain_words(self):
        RF.check_notes("The bars no longer flicker after a screen change.")
        for notes in ("", "x" * 20001, "Unreleased fixes", "Run pacman -U https://example.org/a.pkg.tar.zst",
                      "![picture](a.png)", "<b>bold</b>", "```\nsudo pacman -Syu\n```", "sudo pacman -Syu", "$ make"):
            with self.subTest(notes=notes[:30]):
                with self.assertRaises(RF.ReleaseError):
                    RF.check_notes(notes)

    def test_the_section_is_the_versions_own(self):
        changelog = (f"# Changelog\n\n## Unreleased\n\n## {VERSION} — 2026-11-27\n\nFixed the bars.\n\n"
                     "## 0.1.1-55 — 2026-11-20\n\nOlder.\n")
        self.assertEqual(RF.changelog_section(changelog, VERSION), "Fixed the bars.")
        with self.assertRaises(RF.ReleaseError):
            RF.changelog_section(changelog, "0.1.1-57")


class KeyPlace(unittest.TestCase):
    def setUp(self):
        temporary = scratch("archdock-release-key-")
        self.addCleanup(temporary.cleanup)
        self.base = Path(temporary.name)
        self.module = load(TOOL, "release_files_places")
        self.wiped = self.base / "wiped"
        self.wiped.mkdir()
        # The test's folder lies under /tmp, which the shipped list refuses;
        # here only the test's own "wiped" folder counts as one.
        self.module.temporary_places = lambda: [self.wiped]
        self.seen = self.base / "openssl-calls"
        stand_in = self.base / "openssl"
        stand_in.write_text(f'#!/bin/sh\nprintf "%s\\n" "$*" >> "{self.seen}"\nexec {OPENSSL} "$@"\n')
        stand_in.chmod(0o755)
        self.module.OPENSSL = str(stand_in)

    def test_the_shipped_temporary_places(self):
        places = [str(p) for p in RF.temporary_places()]
        runtime = os.environ.get("XDG_RUNTIME_DIR") or f"/run/user/{os.getuid()}"
        self.assertEqual(places, ["/tmp", "/var/tmp", "/dev/shm", runtime])

    def test_refused_places_never_reach_openssl(self):
        """make itself, with a key in each refused place: it stops with the
        place's reason before openssl (here a recording stand-in) is ever run."""
        work_tree = self.base / "Arch Dock"
        (work_tree / ".git").mkdir(parents=True)
        build = self.base / "build"
        (build / "src").mkdir(parents=True)
        (build / "PKGBUILD").write_text("pkgname=arch-dock\npkgver=0.1.1\npkgrel=56\n")
        out = self.base / "out"
        package = self.base / f"arch-dock-{VERSION}-x86_64.pkg.tar.zst"
        package.write_bytes(package_bytes())
        changelog = self.base / "CHANGELOG.md"
        changelog.write_text(f"# Changelog\n\n## {VERSION} — 2026-11-27\n\nThe bars no longer flicker.\n")
        checkpoint = self.base / "SOURCE_CHECKPOINT.json"
        checkpoint.write_text(json.dumps({"head": "0" * 40}))
        cases = {"git work tree": (work_tree / "keys/arch-dock-update-1.key", "lies in a git work tree"),
                 "build folder": (build / "src/arch-dock-update-1.key", "lies in the build folder"),
                 "output folder": (out / "arch-dock-update-1.key", "lies in the output folder"),
                 "wiped": (self.wiped / "arch-dock-update-1.key", "lies in a folder that is wiped")}
        for label, (place, reason) in cases.items():
            with self.subTest(case=label):
                place.parent.mkdir(parents=True, exist_ok=True)
                make_key(place.parent, "arch-dock-update-1")
                key_list = place.parent / "test-keys.txt"
                key_list.write_text("arch-dock-update-1 00\n")
                with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()) as errors:
                    status = self.module.main(["make", str(package), "--changelog", str(changelog), "--pkgbuild",
                                               str(build / "PKGBUILD"), "--checkpoint", str(checkpoint), "--out", str(out),
                                               "--test-key", str(place), "--test-key-list", str(key_list)])
                self.assertEqual(status, 1)
                self.assertIn(reason, errors.getvalue())
                # The functions themselves say the same.
                with self.assertRaises(self.module.ReleaseError):
                    self.module.check_key_file(place, build / "PKGBUILD", out)
        self.assertFalse(self.seen.exists())

    def test_a_readable_key_is_refused_with_the_line_that_fixes_it(self):
        folder = self.base / "keys"
        folder.mkdir()
        key, _ = make_key(folder, "arch-dock-update-1")
        key.chmod(0o640)
        with self.assertRaisesRegex(self.module.ReleaseError, "chmod 600"):
            self.module.check_key_file(key, self.base / "elsewhere/PKGBUILD", self.base / "out")

    def test_ordinary_and_sibling_folders_are_the_owners_choice(self):
        work_tree = self.base / "Arch Dock"
        (work_tree / ".git").mkdir(parents=True)
        for folder in (self.base / "keys", self.base / "Arch Dock key"):
            with self.subTest(folder=folder.name):
                folder.mkdir()
                key, _ = make_key(folder, "arch-dock-update-1")
                self.assertEqual(self.module.check_key_file(key, work_tree / "PKGBUILD", self.base / "out"),
                                 key.resolve())

    def test_check_key_place_opens_nothing(self):
        work_tree = self.base / "project"
        (work_tree / ".git").mkdir(parents=True)
        self.assertEqual(self.module.main(["check-key-place", str(self.base / "keys/arch-dock-update-1.key")]), 0)
        self.assertEqual(self.module.main(["check-key-place", str(work_tree / "k/arch-dock-update-1.key")]), 1)
        self.assertEqual(self.module.main(["check-key-place", str(self.wiped / "arch-dock-update-1.key")]), 1)
        self.assertFalse(self.seen.exists())


class MakeAndCheck(unittest.TestCase):
    """make, verify and check-published on a synthetic repository."""

    def setUp(self):
        temporary = scratch("archdock-release-make-")
        self.addCleanup(temporary.cleanup)
        self.base = Path(temporary.name)
        self.repo = self.base / "repository"
        (self.repo / "tools").mkdir(parents=True)
        (self.repo / "data").mkdir()
        shutil.copy2(TOOL, self.repo / "tools/release-files.py")
        self.keys = self.base / "keys"
        self.keys.mkdir()
        self.key, self.spki = make_key(self.keys, "arch-dock-test-1")
        self.key_list = self.keys / "test-keys.txt"
        self.key_list.write_text(f"arch-dock-test-1 {self.spki.hex()}\n")
        (self.repo / "data/update-keys.txt").write_text("# none\n")
        self.pkgbuild = self.repo / "PKGBUILD"
        self.pkgbuild.write_text("pkgname=arch-dock\npkgver=0.1.1\npkgrel=56.1\n"
                                 "url='https://github.com/aivars11232/Arch-Dock'\n")
        self.changelog = self.repo / "CHANGELOG.md"
        self.changelog.write_text(f"# Changelog\n\n## {VERSION} — 2026-11-27\n\nThe bars no longer flicker.\n")
        self.env = dict(os.environ, GIT_CONFIG_GLOBAL=os.devnull, GIT_CONFIG_NOSYSTEM="1",
                        GIT_AUTHOR_NAME="Test", GIT_AUTHOR_EMAIL="test@invalid", GIT_COMMITTER_NAME="Test",
                        GIT_COMMITTER_EMAIL="test@invalid", GIT_AUTHOR_DATE="2026-11-27T12:00:00+0000",
                        GIT_COMMITTER_DATE="2026-11-27T12:00:00+0000")
        self.git("init", "--quiet")
        self.git("add", "-A")
        self.git("commit", "--quiet", "-m", "first")
        self.first_commit = self.git("rev-parse", "HEAD")
        (self.repo / "data/notes.txt").write_text("second\n")
        self.git("add", "-A")
        self.git("commit", "--quiet", "-m", "second")
        self.head = self.git("rev-parse", "HEAD")
        self.git("tag", "-a", f"v{VERSION}", "-m", VERSION)
        self.checkpoint = self.base / "SOURCE_CHECKPOINT.json"
        self.checkpoint.write_text(json.dumps({"head": self.head}))
        self.github = StandInGitHub()
        self.addCleanup(self.github.close)
        self.module = load(self.repo / "tools/release-files.py", "release_files_fixture")
        self.module.temporary_places = lambda: []
        self.environment = contextlib.ExitStack()
        self.addCleanup(self.environment.close)
        self.environment.enter_context(_environment(ARCHDOCK_RELEASE_FILES_API=self.github.address,
                                                    ARCHDOCK_RELEASE_FILES_DOWNLOADS=self.github.address))

    def git(self, *arguments):
        return run(GIT, *arguments, cwd=self.repo, env=self.env).stdout.decode().strip()

    def helper(self, build=TEST_BUILD, accepts=True, key=None):
        key = key or f"arch-dock-test-1 {hashlib.sha256(self.spki).hexdigest()}"
        return (f"#!/bin/sh\ncase \"$1\" in\n"
                f"  --about) printf '%s\\n' 'source: github.com/aivars11232/Arch-Dock releases (pinned)' "
                f"'build: {build}' 'key: {key}' 'protocol: 1'; exit 0 ;;\n"
                # Like the real helper: a private runtime folder, or exit 2.
                f"  --check-only) [ -d \"$XDG_RUNTIME_DIR\" ] && [ ! -L \"$XDG_RUNTIME_DIR\" ] && "
                f"[ \"$(/usr/bin/stat -c %a:%u \"$XDG_RUNTIME_DIR\")\" = \"700:$(/usr/bin/id -u)\" ] || exit 2\n"
                f"    /usr/bin/cat > /dev/null; exit {0 if accepts else 16} ;;\n"
                f"esac\nexit 2\n").encode()

    def package(self, **options):
        options.setdefault("helper", self.helper())
        path = self.base / f"arch-dock-{options.get('version', VERSION)}-x86_64.pkg.tar.zst"
        # .BUILDINFO names the PKGBUILD's sha256.
        path.write_bytes(package_bytes(**{**options, "tweak": self._buildinfo(options.get("tweak"))}))
        return path

    def _buildinfo(self, tweak):
        digest = hashlib.sha256(self.pkgbuild.read_bytes()).hexdigest()

        def apply(state):
            state["meta"][".BUILDINFO"] = f"format = 2\npkgbuild_sha256sum = {digest}\n"
            if tweak:
                tweak(state)
        return apply

    def make(self, package, out=None, *more, minimum="0.1.1-55"):
        out = out or self.base / "release"
        arguments = ["make", str(package), "--changelog", str(self.changelog), "--pkgbuild", str(self.pkgbuild),
                     "--checkpoint", str(self.checkpoint), "--out", str(out), "--test-key", str(self.key),
                     "--test-key-list", str(self.key_list), *(("--minimum", minimum) if minimum else ()), *more]
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()) as errors:
            status = self.module.main(arguments)
        return status, errors.getvalue(), out

    def publish(self, tag, manifest, signature, latest=True, immutable=True, assets=None):
        repo = "/aivars11232/Arch-Dock"
        self.github.answers[f"{repo}/releases/download/{tag}/update-manifest.json"] = (200, manifest)
        self.github.answers[f"{repo}/releases/download/{tag}/update-manifest.sig"] = (200, signature)
        release = {"tag_name": tag, "draft": False, "prerelease": False, "immutable": immutable,
                   "assets": assets or []}
        self.github.answers[f"/repos{repo}/releases/tags/{tag}"] = (200, json.dumps(release).encode())
        if latest:
            self.github.answers[f"/repos{repo}/releases/latest"] = (200, json.dumps(release).encode())

    def test_the_first_release_is_made_signed_and_checked_three_ways(self):
        status, errors, out = self.make(self.package(), None, "--first-update-release")
        self.assertEqual(status, 0, errors)
        names = sorted(p.name for p in out.iterdir())
        self.assertEqual(names, sorted(["NOT-FOR-PUBLICATION.txt", "notes.md", "update-manifest.json",
                                        "update-manifest.sig", f"arch-dock-{VERSION}-x86_64.pkg.tar.zst.sha256"]))
        self.assertTrue((out / "notes.md").read_text().startswith("NOT FOR PUBLICATION"))
        manifest = (out / "update-manifest.json").read_bytes()
        self.assertTrue(RF.signature_verifies(self.spki, manifest, (out / "update-manifest.sig").read_bytes()))
        read = RF.read_manifest(manifest, test_build=True)
        self.assertEqual((read["version"], read["source_commit"], read["signing_key_id"]),
                         (VERSION, self.head, "arch-dock-test-1"))
        shutil.copy2(self.base / f"arch-dock-{VERSION}-x86_64.pkg.tar.zst", out)
        checked = run("/usr/bin/sha256sum", "-c", f"arch-dock-{VERSION}-x86_64.pkg.tar.zst.sha256", cwd=out, check=False)
        self.assertEqual(checked.returncode, 0, checked.stderr)
        with contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(self.module.main(["verify", str(out), "--test-key-list", str(self.key_list)]), 0)

    def test_the_first_release_lets_any_older_release_check_it(self):
        status, errors, out = self.make(self.package(), None, "--first-update-release", minimum=None)
        self.assertEqual(status, 0, errors)
        read = RF.read_manifest((out / "update-manifest.json").read_bytes(), test_build=True)
        self.assertEqual(read["minimum_supported_version"], "0.1.1-1")

    def test_the_first_release_needs_its_option_and_no_normal_release(self):
        status, errors, _ = self.make(self.package())
        self.assertEqual(status, 1)
        self.assertIn("--first-update-release", errors)
        older = self._published("0.1.1-56", self.first_commit)
        self.publish("v0.1.1-56", *older)
        status, errors, _ = self.make(self.package(), self.base / "again", "--first-update-release")
        self.assertEqual(status, 1)
        self.assertIn("without a normal release", errors)

    def test_a_later_release_must_be_newer_and_descend_from_the_published_one(self):
        self.publish("v0.1.1-56", *self._published("0.1.1-56", self.first_commit))
        status, errors, _ = self.make(self.package())
        self.assertEqual(status, 0, errors)
        self.publish("v0.1.1-57", *self._published("0.1.1-57", self.first_commit))
        status, errors, _ = self.make(self.package(), self.base / "not-newer")
        self.assertEqual(status, 1)
        self.assertIn("not newer", errors)
        self.git("checkout", "--quiet", "--orphan", "elsewhere")
        self.git("commit", "--quiet", "-m", "unrelated")
        unrelated = self.git("rev-parse", "HEAD")
        self.publish("v0.1.1-56", *self._published("0.1.1-56", unrelated))
        status, errors, _ = self.make(self.package(), self.base / "unrelated")
        self.assertEqual(status, 1)
        self.assertIn("not in the history", errors)

    def _published(self, version, commit):
        manifest = dict(RF.read_manifest((VECTORS / "valid.json").read_bytes()))
        manifest.update(version=version, release_tag=f"v{version}", source_commit=commit,
                        minimum_supported_version="0.1.1-55", signing_key_id="arch-dock-test-1")
        manifest["package"] = dict(manifest["package"], file=f"arch-dock-{version}-x86_64.pkg.tar.zst")
        data = (json.dumps(manifest, indent=2) + "\n").encode()
        path = self.keys / f"manifest-{version}"
        path.write_bytes(data)
        signature = run(OPENSSL, "dgst", "-sha256", "-sign", str(self.key), str(path)).stdout
        return data, signature

    def test_what_make_refuses(self):
        cases = {
            "the helper is not a TEST build": (dict(helper=self.helper(build="release")), "release"),
            "the helper trusts another key": (dict(helper=self.helper(key="arch-dock-test-1 " + "0" * 64)), "trusts"),
            "the helper refuses the release": (dict(helper=self.helper(accepts=False)), "--check-only"),
            "a need not in pacman's form": (dict(depends=("glibc", "-bad")), "dependency form"),
            "a version that does not match": (dict(version="0.1.1-57"), "pkgver"),
        }
        for label, (options, message) in cases.items():
            with self.subTest(case=label):
                status, errors, out = self.make(self.package(**options), self.base / label.replace(" ", "-"),
                                                "--first-update-release")
                self.assertEqual(status, 1)
                self.assertIn(message, errors)
                self.assertFalse(out.exists())

    def test_a_buildinfo_for_another_pkgbuild_is_refused(self):
        package = self.package(tweak=lambda s: s["meta"].update({".BUILDINFO": "pkgbuild_sha256sum = 00\n"}))
        status, errors, _ = self.make(package, None, "--first-update-release")
        self.assertEqual(status, 1)
        self.assertIn("BUILDINFO", errors)

    def test_check_published_refuses_a_release_that_is_not_immutable(self):
        status, errors, out = self.make(self.package(), None, "--first-update-release")
        self.assertEqual(status, 0, errors)
        manifest = (out / "update-manifest.json").read_bytes()
        signature = (out / "update-manifest.sig").read_bytes()
        read = RF.read_manifest(manifest, test_build=True)
        package = read["package"]
        assets = [{"name": package["file"], "size": package["size"], "digest": "sha256:" + package["sha256"]},
                  {"name": "update-manifest.json"}, {"name": "update-manifest.sig"},
                  {"name": package["file"] + ".sha256"}]
        tag = f"v{VERSION}"
        check = ["check-published", tag, "--test-key-list", str(self.key_list)]
        self.publish(tag, manifest, signature, immutable=False, assets=assets)
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()) as errors:
            self.assertEqual(self.module.main(check), 1)
        self.assertIn("immutable", errors.getvalue())
        self.publish(tag, manifest, signature, immutable=True, assets=assets)
        with contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(self.module.main(check), 0)
        self.publish(tag, manifest, signature, immutable=True, assets=assets + [{"name": package["file"] + ".sig"}])
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(self.module.main(check), 1)


@contextlib.contextmanager
def _environment(**values):
    saved = {key: os.environ.get(key) for key in values}
    os.environ.update(values)
    try:
        yield
    finally:
        for key, value in saved.items():
            if value is None:
                os.environ.pop(key, None)
            else:
                os.environ[key] = value


if __name__ == "__main__":
    unittest.main(verbosity=2)
