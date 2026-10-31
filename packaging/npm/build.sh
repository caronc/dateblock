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
# Builds and tests the Node.js add-on on this computer, packs the npm
# packages into <folder>, then checks they install and work:
#  - @caronc/dateblock itself, and the ready-built add-on for this computer
#    (ie: @caronc/dateblock-linux-x64-gnu)
#  - installed together, the ready-built add-on is used (nothing is built)
#  - installed without it, the error says how to build it, and that works
# Usage (from the top of the source tree): packaging/npm/build.sh <folder>
# The GitHub workflow runs it on each platform; 'make docker-npm' runs it
# in a container.
set -euo pipefail

out=${1:?Usage: packaging/npm/build.sh <folder>}
mkdir -p "$out"
out=$(cd "$out" && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

# Windows' Node.js wants Windows paths (C:/...), not the shell's (/tmp/...)
native() {
   if command -v cygpath >/dev/null; then cygpath -m "$1"; else echo "$1"; fi
}

fail() {
   echo "FAIL: $*" >&2
   exit 1
}

os=$(node -p process.platform)
key=$(node -p "require('./node/platform.js')()")
version=$(node -p "require('./package.json').version")
echo "Building @caronc/dateblock $version for $key"

# On musl (ie: Alpine) the C++ runtime is built into the add-on, so it
# doesn't matter which version of it the computer has
case "$key" in
*-musl) export LDFLAGS="-static-libstdc++ -static-libgcc" ;;
esac

npm run build
if [ "$os" = win32 ]; then
   npm test
else
   TZ=UTC npm test
   TZ=America/New_York npm test
fi

node packaging/npm/stage.js main "$(native "$work/main")"
node packaging/npm/stage.js platform "$(native "$work/platform")"

# Nothing may run while installing (npm 12 would ask for it to be approved
# first): no install scripts, and no binding.gyp at the top (npm takes that
# as one)
for staged in "$work/main" "$work/platform"; do
   node -e '
      const scripts = require(process.argv[1]).scripts || {};
      for (const step of ["preinstall", "install", "postinstall"])
        if (scripts[step]) throw new Error(`it has a ${step} script`);
   ' "$(native "$staged/package.json")" || fail "$staged runs a script when installed"
   [ ! -e "$staged/binding.gyp" ] || fail "$staged has a binding.gyp at the top"
done

main_tgz="$out/caronc-dateblock-$version.tgz"
platform_tgz="$out/caronc-dateblock-$key-$version.tgz"
(cd "$work/main" && npm pack --pack-destination "$(native "$out")")
(cd "$work/platform" && npm pack --pack-destination "$(native "$out")")
[ -f "$main_tgz" ] && [ -f "$platform_tgz" ] || fail "npm pack wrote other file names"

# Runs the tests against the copy installed in project folder $1
test_installed() {
   local installed="$1/node_modules/@caronc/dateblock"
   mkdir -p "$installed/node/test"
   cp node/test/*.test.js "$installed/node/test/"
   (cd "$installed" && node --test node/test/*.test.js) ||
      fail "the tests failed against the installed package"
}

new_project() {
   mkdir -p "$1"
   (cd "$1" && npm init -y >/dev/null)
}

echo "Checking it installs with the ready-built add-on"
ready="$work/ready"
new_project "$ready"
(cd "$ready" && npm install --no-audit --no-fund \
   "$(native "$main_tgz")" "$(native "$platform_tgz")")
[ ! -e "$ready/node_modules/@caronc/dateblock/node/build" ] ||
   fail "something was built while installing"
test_installed "$ready"

echo "Checking it can be built where there is no ready-built add-on"
built="$work/built"
new_project "$built"
(cd "$built" && npm install --no-audit --no-fund --omit=optional \
   "$(native "$main_tgz")")
[ ! -e "$built/node_modules/@caronc/dateblock-$key" ] ||
   fail "--omit=optional still installed the ready-built add-on"
if (cd "$built" && node -e "require('@caronc/dateblock')") 2>"$work/error"; then
   fail "it loaded with no add-on at all"
fi
grep -q 'npm explore @caronc/dateblock -- npm run build' "$work/error" ||
   fail "the error didn't say how to build it: $(cat "$work/error")"
(cd "$built" && npm explore @caronc/dateblock -- npm run build)
test_installed "$built"

echo "npm packages written to $out:"
ls -1 "$out"
