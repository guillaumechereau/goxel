# Maintainer: Aditya Mishra <https://github.com/pegvin/goxel2/issues>
pkgname=goxel2-git
pkgver=0.13.3
pkgrel=3 # Update if you changed something but that is so minor change you don't want to change the version
pkgdesc="a cross-platform 3d voxel art editor"
arch=('i686' 'x86_64')
url="https://github.com/pegvin/goxel2"
license=('GPL3')
depends=(glfw gtk3)
makedepends=(git make tar curl scons pkg-config)
optdepends=()
provides=('goxel2')
conflicts=('goxel2')

# Name of the directory the cloned repository will be in.
_gitname=goxel2

build() {
	echo "Getting Goxel2 v$pkgver Source Code from GitHub Release..."
	curl -L "https://github.com/pegvin/goxel2/archive/refs/tags/v$pkgver.tar.gz" --output $_gitname.tar.gz

	echo "Extracting $_gitname.tar.gz..."
	tar -xf $_gitname.tar.gz
	cd $_gitname-$pkgver/

	# don't fail on warnings:
	CFLAGS="${CFLAGS} -Wno-all"
	CXXFLAGS="${CFLAGS}"

	echo "Building Goxel2..."
	make release
}

package() {
	echo "Installing Goxel2..."

	cd $_gitname-$pkgver/
	install -Dm644 data/goxel2.desktop "$pkgdir/usr/share/applications/goxel2.desktop"
	install -Dm644 icon.png "$pkgdir/usr/share/icons/goxel2.png"
	install -Dm755 goxel2 "$pkgdir/usr/bin/goxel2"
}
