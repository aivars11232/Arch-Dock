#!/usr/bin/env python3
"""Export the current source checkpoint and a checksum-pinned local PKGBUILD."""
import argparse
import gzip
import hashlib
import io
import json
from pathlib import Path
import re
import stat
import subprocess
import tarfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path, help="new output directory")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]

    def git(*args):
        return subprocess.check_output(["git", "-C", str(root), *args])

    assert Path(git("rev-parse", "--show-toplevel").decode().strip()) == root
    output = args.output.resolve()
    if output.exists() and any(output.iterdir()):
        parser.error("output must be a new or empty directory")
    recipe = (root / "PKGBUILD").read_text()
    version = re.search(r"^pkgver=([0-9.]+)$", recipe, re.M).group(1)
    head = git("rev-parse", "HEAD").decode().strip()
    epoch = int(git("show", "-s", "--format=%ct", "HEAD"))
    excluded = {"PKGBUILD", "docs/CURRENT_STATE.md", "docs/RELEASE_CHECKLIST.md",
                "docs/POST_TASK_0045_CORRECTIVE_REPORT.md"}
    # Operational record folders: nothing below them is ever exported.
    excluded_folders = {"docs/repairs/"}
    reserved_root_members = {"SOURCE_CHECKPOINT.json"}
    paths = sorted(set(git("ls-files", "--cached", "--others", "--exclude-standard", "-z")
                       .decode().rstrip("\0").split("\0")))
    tracked_modes = {}
    for entry in git("ls-files", "--stage", "-z").decode().split("\0"):
        if not entry:
            continue
        metadata, relative = entry.split("\t", 1)
        mode, _, stage = metadata.split()
        if stage != "0":
            parser.error(f"unmerged source entry: {relative}")
        tracked_modes[relative] = mode
    records = []
    for relative in paths:
        path = root / relative
        first = Path(relative).parts[0]
        if relative in excluded or first == "build" or first.startswith("build-") \
                or any(relative.startswith(folder) for folder in excluded_folders):
            continue
        if path.is_symlink():
            parser.error(f"unsupported source entry: {relative}")
        if not path.exists():  # Files removed in the approved working tree.
            continue
        if first in reserved_root_members:
            parser.error(f"reserved generated root namespace collides with source input: {relative}")
        if not path.is_file() or not path.resolve().is_relative_to(root):
            parser.error(f"unsupported source entry: {relative}")
        if relative in tracked_modes:
            mode = {"100644": 0o644, "100755": 0o755}.get(tracked_modes[relative])
            if mode is None:
                parser.error(f"unsupported Git source mode: {relative}")
        else:
            # Developer-only inputs keep executable intent, never arbitrary
            # checkout read/write bits. Official release inputs must be tracked.
            mode = 0o755 if stat.S_IMODE(path.stat().st_mode) & 0o111 else 0o644
        records.append({"path": relative,
                        "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                        "mode": mode})
    checkpoint = {"head": head, "source_date_epoch": epoch,
                  "excluded_operational_files": sorted(excluded | excluded_folders),
                  "files": records}
    manifest = (json.dumps(checkpoint, indent=2, sort_keys=True) + "\n").encode()
    output.mkdir(parents=True, exist_ok=True)
    archive = output / f"arch-dock-{version}.tar.gz"

    def normalized(info):
        info.uid = info.gid = 0
        info.uname = info.gname = ""
        info.mtime = epoch
        info.pax_headers = {}
        return info

    with archive.open("wb") as raw, gzip.GzipFile(filename="", fileobj=raw, mode="wb", mtime=epoch) as compressed:
        with tarfile.open(fileobj=compressed, mode="w", format=tarfile.PAX_FORMAT) as tar:
            for record in records:
                path = root / record["path"]
                data = path.read_bytes()
                if hashlib.sha256(data).hexdigest() != record["sha256"]:
                    raise RuntimeError(f"source changed during export: {record['path']}")
                info = normalized(tarfile.TarInfo(f"arch-dock-{version}/{record['path']}"))
                info.mode, info.size = record["mode"], len(data)
                tar.addfile(info, io.BytesIO(data))
            info = normalized(tarfile.TarInfo(f"arch-dock-{version}/SOURCE_CHECKPOINT.json"))
            info.mode, info.size = 0o644, len(manifest)
            tar.addfile(info, io.BytesIO(manifest))
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    recipe, replacements = re.subn(r"^sha256sums=\('[0-9a-f]{64}'\)$",
                                  f"sha256sums=('{digest}')", recipe, flags=re.M)
    assert replacements == 1
    (output / "PKGBUILD").write_text(recipe)
    (output / "SOURCE_CHECKPOINT.json").write_bytes(manifest)
    (output / "SHA256SUMS").write_text(f"{digest}  {archive.name}\n")
    print(f"Exported {len(records)} source files at {head}; SHA256 {digest}")


if __name__ == "__main__":
    main()
