pkgname=arch-dock
pkgver=0.1.0
pkgrel=2
pkgdesc='Native Plasma Wayland dock panels with shared rendering and presets'
arch=('x86_64')
url='https://github.com/aivars11232/Arch-Dock'
license=('LicenseRef-Arch-Dock-Unspecified')
depends=('glibc' 'gcc-libs' 'qt6-base>=6.8' 'qt6-declarative>=6.8'
         'qt6-svg' 'qt6-wayland' 'kirigami' 'kservice' 'kio'
         'kglobalaccel' 'kglobalacceld' 'libplasma' 'plasma-workspace'
         'plasma-desktop' 'kwin')
makedepends=('cmake' 'make' 'gcc' 'pkgconf' 'qt6-quick3d')
optdepends=('qt6-quick3d: optional true 3D rendering; ordinary renderers work without it'
            'kpipewire: optional live window thumbnails')
# Bound compiler/linker memory, including when makepkg enables LTO by default.
options=('!lto' '!debug')
source=("arch-dock-${pkgver}.tar.gz")
# tools/prepare-arch-source.py writes a checksummed source and pinned recipe.
sha256sums=('367c3314260bd31ddf268a49ef861472f25b800966f361681f375bf5f8f5b077')

build() {
    cmake -S "$srcdir/arch-dock-$pkgver" -B "$srcdir/build" \
        -G 'Unix Makefiles' -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
        -DCMAKE_AUTOGEN_PARALLEL=1 -DARCHDOCK_ENABLE_QUICK3D=ON \
        -DBUILD_TESTING=OFF
    cmake --build "$srcdir/build" --parallel 1
}

package() {
    DESTDIR="$pkgdir" cmake --install "$srcdir/build"
    install -Dm644 "$srcdir/arch-dock-$pkgver/docs/INSTALL.md" \
        "$pkgdir/usr/share/doc/arch-dock/INSTALL.md"
    install -Dm644 "$srcdir/arch-dock-$pkgver/packaging/LICENSING.md" \
        "$pkgdir/usr/share/licenses/arch-dock/LICENSING.md"
}
