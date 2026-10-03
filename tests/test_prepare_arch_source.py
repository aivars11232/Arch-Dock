#!/usr/bin/env python3
"""Exercise the current exporter only in owned synthetic Git fixtures."""
import copy
import gzip
import hashlib
import io
import json
import os
from pathlib import Path
import re
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

    def test_reserved_metadata_descendants(self):
        for relative in ("SOURCE_CHECKPOINT.json/notes.txt", "SOURCE_CHECKPOINT.json/a/b.txt"):
            for tracked in (False, True):
                with self.subTest(relative=relative, tracked=tracked):
                    payload = b"conflicting source must remain untouched\n"
                    source = self.write(relative, payload)
                    try:
                        if tracked:
                            self.git("add", relative)
                            self.git("commit", "--quiet", "-m", "Reserved descendant control")
                        result, output = self.export(f"descendant-{tracked}-{source.name}")
                        self.assertNotEqual(result.returncode, 0, result.stdout)
                        self.assertIn("SOURCE_CHECKPOINT.json", result.stderr)
                        self.assertFalse(list(output.glob("*.tar.gz")), "collision published an archive")
                        self.assertEqual(source.read_bytes(), payload)
                    finally:
                        source.unlink()

    def test_inventory_unique_normalized_and_reproducible(self):
        self.write("plain.txt", b"approved working-tree change\n")
        self.write("approved new ē.txt", b"approved untracked input\n")
        self.write("fixture.gz", gzip.compress(b"legitimate compressed fixture", mtime=0))
        compressed = io.BytesIO()
        with zipfile.ZipFile(compressed, "w") as archive:
            archive.writestr("fixture.txt", "legitimate compressed fixture")
        self.write("fixture.zip", compressed.getvalue())
        self.write("ignored.log", b"ignored operational output\n")
        self.write("test-data/SOURCE_CHECKPOINT.json", b'{"nested":"test data"}\n')
        self.write("nested/SOURCE_CHECKPOINT.json/notes.txt", b"legitimate nested directory\n")
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
            generated = [member for member in members if member.name == prefix + "SOURCE_CHECKPOINT.json"]
            self.assertEqual(len(generated), 1)
            self.assertTrue(generated[0].isreg())
            self.assertFalse(any(name.startswith(prefix + "SOURCE_CHECKPOINT.json/") for name in names))
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
                self.assertEqual(member.mode, 0o755 if relative == "run.sh" else 0o644)
            for excluded in ("PKGBUILD", "docs/CURRENT_STATE.md", "docs/RELEASE_CHECKLIST.md",
                             "docs/POST_TASK_0045_CORRECTIVE_REPORT.md",
                             "build/generated.cpp", "build-old/generated.cpp", "removed.txt", "ignored.log"):
                self.assertNotIn(excluded, records)
            for included in ("plain.txt", "approved new ē.txt", "run.sh", "fixture.gz", "fixture.zip",
                             "docs/SOURCE_CHECKPOINT.json", "test-data/SOURCE_CHECKPOINT.json",
                             "nested/SOURCE_CHECKPOINT.json/notes.txt"):
                self.assertIn(included, records)
            extraction = self.base / "extracted"
            extraction.mkdir()
            exported.extractall(extraction, filter="data")
            extracted_root = extraction / prefix.removesuffix("/")
            generated_path = extracted_root / "SOURCE_CHECKPOINT.json"
            self.assertTrue(generated_path.is_file())
            self.assertEqual(generated_path.read_bytes(), metadata)
            self.assertEqual({path.relative_to(extracted_root).as_posix()
                              for path in extracted_root.rglob("*") if path.is_file()}, set(payloads))
            for relative, record in records.items():
                extracted = extracted_root / relative
                self.assertEqual(extracted.read_bytes(), payloads[relative])
                self.assertEqual(hashlib.sha256(extracted.read_bytes()).hexdigest(), record["sha256"])
                self.assertEqual(stat.S_IMODE(extracted.stat().st_mode), record["mode"])
        digest = hashlib.sha256(archive.read_bytes()).hexdigest()
        self.assertIn(f"sha256sums=('{digest}')", (output / "PKGBUILD").read_text())
        self.assertEqual((output / "SHA256SUMS").read_text(), f"{digest}  {archive.name}\n")

    def test_nested_checkpoint_directory_is_valid(self):
        self.git("rm", "--", "docs/SOURCE_CHECKPOINT.json")
        relative = "docs/SOURCE_CHECKPOINT.json/notes.txt"
        payload = b"ordinary nested same-name directory\n"
        self.write(relative, payload)
        self.git("add", relative)
        self.git("commit", "--quiet", "-m", "Legitimate nested directory control")
        result, output = self.export("nested-directory")
        self.assertEqual(result.returncode, 0, result.stderr)
        extraction = self.base / "extracted"
        extraction.mkdir()
        archive = next(output.glob("*.tar.gz"))
        with tarfile.open(archive) as exported:
            exported.extractall(extraction, filter="data")
        extracted_root = extraction / archive.name.removesuffix(".tar.gz")
        self.assertEqual((extracted_root / relative).read_bytes(), payload)
        self.assertEqual((extracted_root / "SOURCE_CHECKPOINT.json").read_bytes(),
                         (output / "SOURCE_CHECKPOINT.json").read_bytes())

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

    def verify_release(self, output, head, tag="fixture-release"):
        return subprocess.run([
            sys.executable, str(PROJECT / "tools/verify-tagged-arch-source.py"),
            str(output), "--source-root", str(self.root), "--tag", tag,
            "--expected-head", head], capture_output=True, text=True,
            env=self.environment)

    def test_release_guard_requires_clean_exact_annotated_tag(self):
        head = self.git("rev-parse", "HEAD").decode().strip()
        self.git("tag", "-a", "fixture-release", "-m", "Release fixture")
        result, output = self.export("tagged")
        self.assertEqual(result.returncode, 0, result.stderr)
        verified = self.verify_release(output, head)
        self.assertEqual(verified.returncode, 0, verified.stderr)
        wrong_head = self.verify_release(output, "0" * 40)
        self.assertNotEqual(wrong_head.returncode, 0)
        self.assertIn("expected HEAD", wrong_head.stderr)
        self.git("tag", "fixture-lightweight")
        lightweight = self.verify_release(output, head, "fixture-lightweight")
        self.assertNotEqual(lightweight.returncode, 0)
        self.assertIn("annotated", lightweight.stderr)
        other = self.git("commit-tree", "HEAD^{tree}", "-p", head,
                         "-m", "Wrong release target fixture").decode().strip()
        self.git("tag", "-a", "fixture-wrong-target", other, "-m", "Wrong target")
        wrong_target = self.verify_release(output, head, "fixture-wrong-target")
        self.assertNotEqual(wrong_target.returncode, 0)
        self.assertIn("expected HEAD", wrong_target.stderr)
        for relative in ("plain.txt", "untracked-source.txt"):
            with self.subTest(relative=relative):
                path = self.root / relative
                old = path.read_bytes() if path.exists() else None
                path.write_bytes(b"dirty release input\n")
                refused = self.verify_release(output, head)
                self.assertNotEqual(refused.returncode, 0)
                self.assertIn("must be clean", refused.stderr)
                self.assertEqual(path.read_bytes(), b"dirty release input\n")
                if old is None:
                    path.unlink()
                else:
                    path.write_bytes(old)

    def test_release_guard_rejects_precommit_checkpoint(self):
        self.write("plain.txt", b"approved release input\n")
        result, old_output = self.export("precommit")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.git("add", "plain.txt")
        self.git("commit", "--quiet", "-m", "Finalize release source")
        self.git("tag", "-a", "fixture-release", "-m", "Release fixture")
        head = self.git("rev-parse", "HEAD").decode().strip()
        refused = self.verify_release(old_output, head)
        self.assertNotEqual(refused.returncode, 0)
        self.assertIn("checkpoint HEAD/epoch", refused.stderr)
        result, corrected = self.export("committed")
        self.assertEqual(result.returncode, 0, result.stderr)
        verified = self.verify_release(corrected, head)
        self.assertEqual(verified.returncode, 0, verified.stderr)
        self.assertNotEqual(next(old_output.glob("*.tar.gz")).read_bytes(),
                            next(corrected.glob("*.tar.gz")).read_bytes())
        # An external receipt alone must not relabel an older archive.
        (old_output / "SOURCE_CHECKPOINT.json").write_bytes(
            (corrected / "SOURCE_CHECKPOINT.json").read_bytes())
        relabelled = self.verify_release(old_output, head)
        self.assertNotEqual(relabelled.returncode, 0)
        self.assertIn("archived and external checkpoints", relabelled.stderr)
        original = (self.root / "plain.txt").read_bytes()
        self.write("plain.txt", b"uncommitted input under the tagged HEAD\n")
        result, dirty_export = self.export("dirty-tagged-head")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.write("plain.txt", original)
        dirty_payload = self.verify_release(dirty_export, head)
        self.assertNotEqual(dirty_payload.returncode, 0)
        self.assertIn("source inventory bytes/modes differ from tag", dirty_payload.stderr)

    def test_tracked_modes_are_canonical_and_release_bytes_are_identical(self):
        self.git("tag", "-a", "fixture-release", "-m", "Release fixture")
        head = self.git("rev-parse", "HEAD").decode().strip()
        first = None
        for ordinary, executable in ((0o600, 0o700), (0o644, 0o755),
                                     (0o664, 0o775), (0o666, 0o777)):
            with self.subTest(ordinary=oct(ordinary), executable=oct(executable)):
                (self.root / "plain.txt").chmod(ordinary)
                (self.root / "run.sh").chmod(executable)
                self.assertEqual(self.git("status", "--porcelain=v1"), b"")
                result, output = self.export(f"modes-{ordinary}-{executable}")
                self.assertEqual(result.returncode, 0, result.stderr)
                actual = {p.name: p.read_bytes() for p in output.iterdir()}
                if first is None:
                    first = actual
                self.assertEqual(actual, first)
                checkpoint = json.loads(actual["SOURCE_CHECKPOINT.json"])
                records = {record["path"]: record for record in checkpoint["files"]}
                self.assertEqual(records["plain.txt"]["mode"], 0o644)
                self.assertEqual(records["run.sh"]["mode"], 0o755)
                archive = next(output.glob("*.tar.gz"))
                extraction = self.base / f"extracted-{ordinary}-{executable}"
                with tarfile.open(archive) as exported:
                    exported.extractall(extraction, filter="data")
                extracted = extraction / archive.name.removesuffix(".tar.gz")
                for relative, record in records.items():
                    self.assertEqual(stat.S_IMODE((extracted / relative).stat().st_mode),
                                     record["mode"])
                verified = self.verify_release(output, head)
                self.assertEqual(verified.returncode, 0, verified.stderr)

    def test_untracked_developer_modes_are_deterministic(self):
        plain = self.write("developer.txt", b"approved developer input\n")
        script = self.write("developer.sh", b"#!/bin/sh\nexit 0\n")
        first = None
        for ordinary, executable in ((0o600, 0o700), (0o644, 0o755),
                                     (0o664, 0o775), (0o666, 0o777)):
            with self.subTest(ordinary=oct(ordinary), executable=oct(executable)):
                plain.chmod(ordinary)
                script.chmod(executable)
                result, output = self.export(f"developer-modes-{ordinary}-{executable}")
                self.assertEqual(result.returncode, 0, result.stderr)
                actual = {p.name: p.read_bytes() for p in output.iterdir()}
                if first is None:
                    first = actual
                self.assertEqual(actual, first)
                records = {record["path"]: record for record in
                           json.loads(actual["SOURCE_CHECKPOINT.json"])["files"]}
                self.assertEqual(records["developer.txt"]["mode"], 0o644)
                self.assertEqual(records["developer.sh"]["mode"], 0o755)

    def test_release_guard_rejects_modified_generated_artifacts(self):
        self.git("tag", "-a", "fixture-release", "-m", "Release fixture")
        head = self.git("rev-parse", "HEAD").decode().strip()
        result, canonical = self.export("canonical")
        self.assertEqual(result.returncode, 0, result.stderr)
        valid = self.verify_release(canonical, head)
        self.assertEqual(valid.returncode, 0, valid.stderr)
        cases = ("build-command", "package-command", "dependency", "package-metadata",
                 "external-checkpoint", "archived-checkpoint", "source-uid-gid",
                 "source-uname-gname", "source-mode", "checkpoint-mode", "tar-epoch",
                 "gzip-epoch", "member-order", "extra-member", "missing-member",
                 "checksum-receipt", "archive-with-updated-receipts")
        for case in cases:
            with self.subTest(case=case):
                output = self.base / case
                shutil.copytree(canonical, output)
                archive = next(output.glob("*.tar.gz"))
                prefix = archive.name.removesuffix(".tar.gz") + "/"
                recipe = output / "PKGBUILD"
                if case in ("build-command", "package-command", "dependency", "package-metadata"):
                    old, new = {
                        "build-command": ("build() {", "build() {\n    echo MALICIOUS_SIDE_EFFECT"),
                        "package-command": ("package() {", "package() {\n    echo MODIFIED_PACKAGE"),
                        "dependency": ("'glibc'", "'glibc' 'unreviewed-dependency'"),
                        "package-metadata": ("pkgrel=", "pkgrel=9"),
                    }[case]
                    self.assertIn(old, recipe.read_text())
                    recipe.write_text(recipe.read_text().replace(old, new, 1))
                elif case == "external-checkpoint":
                    checkpoint = output / "SOURCE_CHECKPOINT.json"
                    checkpoint.write_bytes(checkpoint.read_bytes() + b"\n")
                elif case == "checksum-receipt":
                    sums = output / "SHA256SUMS"
                    sums.write_text(sums.read_text() + "# unauthorized receipt change\n")
                elif case == "gzip-epoch":
                    data = bytearray(archive.read_bytes())
                    data[4:8] = (int.from_bytes(data[4:8], "little") + 1).to_bytes(4, "little")
                    archive.write_bytes(data)
                elif case == "archive-with-updated-receipts":
                    archive.write_bytes(archive.read_bytes() + b"\0")
                else:
                    with tarfile.open(archive) as exported:
                        entries = [(copy.copy(member), exported.extractfile(member).read())
                                   for member in exported.getmembers()]
                    for index, (member, payload) in enumerate(entries):
                        if member.name == prefix + "plain.txt":
                            if case == "source-uid-gid":
                                member.uid, member.gid = 123, 456
                            elif case == "source-uname-gname":
                                member.uname = member.gname = "evil"
                            elif case == "source-mode":
                                member.mode = 0o666
                            elif case == "tar-epoch":
                                member.mtime += 1
                        if member.name == prefix + "SOURCE_CHECKPOINT.json":
                            if case == "checkpoint-mode":
                                member.mode = 0o600
                            elif case == "archived-checkpoint":
                                payload += b"\n"
                                member.size = len(payload)
                                entries[index] = (member, payload)
                    if case == "member-order":
                        entries.reverse()
                    elif case == "missing-member":
                        entries = [(member, payload) for member, payload in entries
                                   if member.name != prefix + "plain.txt"]
                    elif case == "extra-member":
                        extra = copy.copy(entries[0][0])
                        extra.name = prefix + "unauthorized.txt"
                        extra.size = 5
                        entries.append((extra, b"extra"))
                    epoch = json.loads((output / "SOURCE_CHECKPOINT.json").read_text())["source_date_epoch"]
                    with archive.open("wb") as raw:
                        with gzip.GzipFile(filename="", fileobj=raw, mode="wb", mtime=epoch) as gz:
                            with tarfile.open(fileobj=gz, mode="w", format=tarfile.PAX_FORMAT) as tar:
                                for member, payload in entries:
                                    tar.addfile(member, io.BytesIO(payload))
                # Repair every archive digest pin so changed-byte failures cannot
                # rely only on stale receipts. Never execute candidate recipes.
                if case not in ("external-checkpoint", "checksum-receipt"):
                    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
                    recipe.write_text(re.sub(r"^sha256sums=\('[0-9a-f]{64}'\)$",
                                             f"sha256sums=('{digest}')", recipe.read_text(), flags=re.M))
                    (output / "SHA256SUMS").write_text(f"{digest}  {archive.name}\n")
                refused = self.verify_release(output, head)
                self.assertNotEqual(refused.returncode, 0, refused.stdout)
                self.assertEqual(self.git("status", "--porcelain=v1"), b"")


if __name__ == "__main__":
    unittest.main(verbosity=2)
