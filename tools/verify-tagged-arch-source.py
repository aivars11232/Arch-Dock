#!/usr/bin/env python3
"""Refuse release source that does not match a clean, explicitly tagged checkout."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tarfile
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path, help="export directory to verify")
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--tag", required=True)
    parser.add_argument("--expected-head", required=True, help="exact intended commit SHA")
    args = parser.parse_args()
    root = args.source_root.resolve()

    def git(*arguments):
        try:
            return subprocess.check_output(["git", "-C", str(root), *arguments],
                                           stderr=subprocess.PIPE).decode().strip()
        except subprocess.CalledProcessError as error:
            parser.error(error.stderr.decode().strip())

    if Path(git("rev-parse", "--show-toplevel")) != root:
        parser.error("source-root must be the checkout root")
    reference = f"refs/tags/{args.tag}"
    head = git("rev-parse", "HEAD")
    if git("cat-file", "-t", reference) != "tag":
        parser.error("release tag must be annotated")
    if head != args.expected_head or git("rev-parse", reference + "^{commit}") != head:
        parser.error("checkout HEAD, tag target and expected HEAD must match")
    if git("status", "--porcelain=v1", "--untracked-files=all"):
        parser.error("release source checkout must be clean, including untracked inputs")
    source_modes = {}
    for entry in git("ls-tree", "-r", "-z", head).split("\0"):
        if entry:
            metadata, relative = entry.split("\t", 1)
            mode, _, _ = metadata.split()
            source_modes[relative] = {"100644": 0o644, "100755": 0o755}.get(mode)

    checkpoint_bytes = (args.output / "SOURCE_CHECKPOINT.json").read_bytes()
    checkpoint = json.loads(checkpoint_bytes)
    epoch = int(git("show", "-s", "--format=%ct", head))
    if checkpoint["head"] != head or checkpoint["source_date_epoch"] != epoch:
        parser.error("checkpoint HEAD/epoch must match the release tag commit")
    recipe = (args.output / "PKGBUILD").read_text()
    version = re.search(r"^pkgver=([0-9.]+)$", recipe, re.M).group(1)
    archive = args.output / f"arch-dock-{version}.tar.gz"
    archive_bytes = archive.read_bytes()
    if (archive_bytes[:2] != b"\x1f\x8b"
            or int.from_bytes(archive_bytes[4:8], "little") != epoch):
        parser.error("gzip timestamp must match the release tag commit epoch")
    with tarfile.open(archive) as exported:
        name = f"arch-dock-{version}/SOURCE_CHECKPOINT.json"
        members = exported.getmembers()
        names = [member.name for member in members]
        if len(names) != len(set(names)):
            parser.error("archive member names must be unique")
        checkpoints = [member for member in members if member.name == name]
        if (len(checkpoints) != 1 or not checkpoints[0].isreg()
                or any(member.name.startswith(name + "/") for member in members)):
            parser.error("archive must contain one regular root checkpoint without descendants")
        if exported.extractfile(checkpoints[0]).read() != checkpoint_bytes:
            parser.error("archived and external checkpoints must match")
        if any(member.mtime != epoch for member in members):
            parser.error("archive timestamps must match the release tag commit epoch")
        if any((member.uid, member.gid, member.uname, member.gname) != (0, 0, "", "")
               for member in members):
            parser.error("archive ownership must be normalized")
        if checkpoints[0].mode != 0o644:
            parser.error("generated checkpoint mode must be 0644")
        records = {record["path"]: record for record in checkpoint["files"]}
        prefix = f"arch-dock-{version}/"
        if (len(records) != len(checkpoint["files"])
                or set(names) != {prefix + path for path in records} | {name}):
            parser.error("archive and checkpoint inventories must match")
        for member in members:
            if member.name == name:
                continue
            relative = member.name.removeprefix(prefix)
            record = records[relative]
            source = root / relative
            if (not member.isreg() or source.is_symlink() or not source.is_file()
                    or not source.resolve().is_relative_to(root)):
                parser.error(f"unsupported release source entry: {relative}")
            payload = exported.extractfile(member).read()
            if (hashlib.sha256(payload).hexdigest() != record["sha256"]
                    or payload != source.read_bytes()
                    or member.mode != record["mode"]
                    or member.mode != source_modes.get(relative)):
                parser.error(f"source inventory bytes/modes differ from tag: {relative}")
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    if not re.search(rf"^sha256sums=\('{digest}'\)$", recipe, re.M):
        parser.error("recipe must pin the verified source archive digest")
    sums = (args.output / "SHA256SUMS").read_text().splitlines()
    if f"{digest}  {archive.name}" not in sums:
        parser.error("SHA256SUMS must contain the verified source archive digest")
    with tempfile.TemporaryDirectory(prefix="archdock-release-verify-") as temporary:
        canonical = Path(temporary) / "canonical"
        generated = subprocess.run(
            [sys.executable, str(root / "tools/prepare-arch-source.py"), str(canonical)],
            capture_output=True, text=True)
        if generated.returncode:
            parser.error("canonical source regeneration failed: " + generated.stderr.strip())
        for name in (archive.name, "SOURCE_CHECKPOINT.json", "PKGBUILD", "SHA256SUMS"):
            expected = canonical / name
            if not expected.is_file() or (args.output / name).read_bytes() != expected.read_bytes():
                parser.error(f"artifact differs from canonical regeneration: {name}")
    if git("status", "--porcelain=v1", "--untracked-files=all"):
        parser.error("release source changed during verification")
    print(f"Tagged source PASS: {args.tag} at {head}; SHA256 {digest}")


if __name__ == "__main__":
    main()
