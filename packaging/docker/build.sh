#!/bin/sh
# Dateblock blocks (sleeps) until a cron-style moment in time.
# Copyright (C) 2013-2026 Chris Caron <lead2gold@gmail.com>
#
# This file is part of Dateblock.  Dateblock is free software; you can
# redistribute it and/or modify it under the terms of the GNU General Public
# License as published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# Runs inside a build container (see packaging/docker/Dockerfile.*).
#  - The source tree is mounted read-only at /src; it is copied first so
#    nothing in your checkout is touched.
#  - Results are written to /out (dist/<distro>/ on your machine).
#
# Usage: build.sh build   compile, then run every test
#        build.sh rpm     build the RPMs     (Fedora / EL images)
#        build.sh deb     build the .debs    (Debian image)
#        build.sh arch    build the packages (Arch image)
#        build.sh man     regenerate the man page from its ronn source
set -e
mode=${1:-build}
export LANG=C.UTF-8

# Copy the source (only files git knows about when it is a git checkout)
mkdir -p /tmp/src
cd /src
if git -c safe.directory=/src rev-parse --git-dir >/dev/null 2>&1; then
   git -c safe.directory=/src ls-files -z -co --exclude-standard | \
      tar --null --ignore-failed-read -T - -cf - | tar -xf - -C /tmp/src
else
   cp -a /src/. /tmp/src/
fi
cd /tmp/src

if [ "$mode" = man ]; then
   ronn --roff --pipe --manual="Dateblock Manual" --organization=Dateblock \
      packaging/man/dateblock.md > /out/dateblock.1
   ronn --html --pipe --manual="Dateblock Manual" --organization=Dateblock \
      packaging/man/dateblock.md > /out/dateblock.1.html
   exit 0
fi

./autogen.sh
./configure
version=$(sed -n 's/^PACKAGE_VERSION = //p' Makefile)

case "$mode" in
build)
   make
   TZ=UTC make check

   # Test the Python bindings both ways: linked against the library just
   # built (as the distribution packages do), and with the C++ code built
   # in (as the PyPI wheels do)
   DATEBLOCK_SHARED=1 python3 setup.py -q build -b /tmp/pybuild-shared \
      --build-lib /tmp/pylib-shared
   TZ=UTC LD_LIBRARY_PATH=src/.libs PYTHONPATH=/tmp/pylib-shared \
      python3 -m unittest discover -v -s python/tests
   python3 setup.py -q build -b /tmp/pybuild --build-lib /tmp/pylib
   TZ=UTC PYTHONPATH=/tmp/pylib python3 -m unittest discover -v -s python/tests

   # ...and once more acting like Windows, whose own date functions only
   # work from 1970 to the year 3000 (the code works around that)
   CFLAGS=-DDATEBLOCK_TEST_NARROW_TIME python3 setup.py -q build \
      -b /tmp/pybuild-narrow --build-lib /tmp/pylib-narrow
   TZ=UTC PYTHONPATH=/tmp/pylib-narrow \
      python3 -m unittest discover -s python/tests

   make dist
   cp dateblock-$version.tar.gz /out/
   ;;
rpm)
   make dist
   rpmbuild -ba --define "_topdir /tmp/rpmbuild" --define "_sourcedir $PWD" \
      packaging/redhat/dateblock.spec
   cp /tmp/rpmbuild/RPMS/*/*.rpm /tmp/rpmbuild/SRPMS/*.rpm /out/
   ;;
deb)
   # Debian builds from the release tarball plus a debian/ directory
   make dist
   mkdir -p /tmp/deb
   cp dateblock-$version.tar.gz /tmp/deb/dateblock_$version.orig.tar.gz
   cd /tmp/deb
   tar -xzf dateblock_$version.orig.tar.gz
   cp -a /tmp/src/packaging/debian dateblock-$version/debian
   (cd dateblock-$version && dpkg-buildpackage -us -uc)
   lintian --info dateblock_*.changes || true
   cp *.deb *.dsc *.orig.tar.gz *.debian.tar.* /out/
   ;;
arch)
   # makepkg uses the tarball in place of downloading the release
   make dist
   mkdir -p /tmp/arch
   cp dateblock-$version.tar.gz packaging/arch/PKGBUILD /tmp/arch/
   cd /tmp/arch
   makepkg --force
   cp *.pkg.tar.* /out/
   ;;
*)
   echo "Unknown mode: $mode (expected build, rpm, deb, arch or man)" >&2
   exit 2
   ;;
esac
