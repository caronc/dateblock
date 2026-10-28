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
# How npm (node-gyp) builds the Node.js add-on when the package is
# installed: node/dateblock.cpp plus the dateblock engine (src/Date.cpp).
# C++ exceptions are turned on everywhere; the engine reports bad input
# with them.  C++17 is used on Linux: GCC 12 (Debian 12) gives a false
# warning (-Wrestrict) when building the engine as C++20, node-gyp's
# default, and nothing here needs C++20.
{
  "targets": [
    {
      "target_name": "dateblock",
      "sources": ["node/dateblock.cpp", "src/Date.cpp"],
      "include_dirs": ["src"],
      "defines": ["NAPI_VERSION=8"],
      "cflags_cc!": ["-fno-exceptions", "-std=gnu++20"],
      "cflags_cc": ["-std=gnu++17"],
      "xcode_settings": {
        "GCC_ENABLE_CPP_EXCEPTIONS": "YES",
        "CLANG_CXX_LIBRARY": "libc++",
        "MACOSX_DEPLOYMENT_TARGET": "10.15"
      },
      "msvs_settings": {
        "VCCLCompilerTool": {"ExceptionHandling": 1}
      }
    }
  ]
}
