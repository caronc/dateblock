#!/bin/bash
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
# this program; if not, see <https://www.gnu.org/licenses/>.
#
# Writes the files to publish for a tagged release into /out (run by
# 'make release-recipes VERSION=x.y.z', inside the Arch Linux container):
#  - aur/PKGBUILD and aur/.SRCINFO: for the Arch User Repository
#  - homebrew/dateblock.rb: the Homebrew formula
# Both download GitHub's tarball of the vx.y.z tag, so the checksum of that
# tarball is filled in (the tag has to be pushed first).
set -eo pipefail

version=$1
github=https://github.com/caronc/dateblock/archive

# The checksum of a tarball on GitHub
checksum() {
   curl -fsSL "$1" | sha256sum | cut -d' ' -f1
}

grep -q "^pkgver=$version\$" /src/packaging/arch/PKGBUILD || {
   echo "packaging/arch/PKGBUILD is not at version $version" >&2
   exit 1
}
if ! aur_sum=$(checksum "$github/v$version.tar.gz") ||
   ! brew_sum=$(checksum "$github/refs/tags/v$version.tar.gz"); then
   echo "GitHub has no v$version tag yet; push it first" >&2
   exit 1
fi

# AUR: the PKGBUILD with the real checksum (the copy in packaging/arch keeps
# 'SKIP' so local builds can use their own tarball), plus .SRCINFO
mkdir -p /out/aur /tmp/aur
sed -e "s/^sha256sums=('SKIP')\$/sha256sums=('$aur_sum')/" \
    -e "/^# Replace with the real checksum/d" \
    /src/packaging/arch/PKGBUILD > /tmp/aur/PKGBUILD
(cd /tmp/aur && makepkg --printsrcinfo > .SRCINFO)
cp /tmp/aur/PKGBUILD /tmp/aur/.SRCINFO /out/aur/

# Homebrew: the formula with the version and checksum filled in
mkdir -p /out/homebrew
sed -e '/^# @VERSION@ and @SHA256@ are filled in/,+1d' -e '/^#$/d' \
    -e "s/@VERSION@/$version/g" -e "s/@SHA256@/$brew_sum/g" \
    /src/packaging/homebrew/dateblock.rb > /out/homebrew/dateblock.rb

echo "AUR:      $aur_sum"
echo "Homebrew: $brew_sum"
