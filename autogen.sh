#!/bin/sh
# Generates the ./configure script (and friends) from configure.ac
set -e
cd "$(dirname "$0")"
autoreconf --force --install
