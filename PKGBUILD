# Contributor: Sean Pringle <sean.pringle@gmail.com>

pkgname=goomwwm-git
pkgver=20120625
pkgrel=1
pkgdesc="Get out of my way, Window Manager!"
arch=('i686' 'x86_64')
url="http://github.com/seanpringle/goomwwm"
license=('MIT')
depends=('libx11' 'libxft' 'freetype2')
makedepends=('git')
provides=('goomwwm')

_gitroot="git://github.com/seanpringle/goomwwm.git"
_gitname="goomwwm"

build() {
  cd "$srcdir"
  msg "Connecting to GIT server...."

  if [ -d $_gitname ] ; then
    cd $_gitname && git pull origin
    msg "The local files are updated."
  else
    git clone $_gitroot
  fi

  msg "GIT checkout done or server timeout"
  msg "Starting make..."

  rm -rf "$srcdir/$_gitname-build"
  git clone "$srcdir/$_gitname" "$srcdir/$_gitname-build"
  cd "$srcdir/$_gitname-build"

  make
}

package() {
  cd "$srcdir/$_gitname-build"
  install -Dm 755 $_gitname "$pkgdir/usr/bin/goomwwm"
  gzip -c "$_gitname.1" > "$_gitname.1.gz"
  install -Dm644 "$_gitname.1.gz" "$pkgdir/usr/share/man/man1/$_gitname.1.gz"
}