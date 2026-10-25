# -*- coding: utf-8 -*-
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
"""Tests for the dateblock Python module (run with: python3 -m unittest)."""
import time as _time
import unittest
from datetime import date, datetime, time, timedelta, timezone

import dateblock
from dateblock import dateblock as db


class DateblockTests(unittest.TestCase):

    def test_datetime_reference(self):
        ref = datetime(2000, 5, 3, 10, 10, 0)
        self.assertEqual(db("*/1", ref=ref, block=False),
                         datetime(2000, 5, 3, 10, 10, 1))

    def test_datetime_plus_timedelta_reference(self):
        ref = datetime(2000, 5, 3, 10, 10, 0) + timedelta(days=1)
        self.assertEqual(db("*/1", ref=ref, block=False),
                         datetime(2000, 5, 4, 10, 10, 1))

    def test_date_reference_starts_at_midnight(self):
        self.assertEqual(db("*/1", ref=date(2000, 5, 3), block=False),
                         datetime(2000, 5, 3, 0, 0, 1))

    def test_time_reference_uses_today(self):
        result = db("*/1", ref=time(20, 5, 3), block=False)
        self.assertEqual(result.time(), time(20, 5, 4))
        self.assertEqual(result.date(), date.today())

    def test_epoch_reference(self):
        epoch = 7999434323
        expected = datetime.fromtimestamp(epoch) + timedelta(seconds=1)
        self.assertEqual(db("*/1", ref=epoch, block=False), expected)
        self.assertEqual(db("*/1", ref=float(epoch), block=False), expected)

    def test_timezone_aware_reference(self):
        ref = datetime(2000, 1, 1, tzinfo=timezone.utc)
        expected = datetime.fromtimestamp(ref.timestamp() + 1)
        self.assertEqual(db("*/1", ref=ref, block=False), expected)

    def test_timedelta_reference_future(self):
        actual = db("*/1", ref=timedelta(seconds=5), block=False)
        expected = db("*/1", ref=datetime.now() + timedelta(seconds=5),
                      block=False)
        self.assertLessEqual(abs((actual - expected).total_seconds()), 1)

    def test_timedelta_reference_past(self):
        actual = db("*/1", ref=timedelta(seconds=-5), block=False)
        expected = db("*/1", ref=datetime.now() + timedelta(seconds=-5),
                      block=False)
        self.assertLessEqual(abs((actual - expected).total_seconds()), 1)

    def test_no_reference_means_now(self):
        # Repeat so that some calls land right on a second boundary
        for _ in range(500):
            before = datetime.now().replace(microsecond=0)
            result = db("*/1", ref=None, block=False)
            self.assertGreater(result, before)
            self.assertLessEqual(result, before + timedelta(seconds=2))

    def test_inline_drift(self):
        self.assertEqual(db("*/10 +5", ref=date(2000, 1, 1), block=False),
                         datetime(2000, 1, 1, 0, 0, 5))

    def test_drift_field(self):
        self.assertEqual(
            db("*/10 * * * * * 5", ref=date(2000, 2, 1), block=False),
            datetime(2000, 2, 1, 0, 0, 5))

    def test_multiple_drifts_pick_the_earliest(self):
        self.assertEqual(db("* 10 +5,8", ref=date(2000, 3, 1), block=False),
                         datetime(2000, 3, 1, 0, 10, 5))

    def test_isc_format_has_no_seconds_field(self):
        ref = datetime(2000, 1, 1, 0, 0, 30)
        self.assertEqual(db("/1", isc=True, ref=ref, block=False),
                         datetime(2000, 1, 1, 0, 1, 0))

    def test_past_reference_does_not_block(self):
        start = _time.monotonic()
        db("*/10 +7", ref=999434323, block=True)
        self.assertLess(_time.monotonic() - start, 1)

    def test_blocks_until_next_second(self):
        start = _time.monotonic()
        result = db("/1")
        self.assertLess(_time.monotonic() - start, 1.5)
        self.assertLessEqual(result, datetime.now())

    def test_invalid_cron_raises_syntax_error(self):
        with self.assertRaises(SyntaxError):
            db("* 99", block=False)
        with self.assertRaises(SyntaxError):
            db("* 5 +20 1", block=False)

    def test_impossible_cron_raises_value_error(self):
        with self.assertRaises(ValueError):
            db("0 0 0 31 2 *", block=False)

    def test_impossible_combination_is_skipped(self):
        self.assertEqual(
            db("0 0 0 30,31 2,3 *", ref=date(2026, 10, 24), block=False),
            datetime(2027, 3, 30))

    def test_leap_day(self):
        self.assertEqual(db("0 0 0 29 2", ref=date(2097, 3, 1), block=False),
                         datetime(2104, 2, 29))

    def test_leap_day_on_a_weekday(self):
        # The next Feb 29th that is a Monday is 31 years away
        self.assertEqual(
            db("0 0 0 29 2 1", ref=date(2081, 3, 1), block=False),
            datetime(2112, 2, 29))

    def test_century_leap_years(self):
        # 2100 is not a leap year, 2400 is
        self.assertEqual(db("0 0 0 29 2", ref=date(2097, 3, 1), block=False),
                         datetime(2104, 2, 29))
        self.assertEqual(db("0 0 0 29 2", ref=date(2399, 3, 1), block=False),
                         datetime(2400, 2, 29))

    def test_leap_day_and_weekday_list(self):
        self.assertEqual(
            db("0 0 0 29 2 1,3,5", ref=date(2025, 3, 1), block=False),
            datetime(2036, 2, 29))

    def test_isc_leap_day_or_weekday(self):
        # Feb 29th OR any Monday in February
        self.assertEqual(
            db("0 0 29 2 1", isc=True, ref=date(2025, 3, 1), block=False),
            datetime(2026, 2, 2))
        # A '*' weekday means only Feb 29th counts
        self.assertEqual(
            db("0 0 29 2 *", isc=True, ref=date(2025, 3, 1), block=False),
            datetime(2028, 2, 29))

    def test_isc_impossible_day_matches_its_weekday(self):
        self.assertEqual(
            db("0 0 30 2 1", isc=True, ref=date(2025, 3, 1), block=False),
            datetime(2026, 2, 2))
        with self.assertRaises(ValueError):
            db("0 0 30 2 *", isc=True, block=False)

    def test_long_drift_list_on_a_rare_day_is_quick(self):
        start = _time.monotonic()
        self.assertEqual(
            db("0 0 0 29 2 1 0-9999", ref=date(2025, 6, 1), block=False),
            datetime(2044, 2, 29))
        self.assertLess(_time.monotonic() - start, 1)

    def test_drift_list_matches_each_drift_alone(self):
        ref = datetime(2024, 2, 29, 23, 59, 59)
        drifts = ["0", "61", "3600", "90061", "31622400", "2147483647"]
        for cron in ("0 /10", "0 0 0 29 2", "/7 /11 3,15 * 2,8"):
            alone = min(db("%s +%s" % (cron, d), ref=ref, block=False)
                        for d in drifts)
            self.assertEqual(
                db("%s +%s" % (cron, ",".join(drifts)), ref=ref,
                   block=False),
                alone)

    def test_empty_list_entries_raise_syntax_error(self):
        for cron in (",", "5,", "5,,6", "* 5 +"):
            with self.assertRaises(SyntaxError):
                db(cron, block=False)

    def test_result_past_year_9999_raises_before_blocking(self):
        start = _time.monotonic()
        with self.assertRaises(ValueError):
            db("0 0 0 1 1", ref=datetime(9999, 6, 1))
        self.assertLess(_time.monotonic() - start, 1)

    def test_day_of_month_and_week_both_match(self):
        self.assertEqual(db("0 0 0 1 * 1", ref=date(2025, 6, 3), block=False),
                         datetime(2025, 9, 1))

    def test_isc_day_of_month_or_week_matches(self):
        self.assertEqual(
            db("0 0 1 * 1", isc=True, ref=date(2025, 6, 3), block=False),
            datetime(2025, 6, 9))

    def test_isc_step_counts_from_lowest_value(self):
        self.assertEqual(
            db("0 0 */5 * *", isc=True, ref=date(2025, 6, 2), block=False),
            datetime(2025, 6, 6))

    def test_range_step(self):
        ref = datetime(2025, 1, 15, 3, 2, 0)
        self.assertEqual(db("0 1-10/3", ref=ref, block=False),
                         datetime(2025, 1, 15, 3, 4, 0))

    def test_month_only_starts_on_the_first(self):
        self.assertEqual(db("* * * * 3", ref=date(2025, 1, 15), block=False),
                         datetime(2025, 3, 1))

    def test_full_field_lists_are_quick(self):
        start = _time.monotonic()
        self.assertEqual(
            db("0-59 0-59 0-23 1-31 1-12 0-6", ref=date(2025, 1, 15),
               block=False),
            datetime(2025, 1, 15, 0, 0, 1))
        self.assertLess(_time.monotonic() - start, 2)

    def test_huge_drift_list_raises_syntax_error(self):
        with self.assertRaises(SyntaxError):
            db("* * * * * * 0-2147483647", block=False)

    def test_out_of_range_float_reference(self):
        for ref in (float("nan"), float("inf"), -float("inf"), 1e300):
            with self.assertRaises(OverflowError):
                db("*/1", ref=ref, block=False)

    def test_bad_argument_types(self):
        with self.assertRaises(TypeError):
            db(5, block=False)
        with self.assertRaises(TypeError):
            db("*/1", ref="yesterday", block=False)

    def test_constants(self):
        self.assertEqual(dateblock.SEC_MAX, 59)
        self.assertEqual(dateblock.DOW_MIN, 0)
        self.assertEqual(dateblock.MONTH_MAX, 12)


if __name__ == '__main__':
    unittest.main()
