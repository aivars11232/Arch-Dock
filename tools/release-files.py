#!/usr/bin/env python3
"""Arch Dock's release files: what the Update button reads for a release.

    release-files.py make <package> --changelog CHANGELOG.md --pkgbuild PKGBUILD
        --checkpoint SOURCE_CHECKPOINT.json --key-file <key folder>/arch-dock-update-1.key
        --out DIR [--first-update-release]
    release-files.py verify DIR
    release-files.py check-published <tag>

`make` writes update-manifest.json (format 1: the release's version, notes
and package, as Arch Dock's Update button reads it), update-manifest.sig
(an ECDSA P-256 SHA-256 signature over its exact bytes, made with the owner's
key by openssl), <package>.sha256 and notes.md. It refuses a package that any
installed Arch Dock's helper would refuse, and checks the signature three ways
before it writes anything. Only openssl ever opens the key file: this tool
looks at its place and mode, and never prints or copies it.

`verify` checks a folder of release files again. `check-published` checks,
without credentials, what the Update button sees of a published release.

With `--test-key`, outputs are marked NOT FOR PUBLICATION (tests and
rehearsals only).
"""
import argparse
import base64
import datetime
import gzip
import hashlib
import json
import os
from pathlib import Path
import re
import stat
import subprocess
import sys
import tarfile
import tempfile
import urllib.error
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
OPENSSL = "/usr/bin/openssl"
GIT = "/usr/bin/git"

FORMAT = 1
PRODUCT = "arch-dock"
CHANNEL = "stable"
UPDATER_PROTOCOL = 1
MINIMUM_SUPPORTED_VERSION = "0.1.1-55"
# The first update release names any earlier release as its minimum. No older
# Arch Dock has the Update button, so this changes nothing for anyone; it lets
# the packaged helper's own check run on the PC that signs it, which still
# runs an older release until the first one is installed by hand.
FIRST_RELEASE_MINIMUM = "0.1.1-1"
MANIFEST_NAME = "update-manifest.json"
SIGNATURE_NAME = "update-manifest.sig"
MAX_MANIFEST_BYTES = 64 * 1024
MAX_SIGNATURE_BYTES = 1024
MIN_PACKAGE_BYTES = 1024 * 1024
MAX_PACKAGE_BYTES = 256 * 1024 * 1024
MAX_NOTES = 20000
MAX_NEEDS = 64
MAX_UPDATER_VERSION = 2147483647
GITHUB_API_VERSION = "2026-03-10"
USER_AGENT = "ArchDock-release-files"
P256_SPKI_PREFIX = bytes.fromhex("3059301306072a8648ce3d020106082a8648ce3d030107034200")

TOP_KEYS = ("format", "product", "channel", "version", "release_tag", "source_commit", "published_at",
            "release_notes", "minimum_supported_version", "signing_key_id", "package")
PACKAGE_KEYS = ("file", "size", "sha256", "arch", "needs", "required_updater_version")

# Every rule matches the whole value (re.fullmatch): a plain ^...$ would let a
# trailing newline through. re.ASCII keeps \d to 0-9, as in Qt's reader.
RELEASE_VERSION = re.compile(r"\d+(?:\.\d+)*-\d+", re.ASCII)
TEST_VERSION = re.compile(r"\d+(?:\.\d+)*-\d+(?:\.\d+)?", re.ASCII)
HEX40 = re.compile(r"[0-9a-f]{40}", re.ASCII)
HEX64 = re.compile(r"[0-9a-f]{64}", re.ASCII)
TIMESTAMP = re.compile(r"\d{4}-(?:0[1-9]|1[0-2])-(?:0[1-9]|[12]\d|3[01])T(?:[01]\d|2[0-3]):[0-5]\d:[0-5]\dZ", re.ASCII)
KEY_ID = re.compile(r"[a-z0-9-]{1,64}", re.ASCII)
# pacman's dependency form, as makepkg checks it, without the characters at
# which pacman's own reader would split differently: a name, then optionally
# an operator and a version ([epoch:]version[-release]).
NEED = re.compile(r"[A-Za-z0-9@_+][A-Za-z0-9@._+-]{0,127}"
                  r"(?:(?:<=|>=|<|=|>)(?=.{1,128}\Z)(?:\d+:)?[!-,.0-9;?-~]+(?:-\d+(?:\.\d+)?)?)?", re.ASCII)


class ReleaseError(Exception):
    """A release file or input that must not be used; the message says why."""


def fail(message):
    raise ReleaseError(message)


# --- version order: pacman's alpm_pkg_vercmp (libalpm/version.c), ported ---

def _rpmvercmp(a, b):
    if a == b:
        return 0
    one, two = 0, 0
    ptr1, ptr2 = 0, 0
    while one < len(a) and two < len(b):
        while one < len(a) and not (a[one].isascii() and a[one].isalnum()):
            one += 1
        while two < len(b) and not (b[two].isascii() and b[two].isalnum()):
            two += 1
        if not (one < len(a) and two < len(b)):
            break
        if (one - ptr1) != (two - ptr2):
            return -1 if (one - ptr1) < (two - ptr2) else 1
        ptr1, ptr2 = one, two
        if a[ptr1].isdigit():
            while ptr1 < len(a) and a[ptr1].isascii() and a[ptr1].isdigit():
                ptr1 += 1
            while ptr2 < len(b) and b[ptr2].isascii() and b[ptr2].isdigit():
                ptr2 += 1
            number = True
        else:
            while ptr1 < len(a) and a[ptr1].isascii() and a[ptr1].isalpha():
                ptr1 += 1
            while ptr2 < len(b) and b[ptr2].isascii() and b[ptr2].isalpha():
                ptr2 += 1
            number = False
        segment1, segment2 = a[one:ptr1], b[two:ptr2]
        if not segment1:
            return -1
        if not segment2:
            return 1 if number else -1
        if number:
            segment1, segment2 = segment1.lstrip("0"), segment2.lstrip("0")
            if len(segment1) != len(segment2):
                return 1 if len(segment1) > len(segment2) else -1
        if segment1 != segment2:
            return -1 if segment1 < segment2 else 1
        one, two = ptr1, ptr2
    if one >= len(a) and two >= len(b):
        return 0
    rest1 = a[one] if one < len(a) else ""
    rest2 = b[two] if two < len(b) else ""
    if (not rest1 and not (rest2.isascii() and rest2.isalpha())) or (rest1.isascii() and rest1.isalpha()):
        return -1
    return 1


def _parse_evr(text):
    index = 0
    while index < len(text) and text[index].isascii() and text[index].isdigit():
        index += 1
    release_at = text.rfind("-", index)
    if index < len(text) and text[index] == ":":
        epoch = text[:index] or "0"
        version_start = index + 1
    else:
        epoch, version_start = "0", 0
    if release_at >= 0:
        return epoch, text[version_start:release_at], text[release_at + 1:]
    return epoch, text[version_start:], None


def vercmp(a, b):
    """pacman's order of two versions: -1, 0 or 1."""
    if a == b:
        return 0
    epoch1, version1, release1 = _parse_evr(a)
    epoch2, version2, release2 = _parse_evr(b)
    result = _rpmvercmp(epoch1, epoch2)
    if result == 0:
        result = _rpmvercmp(version1, version2)
        if result == 0 and release1 is not None and release2 is not None:
            result = _rpmvercmp(release1, release2)
    return result


# --- the manifest, format 1 ---

def scan_keys(text):
    """Refuses JSON whose objects name one key twice or spell a key with a
    backslash escape. Python's reader keeps the last of two equal keys and
    decodes escapes, so the text itself is scanned, as Arch Dock's C++ reader
    does."""
    stack = []
    expect_key = False
    index, length = 0, len(text)
    while index < length:
        char = text[index]
        if char == '"':
            end = index + 1
            while end < length and text[end] != '"':
                end += 2 if text[end] == "\\" else 1
            raw = text[index + 1:end]
            if expect_key and stack and stack[-1] is not None:
                if "\\" in raw:
                    fail(f"the manifest spells a key with an escape: {raw!r}")
                if raw in stack[-1]:
                    fail(f"the manifest names the key {raw!r} twice")
                stack[-1].add(raw)
                expect_key = False
            index = end + 1
            continue
        if char == "{":
            stack.append(set())
            expect_key = True
        elif char == "[":
            stack.append(None)
            expect_key = False
        elif char in "}]":
            if stack:
                stack.pop()
            expect_key = False
        elif char == ",":
            expect_key = bool(stack) and stack[-1] is not None
        elif char == ":":
            expect_key = False
        index += 1


def _refuse_constant(name):
    fail(f"the manifest holds {name}, which is not JSON")


def _check_text(value):
    """Every text must be valid UTF-8 (no lone surrogates)."""
    if isinstance(value, str):
        try:
            value.encode("utf-8")
        except UnicodeEncodeError:
            fail("the manifest holds a text that is not valid Unicode")
    elif isinstance(value, dict):
        for item in value.values():
            _check_text(item)
    elif isinstance(value, list):
        for item in value:
            _check_text(item)


def _members(value, keys, what):
    if not isinstance(value, dict):
        fail(f"{what} is not an object")
    if set(value) != set(keys):
        missing = sorted(set(keys) - set(value))
        extra = sorted(set(value) - set(keys))
        fail(f"{what} has missing keys {missing} or unknown keys {extra}")
    return value


def _text(value, key):
    if not isinstance(value, str):
        fail(f"{key} is not a text")
    return value


def _whole(value, key, low, high):
    """A JSON number read by value: 4.0 is the number 4. Never true or false."""
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        fail(f"{key} is not a number")
    if isinstance(value, float):
        if not value.is_integer():
            fail(f"{key} is not a whole number")
        value = int(value)
    if not low <= value <= high:
        fail(f"{key} {value} is outside {low} to {high}")
    return value


def read_manifest(data, test_build=False):
    """The manifest as a dict, once every reading rule holds; otherwise ReleaseError."""
    if not isinstance(data, (bytes, bytearray)):
        fail("the manifest is not bytes")
    if len(data) > MAX_MANIFEST_BYTES:
        fail(f"the manifest has {len(data)} bytes, more than {MAX_MANIFEST_BYTES}")
    try:
        text = bytes(data).decode("utf-8", errors="strict")
    except UnicodeDecodeError:
        fail("the manifest is not valid UTF-8")
    scan_keys(text)
    try:
        value = json.loads(text, parse_constant=_refuse_constant)
    except json.JSONDecodeError as error:
        fail(f"the manifest is not JSON: {error}")
    _check_text(value)
    manifest = _members(value, TOP_KEYS, "the manifest")
    package = _members(manifest["package"], PACKAGE_KEYS, "the package")

    if _whole(manifest["format"], "format", 0, MAX_UPDATER_VERSION) != FORMAT:
        fail(f"the manifest has format {manifest['format']}, not {FORMAT}")
    if _text(manifest["product"], "product") != PRODUCT:
        fail(f"the manifest is for {manifest['product']!r}, not {PRODUCT}")
    if _text(manifest["channel"], "channel") != CHANNEL:
        fail(f"the manifest is for the {manifest['channel']!r} channel")
    version_form = TEST_VERSION if test_build else RELEASE_VERSION
    version = _text(manifest["version"], "version")
    if not version_form.fullmatch(version):
        fail(f"the version {version!r} is not in the form 0.1.1-56")
    if _text(manifest["release_tag"], "release_tag") != "v" + version:
        fail(f"the release tag {manifest['release_tag']!r} is not v{version}")
    if not HEX40.fullmatch(_text(manifest["source_commit"], "source_commit")):
        fail("the source commit is not 40 lowercase hex digits")
    if not TIMESTAMP.fullmatch(_text(manifest["published_at"], "published_at")):
        fail("published_at is not in the form 2026-12-03T12:00:00Z")
    if len(_text(manifest["release_notes"], "release_notes")) > MAX_NOTES:
        fail(f"the release notes are longer than {MAX_NOTES} characters")
    minimum = _text(manifest["minimum_supported_version"], "minimum_supported_version")
    if not version_form.fullmatch(minimum):
        fail(f"the minimum supported version {minimum!r} is not in the form 0.1.1-55")
    if vercmp(minimum, version) > 0:
        fail(f"the minimum supported version {minimum} is newer than the release {version}")
    if not KEY_ID.fullmatch(_text(manifest["signing_key_id"], "signing_key_id")):
        fail("the signing key id is not 1 to 64 of a-z, 0-9 and -")

    if _text(package["file"], "file") != f"arch-dock-{version}-x86_64.pkg.tar.zst":
        fail(f"the package file {package['file']!r} does not match the version")
    _whole(package["size"], "size", MIN_PACKAGE_BYTES, MAX_PACKAGE_BYTES)
    if not HEX64.fullmatch(_text(package["sha256"], "sha256")):
        fail("the package's sha256 is not 64 lowercase hex digits")
    if _text(package["arch"], "arch") != "x86_64":
        fail(f"the package is for {package['arch']!r}, not x86_64")
    needs = package["needs"]
    if not isinstance(needs, list) or len(needs) > MAX_NEEDS:
        fail(f"needs is not a list of at most {MAX_NEEDS} entries")
    for need in needs:
        if not isinstance(need, str) or not NEED.fullmatch(need):
            fail(f"the need {need!r} is not in pacman's dependency form")
    _whole(package["required_updater_version"], "required_updater_version", 1, MAX_UPDATER_VERSION)
    manifest["package"]["size"] = int(package["size"])
    manifest["package"]["required_updater_version"] = int(package["required_updater_version"])
    return manifest


# --- keys and signatures (through the openssl command) ---

def read_key_list(text, where):
    """`<id> <hex SPKI>` lines with # comments, as data/update-keys.txt; at most one key."""
    keys = []
    for line in text.splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        fields = line.split()
        if len(fields) != 2:
            fail(f"{where}: this line is not <key id> <hex>: {line}")
        key_id, key_hex = fields[0], fields[1].lower()
        if not KEY_ID.fullmatch(key_id):
            fail(f"{where}: {key_id!r} is not a key id")
        if not re.fullmatch(r"[0-9a-f]{182}", key_hex):
            fail(f"{where}: key {key_id} is not an ECDSA P-256 public key")
        spki = bytes.fromhex(key_hex)
        if not spki.startswith(P256_SPKI_PREFIX) or spki[len(P256_SPKI_PREFIX)] != 0x04:
            fail(f"{where}: key {key_id} is not an ECDSA P-256 public key")
        if any(existing[0] == key_id for existing in keys):
            fail(f"{where}: key {key_id} is listed twice")
        keys.append((key_id, spki))
    if len(keys) > 1:
        fail(f"{where} lists {len(keys)} update keys; Arch Dock's Update button trusts exactly one")
    return keys


def fingerprint(spki):
    return hashlib.sha256(spki).hexdigest()


def pem(spki):
    body = base64.b64encode(spki).decode("ascii")
    lines = [body[i:i + 64] for i in range(0, len(body), 64)]
    return "-----BEGIN PUBLIC KEY-----\n" + "\n".join(lines) + "\n-----END PUBLIC KEY-----\n"


def openssl(*arguments, input_bytes=None):
    return subprocess.run((OPENSSL, *arguments), input=input_bytes, capture_output=True,
                          env={"PATH": "/usr/bin", "LC_ALL": "C"},
                          stdin=None if input_bytes is not None else subprocess.DEVNULL)


def signature_verifies(spki, manifest_bytes, signature_bytes):
    """Whether openssl says the signature over the manifest's exact bytes verifies with this key."""
    if not signature_bytes or len(signature_bytes) > MAX_SIGNATURE_BYTES:
        return False
    with tempfile.TemporaryDirectory(prefix="archdock-verify-") as folder:
        folder = Path(folder)
        (folder / "key.pem").write_text(pem(spki), encoding="ascii")
        (folder / "manifest").write_bytes(manifest_bytes)
        (folder / "signature").write_bytes(signature_bytes)
        result = openssl("dgst", "-sha256", "-verify", str(folder / "key.pem"),
                         "-signature", str(folder / "signature"), str(folder / "manifest"))
        return result.returncode == 0 and result.stdout == b"Verified OK\n"


def public_half(key_file):
    """The DER SubjectPublicKeyInfo of a private key file, derived by openssl."""
    result = openssl("pkey", "-in", str(key_file), "-pubout", "-outform", "DER")
    if result.returncode != 0 or not result.stdout.startswith(P256_SPKI_PREFIX):
        fail(f"openssl could not read an ECDSA P-256 key from {key_file}")
    return result.stdout


def sign(key_file, manifest_path, signature_path):
    result = openssl("dgst", "-sha256", "-sign", str(key_file), "-out", str(signature_path), str(manifest_path))
    if result.returncode != 0:
        fail("openssl could not sign the manifest: " + result.stderr.decode(errors="replace").strip())


def temporary_places():
    """The places that are wiped: a key there would be lost."""
    runtime = os.environ.get("XDG_RUNTIME_DIR") or f"/run/user/{os.getuid()}"
    return [Path("/tmp"), Path("/var/tmp"), Path("/dev/shm"), Path(runtime)]


def check_place(path, avoid=()):
    """A key's place: never in or below a git work tree, one of `avoid` (the
    build and output folders) or a temporary place, where it could be
    committed, packed into a build or wiped. Paths are compared by their
    resolved parts, so "/mnt/F/Arch Dock key" is not inside "/mnt/F/Arch Dock".
    Nothing is opened or listed: only `<folder>/.git` is looked for."""
    resolved = Path(os.path.realpath(path))
    for folder in (resolved.parent, *resolved.parent.parents):
        if (folder / ".git").exists():
            fail(f"{path} lies in a git work tree ({folder}); keep the key in a folder outside any repository")
    for place, why in [(Path(os.path.realpath(folder)), what) for folder, what in avoid] + [
            (Path(os.path.realpath(folder)), "a folder that is wiped") for folder in temporary_places()]:
        if resolved == place or resolved.is_relative_to(place):
            fail(f"{path} lies in {why} ({place}); keep the key in a folder of its own")
    return resolved


def check_key_file(key_file, pkgbuild, out):
    """The key file make signs with: its place, and a mode that keeps it to the owner's account."""
    resolved = check_place(key_file, [(Path(pkgbuild).parent, "the build folder"), (out, "the output folder")])
    if not resolved.is_file():
        fail(f"the key file {key_file} does not exist")
    mode = stat.S_IMODE(os.stat(resolved).st_mode)
    if mode & 0o077:
        fail(f"other accounts on this PC can use the key file (mode {mode:o}); run: chmod 600 '{resolved}'")
    return resolved


# --- the package ---

def _pkginfo(text):
    values = {}
    for line in text.splitlines():
        if not line or line.startswith("#"):
            continue
        key, separator, value = line.partition(" = ")
        if not separator:
            fail(f".PKGINFO line {line!r} is not 'key = value'")
        values.setdefault(key, []).append(value)
    return values


def _mtree_entries(data):
    """The paths .MTREE lists (gzip-compressed mtree text). mtree writes every
    byte outside plain ASCII as \\ooo, so the escapes are bytes, decoded as
    UTF-8 only once they are put back together, as tarfile decodes names."""
    try:
        raw = gzip.decompress(data)
    except OSError:
        fail(".MTREE cannot be read")
    paths = set()
    for line in raw.splitlines():
        if not line or line.startswith(b"#") or line.startswith(b"/"):
            continue
        path = line.split(b" ", 1)[0]
        if path.startswith(b"./"):
            path = path[2:]
        path = re.sub(rb"\\([0-7]{3})", lambda m: bytes([int(m.group(1), 8)]), path)
        paths.add(path.decode("utf-8", "surrogateescape").rstrip("/"))
    return paths


def _path_parts(name):
    """The parts of an entry's path, or None when it is absolute, starts
    with "./", or has an empty, "." or ".." part (the helper's rule)."""
    if not name or name.startswith("/") or name.startswith("./"):
        return None
    parts = name.split("/")
    if any(part in ("", ".", "..") for part in parts):
        return None
    return parts


def _link_stays_in_usr(parts, target):
    """Whether a symlink at `parts` pointing at `target` stays inside usr/."""
    if not target:
        return False
    resolved = [] if target.startswith("/") else parts[:-1]
    for part in target.split("/"):
        if part in ("", "."):
            continue
        if part == "..":
            if not resolved:
                return False
            resolved.pop()
            continue
        resolved.append(part)
    return bool(resolved) and resolved[0] == "usr"


def read_package(path):
    """What a package says about itself, after the helper's content rules:
    a tar archive compressed with zstd; every entry owned by root with no
    group or other write bit, no xattrs or ACLs; no path twice, no absolute
    path, no "./" start and no empty, "." or ".." part; one pkgname, pkgbase,
    pkgver and arch; .MTREE listing exactly the entries; of the "." files
    only .PKGINFO, .BUILDINFO and .MTREE, as files; everything else under
    usr/, and links pointing inside usr/."""
    path = Path(path)
    with path.open("rb") as stream:
        if stream.read(4) != b"\x28\xb5\x2f\xfd":
            fail(f"{path.name} is not compressed with zstd")
    seen = set()
    files = {}
    try:
        archive = tarfile.open(path, mode="r:zst")
    except (tarfile.TarError, OSError) as error:
        fail(f"{path.name} is not a tar archive compressed with zstd: {error}")
    with archive:
        for member in archive:
            name = member.name.rstrip("/") if member.isdir() else member.name
            parts = _path_parts(name)
            if parts is None:
                fail(f"{path.name} holds the path {member.name!r}")
            if name in seen:
                fail(f"{path.name} lists {name} twice")
            seen.add(name)
            if member.uid != 0 or member.gid != 0:
                fail(f"{name} is not owned by root")
            if member.mode & 0o022 and not member.issym():
                fail(f"{name} can be written by group or others")
            if member.mode & (stat.S_ISUID | stat.S_ISGID) or not (member.isfile() or member.isdir() or member.issym()):
                fail(f"{name} is a setuid, setgid or special file")
            if any(key.startswith(("SCHILY.xattr.", "SCHILY.acl.", "LIBARCHIVE.xattr.")) for key in member.pax_headers):
                fail(f"{name} carries xattrs or ACLs")
            if name.startswith("."):
                if name == ".INSTALL":
                    fail(f"{path.name} has an install script (.INSTALL)")
                if name not in (".PKGINFO", ".BUILDINFO", ".MTREE"):
                    fail(f"{path.name} holds {name} outside usr/")
                if not member.isfile():
                    fail(f"{name} is not a file")
                files[name] = archive.extractfile(member).read()
                continue
            if parts[0] != "usr":
                fail(f"{name} lies outside usr/")
            if member.issym() and not _link_stays_in_usr(parts, member.linkname):
                fail(f"the link {name} points outside usr/")
    for needed in (".PKGINFO", ".BUILDINFO", ".MTREE"):
        if needed not in files:
            fail(f"{path.name} has no {needed}")
    info = _pkginfo(files[".PKGINFO"].decode("utf-8"))
    for key in ("pkgname", "pkgbase", "pkgver", "arch"):
        if len(info.get(key, [])) != 1:
            fail(f".PKGINFO names {key} {len(info.get(key, []))} times, not once")
    listed = _mtree_entries(files[".MTREE"])
    entries = {name for name in seen if not name.startswith(".")} | {".PKGINFO", ".BUILDINFO"}
    if listed != entries:
        fail(f".MTREE does not list exactly the package's entries "
             f"({len(listed - entries)} listed only there, {len(entries - listed)} missing)")
    buildinfo = _pkginfo(files[".BUILDINFO"].decode("utf-8"))
    return {"name": info["pkgname"][0], "version": info["pkgver"][0], "arch": info["arch"][0],
            "depends": info.get("depend", []), "pkgbuild_sha256": (buildinfo.get("pkgbuild_sha256sum") or [""])[0],
            "entries": seen}


# --- the release notes ---

def changelog_section(changelog, version):
    """The text under "## <version> — <date>", up to the next "## " heading."""
    lines = changelog.splitlines()
    heading = re.compile(r"## " + re.escape(version) + r" — \d{4}-\d{2}-\d{2}")
    starts = [index for index, line in enumerate(lines) if heading.fullmatch(line)]
    if len(starts) != 1:
        fail(f"CHANGELOG.md has {len(starts)} headings '## {version} — <date>', not one")
    body = []
    for line in lines[starts[0] + 1:]:
        if line.startswith("## "):
            break
        body.append(line)
    return "\n".join(body).strip()


def check_notes(notes):
    if not notes:
        fail("the release notes are empty")
    if len(notes) > MAX_NOTES:
        fail(f"the release notes have {len(notes)} characters, more than {MAX_NOTES}")
    if "Unreleased" in notes:
        fail("the release notes still say Unreleased")
    if re.search(r"pacman\s+-U\s+https?:", notes):
        fail("the release notes install from a web address")
    if "![" in notes or re.search(r"<\s*img\b", notes, re.I):
        fail("the release notes hold an image")
    if re.search(r"</?[A-Za-z][^>]*>", notes):
        fail("the release notes hold HTML")
    if "```" in notes or re.search(r"(?m)^\s*(?:\$\s|sudo\s|pacman\s|curl\s|wget\s|bash\s|sh\s)", notes):
        fail("the release notes hold a command; they are plain words")


# --- GitHub, without credentials ---

def endpoints(test_override_allowed):
    """GitHub's API and download addresses. A test may point both at a
    stand-in server on 127.0.0.1, and nowhere else."""
    api = os.environ.get("ARCHDOCK_RELEASE_FILES_API", "https://api.github.com")
    downloads = os.environ.get("ARCHDOCK_RELEASE_FILES_DOWNLOADS", "https://github.com")
    for address in (api, downloads):
        if address.startswith("https://"):
            continue
        if not (test_override_allowed and re.fullmatch(r"http://127\.0\.0\.1:\d{1,5}", address)):
            fail(f"{address} is not GitHub; only tests may use a stand-in at http://127.0.0.1:<port>")
    return api.rstrip("/"), downloads.rstrip("/")


def repository():
    """The pinned repository, owner/name, as the PKGBUILD's url gives it."""
    match = re.search(r"^url='https://github\.com/([A-Za-z0-9-]+/[A-Za-z0-9_.-]+)'$",
                      (ROOT / "PKGBUILD").read_text(encoding="utf-8"), re.M)
    if not match:
        fail("the PKGBUILD's url does not name a GitHub repository")
    return match.group(1)


def http_get(url, limit):
    request = urllib.request.Request(url, headers={
        "Accept": "application/vnd.github+json", "User-Agent": USER_AGENT,
        "X-GitHub-Api-Version": GITHUB_API_VERSION})
    try:
        with urllib.request.urlopen(request, timeout=60) as response:
            data = response.read(limit + 1)
            if len(data) > limit:
                fail(f"{url} answered with more than {limit} bytes")
            return response.status, data
    except urllib.error.HTTPError as error:
        return error.code, b""
    except (urllib.error.URLError, TimeoutError, OSError) as error:
        fail(f"{url} could not be reached: {error}")


def published_files(downloads, repo, tag):
    """A release's manifest and signature, fetched by name from that release."""
    files = {}
    for name, limit in ((MANIFEST_NAME, MAX_MANIFEST_BYTES), (SIGNATURE_NAME, MAX_SIGNATURE_BYTES)):
        status, data = http_get(f"{downloads}/{repo}/releases/download/{tag}/{name}", limit)
        if status != 200:
            fail(f"release {tag} has no {name} (GitHub answered {status})")
        files[name] = data
    return files[MANIFEST_NAME], files[SIGNATURE_NAME]


def verified_manifest(keys, manifest_bytes, signature_bytes, test_build):
    """The signature first, with the one listed key; only then the manifest is read."""
    if len(keys) != 1:
        fail("exactly one update key must be listed")
    key_id, spki = keys[0]
    if not signature_verifies(spki, manifest_bytes, signature_bytes):
        fail(f"the signature does not verify with the key {key_id}")
    manifest = read_manifest(manifest_bytes, test_build)
    if manifest["signing_key_id"] != key_id:
        fail(f"the manifest names the key {manifest['signing_key_id']}, not {key_id}")
    return manifest


def git(*arguments):
    return subprocess.run((GIT, *arguments), cwd=ROOT, capture_output=True, text=True,
                          stdin=subprocess.DEVNULL)


def check_history(version, head, first_release, keys, test_build):
    """As LCL does: the latest normal release, checked as an installed Arch
    Dock would; the new version must be newer and descend from it."""
    api, downloads = endpoints(test_build)
    repo = repository()
    status, body = http_get(f"{api}/repos/{repo}/releases/latest", 1024 * 1024)
    if status == 404:
        if not first_release:
            fail("the repository has no normal release yet: the first update-capable release needs "
                 "--first-update-release")
        return None
    if status != 200:
        fail(f"GitHub answered {status} for the latest release")
    if first_release:
        fail("--first-update-release is only for a repository without a normal release")
    tag = json.loads(body).get("tag_name")
    if not isinstance(tag, str):
        fail("GitHub's latest release has no tag")
    previous = verified_manifest(keys, *published_files(downloads, repo, tag), test_build)
    if vercmp(version, previous["version"]) <= 0:
        fail(f"{version} is not newer than the published {previous['version']}")
    ancestor = git("merge-base", "--is-ancestor", previous["source_commit"], head)
    if ancestor.returncode != 0:
        fail(f"the published release's source commit {previous['source_commit']} is not in the history of {head}")
    return previous["version"]


# --- the packaged helper ---

HELPER_PATH = "usr/lib/arch-dock/arch-dock-install-update"


def packaged_helper(package, folder):
    """The helper inside the package, unpacked into `folder` to be asked."""
    with tarfile.open(package, mode="r:zst") as archive:
        for member in archive:
            name = member.name[2:] if member.name.startswith("./") else member.name
            if name == HELPER_PATH and member.isfile():
                target = Path(folder) / "arch-dock-install-update"
                target.write_bytes(archive.extractfile(member).read())
                target.chmod(0o700)
                return target
    fail(f"the package has no {HELPER_PATH}")


def helper_environment():
    return {"PATH": "/usr/bin", "LC_ALL": "C", "HOME": os.environ.get("HOME", "/nonexistent")}


def helper_about(helper):
    result = subprocess.run((str(helper), "--about"), capture_output=True, text=True,
                            env=helper_environment(), stdin=subprocess.DEVNULL, timeout=60)
    if result.returncode != 0:
        fail(f"the packaged helper's --about failed: {result.stderr.strip()}")
    about = {}
    for line in result.stdout.splitlines():
        key, _, value = line.partition(": ")
        about[key] = value
    return about


def helper_accepts(helper, manifest_bytes, signature_bytes, package, runtime):
    """The helper's --check-only, which works in a folder of `runtime` (a
    private folder of this account, mode 0700, as its runtime folder)."""
    stream = (base64.b64encode(manifest_bytes) + b"\n" + base64.b64encode(signature_bytes) + b"\n"
              + Path(package).read_bytes())
    result = subprocess.run((str(helper), "--check-only"), input=stream, capture_output=True,
                            env={**helper_environment(), "XDG_RUNTIME_DIR": str(runtime)}, timeout=300)
    return result.returncode == 0, result.stderr.decode(errors="replace").strip()


# --- the commands ---

def key_list(test_list):
    path = Path(test_list) if test_list else ROOT / "data/update-keys.txt"
    return read_key_list(path.read_text(encoding="utf-8"), str(path))


def utc_now():
    return datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def make(args):
    test = args.test_key is not None
    if test and not args.test_key_list:
        fail("--test-key needs --test-key-list")
    if not test and not args.key_file:
        fail("make needs --key-file")
    key_file = Path(args.test_key if test else args.key_file)
    package = Path(args.package).resolve()
    out = Path(args.out)
    check_key_file(key_file, args.pkgbuild, out)
    if out.exists() and (not out.is_dir() or any(out.iterdir())):
        fail(f"{out} is not a new or empty folder")

    info = read_package(package)
    version = info["version"]
    pkgbuild_bytes = Path(args.pkgbuild).read_bytes()
    pkgbuild = pkgbuild_bytes.decode("utf-8")
    pkgver = re.search(r"^pkgver=(\S+)$", pkgbuild, re.M)
    pkgrel = re.search(r"^pkgrel=(\S+)$", pkgbuild, re.M)
    if re.search(r"^epoch=", pkgbuild, re.M) or ":" in version:
        fail("Arch Dock's releases have no epoch")
    if not (TEST_VERSION if test else RELEASE_VERSION).fullmatch(version):
        fail(f"{version} is not a release version (a subrelease is for --test-key only)")
    if not pkgver or not pkgrel or f"{pkgver.group(1)}-{pkgrel.group(1)}" != version:
        fail(f"the PKGBUILD's pkgver and pkgrel do not make {version}")
    if package.name != f"arch-dock-{version}-x86_64.pkg.tar.zst":
        fail(f"the package file {package.name} does not match its version {version}")
    if info["name"] != "arch-dock" or info["arch"] != "x86_64":
        fail(f"the package is {info['name']} for {info['arch']}, not arch-dock for x86_64")
    if info["pkgbuild_sha256"] != hashlib.sha256(pkgbuild_bytes).hexdigest():
        fail(".BUILDINFO's pkgbuild_sha256sum is not the PKGBUILD's sha256")
    checkpoint = json.loads(Path(args.checkpoint).read_text(encoding="utf-8"))
    head = checkpoint.get("head")
    if not isinstance(head, str) or not HEX40.fullmatch(head):
        fail("the source checkpoint names no commit")
    tagged = git("rev-parse", "--verify", "--quiet", f"refs/tags/v{version}^{{commit}}")
    if tagged.returncode != 0 or tagged.stdout.strip() != head:
        fail(f"the tag v{version} does not point at the exported commit {head}")
    notes = changelog_section(Path(args.changelog).read_text(encoding="utf-8"), version)
    check_notes(notes)
    needs = info["depends"]
    if len(needs) > MAX_NEEDS or not all(NEED.fullmatch(need) for need in needs):
        fail("the package's dependencies are not all in pacman's dependency form")

    spki = public_half(key_file)
    keys = key_list(args.test_key_list if test else None)
    if len(keys) != 1 or keys[0][1] != spki:
        fail("the key file is not the key that data/update-keys.txt lists")
    key_id = keys[0][0]
    with tempfile.TemporaryDirectory(prefix="archdock-release-files-") as work:
        work = Path(work)
        helper = packaged_helper(package, work)
        about = helper_about(helper)
        wanted_build = "TEST: accepts a test key and a test server" if test else "release"
        if about.get("build") != wanted_build:
            fail(f"the packaged helper is a {about.get('build')!r} build, not {wanted_build!r}")
        if about.get("key") != f"{key_id} {fingerprint(spki)}":
            fail(f"the packaged helper trusts {about.get('key')!r}, not {key_id} {fingerprint(spki)}")
        previous = check_history(version, head, args.first_update_release, keys, test)

        digest = hashlib.sha256(package.read_bytes()).hexdigest()
        manifest = {
            "format": FORMAT, "product": PRODUCT, "channel": CHANNEL,
            "version": version, "release_tag": f"v{version}", "source_commit": head,
            "published_at": utc_now(), "release_notes": notes,
            "minimum_supported_version": args.minimum
                or (FIRST_RELEASE_MINIMUM if args.first_update_release else MINIMUM_SUPPORTED_VERSION),
            "signing_key_id": key_id,
            "package": {"file": package.name, "size": package.stat().st_size, "sha256": digest,
                        "arch": "x86_64", "needs": needs, "required_updater_version": UPDATER_PROTOCOL},
        }
        manifest_bytes = (json.dumps(manifest, indent=2, ensure_ascii=False) + "\n").encode("utf-8")
        read_manifest(manifest_bytes, test)
        (work / MANIFEST_NAME).write_bytes(manifest_bytes)
        sign(key_file, work / MANIFEST_NAME, work / SIGNATURE_NAME)
        signature_bytes = (work / SIGNATURE_NAME).read_bytes()

        # Three ways, before anything is written to the output folder.
        if not signature_verifies(spki, manifest_bytes, signature_bytes):
            fail("the signature does not verify with the key's own public half")
        verified_manifest(keys, manifest_bytes, signature_bytes, test)
        runtime = work / "runtime"
        runtime.mkdir(mode=0o700)
        accepted, why = helper_accepts(helper, manifest_bytes, signature_bytes, package, runtime)
        if not accepted:
            fail(f"the packaged helper's --check-only refuses the release: {why}")

        out.mkdir(parents=True, exist_ok=True)
        marker = "NOT FOR PUBLICATION: signed with a test key.\n\n" if test else ""
        (out / MANIFEST_NAME).write_bytes(manifest_bytes)
        (out / SIGNATURE_NAME).write_bytes(signature_bytes)
        (out / f"{package.name}.sha256").write_text(f"{digest}  {package.name}\n", encoding="utf-8")
        (out / "notes.md").write_text(marker + notes + "\n", encoding="utf-8")
        if test:
            (out / "NOT-FOR-PUBLICATION.txt").write_text(marker, encoding="utf-8")
    print(f"Release files for {version} in {out}" + (" (NOT FOR PUBLICATION)" if test else ""))
    print(f"The key: {key_id}, fingerprint {fingerprint(spki)}")
    print(f"The package's signed sha256: {digest}")
    print(f"After: {previous}" if previous else "The first update-capable release.")


def verify(args):
    folder = Path(args.folder)
    test = (folder / "NOT-FOR-PUBLICATION.txt").exists()
    if test and not args.test_key_list:
        fail("these files were signed with a test key: give --test-key-list")
    manifest_bytes = (folder / MANIFEST_NAME).read_bytes()
    manifest = verified_manifest(key_list(args.test_key_list if test else None), manifest_bytes,
                                 (folder / SIGNATURE_NAME).read_bytes(), test)
    package = manifest["package"]
    sums = (folder / f"{package['file']}.sha256").read_text(encoding="utf-8")
    if sums != f"{package['sha256']}  {package['file']}\n":
        fail(f"{package['file']}.sha256 does not hold the signed sha256")
    marker = "NOT FOR PUBLICATION: signed with a test key.\n\n" if test else ""
    if (folder / "notes.md").read_text(encoding="utf-8") != marker + manifest["release_notes"] + "\n":
        fail("notes.md is not the signed release notes")
    packaged = folder / package["file"]
    if packaged.exists():
        if packaged.stat().st_size != package["size"] or hashlib.sha256(packaged.read_bytes()).hexdigest() != package["sha256"]:
            fail(f"{package['file']} is not the file the manifest signs")
    print(f"{folder}: the release files for {manifest['version']} verify")


def check_published(args):
    """What the Update button sees of a published release, checked without credentials."""
    tag = args.tag
    test = bool(args.test_key_list)
    api, downloads = endpoints(test)
    repo = repository()
    status, body = http_get(f"{api}/repos/{repo}/releases/tags/{tag}", 1024 * 1024)
    if status != 200:
        fail(f"GitHub has no release {tag} (answered {status})")
    release = json.loads(body)
    if release.get("draft") or release.get("prerelease"):
        fail(f"{tag} is a draft or a pre-release, which the Update button never sees")
    if release.get("immutable") is not True:
        fail(f"{tag} is not an immutable release")
    status, body = http_get(f"{api}/repos/{repo}/releases/latest", 1024 * 1024)
    if status != 200 or json.loads(body).get("tag_name") != tag:
        fail(f"{tag} is not the repository's latest release")
    manifest = verified_manifest(key_list(args.test_key_list), *published_files(downloads, repo, tag), test)
    if "v" + manifest["version"] != tag:
        fail(f"the manifest of {tag} is for {manifest['version']}")
    package = manifest["package"]
    assets = {asset.get("name"): asset for asset in release.get("assets", [])}
    for name in (package["file"], MANIFEST_NAME, SIGNATURE_NAME, f"{package['file']}.sha256"):
        if name not in assets:
            fail(f"{tag} has no asset {name}")
    if f"{package['file']}.sig" in assets:
        fail(f"{tag} carries {package['file']}.sig, which pacman would check")
    asset = assets[package["file"]]
    if asset.get("size") != package["size"] or asset.get("digest") != "sha256:" + package["sha256"]:
        fail(f"GitHub's {package['file']} is not the file the manifest signs")
    print(f"{tag}: published, latest and immutable; the Update button reads {manifest['version']} "
          f"and its package verifies")


def check_key_place(args):
    for path in args.paths:
        resolved = check_place(path)
        print(f"{resolved}: the key may go here")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest="command", required=True)
    making = commands.add_parser("make", help="make and sign a release's files")
    making.add_argument("package")
    making.add_argument("--changelog", required=True)
    making.add_argument("--pkgbuild", required=True)
    making.add_argument("--checkpoint", required=True)
    making.add_argument("--key-file")
    making.add_argument("--out", required=True)
    making.add_argument("--first-update-release", action="store_true")
    making.add_argument("--minimum", help=argparse.SUPPRESS)
    making.add_argument("--test-key", help="tests and rehearsals only: a test key, output NOT FOR PUBLICATION")
    making.add_argument("--test-key-list", help=argparse.SUPPRESS)
    checking = commands.add_parser("verify", help="check a folder of release files again")
    checking.add_argument("folder")
    checking.add_argument("--test-key-list", help=argparse.SUPPRESS)
    published = commands.add_parser("check-published", help="check a published release as the button sees it")
    published.add_argument("tag")
    published.add_argument("--test-key-list", help=argparse.SUPPRESS)
    place = commands.add_parser("check-key-place", help="check where the key and its backup may go")
    place.add_argument("paths", nargs="+")
    args = parser.parse_args(argv)
    try:
        {"make": make, "verify": verify, "check-published": check_published,
         "check-key-place": check_key_place}[args.command](args)
    except ReleaseError as error:
        print(f"release-files.py: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
