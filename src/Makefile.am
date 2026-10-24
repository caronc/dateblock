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

# Shared library holding the date/cron logic (used by the dateblock tool and
# the Python bindings)
lib_LTLIBRARIES = libdateblock.la
libdateblock_la_SOURCES = Date.cpp
libdateblock_la_LDFLAGS = -version-info $(DATEBLOCK_LT_VERSION) -no-undefined

dateblockincludedir = $(includedir)/dateblock
dateblockinclude_HEADERS = Date.h

bin_PROGRAMS = dateblock
dateblock_SOURCES = dateblock.cpp dateblock.h
dateblock_LDADD = libdateblock.la $(BOOST_PROGRAM_OPTIONS_LIBS)

