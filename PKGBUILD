# Contributor: Sean Pringle <sean.pringle@gmail.com>

pkgname=goomwwm
pkgver=1.0_rc2
pkgrel=1
pkgdesc="Get out of my way, Window Manager!"
arch=('i686' 'x86_64')
url="http://aerosuidae.net/goomwwm"
license=('MIT')
depends=('libx11' 'libxft' 'freetype2')
optdepends=('dmenu')
makedepends=()
provides=('goomwwm')
conflicts=('goomwwm-git')
source=("http://aerosuidae.net/goomwwm/$pkgname-$pkgver.tar.gz")
md5sums=('d4b30d3c23932a6f91d8afc2231a2300')

build() {
  cd "${srcdir}/$pkgname-$pkgver"
  make
}

package() {
  cd "${srcdir}/$pkgname-$pkgver"
  install -Dm 755 $pkgname "$pkgdir/usr/bin/$pkgname"
  install -Dm 644 "$pkgname.desktop" "$pkgdir/usr/share/xsessions/$pkgname.desktop"
  gzip -c "$pkgname.1" > "$pkgname.1.gz"
  install -Dm644 "$pkgname.1.gz" "$pkgdir/usr/share/man/man1/$pkgname.1.gz"
}