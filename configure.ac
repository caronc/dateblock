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
AC_PREREQ([2.69])
# The version comes from __version__ in python/dateblock/__init__.py, the
# one place it is set
AC_INIT([dateblock],
        m4_esyscmd_s([sed -n 's/^__version__ = "\(.*\)"$/\1/p' python/dateblock/__init__.py]),
        [lead2gold@gmail.com], [dateblock],
        [https://github.com/caronc/dateblock])

AC_CONFIG_SRCDIR([src/Date.cpp])
AC_CONFIG_HEADERS([src/config.h])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([foreign subdir-objects])

AC_PROG_CXX
LT_INIT([disable-static])

# Shared library version (current:revision:age); see the libtool manual
# before changing this.  It produces libdateblock.so.1
AC_SUBST([DATEBLOCK_LT_VERSION], [1:0:0])

AC_CONFIG_FILES([Makefile src/Makefile tests/Makefile dateblock.pc])
AC_OUTPUT
