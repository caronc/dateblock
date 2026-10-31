// Dateblock blocks (sleeps) until a cron-style moment in time.
// Copyright (C) 2013-2026 Chris Caron <lead2gold@gmail.com>
//
// This file is part of Dateblock.  Dateblock is free software; you can
// redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation; either version 2 of
// the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along with
// this program; if not, see <https://www.gnu.org/licenses/>.
'use strict';

// This computer's name in platforms.json (ie: "linux-x64-gnu"); its
// ready-built add-on is the package @caronc/dateblock-<name>
module.exports = function platformKey() {
  let key = `${process.platform}-${process.arch}`;
  if (process.platform === 'linux') {
    // Node.js reports the glibc version it runs on; musl (ie: Alpine)
    // has none to report
    let glibc;
    try {
      glibc = process.report.getReport().header.glibcVersionRuntime;
    } catch {
      glibc = undefined;
    }
    key += glibc ? '-gnu' : '-musl';
  }
  return key;
};
