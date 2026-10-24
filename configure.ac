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
AC_PREREQ([2.69])
AC_INIT([dateblock], [1.1.0], [lead2gold@gmail.com], [dateblock],
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

# Boost is only needed by the command line tool (option parsing)
AC_LANG_PUSH([C++])
AC_CHECK_HEADERS([boost/program_options.hpp], [],
    [AC_MSG_ERROR([You need the Boost program_options library.])])

dateblock_save_LIBS="$LIBS"
LIBS="$LIBS -lboost_program_options"
AC_MSG_CHECKING([for the boost_program_options library])
AC_LINK_IFELSE(
    [AC_LANG_PROGRAM([[#include <boost/program_options.hpp>]],
                     [[boost::program_options::options_description o("x");]])],
    [AC_MSG_RESULT([yes])],
    [AC_MSG_RESULT([no])
     AC_MSG_ERROR([You need the Boost program_options library.])])
LIBS="$dateblock_save_LIBS"
AC_SUBST([BOOST_PROGRAM_OPTIONS_LIBS], [-lboost_program_options])
AC_LANG_POP([C++])

AC_CONFIG_FILES([Makefile src/Makefile tests/Makefile dateblock.pc])
AC_OUTPUT
