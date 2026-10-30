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
"""The asyncio version of dateblock(): ``await dateblock.wait(...)``."""

import asyncio
import time

from ._dateblock import _next

# How often a long wait checks the clock again (in seconds).  asyncio's
# timers stop counting while the computer sleeps and don't notice the
# clock being changed, so a long wait is made of short sleeps that each
# look at the real time when they wake.
CHECK_EVERY = 60


async def wait(cron, ref=None, isc=False):
    """Wait (without blocking asyncio) until the next moment that matches a
    cron string, and return it.

    - cron: a dateblock cron string ('sec min hour dom month dow drift'),
      or a regular crontab string when isc=True.
    - ref:  calculate relative to this datetime, date, time, timedelta or
      epoch value instead of now.
    - isc:  treat the cron string as a regular 5 field crontab entry.

    Returns the same datetime as ``dateblock(cron, block=False, ...)`` and
    raises the same errors.  Other tasks keep running while it waits;
    cancel the task (or use ``asyncio.wait_for()``) to stop waiting.
    """
    result, when = _next(cron, ref=ref, isc=isc)
    while True:
        left = when - time.time()
        if left <= 0:
            return result
        await asyncio.sleep(min(left, CHECK_EVERY))
