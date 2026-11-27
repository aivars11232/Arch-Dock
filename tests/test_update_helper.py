#!/usr/bin/env python3
"""The update helper (src/updates/helper), through its test build, which is
never installed: it takes its tool folder, /run, /proc, /etc and its trusted
key from a test root. It runs as root inside a user namespace, with a
stand-in pacman and systemd-run, the real openssl, and a vercmp that records
its arguments. Packages and signed manifests are made here.

Arguments: <the helper's test build>.
"""
import base64
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
from fence import NAMESPACE, namespaces_work  # noqa: E402
from test_release_files import package_bytes  # noqa: E402

if __name__ == "__main__":
    HELPER = sys.argv[1]
    del sys.argv[1:2]

OPENSSL = "/usr/bin/openssl"
VERSION = "0.1.1-56"
NEEDS = ("glibc", "qt6-base>=6.12")
# The frozen exit codes every installed Arch Dock knows.
TABLE = {0, 2} | set(range(10, 23))


def run(*command, **options):
    return subprocess.run(command, capture_output=True, check=True, stdin=subprocess.DEVNULL, **options)


class Helper(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not namespaces_work():
            raise unittest.SkipTest("user namespaces are not available here")
        cls.keys = tempfile.TemporaryDirectory(prefix="archdock-helper-keys-")
        folder = Path(cls.keys.name)
        cls.key = folder / "test.key"
        run(OPENSSL, "ecparam", "-name", "prime256v1", "-genkey", "-noout", "-out", str(cls.key))
        cls.spki = run(OPENSSL, "pkey", "-in", str(cls.key), "-pubout", "-outform", "DER").stdout
        cls.other = folder / "other.key"
        run(OPENSSL, "ecparam", "-name", "prime256v1", "-genkey", "-noout", "-out", str(cls.other))
        cls.package = package_bytes(version=VERSION, depends=NEEDS)

    @classmethod
    def tearDownClass(cls):
        cls.keys.cleanup()

    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="archdock-helper-root-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        for folder in ("tools", "run/systemd/system", "proc/self", "etc", "var/lib/pacman", "state"):
            (self.root / folder).mkdir(parents=True)
        (self.root / "proc/self/cgroup").write_text("0::/system.slice/arch-dock-install-update-77.scope\n")
        (self.root / "proc/self/oom_score_adj").write_text("100\n")
        (self.root / "etc/arch-dock-update-test-root").write_text("")
        (self.root / "trusted-key.txt").write_text(f"arch-dock-test-1 {self.spki.hex()}\n")
        os.symlink(OPENSSL, self.root / "tools/openssl")
        # Work folders a test puts there itself, as an earlier run's leftovers.
        self.planted = set()
        self.calls = self.root / "state/calls"
        self.installed = self.root / "state/installed"
        self.installed.write_text("0.1.1-55")
        self.pacman()
        self.tool("vercmp", f'printf "vercmp %s\\n" "$*" >> "{self.calls}"\nexec /usr/bin/vercmp "$@"\n')
        self.tool("systemd-run", f'printf "systemd-run %s\\n" "$*" >> "{self.calls}"\n'
                  'while [ "$#" -gt 0 ] && [ "$1" != "--" ]; do shift; done\nshift\nexec "$@"\n')

    def tool(self, name, body):
        path = self.root / "tools" / name
        path.write_text("#!/bin/sh\n" + body)
        path.chmod(0o755)

    def pacman(self, unmet="", printed=None, depends=None, install="ok", answer_q=True, q_later=None, q_after=1,
               t_fails_after=None):
        """The stand-in pacman. `q_later`: what -Q shows after its first
        `q_after` answers (another install in between); `t_fails_after`: -T
        finds a need unmet after that many calls."""
        printed = printed if printed is not None else f"arch-dock {VERSION} x86_64"
        depends = depends if depends is not None else " ".join(NEEDS)
        counts = self.root / "state"
        if not answer_q:
            q = '[ "$1 $2" = "-Q arch-dock" ] && exit 1\n'
        else:
            q = (f'[ "$1 $2" = "-Q arch-dock" ] && {{ n=$(/usr/bin/cat "{counts}/q-count" 2>/dev/null || echo 0); '
                 f'n=$((n + 1)); printf "%s" "$n" > "{counts}/q-count"; '
                 f'if [ -n "{q_later or ""}" ] && [ "$n" -gt {q_after} ]; then printf "arch-dock %s\\n" "{q_later or ""}"; '
                 f'else printf "arch-dock %s\\n" "$(/usr/bin/cat "{self.installed}")"; fi; exit 0; }}\n')
        unmet_now = (f'n=$(/usr/bin/cat "{counts}/t-count" 2>/dev/null || echo 0); n=$((n + 1)); printf "%s" "$n" > "{counts}/t-count"; '
                     f'[ "$n" -gt {t_fails_after} ] && {{ printf "qt6-base>=6.12\\n"; exit 127; }}; '
                     if t_fails_after is not None else "")
        installing = {
            "ok": f'printf "installing arch-dock\\n"; printf "%s" "{VERSION}" > "{self.installed}"; exit 0',
            "downgrade": 'printf "warning: downgrading package arch-dock\\n"; exit 0',
            "fails": 'printf "error: failed to commit transaction\\n"; exit 1',
            # How pacman starts: its limits, its signals, its input.
            "probe": (f'{{ printf "fsize %s\\n" "$(ulimit -f)"; printf "cpu %s\\n" "$(ulimit -t)"; '
                      f'/usr/bin/grep -E "^Sig(Ign|Blk):" /proc/$$/status; '
                      f'printf "stdin %s\\n" "$(/usr/bin/readlink /proc/$$/fd/0)"; }} > "{counts}/probe"; '
                      f'printf "%s" "{VERSION}" > "{self.installed}"; exit 0'),
            # Far more output than a pipe holds.
            "loud": (f'i=0; while [ "$i" -lt 4000 ]; do printf "%s\\n" "checking package integrity, a long line of pacman output $i"; '
                     f'i=$((i + 1)); done; printf "%s" "{VERSION}" > "{self.installed}"; exit 0'),
        }[install]
        self.tool("pacman", f'printf "pacman %s\\n" "$*" >> "{self.calls}"\n' + q + f"""case "$1" in
  -T) {unmet_now}[ -n "{unmet}" ] && {{ printf "%s\\n" "{unmet}"; exit 127; }}; exit 0 ;;
  -U)
    case "$*" in
      *"%n %v %a"*) printf "%s\\n" "{printed}"; exit 0 ;;
      *"%D"*) printf "%s\\n" "{depends}"; exit 0 ;;
      *--noconfirm*) {installing} ;;
    esac ;;
esac
exit 1
""")

    def manifest(self, key=None, **changes):
        manifest = {
            "format": 1, "product": "arch-dock", "channel": "stable", "version": VERSION,
            "release_tag": f"v{VERSION}", "source_commit": "0" * 40, "published_at": "2026-12-03T12:00:00Z",
            "release_notes": "The bars no longer flicker.", "minimum_supported_version": "0.1.1-55",
            "signing_key_id": "arch-dock-test-1",
            "package": {"file": f"arch-dock-{VERSION}-x86_64.pkg.tar.zst", "size": len(self.package),
                        "sha256": hashlib.sha256(self.package).hexdigest(), "arch": "x86_64",
                        "needs": list(NEEDS), "required_updater_version": 1}}
        for name, value in changes.items():
            (manifest["package"] if name in manifest["package"] else manifest)[name] = value
        data = (json.dumps(manifest, indent=2) + "\n").encode()
        path = self.root / "state/manifest"
        path.write_bytes(data)
        signature = run(OPENSSL, "dgst", "-sha256", "-sign", str(key or self.key), str(path)).stdout
        return data, signature

    def stream(self, manifest=None, package=None):
        data, signature = manifest or self.manifest()
        return base64.b64encode(data) + b"\n" + base64.b64encode(signature) + b"\n" + (package or self.package)

    def helper(self, *arguments, stream=b"", as_root=True, pkexec=True, extra=None, before=(), preexec=None):
        environment = {"ARCHDOCK_HELPER_TEST_ROOT": str(self.root), "PATH": "/usr/bin"}
        if pkexec:
            environment["PKEXEC_UID"] = "1000"
        environment.update(extra or {})
        command = (*before, *NAMESPACE, HELPER, *arguments) if as_root else (*before, HELPER, *arguments)
        result = subprocess.run(command, input=stream, capture_output=True, env=environment, timeout=120,
                                preexec_fn=preexec)
        # Every code the helper ends with is one of the frozen table's.
        self.assertIn(result.returncode, TABLE, result.stderr.decode(errors="replace"))
        return result

    def recorded(self):
        return self.calls.read_text().splitlines() if self.calls.exists() else []

    def assertCode(self, result, code):
        self.assertEqual(result.returncode, code, result.stderr.decode(errors="replace"))
        # The work folder goes on every path.
        folder = self.root / "run/arch-dock-update"
        if folder.is_dir() and not folder.is_symlink() and os.access(folder, os.R_OK | os.X_OK):
            made = [p.name for p in folder.iterdir() if re.fullmatch(r"work\.[A-Za-z0-9]{6}", p.name)
                    and not p.is_symlink() and p.name not in self.planted]
            self.assertEqual(made, [])

    # Who may run it.
    def test_only_root_through_the_password_window(self):
        self.assertCode(self.helper(stream=self.stream(), as_root=False), 2)
        self.assertCode(self.helper(stream=self.stream(), pkexec=False), 2)
        (self.root / "etc/arch-dock-update-test-root").unlink()
        self.assertCode(self.helper(stream=self.stream()), 2)
        self.assertEqual(self.recorded(), [])

    def test_about_runs_as_the_user_only(self):
        about = self.helper("--about", as_root=False, pkexec=False)
        self.assertCode(about, 0)
        self.assertIn(f"key: arch-dock-test-1 {hashlib.sha256(self.spki).hexdigest()}", about.stdout.decode())
        self.assertIn("build: TEST: accepts a test key and a test server", about.stdout.decode())
        self.assertCode(self.helper("--about", as_root=True, pkexec=False), 2)
        self.assertCode(self.helper("--about", as_root=False, pkexec=True), 2)

    # A good run, and its order.
    def test_a_good_install(self):
        result = self.helper(stream=self.stream())
        self.assertCode(result, 0)
        self.assertEqual(result.stdout.count(b"started\n"), 1)
        calls = self.recorded()
        scopes = [c for c in calls if c.startswith("systemd-run")]
        self.assertEqual(len(scopes), 1)
        self.assertIn("--system --scope --quiet --collect --unit=arch-dock-install-update-", scopes[0])
        self.assertIn("--description=Arch Dock update -p KillSignal=SIGINT --", scopes[0])
        installs = [c for c in calls if "--noconfirm" in c]
        self.assertEqual(len(installs), 1)
        self.assertRegex(installs[0], r"pacman -U --noconfirm --noprogressbar --color never -- .*/run/arch-dock-update/work\.[A-Za-z0-9]{6}/arch-dock-0\.1\.1-56-x86_64\.pkg\.tar\.zst$")
        self.assertEqual(calls[-1], "pacman -Q arch-dock")
        for call in calls:
            if call.startswith("vercmp"):
                self.assertNotIn("--", call.split())
        # The work folder goes on every path; the helper's own folder stays root's.
        folder = self.root / "run/arch-dock-update"
        self.assertEqual(oct(folder.stat().st_mode & 0o777), "0o700")
        self.assertEqual([p.name for p in folder.iterdir() if p.name.startswith("work.")], [])

    def test_without_systemd_it_runs_directly(self):
        shutil.rmtree(self.root / "run/systemd")
        self.assertCode(self.helper(stream=self.stream()), 0)
        self.assertFalse(any(c.startswith("systemd-run") for c in self.recorded()))

    def test_the_inner_copy_needs_its_own_system_scope(self):
        (self.root / "proc/self/cgroup").write_text("0::/user.slice/user-1000.slice/user@1000.service/app.slice/arch-dock-install-update-1.scope\n")
        self.assertCode(self.helper(stream=self.stream()), 19)
        self.assertEqual(len([c for c in self.recorded() if c.startswith("systemd-run")]), 1)

    # The helper's folder and pacman's lock.
    def test_a_folder_that_is_a_link_is_refused(self):
        # The link's target is root's alone (0700): only the link rule refuses it.
        elsewhere = self.root / "state/elsewhere"
        elsewhere.mkdir()
        os.chmod(elsewhere, 0o700)
        (self.root / "run/arch-dock-update").symlink_to(elsewhere)
        result = self.helper(stream=self.stream())
        self.assertCode(result, 19)
        self.assertIn(b"is not a folder of root's alone", result.stderr)
        self.assertEqual(list(elsewhere.iterdir()), [])

    def test_a_folder_of_another_mode_is_refused(self):
        folder = self.root / "run/arch-dock-update"
        folder.mkdir()
        os.chmod(folder, 0o755)
        result = self.helper(stream=self.stream())
        self.assertCode(result, 19)
        self.assertIn(b"is not a folder of root's alone", result.stderr)
        self.assertEqual(list(folder.iterdir()), [])

    def test_a_folder_of_another_owner_is_refused(self):
        folder = self.root / "run/arch-dock-update"
        folder.mkdir(mode=0o700)
        # Another account's folder: chowned in a namespace with more ids, it
        # is no one the helper's namespace knows, so not root's.
        moved = subprocess.run(("/usr/bin/unshare", "--user", "--map-auto", "--map-root-user", "/usr/bin/chown", "1:1", str(folder)),
                               capture_output=True, stdin=subprocess.DEVNULL, timeout=30)
        if moved.returncode != 0:
            self.skipTest("this system has no subordinate ids for another owner")
        result = self.helper(stream=self.stream())
        self.assertCode(result, 19)
        self.assertIn(b"is not a folder of root's alone", result.stderr)

    def test_leftover_work_goes_only_while_pacmans_lock_is_free(self):
        folder = self.root / "run/arch-dock-update"
        folder.mkdir(mode=0o700)
        leftover = folder / "work.AbC123"
        leftover.mkdir()
        self.planted.add(leftover.name)
        (leftover / "arch-dock-0.1.1-56-x86_64.pkg.tar.zst").write_bytes(b"old")
        other = folder / "work.zz"
        other.mkdir()
        precious = self.root / "state/precious"
        precious.mkdir()
        (precious / "keep").write_text("keep")
        (folder / "work.Lnk123").symlink_to(precious)
        # pacman's lock held: nothing is removed, and the helper stops at the lock.
        lock = self.root / "var/lib/pacman/db.lck"
        lock.write_text("")
        (self.root / "proc/4242/fd").mkdir(parents=True)
        (self.root / "proc/4242/fd/3").symlink_to(lock)
        self.assertCode(self.helper(stream=self.stream()), 10)
        self.assertTrue(leftover.exists())
        # The lock free again: only the leftover work folder goes.
        lock.unlink()
        self.assertCode(self.helper(stream=self.stream()), 0)
        self.assertFalse(leftover.exists())
        self.assertTrue(other.exists())
        self.assertTrue((folder / "work.Lnk123").is_symlink())
        self.assertEqual((precious / "keep").read_text(), "keep")
        self.assertTrue((folder / "lock").exists())

    def test_another_update_running(self):
        folder = self.root / "run/arch-dock-update"
        folder.mkdir(mode=0o700)
        with open(folder / "lock", "w") as lock:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
            self.assertCode(self.helper(stream=self.stream()), 10)

    def test_pacmans_lock_held_or_left_behind(self):
        lock = self.root / "var/lib/pacman/db.lck"
        lock.write_text("")
        self.assertCode(self.helper(stream=self.stream()), 18)
        self.assertTrue(lock.exists())
        alias = self.root / "state/db.lck.alias"
        os.link(lock, alias)
        (self.root / "proc/4242/fd").mkdir(parents=True)
        (self.root / "proc/4242/fd/3").symlink_to(alias)
        self.assertCode(self.helper(stream=self.stream()), 10)
        self.assertTrue(lock.exists())

    # Versions, dependencies and pacman's own reading.
    def test_no_answer_for_the_installed_version(self):
        self.pacman(answer_q=False)
        self.assertCode(self.helper(stream=self.stream()), 21)

    def test_not_newer(self):
        self.installed.write_text(VERSION)
        self.assertCode(self.helper(stream=self.stream()), 13)
        self.assertFalse(any("--noconfirm" in c for c in self.recorded()))

    def test_a_version_installed_in_between_is_caught(self):
        # pacman -Q shows 0.1.1-57 from the second answer on (the check when
        # the copy is ready) or from the third (right before -U): either way
        # nothing is installed, so the update never goes back.
        for after in (1, 2):
            with self.subTest(changed_after=after):
                self.calls.unlink(missing_ok=True)
                (self.root / "state/q-count").unlink(missing_ok=True)
                self.pacman(q_later="0.1.1-57", q_after=after)
                self.assertCode(self.helper(stream=self.stream()), 13)
                self.assertFalse(any("--noconfirm" in c for c in self.recorded()))

    def test_a_need_removed_in_between_is_caught(self):
        # -T passes when the copy is ready and fails right before -U.
        self.pacman(t_fails_after=1)
        self.assertCode(self.helper(stream=self.stream()), 11)
        self.assertFalse(any("--noconfirm" in c for c in self.recorded()))

    def test_unmet_dependencies(self):
        self.pacman(unmet="qt6-base>=6.12")
        self.assertCode(self.helper(stream=self.stream()), 11)
        calls = self.recorded()
        self.assertFalse(any(c.startswith("pacman -U") for c in calls))
        self.assertTrue(any(c.startswith("pacman -T -- glibc") for c in calls))

    def test_pacmans_reading_must_match(self):
        self.pacman(printed=f"arch-dock {VERSION} x86_64\nqt6-foo 1-1 x86_64")
        self.assertCode(self.helper(stream=self.stream()), 11)
        self.pacman(printed=f"arch-dock 0.1.1-57 x86_64")
        self.assertCode(self.helper(stream=self.stream()), 12)
        self.pacman(depends="glibc")
        self.assertCode(self.helper(stream=self.stream()), 12)

    # Signature, format and the copy.
    def test_another_key_is_refused_before_any_reading(self):
        self.assertCode(self.helper(stream=self.stream(self.manifest(key=self.other))), 16)
        self.assertFalse(any(c.startswith("pacman") for c in self.recorded()))

    def test_a_key_written_twice_is_a_failed_check(self):
        # Signed with the right key, but a key written twice: a reading rule
        # refuses it (12), not the signature (16).
        data, _ = self.manifest()
        twice = data.replace(b'  "channel": "stable",', b'  "channel": "stable",\n  "channel": "stable",')
        path = self.root / "state/manifest"
        path.write_bytes(twice)
        signature = run(OPENSSL, "dgst", "-sha256", "-sign", str(self.key), str(path)).stdout
        self.assertCode(self.helper(stream=self.stream((twice, signature))), 12)

    def test_format_2_needs_an_install_by_hand(self):
        self.assertCode(self.helper(stream=self.stream(self.manifest(format=2))), 20)

    def test_other_bytes_than_signed(self):
        self.assertCode(self.helper(stream=self.stream(package=self.package[:-1] + b"x")), 12)
        self.assertCode(self.helper(stream=self.stream(package=self.package[:-10])), 12)
        self.assertCode(self.helper(stream=self.stream(package=self.package + b"extra")), 12)

    def test_a_reported_downgrade(self):
        self.pacman(install="downgrade")
        self.assertCode(self.helper(stream=self.stream()), 15)

    def test_a_failed_install(self):
        self.pacman(install="fails")
        self.assertCode(self.helper(stream=self.stream()), 14)

    # Before anything else, and how pacman starts.
    def test_first_things(self):
        # Low soft limits and SIGINT and SIGHUP ignored and SIGTERM blocked
        # in the caller: the helper raises the limits, and pacman starts with
        # every signal at its default, none blocked, /dev/null as input.
        self.pacman(install="probe")

        def caller():
            signal.signal(signal.SIGINT, signal.SIG_IGN)
            signal.signal(signal.SIGHUP, signal.SIG_IGN)
            signal.pthread_sigmask(signal.SIG_BLOCK, {signal.SIGTERM})
        result = self.helper(stream=self.stream(), before=("/usr/bin/prlimit", "--fsize=65536:unlimited", "--cpu=100:unlimited", "--"),
                             preexec=caller)
        self.assertCode(result, 0)
        probe = dict(line.split(maxsplit=1) for line in (self.root / "state/probe").read_text().splitlines())
        self.assertEqual(probe["fsize"], "unlimited")
        self.assertEqual(probe["cpu"], "unlimited")
        self.assertEqual(int(probe["SigIgn:"], 16), 0)
        self.assertEqual(int(probe["SigBlk:"], 16), 0)
        self.assertEqual(probe["stdin"], "/dev/null")
        self.assertEqual((self.root / "proc/self/oom_score_adj").read_text().strip(), "0")

    # The scope's own ends.
    def test_systemd_run_failing_on_its_own(self):
        self.tool("systemd-run", f'printf "systemd-run %s\\n" "$*" >> "{self.calls}"\nexit 1\n')
        self.assertCode(self.helper(stream=self.stream()), 19)
        self.assertFalse(any(c.startswith("pacman") for c in self.recorded()))

    def test_an_inner_copy_ended_by_a_signal(self):
        # systemd-run runs the inner copy as itself; a signal that ends it is 22.
        self.tool("systemd-run", f'printf "systemd-run %s\\n" "$*" >> "{self.calls}"\nexec /bin/sh -c \'kill -KILL $$\'\n')
        self.assertCode(self.helper(stream=self.stream()), 22)

    # A reader that stops reading never stalls pacman.
    def test_a_reader_that_stops_reading(self):
        self.pacman(install="loud")
        environment = {"ARCHDOCK_HELPER_TEST_ROOT": str(self.root), "PATH": "/usr/bin", "PKEXEC_UID": "1000"}
        process = subprocess.Popen((*NAMESPACE, HELPER), stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                   stderr=subprocess.DEVNULL, env=environment)
        try:
            process.stdin.write(self.stream())
            process.stdin.close()
            self.assertEqual(process.wait(timeout=120), 0)
        finally:
            if process.poll() is None:
                process.kill()
                process.wait()
            process.stdout.close()
        self.assertEqual(self.installed.read_text(), VERSION)

    # --check-only, as the user.
    def test_check_only_as_the_user(self):
        runtime = self.root / "runtime"
        runtime.mkdir(mode=0o700)
        result = self.helper("--check-only", stream=self.stream(), as_root=False, pkexec=False,
                             extra={"XDG_RUNTIME_DIR": str(runtime)})
        self.assertCode(result, 0)
        self.assertEqual(result.stdout, f"would install arch-dock {VERSION}\n".encode())
        self.assertFalse(any("--noconfirm" in c for c in self.recorded()))
        self.assertCode(self.helper("--check-only", stream=self.stream(), as_root=False, pkexec=True,
                                    extra={"XDG_RUNTIME_DIR": str(runtime)}), 2)
        runtime.chmod(0o755)
        self.assertCode(self.helper("--check-only", stream=self.stream(), as_root=False, pkexec=False,
                                    extra={"XDG_RUNTIME_DIR": str(runtime)}), 2)


if __name__ == "__main__":
    unittest.main(verbosity=2)
