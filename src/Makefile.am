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

# Shared library holding the date/cron logic (used by the dateblock tool and
# the Python bindings)
lib_LTLIBRARIES = libdateblock.la
libdateblock_la_SOURCES = Date.cpp
libdateblock_la_LDFLAGS = -version-info $(DATEBLOCK_LT_VERSION) -no-undefined

dateblockincludedir = $(includedir)/dateblock
dateblockinclude_HEADERS = Date.h

bin_PROGRAMS = dateblock
dateblock_SOURCES = dateblock.cpp dateblock.h
dateblock_LDADD = libdateblock.la

# Installing into a staging folder (make install DESTDIR=..., as every
# package build does) makes libtool print harmless warnings: the library
# isn't in its final folder yet, and ldconfig can't run there.  Keep those
# quiet while installing, but not while compiling or linking (each target's
# own flags below replace AM_LIBTOOLFLAGS for those steps).
# ('--warnings none' rather than '--no-warnings': libtool 2.4.x, as in EL9
# and EL10, ignores the latter)
AM_LIBTOOLFLAGS = --warnings none
libdateblock_la_LIBTOOLFLAGS =
dateblock_LIBTOOLFLAGS =

