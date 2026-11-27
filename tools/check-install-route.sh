#!/usr/bin/env bash
# The install route on a clean Arch root (F1). The root comes from Arch's
# bootstrap image, checked with gpgv against the Arch keyring, and lives in a
# work folder inside a user namespace of its own; the host's package cache is
# only read. In it:
#   F1-b   the first install of 0.1.1-54.1 with the documented line, as a user
#          with sudo;
#   F1-e   the 54.1 helper's --check-only, as that user, accepts 54.2;
#   F1-c   54.2 through the 54.1 helper, started as pkexec starts it;
#   F1-c'  54.3, with the helper's parent ended and its reader closed right
#          after "started";
#   F1-d   the helper's refusals, each leaving pacman's log and the version
#          unchanged.
# The verification packages are TEST builds that trust the test key, which
# signs their manifests here. The root and the test key are removed at the end.
# Heavy: up to about 2 GB downloaded (most from the host cache), 4-6 GB of
# disk, 15-30 minutes. Run it alone.
#
#   tools/check-install-route.sh <new work folder> <packages folder> <test key>
set -euo pipefail
project_root="$(cd -- "$(dirname -- "$0")/.." && pwd)"
work="${1:?a new work folder under /mnt/F/archdock-audit-2026-10-08/t/}"
packages="$(realpath -e -- "${2:?the folder with the 54.x verification packages}")"
key="$(realpath -e -- "${3:?the test key that signs their manifests}")"
case "$(realpath -m -- "$work")/" in
    /mnt/F/archdock-audit-2026-10-08/t/?*) ;;
    *) printf 'The work folder must lie under /mnt/F/archdock-audit-2026-10-08/t/.\n' >&2; exit 2 ;;
esac
[[ ! -e "$work" ]] || { printf '%s already exists.\n' "$work" >&2; exit 2; }
for version in 54.1 54.2 54.3 54.9; do
    [[ -f "$packages/arch-dock-0.1.1-$version-x86_64.pkg.tar.zst" ]] || {
        printf 'The package %s is missing.\n' "0.1.1-$version" >&2; exit 2; }
done
# Never the real update key: this run deletes its key at the end.
public=$(openssl pkey -in "$key" -pubout -outform DER | od -An -v -tx1 | tr -d ' \n')
if grep -v '^#' "$project_root/data/update-keys.txt" | grep -qi -- "$public"; then
    printf 'That is the release key data/update-keys.txt lists; F1 takes a test key only.\n' >&2
    exit 2
fi
mkdir -p "$work/evidence"
work="$(realpath -e -- "$work")"
evidence="$work/evidence"

# The helpers' inputs: each case's two lines (manifest and signature in
# base64) and its package, made here on the host, where Python is.
PYTHONDONTWRITEBYTECODE=1 /usr/bin/python3 -I -B - "$project_root" "$packages" "$key" "$work/inputs" \
    > "$evidence/inputs.log" 2>&1 <<'PY'
import base64, hashlib, importlib.util, json, os, random, subprocess, sys, tarfile, tempfile
from pathlib import Path

root, packages, key, out = map(Path, sys.argv[1:5])
sys.path.insert(0, str(root / "tests"))
from test_release_files import package_bytes  # noqa: E402  the release tool's synthetic packages

spec = importlib.util.spec_from_file_location("release_files", root / "tools/release-files.py")
release = importlib.util.module_from_spec(spec)
spec.loader.exec_module(release)
out.mkdir(parents=True)
work = Path(tempfile.mkdtemp(prefix="inputs-", dir=out.parent))
real = {v: packages / f"arch-dock-0.1.1-{v}-x86_64.pkg.tar.zst" for v in ("54.1", "54.2", "54.3", "54.9")}
other_key = work / "other.key"
subprocess.run(("/usr/bin/openssl", "ecparam", "-name", "prime256v1", "-genkey", "-noout", "-out", str(other_key)),
               check=True)
expected = []


def pkginfo_depends(path):
    with tarfile.open(path, mode="r:*") as archive:
        for member in archive:
            if member.name in (".PKGINFO", "./.PKGINFO"):
                text = archive.extractfile(member).read().decode()
                return [line.split(" = ", 1)[1] for line in text.splitlines() if line.startswith("depend = ")]
    return []


def manifest(path, version, needs=None, sha256=None, key_id="arch-dock-test-1", form=1):
    data = path.read_bytes()
    body = {"format": form, "product": "arch-dock", "channel": "stable", "version": version,
            "release_tag": f"v{version}", "source_commit": "0" * 40, "published_at": "2026-11-27T12:00:00Z",
            "release_notes": "A verification package; never published.",
            "minimum_supported_version": "0.1.1-54.1", "signing_key_id": key_id,
            "package": {"file": f"arch-dock-{version}-x86_64.pkg.tar.zst", "size": len(data),
                        "sha256": sha256 or hashlib.sha256(data).hexdigest(), "arch": "x86_64",
                        "needs": pkginfo_depends(path) if needs is None else needs,
                        "required_updater_version": 1}}
    return (json.dumps(body, indent=2) + "\n").encode()


def sign(data, with_key=key):
    source = work / "manifest"
    source.write_bytes(data)
    return subprocess.run(("/usr/bin/openssl", "dgst", "-sha256", "-sign", str(with_key), str(source)),
                          check=True, capture_output=True).stdout


def case(name, package, data, code, reason, signature=None):
    signature = sign(data) if signature is None else signature
    (out / f"{name}.head").write_bytes(base64.b64encode(data) + b"\n" + base64.b64encode(signature) + b"\n")
    target = out / f"{name}.pkg"
    if package.parent == work:
        package.rename(target)
    else:
        os.link(package, target)
    expected.append(f"{name} {code} {reason}")


def synthetic(name, tweak=None, version="0.1.1-54.4", depends=("glibc", "qt6-base>=6.12")):
    path = work / f"{name}.pkg.tar.zst"
    path.write_bytes(package_bytes(version=version, depends=depends, tweak=tweak))
    # Under 1 MiB the manifest's size is refused before any package rule runs.
    assert path.stat().st_size >= 1024 * 1024, name
    return path


# The route: F1-e and F1-c, then F1-c'.
case("54.2", real["54.2"], manifest(real["54.2"], "0.1.1-54.2"), 0, "-")
case("54.3", real["54.3"], manifest(real["54.3"], "0.1.1-54.3"), 0, "-")
# The refusals, with 54.3 installed (F1-d), each with the helper's reason.
case("wrong-hash", real["54.3"], manifest(real["54.3"], "0.1.1-54.3", sha256="0" * 64), 12,
     "the package is not the file the manifest describes")
case("54.3-again", real["54.3"], manifest(real["54.3"], "0.1.1-54.3"), 13, "is not newer than the installed")
case("54.1-older", real["54.1"], manifest(real["54.1"], "0.1.1-54.1"), 13, "is not newer than the installed")
other = synthetic("another-package-renamed", lambda s: s["meta"].update(
    {".PKGINFO": s["meta"][".PKGINFO"].replace("pkgname = arch-dock", "pkgname = which")
                                      .replace("pkgbase = arch-dock", "pkgbase = which")}))
case("another-package-renamed", other, manifest(other, "0.1.1-54.4"), 12, "not arch-dock")
case("54.9-unmet-qt-bound", real["54.9"], manifest(real["54.9"], "0.1.1-54.9"), 11, "missing or too old")
payload = "usr/share/arch-dock/payload.bin"
tweaks = {
    "install-script": (lambda s: s["meta"].update({".INSTALL": "post_install() { :; }\n"}),
                       "the package has an install script"),
    "setuid": (lambda s: s["files"].update({payload: (s["files"][payload][0], 0o4755)}), "is setuid or setgid"),
    "second-pkginfo": (lambda s: s["extra"].append((".PKGINFO", s["meta"][".PKGINFO"].encode(), 0o644)), "twice"),
    "path-twice": (lambda s: s["extra"].append((payload, b"again", 0o644)), "twice"),
    "pkgver-twice": (lambda s: s["meta"].update({".PKGINFO": s["meta"][".PKGINFO"] + "pkgver = 0.1.1-54.4\n"}),
                     ".PKGINFO names pkgver"),
    "dot-dot-path": (lambda s: s["extra"].append(("usr/share/../../etc/arch-dock-escape", b"x", 0o644)),
                     "the package holds the path"),
    "hard-link": (lambda s: s["links"].append(("usr/share/arch-dock/linked.bin", payload)), "is a hard link"),
    "owned-by-1000": (lambda s: s.update(owner=1000), "is not owned by root"),
    "group-writable-folder": (lambda s: s["folder_modes"].update({"usr/share/arch-dock": 0o775}),
                              "can be written by group or others"),
    "capability-xattr": (lambda s: s["pax"].update(
        {payload: {"LIBARCHIVE.xattr.security.capability": base64.b64encode(bytes(20)).decode()}}),
        "carries xattrs or ACLs"),
    "mtree-mismatch": (lambda s: s["mtree_skip"].add(payload), ".MTREE does not list exactly"),
}
for name, (tweak, reason) in tweaks.items():
    path = synthetic(name, tweak)
    case(name, path, manifest(path, "0.1.1-54.4", needs=["glibc", "qt6-base>=6.12"]), 12, reason)
dash = synthetic("need-with-dash", depends=("-bad",))
case("need-with-dash", dash, manifest(dash, "0.1.1-54.4", needs=["-bad"]), 12, "a need is not in pacman's dependency form")
# The signature and the manifest's form.
valid = synthetic("valid")
good = manifest(valid, "0.1.1-54.4", needs=["glibc", "qt6-base>=6.12"])
unsigned = "the release is not signed with the key"
for name, data, signature, code, reason in (
        ("unsigned", good, b"not a signature of it", 16, unsigned),
        ("another-key", good, sign(good, other_key), 16, unsigned),
        ("other-bytes", good + b" ", sign(good), 16, unsigned),
        ("garbage", random.Random(7).randbytes(300), random.Random(8).randbytes(72), 16, unsigned),
        ("another-key-id", manifest(valid, "0.1.1-54.4", needs=["glibc", "qt6-base>=6.12"], key_id="arch-dock-other-1"),
         None, 16, "the manifest names the key"),
        ("format-2", manifest(valid, "0.1.1-54.4", needs=["glibc", "qt6-base>=6.12"], form=2), None, 20,
         "the manifest has format 2")):
    copy = work / f"{name}.pkg.tar.zst"
    copy.write_bytes(valid.read_bytes())
    case(name, copy, data, code, reason, signature)
# pacman's lock, last: a valid, newer package that only the lock stops.
for name, code, reason in (("lock-held", 10, "another program is installing software"),
                           ("lock-left-behind", 18, "left pacman's lock behind")):
    copy = work / f"{name}.pkg.tar.zst"
    copy.write_bytes(valid.read_bytes())
    case(name, copy, good, code, reason)
(out / "expected.txt").write_text("\n".join(expected) + "\n")
fingerprint = subprocess.run(("/usr/bin/openssl", "pkey", "-in", str(key), "-pubout", "-outform", "DER"),
                             check=True, capture_output=True).stdout
(out / "key-fingerprint.txt").write_text(hashlib.sha256(fingerprint).hexdigest() + "\n")
for leftover in work.iterdir():
    leftover.unlink()
work.rmdir()
print(f"{len(expected)} cases")
PY
printf 'Inputs: %s\n' "$(tail -n 1 "$evidence/inputs.log")"

# Arch's bootstrap image, checked against the Arch keyring (no keyring of the
# owner's is read: gpgv gets a home of its own).
mirror=https://geo.mirror.pkgbuild.com
for file in archlinux-bootstrap-x86_64.tar.zst archlinux-bootstrap-x86_64.tar.zst.sig; do
    curl --fail --location --silent --show-error -o "$work/$file" "$mirror/iso/latest/$file"
done
mkdir -m 700 "$work/gnupg"
gpg --homedir "$work/gnupg" --batch --quiet --dearmor --output "$work/gnupg/archlinux-keyring.gpg" \
    /usr/share/pacman/keyrings/archlinux.gpg
GNUPGHOME="$work/gnupg" gpgv --keyring "$work/gnupg/archlinux-keyring.gpg" \
    "$work/archlinux-bootstrap-x86_64.tar.zst.sig" "$work/archlinux-bootstrap-x86_64.tar.zst" \
    > "$evidence/bootstrap-gpgv.log" 2>&1
printf 'Bootstrap image verified: %s\n' "$(grep -o 'Good signature from "[^"]*"' "$evidence/bootstrap-gpgv.log")"

cat > "$work/inside.sh" <<'INSIDE'
set -uo pipefail
cd "$WORK"
results="$WORK/evidence/results.txt"
: > "$results"
check() { if eval "$2"; then printf 'PASS  %s\n' "$1" >> "$results"; else printf 'FAIL  %s\n' "$1" >> "$results"; fi; }
cleanup() {
    cd "$WORK"
    # One lazy detach of the root's own bind takes every mount below it along.
    umount -l "$WORK/root" 2>/dev/null
    if grep -q " $WORK/root[ /]" /proc/self/mountinfo; then
        printf 'FAIL  the root is still mounted; it is left in place\n' >> "$results"
    else
        rm -rf --one-file-system "$WORK/root"
    fi
    rm -rf --one-file-system "$WORK/inputs" "$WORK/gnupg"
}
trap cleanup EXIT
mkdir root
tar -xf archlinux-bootstrap-x86_64.tar.zst -C root --strip-components=1 --numeric-owner || exit 1
mount --bind root root
mount -t proc proc root/proc
# A /dev of the root's own with single device nodes bound in (as arch-chroot
# does), never the host's /dev itself.
mount -t tmpfs -o mode=755 dev root/dev
for node in null zero full random urandom tty; do
    : > "root/dev/$node"
    mount --bind "/dev/$node" "root/dev/$node"
done
ln -s /proc/self/fd root/dev/fd
ln -s /proc/self/fd/0 root/dev/stdin
ln -s /proc/self/fd/1 root/dev/stdout
ln -s /proc/self/fd/2 root/dev/stderr
mkdir -p root/dev/shm root/dev/pts
mount -t tmpfs -o mode=1777 shm root/dev/shm
mount -t tmpfs tmpfs root/run
mount -t tmpfs tmpfs root/tmp
mkdir -p root/host-cache
mount --bind /var/cache/pacman/pkg root/host-cache
mount -o remount,bind,ro root/host-cache
cp -L /etc/resolv.conf root/etc/resolv.conf
printf 'Server = https://geo.mirror.pkgbuild.com/$repo/os/$arch\n' > root/etc/pacman.d/mirrorlist
sed -i 's|^\[options\]$|[options]\nCacheDir = /var/cache/pacman/pkg/\nCacheDir = /host-cache/|' root/etc/pacman.conf
inside() { chroot root /usr/bin/env -i PATH=/usr/bin LANG=C.UTF-8 HOME=/root "$@"; }
as_user() {
    chroot --userspec=1000:1000 root /usr/bin/env -i PATH=/usr/bin LANG=C.UTF-8 HOME=/home/tester USER=tester \
        XDG_RUNTIME_DIR=/home/tester/runtime "$@"
}
# As pkexec starts the helper: its environment, and a parent whose end sends TERM.
helper_as_pkexec() {
    chroot root /usr/bin/env -i PKEXEC_UID=1000 PATH=/usr/sbin:/usr/bin LANG=C.UTF-8 \
        /usr/bin/setpriv --pdeathsig TERM /usr/lib/arch-dock/arch-dock-install-update
}
{ inside pacman-key --init && inside pacman-key --populate archlinux &&
  inside pacman -Syu --noconfirm sudo; } > evidence/root-setup.log 2>&1 || { echo "FAIL  the root could not be set up" >> "$results"; exit 1; }
inside useradd -m -u 1000 -U tester
printf 'tester ALL=(ALL) NOPASSWD: ALL\n' > root/etc/sudoers.d/tester
chmod 440 root/etc/sudoers.d/tester
mkdir -p root/home/tester/packages root/home/tester/runtime
cp "$PACKAGES"/arch-dock-0.1.1-54.*-x86_64.pkg.tar.zst root/home/tester/packages/
chown -R 1000:1000 root/home/tester
chmod 700 root/home/tester/runtime
mkdir -p root/f1-inputs
cp "$WORK"/inputs/* root/f1-inputs/
chmod -R a+rX root/f1-inputs
# The test-root marker, inside this root only: a TEST helper installs nowhere else.
touch root/etc/arch-dock-update-test-root
transactions() { grep -c '\[ALPM\] transaction started' root/var/log/pacman.log; }
installed() { inside pacman -Q arch-dock 2>/dev/null; }

# F1-b: the first install, with the documented line.
as_user bash -c 'cd ~/packages && sudo pacman -U --noconfirm ./arch-dock-0.1.1-54.1-x86_64.pkg.tar.zst' \
    > evidence/f1-b-install.log 2>&1
check "F1-b 54.1 installed" '[[ "$(installed)" == "arch-dock 0.1.1-54.1" ]]'
awk '/\[ALPM\] running .arch-dock-qt\.hook/{f=1;next} f && /\[(ALPM|PACMAN)\] /{exit} f' root/var/log/pacman.log \
    > evidence/f1-b-qt-hook.log
inside /usr/share/libalpm/scripts/arch-dock-check-qt < /dev/null > evidence/f1-b-check-qt.log 2>&1
qt_code=$?
check "F1-b the Qt hook ran and printed nothing, and the script alone is quiet" \
    'grep -q "running .arch-dock-qt.hook." root/var/log/pacman.log && [[ ! -s evidence/f1-b-qt-hook.log && $qt_code == 0 && ! -s evidence/f1-b-check-qt.log ]]'
: > evidence/f1-b-imports.log
for file in $(inside pacman -Qlq arch-dock); do
    [[ -f "root$file" ]] && [[ "$(head -c 4 "root$file" | od -An -c | tr -d ' ')" == '177ELF' ]] || continue
    readelf --dyn-syms --wide "root$file" | grep -E "@Qt_6_PRIVATE_API|_Z(T[VIS])?NK?11QQmlPrivate[0-9]+AOT" \
        | sed "s|^|$file: |" >> evidence/f1-b-imports.log
done
check "F1-b no private or ahead-of-time import" '[[ ! -s evidence/f1-b-imports.log ]]'
as_user arch-dock --version > evidence/f1-b-version.log 2>&1
check "F1-b arch-dock --version gives 0.1.1-54.1" 'grep -q "0.1.1-54.1" evidence/f1-b-version.log'
check "F1-b the policy, helper and finisher are present" \
    '[[ -f root/usr/share/polkit-1/actions/org.archdock.ArchDock.policy && -x root/usr/lib/arch-dock/arch-dock-install-update && -x root/usr/lib/arch-dock/arch-dock-finish-update ]]'
as_user /usr/lib/arch-dock/arch-dock-install-update --about > evidence/f1-b-about.log 2>&1
check "F1-b --about says TEST and the test key's fingerprint" \
    'grep -qx "build: TEST: accepts a test key and a test server" evidence/f1-b-about.log &&
     grep -qx "key: arch-dock-test-1 $(cat inputs/key-fingerprint.txt)" evidence/f1-b-about.log'

# F1-e: --check-only as the user, in a runtime folder of its own with mode 0700.
as_user bash -c 'cat /f1-inputs/54.2.head /f1-inputs/54.2.pkg | /usr/lib/arch-dock/arch-dock-install-update --check-only' \
    > evidence/f1-e.log 2>&1
check "F1-e the 54.1 helper's --check-only accepts 54.2" \
    'grep -q "would install arch-dock 0.1.1-54.2" evidence/f1-e.log && [[ "$(installed)" == "arch-dock 0.1.1-54.1" ]]'

# F1-c: 54.2 through the 54.1 helper.
hooks_before=$(grep -c "running .arch-dock-qt.hook." root/var/log/pacman.log)
cat inputs/54.2.head inputs/54.2.pkg | helper_as_pkexec > evidence/f1-c.out 2> evidence/f1-c.err
code=$?
check "F1-c exit 0 and arch-dock 0.1.1-54.2" '[[ $code == 0 && "$(installed)" == "arch-dock 0.1.1-54.2" ]]'
check "F1-c the hook ran" '[[ "$(grep -c "running .arch-dock-qt.hook." root/var/log/pacman.log)" -gt "$hooks_before" ]]'
check "F1-c pacman read only the root-only copy" \
    'grep -E "Running .[^ ]*pacman -U " root/var/log/pacman.log | tail -n 1 | grep -q " /run/arch-dock-update/work\.[^/ ]*/arch-dock-0\.1\.1-54\.2-x86_64\.pkg\.tar\.zst"'
check "F1-c only the lock is left in /run/arch-dock-update" '[[ "$(ls -A root/run/arch-dock-update)" == lock ]]'
check "F1-c the log says it ran without its own scope" 'grep -q "runs without its own scope" evidence/f1-c.err'
check "F1-c \"started\" came first" '[[ "$(head -n 1 evidence/f1-c.out)" == started ]]'

# F1-c': 54.3, with the parent ended and the reader closed right after "started".
rm -f evidence/f1-c2.pid
mkfifo f1-c2.out
bash -c 'cat inputs/54.3.head inputs/54.3.pkg | chroot root /usr/bin/env -i PKEXEC_UID=1000 PATH=/usr/sbin:/usr/bin \
    LANG=C.UTF-8 /usr/bin/setpriv --pdeathsig TERM /usr/lib/arch-dock/arch-dock-install-update \
    > f1-c2.out 2> evidence/f1-c2.err & echo $! > evidence/f1-c2.pid; exec sleep 600' &
parent=$!
IFS= read -r first < f1-c2.out
printf '%s\n' "$first" > evidence/f1-c2.first
kill -KILL "$parent"
wait "$parent" 2>/dev/null
for _ in $(seq 1 600); do
    pgrep -f "/usr/lib/arch-dock/arch-dock-install-update" > /dev/null || break
    sleep 1
done
rm -f f1-c2.out
check "F1-c' the helper went on: arch-dock 0.1.1-54.3" '[[ "$first" == started && "$(installed)" == "arch-dock 0.1.1-54.3" ]]'
check "F1-c' no db.lck is left" '[[ ! -e root/var/lib/pacman/db.lck ]]'

# F1-d: the refusals, each with no new transaction and the version unchanged.
while read -r name wanted reason; do
    [[ "$name" == 54.2 || "$name" == 54.3 ]] && continue
    holder=''
    case "$name" in
        lock-held)
            touch root/var/lib/pacman/db.lck
            chroot root /usr/bin/env -i PATH=/usr/bin /usr/bin/bash -c 'exec 9< /var/lib/pacman/db.lck; exec sleep 300' &
            holder=$!
            sleep 1 ;;
        lock-left-behind) touch root/var/lib/pacman/db.lck ;;
    esac
    before=$(transactions)
    stamp=$(stat -c '%i %Y' root/var/lib/pacman/db.lck 2>/dev/null)
    cat "inputs/$name.head" "inputs/$name.pkg" | helper_as_pkexec > "evidence/f1-d-$name.out" 2> "evidence/f1-d-$name.err"
    code=$?
    check "F1-d $name gives $wanted (got $code) for \"$reason\", no transaction, 54.3 kept" \
        '[[ $code == "$wanted" && "$(transactions)" == "$before" && "$(installed)" == "arch-dock 0.1.1-54.3" ]] && grep -qF -- "$reason" "evidence/f1-d-$name.err"'
    if [[ "$name" == lock-* ]]; then
        check "F1-d $name leaves the lock untouched" '[[ "$(stat -c "%i %Y" root/var/lib/pacman/db.lck 2>/dev/null)" == "$stamp" ]]'
        [[ -n "$holder" ]] && kill "$holder" 2>/dev/null
        rm -f root/var/lib/pacman/db.lck
    fi
done < inputs/expected.txt
cp root/var/log/pacman.log evidence/root-pacman.log
INSIDE

# Inside a user namespace of its own; the root and the test key go with it.
if ! unshare --user --map-auto --map-root-user --mount --pid --fork --kill-child --mount-proc /usr/bin/true 2> "$evidence/unshare.log"; then
    printf 'This account cannot map its user ID range (unshare --map-auto). Ask the owner.\n' >&2
    exit 3
fi
unshare --user --map-auto --map-root-user --mount --pid --fork --kill-child --mount-proc \
    env -i PATH=/usr/bin:/usr/sbin WORK="$work" PACKAGES="$packages" KEY="$key" LANG=C.UTF-8 \
    /usr/bin/bash "$work/inside.sh" > "$evidence/inside.log" 2>&1 || true
rm -f "$work/archlinux-bootstrap-x86_64.tar.zst"
cat "$evidence/results.txt"
failed=$(grep -c '^FAIL' "$evidence/results.txt" || true)
passed=$(grep -c '^PASS' "$evidence/results.txt" || true)
printf 'F1: %s passed, %s failed. Evidence in %s\n' "$passed" "$failed" "$evidence"
# The test key goes once F1 has passed; after a failed run it is kept for the
# run after the fix, and deleted by hand when F1 is done.
if [[ "$failed" == 0 && "$passed" -gt 0 ]]; then
    rm -f "$key"
    printf 'The test key is deleted.\n'
else
    printf 'The test key is kept for another run: %s\n' "$key"
    exit 1
fi
