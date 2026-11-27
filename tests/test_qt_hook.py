#!/usr/bin/env python3
"""The pacman hook that says when Arch Dock no longer fits Qt or Plasma
(data/arch-dock-qt.hook.in, data/arch-dock-check-qt.in).

The installed script is only read as text: no test runs it. Its test copy,
configured with a stand-in pacman and a stand-in loader, runs behind the
fence (tests/fence.py) with scripted answers.

Arguments: <installed script> <test copy> <test copy's tool folder>
<hook file> <the Qt minor version Arch Dock is built with>.
"""
from pathlib import Path
import re
import shlex
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
from fence import Fence  # noqa: E402

if __name__ == "__main__":
    PRODUCTION, TEST_COPY, TOOLS, HOOK, BUILT = sys.argv[1:6]
    del sys.argv[1:6]

ELF = b"\x7fELF\x02\x01\x01\x00" + bytes(56)


def qt_line(built, now):
    return (f"Arch Dock was built for Qt {built} and this system now has Qt {now}. It keeps working, "
            "but its bars may stay shown instead of hiding. Ask for a rebuilt Arch Dock; once it is "
            "published, Panel Studio > Updates installs it.")


def next_minor(built):
    major, minor = built.split(".")
    return f"{major}.{int(minor) + 1}"


class InstalledScriptForm(unittest.TestCase):
    """The script pacman runs as root, read as text."""

    def setUp(self):
        self.text = Path(PRODUCTION).read_text(encoding="utf-8")
        self.code = [line for line in self.text.splitlines()[1:]
                     if line.strip() and not line.lstrip().startswith("#")]

    def test_its_first_statement_sets_path_and_locale(self):
        self.assertTrue(self.text.startswith("#!/bin/sh\n"))
        self.assertEqual(self.code[0], "PATH=/usr/bin; export PATH; LC_ALL=C; export LC_ALL")

    def test_it_calls_its_tools_by_full_path(self):
        for assignment in ("pacman='/usr/bin/pacman'", "od='/usr/bin/od'", "env='/usr/bin/env'",
                           "loader='/usr/lib/ld-linux-x86-64.so.2'", f"built='{BUILT}'"):
            self.assertIn(assignment, self.code)
        # Outside comments and the lines it prints, no tool is named bare.
        commands = "\n".join(line for line in self.code if "printf '%s\\n' \"" not in line)
        for name in ("pacman", "od", "env", "ldd", "ld.so", "grep", "sed", "awk", "cut", "tr",
                     "head", "tail", "cat", "readelf", "sort", "uniq", "xargs"):
            bare = re.findall(r"(?<![\w/.$'\"-])" + re.escape(name) + r"(?![\w.='-])", commands)
            self.assertEqual(bare, [], f"{name} is called by its bare name")

    def test_the_loader_runs_only_with_an_empty_environment(self):
        uses = [line.strip() for line in self.code if '"$loader"' in line]
        self.assertEqual(uses, ['listing=$("$env" -i "$loader" --list "$file" 2>&1)'])


class HookFields(unittest.TestCase):
    def test_the_hook_runs_the_script_after_qt_plasma_and_arch_dock(self):
        fields = []
        section = None
        for line in Path(HOOK).read_text(encoding="utf-8").splitlines():
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            if line.startswith("["):
                section = line
                continue
            key, _, value = line.partition("=")
            fields.append((section, key.strip(), value.strip()))
        self.assertEqual(fields, [
            ("[Trigger]", "Operation", "Install"),
            ("[Trigger]", "Operation", "Upgrade"),
            ("[Trigger]", "Type", "Package"),
            ("[Trigger]", "Target", "qt6-base"),
            ("[Trigger]", "Target", "qt6-declarative"),
            ("[Trigger]", "Target", "libplasma"),
            ("[Trigger]", "Target", "layer-shell-qt"),
            ("[Trigger]", "Target", "arch-dock"),
            ("[Action]", "Description", "Checking that Arch Dock fits the installed Qt and Plasma..."),
            ("[Action]", "When", "PostTransaction"),
            ("[Action]", "Exec", "/usr/share/libalpm/scripts/arch-dock-check-qt"),
        ])


class TestCopyAnswers(unittest.TestCase):
    """The test copy with a scripted pacman and loader, behind the fence."""

    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="archdock-qt-hook-")
        self.addCleanup(temporary.cleanup)
        self.base = Path(temporary.name)
        self.fence = Fence(self.base)
        self.tools = Path(TOOLS)
        self.tools.mkdir(parents=True, exist_ok=True)
        self.addCleanup(self.clear_tools)
        root = self.base / "package"
        self.files = {
            "backend": root / "usr/bin/arch-dock",
            "bars": root / "usr/bin/arch-dock-bars",
            "icon": root / "usr/bin/arch-dock-icon-object",
            "quick3d": root / "usr/lib/qt6/qml/ArchDock/Decorations3D/libarchdock-decorations3dplugin.so",
            "plugin": root / "usr/lib/qt6/qml/ArchDock/Input/libarchdock-inputplugin.so",
            "text": root / "usr/share/doc/arch-dock/INSTALL.md",
        }
        for key, path in self.files.items():
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b"# Installing Arch Dock\n" if key == "text" else ELF)
        self.listing = [str(root / "usr") + "/", str(root / "usr/bin") + "/"] + [str(p) for p in self.files.values()]
        self.calls = self.base / "loader-calls"
        self.seen = self.base / "loader-environment"

    def clear_tools(self):
        for name in ("pacman", "ld-linux"):
            (self.tools / name).unlink(missing_ok=True)

    def write_tool(self, name, body):
        path = self.tools / name
        path.write_text("#!/bin/sh\n" + body, encoding="utf-8")
        path.chmod(0o755)

    def write_pacman(self, qt, installed=("onnxruntime-cpu", "qt6-quick3d")):
        body = 'case "$1" in\n  -Qlq) printf "%s\\n" ' + " ".join(shlex.quote(x) for x in self.listing)
        body += "; exit 0 ;;\n  -Q) case \"$2\" in\n"
        if qt is not None:
            body += f'      qt6-base) printf "%s\\n" "qt6-base {qt}"; exit 0 ;;\n'
        for name in installed:
            body += f'      {name}) printf "%s\\n" "{name} 1.0-1"; exit 0 ;;\n'
        body += "      *) exit 1 ;;\n    esac ;;\nesac\nexit 1\n"
        self.write_tool("pacman", body)

    def write_loader(self, missing_for=None, library="libPlasma.so.9"):
        body = (f'printf "%s\\n" "$2" >> {shlex.quote(str(self.calls))}\n'
                f"export -p > {shlex.quote(str(self.seen))}\n")
        if missing_for is not None:
            body += (f'if [ "$2" = {shlex.quote(str(self.files[missing_for]))} ]; then '
                     f'printf "\\t%s => not found\\n" {library}; fi\n')
        body += 'printf "\\tlibc.so.6 => /usr/lib/libc.so.6 (0x00007f0000000000)\\n"\nexit 0\n'
        self.write_tool("ld-linux", body)

    def run_hook(self, extra=None):
        result = self.fence.run(["/bin/sh", TEST_COPY], extra=extra)
        self.assertEqual(result.returncode, 0, result.stderr)
        return result

    def loader_calls(self):
        return self.calls.read_text(encoding="utf-8").splitlines() if self.calls.exists() else []

    def test_the_same_minor_says_nothing(self):
        self.write_pacman(f"{BUILT}.2-5")
        self.write_loader()
        self.assertEqual(self.run_hook().stdout, "")
        self.assertEqual(sorted(self.loader_calls()),
                         sorted(str(self.files[k]) for k in ("backend", "bars", "icon", "quick3d", "plugin")))

    def test_another_minor_gives_the_qt_line(self):
        self.write_pacman(f"{next_minor(BUILT)}.0-1")
        self.write_loader()
        self.assertEqual(self.run_hook().stdout, qt_line(BUILT, next_minor(BUILT)) + "\n")

    def test_the_epoch_and_release_are_left_out(self):
        self.write_pacman(f"1:{next_minor(BUILT)}.0-1")
        self.write_loader()
        self.assertEqual(self.run_hook().stdout, qt_line(BUILT, next_minor(BUILT)) + "\n")

    def test_a_missing_plasma_library_gives_only_the_bars_line(self):
        self.write_pacman(f"{next_minor(BUILT)}.0-1")
        self.write_loader(missing_for="bars")
        self.assertEqual(self.run_hook().stdout,
                         "Arch Dock's bars cannot start with this Plasma (libPlasma.so.9 is missing). Ask for a "
                         "rebuilt Arch Dock; once it is published, Panel Studio > Updates installs it.\n")

    def test_a_library_missing_for_the_backend_says_to_install_by_hand(self):
        self.write_pacman(f"{BUILT}.0-1")
        self.write_loader(missing_for="backend", library="libKF6Kirigami.so.7")
        self.assertEqual(self.run_hook().stdout,
                         "Arch Dock cannot start with this system (libKF6Kirigami.so.7 is missing). Ask for a "
                         "rebuilt Arch Dock and install it by hand, as the releases page says.\n")

    def test_a_library_missing_for_another_part(self):
        self.write_pacman(f"{BUILT}.0-1")
        self.write_loader(missing_for="plugin", library="libQt6Quick3D.so.6")
        self.assertEqual(self.run_hook().stdout,
                         "Part of Arch Dock cannot start with this system (libQt6Quick3D.so.6 is missing). Ask "
                         "for a rebuilt Arch Dock; once it is published, Panel Studio > Updates installs it.\n")

    def test_the_loader_sees_none_of_the_callers_environment(self):
        self.write_pacman(f"{BUILT}.0-5")
        self.write_loader()
        self.run_hook(extra={"LD_PRELOAD": "libc.so.6", "LD_LIBRARY_PATH": str(self.base / "nowhere"),
                             "ARCHDOCK_HOOK_TEST_MARK": "1"})
        seen = self.seen.read_text(encoding="utf-8")
        for name in ("LD_PRELOAD", "LD_LIBRARY_PATH", "ARCHDOCK_HOOK_TEST_MARK", "HOME", "XDG_RUNTIME_DIR"):
            self.assertNotIn(name + "=", seen)

    def test_files_that_are_not_programs_are_skipped(self):
        self.write_pacman(f"{BUILT}.0-5")
        self.write_loader()
        self.run_hook()
        calls = self.loader_calls()
        self.assertNotIn(str(self.files["text"]), calls)
        self.assertFalse(any(call.endswith("/") for call in calls))

    def test_optional_parts_are_skipped_without_their_packages(self):
        self.write_pacman(f"{BUILT}.0-5", installed=())
        self.write_loader(missing_for="icon", library="libonnxruntime.so.1")
        self.assertEqual(self.run_hook().stdout, "")
        calls = self.loader_calls()
        self.assertNotIn(str(self.files["icon"]), calls)
        self.assertNotIn(str(self.files["quick3d"]), calls)
        self.assertIn(str(self.files["plugin"]), calls)

    def test_without_pacman_it_says_nothing(self):
        self.write_loader()
        self.assertEqual(self.run_hook().stdout, "")
        self.assertEqual(self.loader_calls(), [])


if __name__ == "__main__":
    unittest.main(verbosity=2)
