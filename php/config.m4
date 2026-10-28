dnl Dateblock blocks (sleeps) until a cron-style moment in time.
dnl Copyright (C) 2013-2026 Chris Caron <lead2gold@gmail.com>
dnl
dnl This file is part of Dateblock.  Dateblock is free software; you can
dnl redistribute it and/or modify it under the terms of the GNU General Public
dnl License as published by the Free Software Foundation; either version 2 of
dnl the License, or (at your option) any later version.
dnl
dnl This program is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl GNU General Public License for more details.
dnl
dnl You should have received a copy of the GNU General Public License along with
dnl this program; if not, see <https://www.gnu.org/licenses/>.
dnl
dnl The PHP extension; build it with: phpize && ./configure && make
dnl  - By default the dateblock engine (../src/Date.cpp) is built in, so
dnl    nothing else needs to be installed (this is what PIE does).
dnl  - --with-libdateblock links the installed libdateblock instead (the
dnl    distribution packages do this).  Point LDFLAGS at it if it isn't in
dnl    a standard place, ie: LDFLAGS=-L../src/.libs

PHP_ARG_ENABLE([dateblock],
  [whether to enable dateblock support],
  [AS_HELP_STRING([--enable-dateblock], [Enable dateblock support])],
  [yes])

PHP_ARG_WITH([libdateblock],
  [whether to link against the installed libdateblock],
  [AS_HELP_STRING([--with-libdateblock],
    [Link against the installed libdateblock instead of building the
     dateblock engine in])],
  [no],
  [no])

if test "$PHP_DATEBLOCK" != "no"; then
  dnl The version comes from the same place as everything else's
  dateblock_srcdir=PHP_EXT_SRCDIR(dateblock)
  DATEBLOCK_VERSION=`sed -n 's/^__version__ = "\(.*\)"$/\1/p' \
    "$dateblock_srcdir/../python/dateblock/__init__.py"`
  if test -z "$DATEBLOCK_VERSION"; then
    AC_MSG_ERROR([could not read the version from ../python/dateblock/__init__.py])
  fi
  AC_DEFINE_UNQUOTED([PHP_DATEBLOCK_VERSION], ["$DATEBLOCK_VERSION"],
    [The dateblock version])

  dnl The C++ compiler links its own runtime (libstdc++, or libc++ on macOS)
  PHP_REQUIRE_CXX()

  if test "$PHP_LIBDATEBLOCK" != "no"; then
    PHP_ADD_LIBRARY(dateblock, 1, DATEBLOCK_SHARED_LIBADD)
    dateblock_sources="dateblock.cpp"
  else
    dateblock_sources="dateblock.cpp dateblock_engine.cpp"
  fi
  PHP_SUBST(DATEBLOCK_SHARED_LIBADD)

  PHP_NEW_EXTENSION(dateblock, $dateblock_sources, $ext_shared,,
    [-I@ext_srcdir@/../src -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1], [cxx])
fi
