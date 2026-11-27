"""The fence for tests that run Arch Dock's update and system scripts.

Nothing such a test starts may reach the real pkexec, pacman (beyond what the
script itself reads by full path), systemctl, systemd-run, pkill or the
network. So every test command runs:
- with a cleared environment whose PATH is a folder of stand-ins, and whose
  home, XDG folders and bus addresses lie in the test's temporary folder (the
  bus addresses point at sockets that do not exist);
- after a gate: every risky name must resolve to that folder, or the test
  stops before anything runs;
- inside a user namespace without network and with its own process list,
  where this system allows one; otherwise in the environment fence alone,
  which the log says.
"""
import os
from pathlib import Path
import subprocess
import sys

# Names a script could reach the system with. Each gets a stand-in that
# fails loudly unless a test writes a scripted one.
RISKY = ("pkexec", "pacman", "vercmp", "pkill", "systemctl", "systemd-run", "systemd-inhibit",
         "busctl", "gdbus", "dbus-send", "curl", "wget", "sudo", "su", "setpriv", "ldd", "ld.so")

UNSHARE = "/usr/bin/unshare"
NAMESPACE = (UNSHARE, "--user", "--map-root-user", "--net", "--pid", "--fork", "--mount-proc")

_namespaces = None


def namespaces_work():
    """Whether this system lets an ordinary user make the fence's namespaces."""
    global _namespaces
    if _namespaces is None:
        try:
            _namespaces = subprocess.run((*NAMESPACE, "/usr/bin/true"), env={},
                                         stdin=subprocess.DEVNULL, capture_output=True,
                                         timeout=20).returncode == 0
        except (OSError, subprocess.SubprocessError):
            _namespaces = False
        if not _namespaces:
            print("fence: user namespaces are not available here; the environment fence alone is used",
                  file=sys.stderr)
    return _namespaces


class Fence:
    """A stand-in folder and a fenced environment under `base`."""

    def __init__(self, base):
        self.base = Path(base)
        self.bin = self.base / "fence-bin"
        self.bin.mkdir(parents=True, exist_ok=True)
        for name in RISKY:
            self.stand_in(name, f'printf "%s\\n" "fence: unexpected call of {name} $*" >&2\nexit 99\n',
                          replace=False)
        self.environment = {
            "PATH": str(self.bin),
            "HOME": str(self.base / "home"),
            "XDG_CONFIG_HOME": str(self.base / "config"),
            "XDG_DATA_HOME": str(self.base / "data"),
            "XDG_CACHE_HOME": str(self.base / "cache"),
            "XDG_STATE_HOME": str(self.base / "state"),
            "XDG_RUNTIME_DIR": str(self.base / "runtime"),
            "DBUS_SESSION_BUS_ADDRESS": f"unix:path={self.base / 'no-session-bus'}",
            "DBUS_SYSTEM_BUS_ADDRESS": f"unix:path={self.base / 'no-system-bus'}",
            "LANG": "C.UTF-8",
        }
        for key in ("HOME", "XDG_CONFIG_HOME", "XDG_DATA_HOME", "XDG_CACHE_HOME",
                    "XDG_STATE_HOME", "XDG_RUNTIME_DIR"):
            Path(self.environment[key]).mkdir(parents=True, exist_ok=True)
        os.chmod(self.environment["XDG_RUNTIME_DIR"], 0o700)

    def stand_in(self, name, body, replace=True):
        """Writes a /bin/sh stand-in called `name` into the stand-in folder."""
        path = self.bin / name
        if path.exists() and not replace:
            return path
        path.write_text("#!/bin/sh\n" + body, encoding="utf-8")
        path.chmod(0o755)
        return path

    def link(self, name, real):
        """Puts a real, harmless tool into the stand-in folder."""
        if name in RISKY:
            raise ValueError(f"{name} may only be a stand-in")
        target = self.bin / name
        if target.exists() or target.is_symlink():
            target.unlink()
        target.symlink_to(real)

    def gate(self, environment=None):
        """Stops the test unless every risky name resolves to the stand-in folder."""
        environment = environment or self.environment
        for name in RISKY:
            found = subprocess.run(("/bin/sh", "-c", 'command -v -- "$1"', "sh", name),
                                   env=environment, capture_output=True, text=True).stdout.strip()
            if Path(found).parent != self.bin:
                raise RuntimeError(f"fence: {name} resolves to {found or 'nothing'}, not to a stand-in")

    def run(self, argv, extra=None, **options):
        """Runs argv behind the fence and returns the completed process."""
        environment = dict(self.environment)
        environment.update(extra or {})
        self.gate(environment)
        command = (*NAMESPACE, *argv) if namespaces_work() else tuple(argv)
        options.setdefault("capture_output", True)
        options.setdefault("text", True)
        options.setdefault("timeout", 60)
        if "input" not in options:
            options.setdefault("stdin", subprocess.DEVNULL)
        return subprocess.run(command, env=environment, cwd=str(self.base), **options)


def require_tool(path):
    """The real tools a fenced script calls by full path must exist."""
    if not Path(path).exists():
        raise RuntimeError(f"{path} is missing")
    return path


__all__ = ["Fence", "RISKY", "namespaces_work", "require_tool"]
