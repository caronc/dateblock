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

# 'make check' runs these against the library and tool built in src/
AM_CPPFLAGS = -I$(top_srcdir)/src

check_PROGRAMS = unittest unittest-narrow unittest-macos
unittest_SOURCES = unittest.cpp
unittest_LDADD = $(top_builddir)/src/libdateblock.la

# The same tests, with the library acting like systems whose own date
# functions give up on dates far from today (the code works around that):
#  - Windows: they only work from 1970 to the year 3000
#  - macOS: mktime() refuses years before 1900
unittest_narrow_SOURCES = unittest.cpp $(top_srcdir)/src/Date.cpp
unittest_narrow_CPPFLAGS = $(AM_CPPFLAGS) -DDATEBLOCK_TEST_NARROW_TIME
unittest_macos_SOURCES = unittest.cpp $(top_srcdir)/src/Date.cpp
unittest_macos_CPPFLAGS = $(AM_CPPFLAGS) -DDATEBLOCK_TEST_OLD_MKTIME

dist_check_SCRIPTS = cli-test.sh
TESTS = unittest unittest-narrow unittest-macos cli-test.sh
AM_TESTS_ENVIRONMENT = DATEBLOCK='$(top_builddir)/src/dateblock'; export DATEBLOCK;
