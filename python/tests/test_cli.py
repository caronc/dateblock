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
"""Tests for the Python dateblock command (python -m dateblock)."""
import contextlib
import io
import re
import time
import unittest

from dateblock import cli


def run(*argv):
    """Runs the command and returns (exit code, stdout, stderr)."""
    out, err = io.StringIO(), io.StringIO()
    with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
        try:
            code = cli.main(list(argv))
        except SystemExit as e:
            code = e.code
    return code, out.getvalue(), err.getvalue()


class CommandTests(unittest.TestCase):

    def test_test_mode_shows_when_it_would_unblock(self):
        code, out, _ = run("-t", "-c", "* /5")
        self.assertEqual(code, 0)
        self.assertRegex(out, r"Current Time : \d{4}-\d\d-\d\d ")
        until = re.search(r"Block Until  : \S+ \d\d:(\d\d):(\d\d)", out)
        self.assertIsNotNone(until)
        self.assertEqual(int(until.group(1)) % 5, 0)
        self.assertEqual(until.group(2), "00")

    def test_field_options_match_the_cron_string(self):
        def until(*argv):
            return re.search(r"Block Until.*", run(*argv)[1]).group(0)
        self.assertEqual(until("-t", "-n", "5", "-x", "20"),
                         until("-t", "-c", "* 5 +20"))

    def test_isc_format(self):
        code, out, _ = run("-t", "-i", "-c", "0 9 * * 1-5")
        self.assertEqual(code, 0)
        self.assertIn("09:00:00", out)

    def test_blocks_until_next_second(self):
        start = time.monotonic()
        self.assertEqual(run("-s", "/1")[0], 0)
        self.assertLess(time.monotonic() - start, 1.5)

    def test_errors_exit_with_one(self):
        self.assertEqual(run("-t", "-c", "* 99")[0], 1)
        self.assertEqual(run("-t", "-c", "0 0 0 31 2 *")[0], 1)
        self.assertEqual(run("-t")[0], 1)

    def test_bad_command_line_exits_with_255(self):
        self.assertEqual(run("--bogus")[0], 255)


if __name__ == '__main__':
    unittest.main()
