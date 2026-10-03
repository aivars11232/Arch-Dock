#!/usr/bin/env python3
"""Exercise the current exporter only in owned synthetic Git fixtures."""
import gzip
import hashlib
import io
import json
import os
from pathlib import Path
import shutil
import stat
import subprocess
import sys
import tarfile
import tempfile
import unittest
import zipfile


PROJECT = Path(__file__).resolve().parents[1]


class PrepareArchSourceTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="archdock-export-test-")
        self.addCleanup(self.temporary.cleanup)
        self.base = Path(self.temporary.name)
        self.root = self.base / "repository"
        self.root.mkdir()
        self.environment = dict(os.environ, GIT_CONFIG_GLOBAL=os.devnull,
                                GIT_CONFIG_NOSYSTEM="1",
                                GIT_AUTHOR_DATE="2020-01-02T03:04:05+0000",
                                GIT_COMMITTER_DATE="2020-01-02T03:04:05+0000")
        # Test-fixture Git writes only; never operate on the project history.
        self.git("init", "--quiet")
        self.write("tools/prepare-arch-source.py", (PROJECT / "tools/prepare-arch-source.py").read_bytes())
        self.write("PKGBUILD", (PROJECT / "PKGBUILD").read_bytes())
        self.write(".gitignore", b"ignored.log\n")
        self.write("plain.txt", b"tracked source\n")
        self.write("removed.txt", b"removed in approved working tree\n")
        self.write("run.sh", b"#!/bin/sh\nexit 0\n").chmod(0o750)
        self.write("build/generated.cpp", b"excluded tracked build output\n")
        self.write("build-old/generated.cpp", b"excluded tracked old build output\n")
        self.write("docs/CURRENT_STATE.md", b"operational record\n")
        self.write("docs/RELEASE_CHECKLIST.md", b"operational record\n")
        self.write("docs/POST_TASK_0045_CORRECTIVE_REPORT.md", b"operational corrective evidence\n")
        self.write("docs/SOURCE_CHECKPOINT.json", b'{"nested":"ordinary source"}\n')
        self.git("add", ".")
        self.git("commit", "--quiet", "-m", "Synthetic exporter fixture")

    def git(self, *arguments):
        return subprocess.check_output(["git", "-C", str(self.root),
                                        "-c", "user.name=Exporter fixture",
                                        "-c", "user.email=fixture@example.invalid",
                                        *arguments], env=self.environment)

    def write(self, relative, data):
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        return path

    def export(self, name):
        output = self.base / name
        result = subprocess.run([sys.executable, str(self.root / "tools/prepare-arch-source.py"),
                                 str(output)], capture_output=True, text=True, env=self.environment)
        return result, output

    def test_reserved_metadata_collision(self):
        for tracked in (False, True):
            with self.subTest(tracked=tracked):
                self.write("SOURCE_CHECKPOINT.json", b'{"owner":"ordinary user input"}\n')
                if tracked:
                    self.git("add", "SOURCE_CHECKPOINT.json")
                    self.git("commit", "--quiet", "-m", "Tracked reserved-path control")
                result, output = self.export(f"collision-{tracked}")
                self.assertNotEqual(result.returncode, 0, result.stdout)
                self.assertIn("SOURCE_CHECKPOINT.json", result.stderr)
                self.assertFalse(list(output.glob("*.tar.gz")), "collision published an archive")
                self.assertEqual((self.root / "SOURCE_CHECKPOINT.json").read_bytes(),
                                 b'{"owner":"ordinary user input"}\n')

    def test_inventory_unique_normalized_and_reproducible(self):
        self.write("plain.txt", b"approved working-tree change\n")
        self.write("approved new ē.txt", b"approved untracked input\n")
        self.write("fixture.gz", gzip.compress(b"legitimate compressed fixture", mtime=0))
        compressed = io.BytesIO()
        with zipfile.ZipFile(compressed, "w") as archive:
            archive.writestr("fixture.txt", "legitimate compressed fixture")
        self.write("fixture.zip", compressed.getvalue())
        self.write("ignored.log", b"ignored operational output\n")
        (self.root / "removed.txt").unlink()
        first, output = self.export("valid-first")
        second, repeated = self.export("valid-second")
        self.assertEqual(first.returncode, 0, first.stderr)
        self.assertEqual(second.returncode, 0, second.stderr)
        archive = next(output.glob("*.tar.gz"))
        self.assertEqual(archive.read_bytes(), (repeated / archive.name).read_bytes())
        with tarfile.open(archive) as exported:
            members = exported.getmembers()
            names = [member.name for member in members]
            self.assertEqual(len(names), len(set(names)))
            prefix = archive.name.removesuffix(".tar.gz") + "/"
            self.assertTrue(all(name.startswith(prefix) for name in names))
            payloads = {member.name.removeprefix(prefix): exported.extractfile(member).read()
                        for member in members}
            metadata = payloads["SOURCE_CHECKPOINT.json"]
            checkpoint = json.loads(metadata)
            records = {record["path"]: record for record in checkpoint["files"]}
            self.assertEqual(len(records), len(checkpoint["files"]))
            self.assertEqual(set(payloads) - {"SOURCE_CHECKPOINT.json"}, set(records))
            self.assertEqual(checkpoint["head"], self.git("rev-parse", "HEAD").decode().strip())
            self.assertEqual(metadata, (output / "SOURCE_CHECKPOINT.json").read_bytes())
            for member in members:
                self.assertEqual((member.uid, member.gid, member.uname, member.gname), (0, 0, "", ""))
                self.assertEqual(member.mtime, checkpoint["source_date_epoch"])
                relative = member.name.removeprefix(prefix)
                if relative == "SOURCE_CHECKPOINT.json":
                    self.assertEqual(member.mode, 0o644)
                    continue
                source = self.root / relative
                self.assertEqual(payloads[relative], source.read_bytes())
                self.assertEqual(records[relative]["sha256"], hashlib.sha256(payloads[relative]).hexdigest())
                self.assertEqual(member.mode, records[relative]["mode"])
                self.assertEqual(member.mode, stat.S_IMODE(source.stat().st_mode))
            for excluded in ("PKGBUILD", "docs/CURRENT_STATE.md", "docs/RELEASE_CHECKLIST.md",
                             "docs/POST_TASK_0045_CORRECTIVE_REPORT.md",
                             "build/generated.cpp", "build-old/generated.cpp", "removed.txt", "ignored.log"):
                self.assertNotIn(excluded, records)
            for included in ("plain.txt", "approved new ē.txt", "run.sh", "fixture.gz", "fixture.zip",
                             "docs/SOURCE_CHECKPOINT.json"):
                self.assertIn(included, records)
        digest = hashlib.sha256(archive.read_bytes()).hexdigest()
        self.assertIn(f"sha256sums=('{digest}')", (output / "PKGBUILD").read_text())
        self.assertEqual((output / "SHA256SUMS").read_text(), f"{digest}  {archive.name}\n")

    def test_existing_output_is_preserved(self):
        existing = self.base / "existing"
        existing.mkdir()
        old = existing / "old-candidate.tar.gz"
        old.write_bytes(b"existing candidate must remain untouched")
        result, output = self.export("existing")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("new or empty", result.stderr)
        self.assertEqual(list(output.iterdir()), [old])
        self.assertEqual(old.read_bytes(), b"existing candidate must remain untouched")

    def test_source_symlinks_are_refused(self):
        for tracked in (False, True):
            for dangling in (False, True):
                with self.subTest(tracked=tracked, dangling=dangling):
                    relative = f"link-{tracked}-{dangling}.txt"
                    link = self.root / relative
                    link.symlink_to("absent.txt" if dangling else "plain.txt")
                    if tracked:
                        self.git("add", relative)
                        self.git("commit", "--quiet", "-m", "Source symlink refusal control")
                    result, output = self.export(f"symlink-{tracked}-{dangling}")
                    self.assertNotEqual(result.returncode, 0, result.stdout)
                    self.assertIn("unsupported source entry", result.stderr)
                    self.assertFalse(list(output.glob("*.tar.gz")))
                    link.unlink()


if __name__ == "__main__":
    unittest.main(verbosity=2)
