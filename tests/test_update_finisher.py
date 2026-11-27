#!/usr/bin/env python3
"""The finisher (data/arch-dock-finish-update), which an installed update
starts to restart Arch Dock and Plasma's panels by the owner's update rule.

It runs behind the fence (tests/fence.py) with stand-ins that record every
call: pacman, pkill, systemctl and busctl answer from each case's script,
`cat` gives the old backend's scripted /proc start times, and `sleep`
returns at once.
"""
import json
from pathlib import Path
import re
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
from fence import Fence  # noqa: E402

FINISHER = Path(__file__).resolve().parents[1] / "data/arch-dock-finish-update"
VERSION = "0.1.1-56"


def stat_line(start):
    return f"4242 (arch-dock) S 1 4242 4242 0 -1 4194560 100 0 0 0 5 3 0 0 20 0 9 0 {start} 1000 100"


# busctl reads options anywhere before `--`, after its verb too: like the
# real one, the stand-ins refuse an argument starting with - there (the
# finisher's -1 timeout), other than --user.
BUSCTL_OPTIONS = """for argument in "$@"; do
  [ "$argument" = -- ] && break
  case "$argument" in --user) ;; -*) echo "busctl: unrecognized option '$argument'" >&2; exit 1 ;; esac
done
"""


class Finisher(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="archdock-finisher-")
        self.addCleanup(temporary.cleanup)
        self.base = Path(temporary.name)
        self.fence = Fence(self.base)
        self.calls = self.base / "calls"
        self.runtime = Path(self.fence.environment["XDG_RUNTIME_DIR"])
        self.result = self.runtime / "arch-dock-update-result.json"
        for name in ("date", "mv", "id"):
            self.fence.link(name, f"/usr/bin/{name}")
        self.fence.stand_in("sleep", "exit 0\n")
        self.script(installed=f"arch-dock {VERSION}")

    def script(self, installed, starts=("111", None), systemctl=0, start_service=0, owner_after=1, notify=0):
        """Writes the stand-ins for one case. `starts` are the old backend's
        start times in turn (None: the process is gone); the last repeats."""
        record = f'printf "%s\\n" "$0 $*" | sed "s|^{self.fence.bin}/||" >> "{self.calls}"\n'
        self.fence.link("sed", "/usr/bin/sed")
        self.fence.stand_in("pacman", record + (f'printf "%s\\n" "{installed}"\n' if installed else "exit 1\n"))
        self.fence.stand_in("pkill", record + "exit 0\n")
        self.fence.stand_in("systemctl", record + f"exit {systemctl}\n")
        answers = self.base / "starts"
        answers.write_text("\n".join("-" if s is None else s for s in starts) + "\n")
        counter = self.base / "start-count"
        counter.write_text("0")
        self.fence.stand_in("cat", f"""case "$1" in
  /proc/*/stat)
    n=$(/usr/bin/cat "{counter}"); n=$((n + 1)); printf "%s" "$n" > "{counter}"
    line=$(/usr/bin/sed -n "${{n}}p" "{answers}"); [ -n "$line" ] || line=$(/usr/bin/tail -n 1 "{answers}")
    [ "$line" = "-" ] && exit 1
    printf "%s\\n" "{stat_line('$line').replace('$line', '"$line"')}" ;;
  *) exec /usr/bin/cat "$@" ;;
esac
""")
        owner_count = self.base / "owner-count"
        owner_count.write_text("0")
        self.fence.stand_in("busctl", record + BUSCTL_OPTIONS + f"""case "$*" in
  *StartServiceByName*)
    [ -e "{self.result}" ] && /usr/bin/cp "{self.result}" "{self.base}/result-at-start"
    exit {start_service} ;;
  *NameHasOwner*)
    n=$(/usr/bin/cat "{owner_count}"); n=$((n + 1)); printf "%s" "$n" > "{owner_count}"
    if [ "{owner_after}" -gt 0 ] && [ "$n" -ge "{owner_after}" ]; then printf "b true\\n"; else printf "b false\\n"; fi ;;
  *Notify*) exit {notify} ;;
esac
exit 0
""")

    def finish(self, *arguments):
        return self.fence.run(["/bin/sh", str(FINISHER), *arguments], timeout=120)

    def recorded(self):
        return self.calls.read_text().splitlines() if self.calls.exists() else []

    def status(self):
        data = json.loads(self.result.read_text())
        self.assertEqual(set(data), {"format", "version", "status", "at"})
        self.assertEqual((data["format"], data["version"]), (1, VERSION))
        self.assertRegex(data["at"], r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$")
        return data["status"]

    def notices(self):
        return [call for call in self.recorded() if " Notify " in call]

    def test_bad_arguments_call_nothing(self):
        for arguments in ((), (VERSION,), (VERSION, "4242", "x"), ("0.1.1-56;x", "4242"), (VERSION, "42a"), ("", "4242")):
            with self.subTest(arguments=arguments):
                self.assertEqual(self.finish(*arguments).returncode, 2)
        self.assertEqual(self.recorded(), [])
        self.assertFalse(self.result.exists())

    def test_another_installed_version_only_asks_pacman(self):
        self.script(installed="arch-dock 0.1.1-55")
        self.assertEqual(self.finish(VERSION, "4242").returncode, 0)
        self.assertEqual(self.recorded(), ["pacman -Q arch-dock"])
        self.assertEqual(self.status(), "not-installed")

    def test_the_order_of_a_good_finish(self):
        self.assertEqual(self.finish(VERSION, "4242").returncode, 0)
        calls = self.recorded()
        # The account's own processes only: `id -u` is the namespace's 0 here.
        self.assertEqual(calls[0], "pacman -Q arch-dock")
        self.assertTrue(calls[1].startswith("pkill -u ") and calls[1].endswith(" -x arch-dock-bars"))
        self.assertTrue(calls[2].startswith("pkill -u ") and calls[2].endswith(" -x arch-dock"))
        self.assertEqual(calls[3], "systemctl --user restart plasma-plasmashell.service")
        self.assertIn("StartServiceByName su org.archdock.ArchDock 0", calls[4])
        self.assertIn("NameHasOwner s org.archdock.ArchDock", calls[5])
        self.assertEqual(json.loads((self.base / "result-at-start").read_text())["status"], "starting")
        self.assertEqual(self.status(), "done")
        self.assertEqual(self.notices(), [])
        self.assertEqual(list(self.runtime.glob("arch-dock-update-result.json.*")), [])

    def test_an_old_backend_that_stays_means_log_out(self):
        self.script(installed=f"arch-dock {VERSION}", starts=("111",))
        self.assertEqual(self.finish(VERSION, "4242").returncode, 0)
        self.assertFalse(any(call.startswith("systemctl") for call in self.recorded()))
        self.assertEqual(self.status(), "log-out-needed")
        self.assertEqual(len(self.notices()), 1)
        self.assertIn(f"Arch Dock {VERSION} is installed", self.notices()[0])

    def test_a_reused_process_number_counts_as_gone(self):
        self.script(installed=f"arch-dock {VERSION}", starts=("111", "999"))
        self.assertEqual(self.finish(VERSION, "4242").returncode, 0)
        self.assertIn("systemctl --user restart plasma-plasmashell.service", self.recorded())
        self.assertEqual(self.status(), "done")

    def test_plasma_that_does_not_come_back(self):
        self.script(installed=f"arch-dock {VERSION}", systemctl=1, notify=1)
        self.assertEqual(self.finish(VERSION, "4242").returncode, 0)
        notices = self.notices()
        self.assertEqual(len(notices), 20)
        self.assertTrue(all("Plasma did not come back" in notice for notice in notices))
        self.assertTrue(any("StartServiceByName" in call for call in self.recorded()))
        self.assertEqual(self.status(), "plasma-not-back")

    def test_a_failed_start_respects_a_quit(self):
        for record, wanted, notices in ((True, "quit", 0), (False, "not-started", 1)):
            for case in ("start-fails", "no-owner"):
                with self.subTest(record=record, case=case):
                    self.calls.unlink(missing_ok=True)
                    self.result.unlink(missing_ok=True)
                    stop = self.runtime / "arch-dock-intentional-stop.json"
                    stop.unlink(missing_ok=True)
                    if record:
                        stop.write_text('{"reason":"quit"}\n')
                    self.script(installed=f"arch-dock {VERSION}", start_service=1 if case == "start-fails" else 0,
                                owner_after=0)
                    self.assertEqual(self.finish(VERSION, "4242").returncode, 0)
                    self.assertEqual(self.status(), wanted)
                    self.assertEqual(len(self.notices()), notices)
                    if notices:
                        self.assertIn("Arch Dock did not start again", self.notices()[0])

    def test_notices_pass_quotes_and_backslashes_unchanged(self):
        text = FINISHER.read_text()
        notify = re.search(r"^notify\(\) \{\n.*?^\}\n", text, re.S | re.M).group(0)
        probe = self.base / "probe.sh"
        probe.write_text(notify + 'notify "$1" "$2"\n')
        self.fence.stand_in("busctl", BUSCTL_OPTIONS + f'for argument in "$@"; do printf "%s\\0" "$argument"; done > "{self.base}/argv"\n')
        title, body = "It's \"quoted\"", "back\\slash \\n and 'single'"
        self.assertEqual(self.fence.run(["/bin/sh", str(probe), title, body]).returncode, 0)
        argv = (self.base / "argv").read_bytes().split(b"\0")[:-1]
        self.assertEqual([a.decode() for a in argv], [
            "--user", "--", "call", "org.freedesktop.Notifications", "/org/freedesktop/Notifications",
            "org.freedesktop.Notifications", "Notify", "susssasa{sv}i", "Arch Dock", "0",
            "org.archdock.ArchDock", title, body, "0", "0", "-1"])

    def test_it_names_no_tool_by_full_path(self):
        code = [line for line in FINISHER.read_text().splitlines() if not line.lstrip().startswith("#")]
        self.assertEqual([line for line in code if re.search(r"(^|[\s;|&(`$])/(usr/)?s?bin/", line)], [])


if __name__ == "__main__":
    unittest.main(verbosity=2)
