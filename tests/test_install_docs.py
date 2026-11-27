#!/usr/bin/env python3
"""The install route in README.md and docs/INSTALL.md, which ships in the
package: no install from a web address, no fixed package version in the
guide, every update line chained so that nothing is stopped or restarted
unless pacman installed the file, and the help the Update button's messages
point to, word for word as the button shows them. The guide's own update line
runs behind the fence (tests/fence.py), with stand-ins for sudo, pacman,
pkill and systemctl.

Arguments: <source folder>.
"""
from pathlib import Path
import re
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
from fence import Fence  # noqa: E402

SOURCE = Path(__file__).resolve().parent.parent
if __name__ == "__main__" and len(sys.argv) > 1:
    SOURCE = Path(sys.argv[1])
    del sys.argv[1]

README = (SOURCE / "README.md").read_text(encoding="utf-8")
INSTALL = (SOURCE / "docs/INSTALL.md").read_text(encoding="utf-8")
UPDATER = (SOURCE / "src/updates/ArchDockUpdates.cpp").read_text(encoding="utf-8")
HOLD = (SOURCE / "src/updates/UpdateHold.h").read_text(encoding="utf-8")

FIRST_INSTALL = "sudo pacman -U ./arch-dock-<version>-x86_64.pkg.tar.zst"
RESTART = ('&& { pkill -u "$USER" -x arch-dock-bars; pkill -u "$USER" -x arch-dock; '
           'systemctl --user restart plasma-plasmashell.service; }')


def code_lines(text):
    """Every line inside a fenced code block."""
    lines, inside = [], False
    for line in text.splitlines():
        if line.strip().startswith("```"):
            inside = not inside
        elif inside:
            lines.append(line.strip())
    return lines


def section(text, heading):
    """The text under a heading, up to the next heading of the same or a higher level."""
    level = heading.split(" ", 1)[0]
    start = text.index(heading + "\n")
    rest = text[start + len(heading) + 1:]
    following = re.search(r"^#{1,%d} " % len(level), rest, re.M)
    return rest[:following.start()] if following else rest


def flat(text):
    """The text with line breaks and markdown emphasis taken out, for comparing words."""
    return re.sub(r"\s+", " ", text.replace("**", "").replace("`", ""))


class TheRoute(unittest.TestCase):
    def test_nothing_is_installed_from_a_web_address(self):
        for name, text in (("README.md", README), ("INSTALL.md", INSTALL)):
            with self.subTest(document=name):
                self.assertIsNone(re.search(r"pacman\s+-U\s+[\"']?(https?|ftp|file)://", text))

    def test_no_fixed_package_version_is_named(self):
        for name, text in (("README.md", README), ("INSTALL.md", INSTALL)):
            with self.subTest(document=name):
                self.assertIsNone(re.search(r"arch-dock-\d+\.\d+\.\d+-\d+(\.\d+)?-x86_64\.pkg\.tar\.zst", text))

    def test_every_update_line_restarts_only_after_pacman(self):
        chained = 0
        for name, text in (("README.md", README), ("INSTALL.md", INSTALL)):
            for line in code_lines(text):
                if "pacman -U" not in line:
                    continue
                with self.subTest(document=name, line=line):
                    if line == FIRST_INSTALL:
                        continue
                    self.assertRegex(line, r"^sudo pacman -U \./arch-dock-<[a-z ]+>-x86_64\.pkg\.tar\.zst ")
                    self.assertTrue(line.endswith(RESTART), line)
                    chained += 1
        # The update by hand, a self-built file and the way back.
        self.assertGreaterEqual(chained, 3)
        self.assertIn(FIRST_INSTALL, code_lines(section(INSTALL, "## The simple way: the released package")))

    def test_a_part_is_never_installed_alone(self):
        for name, text in (("README.md", README), ("INSTALL.md", INSTALL)):
            with self.subTest(document=name):
                self.assertIsNone(re.search(r"pacman\s+-S(?![a-zA-Z])", text))


class TheHelp(unittest.TestCase):
    def test_every_fixed_message_of_the_button_is_explained(self):
        guide = flat(section(INSTALL, "### What the Update button's messages mean"))
        messages = re.findall(r'\{QStringLiteral\("L\d+"\), QStringLiteral\("((?:[^"\\]|\\.)*)"\)\}', UPDATER)
        messages.append(re.search(r'QString refusal\(\)\s*\{\s*return QStringLiteral\("((?:[^"\\]|\\.)*)"\);', HOLD).group(1))
        self.assertGreaterEqual(len(messages), 20)
        for message in messages:
            with self.subTest(message=message):
                self.assertIn(message, guide)

    def test_the_messages_with_their_own_steps(self):
        guide = flat(section(INSTALL, "### What the Update button's messages mean"))
        # The password, the part that is not installed, and where the lock steps are.
        self.assertIn("After three wrong tries your password is locked for 10 minutes", guide)
        self.assertIn("Copy Install Line copies sudo pacman -Syu --needed qt6-foo", guide)
        self.assertIn("is a partial upgrade, which Arch does not support", guide)
        self.assertIn("(#leftover-install-lock)", section(INSTALL, "### What the Update button's messages mean"))

    def test_the_lock_steps(self):
        steps = flat(section(INSTALL, "### Leftover install lock"))
        for step in ("close Discover and any Konsole window that is installing",
                     "sudo rm /var/lib/pacman/db.lck", "sudo pacman -Syu"):
            self.assertIn(step, steps)
        # The same steps as Copy Details on the page.
        copied = flat(re.search(r'QString ArchDockUpdates::lockSteps\(\) const\n\{\n    return QStringLiteral\(\n(.*?)\);',
                                UPDATER, re.S).group(1).replace('"\n        "', "").replace('\\n', " "))
        for step in ("close Discover and any Konsole window that is installing", "sudo rm /var/lib/pacman/db.lck"):
            self.assertIn(step, copied)

    def test_the_way_back(self):
        back = section(INSTALL, "### If Arch Dock does not come back")
        self.assertIn("Open Arch Dock from the application menu. If it still does not start, log out and in.", flat(back))
        lines = [line for line in code_lines(back) if "pacman -U" in line]
        self.assertEqual(lines, [f"sudo pacman -U ./arch-dock-<previous version>-x86_64.pkg.tar.zst {RESTART}"])
        self.assertIn("downgrading", back)
        self.assertIn("The Update button never goes back to an older version by itself.", flat(back))


class TheLineRuns(unittest.TestCase):
    """The guide's update line, run by bash behind the fence."""

    def setUp(self):
        self.folder = tempfile.TemporaryDirectory(prefix="archdock-install-docs-")
        self.fence = Fence(self.folder.name)
        self.calls = Path(self.fence.environment["HOME"]) / "calls"
        record = 'printf "%s\\n" "$(basename "$0") $*" >> "$HOME/calls"\n'
        self.fence.stand_in("sudo", record + 'exec "$@"\n')
        self.fence.stand_in("pacman", record + 'exit "${PACMAN_EXIT:-0}"\n')
        self.fence.stand_in("pkill", record + 'exit "${PKILL_EXIT:-0}"\n')
        self.fence.stand_in("systemctl", record + "exit 0\n")
        self.fence.link("basename", "/usr/bin/basename")
        line = [line for line in code_lines(section(INSTALL, "### From 0.1.1-54 or older, or from a file you built"))
                if "pacman -U" in line]
        self.assertEqual(len(line), 1)
        self.line = line[0].replace("<version>", "0.1.1-56")

    def tearDown(self):
        self.folder.cleanup()

    def run_line(self, **settings):
        if self.calls.exists():
            self.calls.unlink()
        result = self.fence.run(("/usr/bin/bash", "-c", self.line), extra={"USER": "tester", **settings})
        calls = self.calls.read_text().splitlines() if self.calls.exists() else []
        return result.returncode, calls

    def test_all_fine(self):
        status, calls = self.run_line()
        self.assertEqual(status, 0)
        self.assertEqual(calls, ["sudo pacman -U ./arch-dock-0.1.1-56-x86_64.pkg.tar.zst",
                                 "pacman -U ./arch-dock-0.1.1-56-x86_64.pkg.tar.zst",
                                 "pkill -u tester -x arch-dock-bars", "pkill -u tester -x arch-dock",
                                 "systemctl --user restart plasma-plasmashell.service"])

    def test_a_failing_pacman_stops_everything_after_it(self):
        status, calls = self.run_line(PACMAN_EXIT="1")
        self.assertNotEqual(status, 0)
        self.assertEqual(calls, ["sudo pacman -U ./arch-dock-0.1.1-56-x86_64.pkg.tar.zst",
                                 "pacman -U ./arch-dock-0.1.1-56-x86_64.pkg.tar.zst"])

    def test_nothing_to_stop_still_restarts_the_panels(self):
        status, calls = self.run_line(PKILL_EXIT="1")
        self.assertEqual(status, 0)
        self.assertEqual(calls[-1], "systemctl --user restart plasma-plasmashell.service")
        self.assertEqual(len(calls), 5)


if __name__ == "__main__":
    unittest.main()
