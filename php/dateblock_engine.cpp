/*
 Dateblock blocks (sleeps) until a cron-style moment in time.
 Copyright (C) 2013-2026 Chris Caron <lead2gold@gmail.com>

 This file is part of Dateblock.  Dateblock is free software; you can
 redistribute it and/or modify it under the terms of the GNU General Public
 License as published by the Free Software Foundation; either version 2 of
 the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License along with
 this program; if not, see <https://www.gnu.org/licenses/>.
*/
// Builds the dateblock engine into the extension (when it isn't linked
// against the installed libdateblock).  Compiling it from here keeps its
// object file in this directory, apart from the C++ library build.
#undef HAVE_CONFIG_H
#include "Date.cpp"
