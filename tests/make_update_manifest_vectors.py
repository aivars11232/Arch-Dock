#!/usr/bin/env python3
"""Writes tests/update-manifest-vectors/ (run: python3 tests/make_update_manifest_vectors.py
tests/update-manifest-vectors): the bytes a reader sees once the
signature has verified, and whether it must ACCEPT or REFUSE them."""
import copy
import json
import sys
from pathlib import Path

out = Path(sys.argv[1])
out.mkdir(parents=True, exist_ok=True)

VALID = {
    "format": 1,
    "product": "arch-dock",
    "channel": "stable",
    "version": "0.1.1-56",
    "release_tag": "v0.1.1-56",
    "source_commit": "0123456789abcdef0123456789abcdef01234567",
    "published_at": "2026-12-03T12:00:00Z",
    "release_notes": "Panel Studio opens faster.\nThe bars no longer flicker after a screen change.",
    "minimum_supported_version": "0.1.1-55",
    "signing_key_id": "arch-dock-update-1",
    "package": {
        "file": "arch-dock-0.1.1-56-x86_64.pkg.tar.zst",
        "size": 34127137,
        "sha256": "5f0c2b4f0d6d4b6a8e1e3c7a9b2d4f6e8a0c2e4f6a8b0d2f4e6a8c0e2a4c6e8a",
        "arch": "x86_64",
        "needs": ["glibc", "qt6-base>=6.12", "qt6-declarative>=6.12", "libplasma", "polkit"],
        "required_updater_version": 1,
    },
}

cases = []  # (name, bytes, verdict, comment)


def text(value):
    return (json.dumps(value, indent=2, ensure_ascii=False) + "\n").encode("utf-8")


def changed(name, verdict, change, comment=None):
    value = copy.deepcopy(VALID)
    change(value)
    cases.append((name, text(value), verdict, comment))


def raw(name, verdict, data, comment=None):
    cases.append((name, data, verdict, comment))


def with_version(value, version):
    value["version"] = version
    value["release_tag"] = "v" + version
    value["package"]["file"] = "arch-dock-" + version + "-x86_64.pkg.tar.zst"


valid = text(VALID)
raw("valid", "ACCEPT", valid)
raw("invalid-utf8", "REFUSE", valid.replace(b"opens faster", b"opens \xff faster"))
raw("utf8-bom", "REFUSE", b"\xef\xbb\xbf" + valid)
changed("unknown-field", "REFUSE", lambda v: v.update(homepage="https://example.org"))
changed("unknown-package-field", "REFUSE", lambda v: v["package"].update(signature="abc"))
changed("missing-field", "REFUSE", lambda v: v.pop("channel"))
changed("missing-package-field", "REFUSE", lambda v: v["package"].pop("arch"))
changed("malformed-digest", "REFUSE", lambda v: v["package"].update(sha256=v["package"]["sha256"].upper()))
changed("short-digest", "REFUSE", lambda v: v["package"].update(sha256=v["package"]["sha256"][:63]))
changed("oversized-package", "REFUSE", lambda v: v["package"].update(size=256 * 1024 * 1024 + 1))
changed("largest-package", "ACCEPT", lambda v: v["package"].update(size=256 * 1024 * 1024))
changed("undersized-package", "REFUSE", lambda v: v["package"].update(size=1024 * 1024 - 1))
changed("smallest-package", "ACCEPT", lambda v: v["package"].update(size=1024 * 1024))
changed("malformed-version", "REFUSE", lambda v: with_version(v, "0.1.1"))
changed("version-with-letters", "REFUSE", lambda v: with_version(v, "0.1.1a-56"))
changed("wrong-product", "REFUSE", lambda v: v.update(product="lcl"))
changed("wrong-channel", "REFUSE", lambda v: v.update(channel="beta"))
changed("wrong-format", "REFUSE", lambda v: v.update(format=2))
changed("mismatched-release-tag", "REFUSE", lambda v: v.update(release_tag="v0.1.1-57"))
changed("mismatched-file", "REFUSE", lambda v: v["package"].update(file="arch-dock-0.1.1-57-x86_64.pkg.tar.zst"))
changed("minimum-newer-than-release", "REFUSE", lambda v: v.update(minimum_supported_version="0.1.1-57"))
changed("minimum-equal-to-release", "ACCEPT", lambda v: v.update(minimum_supported_version="0.1.1-56"))
changed("malformed-minimum", "REFUSE", lambda v: v.update(minimum_supported_version="0.1.1"))
changed("future-updater-version", "ACCEPT", lambda v: v["package"].update(required_updater_version=2),
        "well formed, for a newer updater: the button says it must be installed by hand, a later check")
changed("bad-updater-version", "REFUSE", lambda v: v["package"].update(required_updater_version=0))
changed("overflowing-updater-version", "REFUSE", lambda v: v["package"].update(required_updater_version=2147483648))
changed("largest-updater-version", "ACCEPT", lambda v: v["package"].update(required_updater_version=2147483647),
        "well formed, for a newer updater: install by hand, a later check")
changed("negative-size", "REFUSE", lambda v: v["package"].update(size=-34127137))
changed("text-for-a-number", "REFUSE", lambda v: v["package"].update(size="34127137"))
changed("true-for-a-number", "REFUSE", lambda v: v["package"].update(required_updater_version=True))
changed("number-for-a-text", "REFUSE", lambda v: v.update(channel=1))
raw("leading-zero-number", "REFUSE", valid.replace(b'"size": 34127137', b'"size": 034127137'))
raw("plus-signed-number", "REFUSE", valid.replace(b'"size": 34127137', b'"size": +34127137'))
raw("whole-number-written-as-decimal", "ACCEPT", valid.replace(b'"size": 34127137', b'"size": 34127137.0'),
    "JSON numbers are read by value: 34127137.0 is the number 34127137")
raw("fractional-updater-version", "REFUSE",
    valid.replace(b'"required_updater_version": 1', b'"required_updater_version": 1.5'))
raw("bare-word-value", "REFUSE", valid.replace(b'"channel": "stable"', b'"channel": stable'))
raw("lone-surrogate", "REFUSE", valid.replace(b"opens faster", b"opens \\ud800 faster"))
raw("trailing-content", "REFUSE", valid + b"{}\n")
raw("top-level-array", "REFUSE", b"[\n" + valid + b"]\n")
raw("top-level-text", "REFUSE", b'"arch-dock"\n')
raw("empty", "REFUSE", b"")
changed("notes-too-long", "REFUSE", lambda v: v.update(release_notes="a" * 20001))
changed("astral-notes-at-limit", "ACCEPT", lambda v: v.update(release_notes="\U0001F600" * 10000 + "a" * 10000),
        "20000 characters, half of them outside the Basic Multilingual Plane")
changed("oversized-manifest", "REFUSE", lambda v: v.update(release_notes="\U0001F600" * 20000),
        "20000 characters, but more than 64 KiB of UTF-8")
changed("malformed-source-commit", "REFUSE", lambda v: v.update(source_commit="0123456789abcdef0123456789abcdef0123456"))
changed("malformed-published-at", "REFUSE", lambda v: v.update(published_at="2026-12-03 12:00:00"))
changed("published-at-with-offset", "REFUSE", lambda v: v.update(published_at="2026-12-03T12:00:00+01:00"))
changed("malformed-key-id", "REFUSE", lambda v: v.update(signing_key_id="Arch Dock"))
changed("wrong-arch", "REFUSE", lambda v: v["package"].update(arch="aarch64"))
# Arch Dock's own cases.
changed("epoch-in-version", "REFUSE", lambda v: with_version(v, "1:0.1.1-56"))
changed("subrelease", "REFUSE", lambda v: with_version(v, "0.1.1-56.1"),
        "a subrelease, which only a TEST build reads")
changed("file-with-path", "REFUSE", lambda v: v["package"].update(file="../arch-dock-0.1.1-56-x86_64.pkg.tar.zst"))
changed("trailing-newline-after-version", "REFUSE", lambda v: with_version(v, "0.1.1-56\n"))
changed("trailing-newline-after-file", "REFUSE",
        lambda v: v["package"].update(file="arch-dock-0.1.1-56-x86_64.pkg.tar.zst\n"))
changed("trailing-newline-after-need", "REFUSE", lambda v: v["package"].update(needs=["glibc\n"]))
raw("escaped-key-beside-plain", "REFUSE",
    valid.replace(b'  "version": "0.1.1-56",', b'  "version": "0.1.1-56",\n  "\\u0076ersion": "0.1.1-56",'))
raw("escaped-key", "REFUSE", valid.replace(b'"version": "0.1.1-56"', b'"\\u0076ersion": "0.1.1-56"'))
raw("duplicate-key", "REFUSE",
    valid.replace(b'  "channel": "stable",', b'  "channel": "stable",\n  "channel": "stable",'))
raw("duplicate-package-key", "REFUSE",
    valid.replace(b'    "arch": "x86_64",', b'    "arch": "x86_64",\n    "arch": "x86_64",'))
# Strict JSON has no raw control character inside a text; the escaped \\n
# between the notes' lines in every vector stays fine.
raw("raw-line-feed-in-notes", "REFUSE", valid.replace(b"opens faster.\\n", b"opens faster.\n"),
    "a line feed written raw inside the notes")
raw("raw-tab-in-notes", "REFUSE", valid.replace(b"Panel Studio opens", b"Panel\tStudio opens"),
    "a tab written raw inside the notes")
raw("raw-nul-in-notes", "REFUSE", valid.replace(b"Panel Studio opens", b"Panel\x00Studio opens"),
    "a NUL written raw inside the notes")
changed("needs-in-pacman-forms", "ACCEPT", lambda v: v["package"].update(needs=[
    "glibc", "qt6-base>=6.12", "libFLAC.so=14-64", "libPlasma.so=8-64", "pipewire=1:1.6.11-2",
    "foo>1.0+r12.gabc-1.2", "bar<2", "baz<=3.0", "qux=4.1"]))
changed("no-needs", "ACCEPT", lambda v: v["package"].update(needs=[]))
changed("too-many-needs", "REFUSE", lambda v: v["package"].update(needs=[f"need{i}" for i in range(65)]))
changed("most-needs", "ACCEPT", lambda v: v["package"].update(needs=[f"need{i}" for i in range(64)]))
changed("needs-not-a-list", "REFUSE", lambda v: v["package"].update(needs="glibc"))
changed("need-not-a-text", "REFUSE", lambda v: v["package"].update(needs=[1]))
for label, need in (("leading-dash", "-foo"), ("leading-dot", ".foo"), ("no-version", "foo>="),
                    ("empty-epoch", "foo>=:1"), ("letter-epoch", "foo>=x:1"), ("two-releases", "foo>=1-2-3"),
                    ("slash", "foo>=1/2"), ("reversed-operator", "foo=>1"), ("letter-release", "foo>=1-x"),
                    ("space", "foo bar"), ("non-ascii-name", "föo"), ("shell-character", "foo;rm"),
                    ("long-name", "a" * 129)):
    changed("need-" + label, "REFUSE", lambda v, need=need: v["package"].update(needs=["glibc", need]))
changed("longest-need-name", "ACCEPT", lambda v: v["package"].update(needs=["a" * 128]))
# Digits outside 0-9 (here Arabic-Indic), which a reader's \\d must not take.
changed("version-with-non-ascii-digits", "REFUSE", lambda v: with_version(v, "0.1.1-\u0665\u0666"))
changed("minimum-with-non-ascii-digits", "REFUSE", lambda v: v.update(minimum_supported_version="0.1.1-\u0665\u0665"))
changed("published-at-with-non-ascii-digits", "REFUSE", lambda v: v.update(published_at="2026-12-0\u0663T12:00:00Z"))
changed("need-non-ascii-epoch", "REFUSE", lambda v: v["package"].update(needs=["glibc", "foo>=\u0661:1"]))

lines = ["# Update manifest vectors: the bytes a reader sees once the signature has",
         "# verified, and whether it must ACCEPT or REFUSE them. Arch Dock's C++ reader",
         "# (tests/UpdateCoreTest.cpp) and tools/release-files.py (tests/test_release_files.py)",
         "# read every file here and must both reach the result listed; a file that is",
         "# not listed fails both. A comment line explains the case below it.",
         "#",
         "# <file> <ACCEPT|REFUSE>"]
names = set()
for name, data, verdict, comment in cases:
    assert name not in names, name
    names.add(name)
    (out / f"{name}.json").write_bytes(data)
    if comment:
        lines.append(f"# {comment}")
    lines.append(f"{name}.json {verdict}")
(out / "expected.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")
print(f"{len(cases)} vectors")
