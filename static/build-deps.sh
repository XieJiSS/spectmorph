#!/bin/bash

set -e

function autoconfconf {
echo "======= $(pwd) ======="
  CPPFLAGS="-I${PREFIX}/include${GLOBAL_CPPFLAGS:+ $GLOBAL_CPPFLAGS}" \
  CFLAGS="${GLOBAL_CFLAGS:+ $GLOBAL_CFLAGS}" \
  CXXFLAGS="${GLOBAL_CXXFLAGS:+ $GLOBAL_CXXFLAGS}" \
  LDFLAGS="${GLOBAL_LDFLAGS:+ $GLOBAL_LDFLAGS}" \
  ./configure --disable-dependency-tracking --prefix=$PREFIX $@
}

function autoconfbuild {
  autoconfconf $@
  make $MAKEFLAGS
  make install
}

SRCDIR=$PWD/src
BUILDD=$PWD/build

mkdir -p $SRCDIR $BUILDD

PREFIX=$PWD/prefix
GLOBAL_CFLAGS="-fPIC -DPIC"
GLOBAL_LDFLAGS="-L$PREFIX/lib"
MAKEFLAGS="-j16"
PATH=$PWD/prefix/bin:$PATH
export PKG_CONFIG_PATH=$PREFIX/lib/pkgconfig:$PKG_CONFIG_PATH

cd $SRCDIR
apt-get source libogg
apt-get source libvorbis
apt-get source flac
apt-get source libsndfile
apt-get source zlib1g
apt-get source libffi
apt-get source glib2.0
apt-get source libao
apt-get source libpng
apt-get source pixman
apt-get source freetype
#apt-get source cairo
apt-get source fftw3

cd $SRCDIR/libogg-1.3.2
autoconfbuild --disable-shared

cd $SRCDIR/libvorbis-1.3.5
autoconfbuild --disable-shared

cd $SRCDIR/flac-1.3.1
./autogen.sh
autoconfbuild --disable-shared

cd $SRCDIR/libsndfile-1.0.25
autoconfbuild --disable-shared

cd $SRCDIR/zlib-1.2.8.dfsg
CFLAGS="${GLOBAL_CFLAGS}" \
LDFLAGS="${GLOBAL_LDFLAGS}" \
./configure --prefix=$PREFIX --static
make $MAKEFLAGS
make install

cd $SRCDIR/libffi-3.2.1
autoconfbuild --disable-shared

cd $SRCDIR/glib2.0-2.48.2
autoconfbuild --disable-shared --with-pcre=internal

# FIXME: could be removed from spectmorph-vst/lv2 deps
cd $SRCDIR/libao-1.1.0
autoconfbuild --disable-shared

cd $SRCDIR/libpng-1.2.54
autoconfbuild --disable-shared

cd $SRCDIR/pixman-0.33.6
autoconfbuild --disable-shared

cd $SRCDIR/freetype-2.6.1
debian/rules patch
cd $SRCDIR/freetype-2.6.1/freetype-2.6.1
autoconfbuild --disable-shared --with-harfbuzz=no --with-png=no --with-bzip2=no

# FIXME: wget -> curl
cd $SRCDIR
wget https://cairographics.org/snapshots/cairo-1.15.10.tar.xz
tar xf cairo-1.15.10.tar.xz
cd $SRCDIR/cairo-1.15.10
autoconfbuild --disable-shared

# cairo 1.14.6 doesn't compile properly
#cd $SRCDIR/cairo-1.14.6
#CPPFLAGS=-I${PREFIX}/include LDFLAGS="$GLOBAL_LDFLAGS" ./autogen.sh # HACK (to avoid calling missing aclocal-1.14)
#autoconfbuild --disable-shared

cd $SRCDIR/fftw3-3.3.4
autoconfbuild --disable-shared --disable-fortran --enable-single
