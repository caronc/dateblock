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
"""
Block (sleep) until a cron-style moment in time.

    >>> from dateblock import dateblock
    >>> dateblock("0 */5", block=False)    # when the next 5th minute starts
    datetime.datetime(2026, 10, 24, 13, 5)
    >>> dateblock("0 */5")                 # wait for it

Run ``help(dateblock)`` for all of the options.
"""

# The package details.
__title__ = "dateblock"
__description__ = "Block (sleep) until a cron-style moment in time"
__version__ = "2.0.0"
__author__ = "Chris Caron"
__email__ = "lead2gold@gmail.com"
__license__ = "GPL-2.0-or-later"
__copyright__ = "Copyright (C) 2013-2026 Chris Caron <lead2gold@gmail.com>"
__status__ = "Production"

from ._dateblock import (  # noqa: E402,F401
    dateblock,
    SEC_MIN, SEC_MAX,
    MIN_MIN, MIN_MAX,
    HOUR_MIN, HOUR_MAX,
    DOM_MIN, DOM_MAX,
    MONTH_MIN, MONTH_MAX,
    YEAR_MIN, YEAR_MAX,
    DOW_MIN, DOW_MAX,
    DRIFT_MIN, DRIFT_MAX,
)
