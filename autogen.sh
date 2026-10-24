#!/bin/sh
# Generates the ./configure script (and friends) from configure.ac
set -e
cd "$(dirname "$0")"

# Homebrew (macOS) installs libtoolize as glibtoolize
if ! command -v libtoolize >/dev/null 2>&1 && \
      command -v glibtoolize >/dev/null 2>&1; then
   export LIBTOOLIZE=glibtoolize
fi

autoreconf --force --install
