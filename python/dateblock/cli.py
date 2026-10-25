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
"""
The ``dateblock`` command, for installs from PyPI (pip install dateblock).

It takes the same options as the C++ dateblock tool shipped with the
distribution packages; see ``dateblock --help`` or ``man dateblock``.

Exit codes:
 - 0:   the time was reached (or worked out, with --test)
 - 1:   bad or impossible schedule, or an unknown timezone
 - 130: interrupted (Ctrl-C)
 - 255: the command line could not be parsed
"""
import argparse
import sys
from datetime import datetime
from zoneinfo import ZoneInfo

from . import __version__
from ._dateblock import dateblock

EXAMPLES = """\
examples:
  dateblock -n /10              the next minute divisible by 10
  dateblock -c '/5 * 3,4'       every 5th second, during hours 3 and 4
  dateblock -c '* 5 +20'        20 seconds after 5 minutes past the hour
  dateblock -i -c '0 9 * * 1-5' 9:00 on weekdays (regular crontab entry)
  dateblock -t -c '0 30 2'      when would 2:30:00 next happen?
  dateblock -z Asia/Tokyo -c '0 0 9'
                                9:00 in Tokyo, whatever timezone this
                                computer is in

cron string fields (--cron):
  sec min hour day-of-month month day-of-week drift
  Each field accepts x, */x (or /x), x-y, x-y/z and x,y; '*' (on its
  own) matches anything.
  Missing fields are filled in with '*'.  An entry such as '+20' jumps
  straight to the drift field.
"""

# Field options, in cron string order (the drift comes last)
FIELDS = (
    ("second", "s", "second(s) to unblock on (0-59)"),
    ("minute", "n", "minute(s) to unblock on (0-59)"),
    ("hour", "o", "hour(s) to unblock on (0-23)"),
    ("dom", "d", "day(s) of the month to unblock on (1-31)"),
    ("month", "m", "month(s) to unblock on (1-12; Jan=1)"),
    ("dow", "w", "day(s) of the week to unblock on (0-6; Sun=0)"),
    ("drift", "x", "extra seconds to wait once the time is matched"),
)

TIME_FORMAT = "%Y-%m-%d %H:%M:%S"


class _Parser(argparse.ArgumentParser):
    """Exits with 255 on a bad command line, like the C++ tool."""

    def error(self, message):
        self.print_usage(sys.stderr)
        self.exit(255, "%s: error: %s\n" % (self.prog, message))


def build_parser():
    parser = _Parser(
        prog="dateblock",
        description="Block (sleep) until a cron-style moment in time.",
        epilog=EXAMPLES,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("-t", "--test", action="store_true",
                        help="only show when we would unblock; don't wait")
    parser.add_argument("-v", "--verbose", action="store_true",
                        help="show the times before and after blocking")
    parser.add_argument("-i", "--isc", action="store_true",
                        help="read --cron as a regular 5 field crontab entry")
    parser.add_argument("-c", "--cron",
                        help="the whole schedule as one cron string")
    parser.add_argument("-z", "--timezone",
                        help="timezone to read the schedule in "
                             "(ie: America/Toronto); defaults to this "
                             "computer's")
    for name, short, text in FIELDS:
        parser.add_argument("-" + short, "--" + name, help=text)
    parser.add_argument("-V", "--version", action="version",
                        version="%(prog)s " + __version__)
    return parser


def _zone(name):
    """The timezone called 'name', or None (after saying why) if it is
    unknown."""
    try:
        return ZoneInfo(name)
    except Exception:
        hint = ""
        if sys.platform == "win32":
            hint = " (pip install tzdata adds the timezone database)"
        print("Error: Unknown timezone '%s'%s" % (name, hint),
              file=sys.stderr)
        return None


def main(argv=None):
    parser = build_parser()
    args = parser.parse_args(argv)

    fields = [getattr(args, name) for name, _, _ in FIELDS]
    if args.cron is None and all(f is None for f in fields[:-1]):
        print("Error: You must specify at least one option.", file=sys.stderr)
        parser.print_usage(sys.stderr)
        return 1

    if args.cron is not None:
        cron, isc = args.cron, args.isc
    else:
        cron = " ".join("*" if f is None else f for f in fields)
        isc = False

    zone, fmt = None, TIME_FORMAT
    if args.timezone is not None:
        zone = _zone(args.timezone)
        if zone is None:
            return 1
        fmt += " %Z"

    start = datetime.now(zone).replace(microsecond=0)
    try:
        finish = dateblock(cron, block=False, ref=start, isc=isc)
    except SyntaxError:
        print("Error: Syntax Invalid : '%s'" % cron, file=sys.stderr)
        return 1
    except ValueError as e:
        print("Error: %s" % e, file=sys.stderr)
        return 1

    if args.verbose or args.test:
        print("Current Time : %s" % start.strftime(fmt + " (%a)"))
        print("Block Until  : %s" % finish.strftime(fmt + " (%a)"))

    if not args.test:
        try:
            dateblock(cron, ref=start, isc=isc)
        except KeyboardInterrupt:
            return 130
        if args.verbose:
            print("UnBlock Time : %s" % datetime.now(zone).strftime(fmt))
    return 0


if __name__ == "__main__":
    sys.exit(main())
