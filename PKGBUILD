pkgname=arch-dock
pkgver=0.1.1
pkgrel=54
pkgdesc='Native Plasma Wayland dock panels with shared rendering and presets'
arch=('x86_64')
url='https://github.com/aivars11232/Arch-Dock'
license=('GPL-3.0-or-later' 'MIT')
# Qt's lowest versions are set in package(), from the Qt the build used.
depends=('glibc' 'gcc-libs' 'qt6-base' 'qt6-declarative'
         'qt6-svg' 'qt6-wayland' 'kirigami' 'kservice' 'kio'
         'kglobalaccel' 'kglobalacceld' 'libplasma>=6.8' 'plasma-workspace'
         'plasma-desktop' 'kwin' 'layer-shell-qt>=6.8' 'kpackage' 'kconfig' 'plasma5support'
         'polkit' 'knotifications' 'libarchive' 'openssl')
makedepends=('cmake' 'make' 'gcc' 'pkgconf' 'qt6-quick3d' 'qt6-shadertools' 'onnxruntime-cpu')
optdepends=('qt6-quick3d: optional true 3D rendering; ordinary renderers work without it'
            'kpipewire: optional live window thumbnails'
            'onnxruntime-cpu: 3D icons made into objects (with the icon models installed)')
# Bound compiler/linker memory, including when makepkg enables LTO by default.
options=('!lto' '!debug')
source=("arch-dock-${pkgver}.tar.gz")
# tools/prepare-arch-source.py writes a checksummed source and pinned recipe.
sha256sums=('eabc08cdebacdb8eb87a9c635541e0fce624818bd25c727aadc21155ed8b10b9')

build() {
    cmake -S "$srcdir/arch-dock-$pkgver" -B "$srcdir/build" \
        -G 'Unix Makefiles' -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
        -DCMAKE_AUTOGEN_PARALLEL=1 -DARCHDOCK_ENABLE_QUICK3D=ON \
        -DARCHDOCK_RELEASE="${pkgver}-${pkgrel}" \
        -DARCHDOCK_UPDATE_REPOSITORY="${url#https://github.com/}" \
        -DBUILD_TESTING=OFF
    cmake --build "$srcdir/build" --parallel 1
}

package() {
    # Arch Dock needs at least the Qt minor version it was built with, and no
    # upper bound: Arch updates Qt for the whole system at once.
    local qt_version qt_major qt_minor qt_rest entry
    local qt_depends=()
    qt_version=$(< "$srcdir/build/qt-build-version")
    IFS=. read -r qt_major qt_minor qt_rest <<< "$qt_version"
    for entry in "${depends[@]}"; do
        case $entry in
            qt6-base|qt6-declarative) qt_depends+=("$entry>=$qt_major.$qt_minor") ;;
            *) qt_depends+=("$entry") ;;
        esac
    done
    depends=("${qt_depends[@]}")

    DESTDIR="$pkgdir" cmake --install "$srcdir/build"
    install -Dm644 "$srcdir/arch-dock-$pkgver/docs/INSTALL.md" \
        "$pkgdir/usr/share/doc/arch-dock/INSTALL.md"
}
