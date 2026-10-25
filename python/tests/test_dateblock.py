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
import os
import random
import time as _time
import unittest
from datetime import date, datetime, time, timedelta, timezone, tzinfo

import dateblock
from dateblock import dateblock as db

try:
    from zoneinfo import ZoneInfo
    ZoneInfo("America/New_York")
except Exception:  # pragma: no cover - no timezone data (ie: Windows)
    ZoneInfo = None

needs_zoneinfo = unittest.skipIf(ZoneInfo is None, "no timezone data")


def brute_force(ref, tz, hours, minutes):
    """The first whole minute after 'ref' whose clock time in 'tz' has an
    hour in 'hours' and a minute in 'minutes' (second 0)."""
    t = int(ref.timestamp()) // 60 * 60 + 60
    for _ in range(3 * 24 * 60):
        local = datetime.fromtimestamp(t, tz)
        if local.hour in hours and local.minute in minutes:
            return local
        t += 60
    raise AssertionError("no match within 3 days")


class _BrokenZone(tzinfo):
    """A tzinfo that fails as soon as the local time is asked for."""

    def utcoffset(self, dt):
        return timedelta(0)

    def dst(self, dt):
        raise ZeroDivisionError("broken zone")


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
        result = db("*/1", ref=ref, block=False)
        self.assertEqual(result, ref + timedelta(seconds=1))
        self.assertIs(result.tzinfo, timezone.utc)

    def test_naive_reference_returns_naive(self):
        self.assertIsNone(db("*/1", ref=datetime(2000, 1, 1),
                             block=False).tzinfo)
        self.assertIsNone(db("*/1", block=False).tzinfo)

    def test_cron_is_read_in_the_reference_timezone(self):
        # The same moment, seen from two timezones
        plus9 = timezone(timedelta(hours=9))
        ref = datetime(2025, 1, 1, 0, 0, 30, tzinfo=plus9)
        self.assertEqual(db("0 0 9", ref=ref, block=False),
                         datetime(2025, 1, 1, 9, tzinfo=plus9))
        utc_ref = ref.astimezone(timezone.utc)   # 2024-12-31 15:00:30
        self.assertEqual(db("0 0 9", ref=utc_ref, block=False),
                         datetime(2025, 1, 1, 9, tzinfo=timezone.utc))

    def test_days_follow_the_reference_timezone(self):
        # Feb 29th 00:00 in +14:00 is still Feb 28th in UTC
        plus14 = timezone(timedelta(hours=14))
        result = db("0 0 0 29 2", ref=datetime(2025, 3, 1, tzinfo=plus14),
                    block=False)
        self.assertEqual(result, datetime(2028, 2, 29, tzinfo=plus14))
        self.assertEqual(result.astimezone(timezone.utc).day, 28)

    @unittest.skipUnless(hasattr(_time, "tzset"), "needs time.tzset()")
    def test_computer_timezone_does_not_matter(self):
        ref = datetime(2025, 3, 8, 12, tzinfo=timezone(timedelta(hours=-5)))
        saved = os.environ.get("TZ")
        results = []
        try:
            for tz in ("UTC", "Asia/Tokyo", "America/Los_Angeles"):
                os.environ["TZ"] = tz
                _time.tzset()
                results.append(db("0 30 2", ref=ref, block=False))
        finally:
            if saved is None:
                os.environ.pop("TZ", None)
            else:
                os.environ["TZ"] = saved
            _time.tzset()
        self.assertEqual(len(set(results)), 1)

    @needs_zoneinfo
    def test_distant_future_zone_with_timedelta(self):
        zone = ZoneInfo("America/New_York")
        # The clocks go forward on the second Sunday of March
        first = date(2090, 3, 1)
        change = first + timedelta(days=(6 - first.weekday()) % 7 + 7)
        ref = datetime(2090, 3, 1, tzinfo=zone) + \
            timedelta(days=change.day - 1)
        # 02:30 doesn't exist that day, so it is the next day's 02:30 (EDT)
        result = db("0 30 2", ref=ref, block=False)
        self.assertEqual(result, datetime(2090, 3, change.day + 1, 2, 30,
                                          tzinfo=zone))
        self.assertIs(result.tzinfo, zone)
        self.assertEqual(result.utcoffset(), timedelta(hours=-4))
        # Noon the same day is already daylight saving time
        noon = db("0 0 12", ref=ref, block=False)
        self.assertEqual(noon.utcoffset(), timedelta(hours=-4))
        # Only 11 real hours after midnight (one went missing at 02:00)
        self.assertEqual(noon.timestamp() - ref.timestamp(), 11 * 3600)

    @needs_zoneinfo
    def test_clock_changes_match_brute_force(self):
        crons = (("0 30 2", {2}, {30}), ("0 0 0", {0}, {0}),
                 ("0 15 1", {1}, {15}), ("0 0,45 0-3", {0, 1, 2, 3},
                                         {0, 45}))
        for name in ("America/New_York", "Australia/Lord_Howe",
                     "Asia/Kathmandu", "Pacific/Kiritimati",
                     "America/Santiago", "Europe/London"):
            zone = ZoneInfo(name)
            for year in (2025, 2090):
                for month in range(1, 13):
                    for day in (1, 2, 5, 6, 7, 8, 9, 13, 14, 25, 26, 27, 28):
                        for hour in (0, 1, 2, 23):
                            ref = datetime(year, month, day, hour, 10,
                                           tzinfo=zone)
                            for cron, hours, minutes in crons:
                                with self.subTest(zone=name, ref=ref,
                                                  cron=cron):
                                    self.assertEqual(
                                        db(cron, ref=ref, block=False),
                                        brute_force(ref, zone, hours,
                                                    minutes))

    @needs_zoneinfo
    def test_isc_in_the_reference_timezone(self):
        zone = ZoneInfo("Europe/London")
        ref = datetime(2025, 3, 29, 12, tzinfo=zone)
        # 01:30 on 2025-03-30 doesn't exist in London
        self.assertEqual(db("30 1 * * *", isc=True, ref=ref, block=False),
                         datetime(2025, 3, 31, 1, 30, tzinfo=zone))

    def test_aware_time_reference_uses_today_over_there(self):
        plus14 = timezone(timedelta(hours=14))
        result = db("*/1", ref=time(10, 0, 0, tzinfo=plus14), block=False)
        self.assertIs(result.tzinfo, plus14)
        self.assertEqual(result.time().replace(tzinfo=None), time(10, 0, 1))
        self.assertIn(result.date(),
                      (datetime.now(plus14).date(),
                       datetime.now(plus14).date() - timedelta(days=1)))

    def test_aware_reference_blocks_until_that_moment(self):
        plus14 = timezone(timedelta(hours=14))
        start = _time.monotonic()
        result = db("/1", ref=datetime.now(plus14))
        self.assertLess(_time.monotonic() - start, 1.5)
        self.assertIs(result.tzinfo, plus14)
        self.assertLessEqual(result, datetime.now(timezone.utc))

    def test_tzinfo_error_is_raised(self):
        with self.assertRaises(ZeroDivisionError):
            db("*/1", ref=datetime(2025, 1, 1, tzinfo=_BrokenZone()),
               block=False)

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
        # Windows can't work with dates past the year 3000 at all, so it
        # refuses the reference itself (OverflowError)
        start = _time.monotonic()
        with self.assertRaises((ValueError, OverflowError)):
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

    def test_huge_epoch_reference(self):
        for ref in (2 ** 62, 2 ** 63 - 1, 2 ** 63, -2 ** 62,
                    float(2 ** 63), float(2 ** 62)):
            with self.assertRaises(OverflowError):
                db("*/1", ref=ref, block=False)

    def test_wildcard_mixed_into_a_list_raises_syntax_error(self):
        for cron, isc in (("0 *,5", False), ("0 0 *,13 * 1", True),
                          ("5,* * * * *", True)):
            with self.assertRaises(SyntaxError):
                db(cron, isc=isc, block=False)
        # A step next to a value is fine
        self.assertEqual(db("0 */20,3", ref=datetime(2025, 6, 1, 0, 0, 30),
                            block=False),
                         datetime(2025, 6, 1, 0, 3))

    def test_huge_repeated_list_raises_syntax_error(self):
        with self.assertRaises(SyntaxError):
            db("* " + ",".join(["0"] * 10001), block=False)

    def test_random_input_only_raises_syntax_or_value_error(self):
        # Junk, and valid crons with a few characters changed: each one is
        # either worked out or refused with SyntaxError (bad cron) or
        # ValueError (never happens, or a NUL character)
        rnd = random.Random(20261024)
        letters = "0123456789*/-,+ \t\n*/-,+5,0-1/\x00\xe9\u65e5a!"
        seeds = ("0 */5 3,4 1-15 * 1-5 +30", "* 5 +20,40", "0 0 0 29 2 1",
                 "0 9 * * 1-5", "*/15 0-3 1,15 */2 *")
        ref = datetime(2025, 1, 15, 3, 10)
        for round_ in range(3000):
            if round_ % 2:
                cron = "".join(rnd.choice(letters)
                               for _ in range(rnd.randrange(30)))
            else:
                cron = list(rnd.choice(seeds))
                for _ in range(rnd.randint(1, 3)):
                    at = rnd.randrange(len(cron) + 1)
                    cron.insert(at, rnd.choice(letters))
                cron = "".join(cron)
            for isc in (False, True):
                with self.subTest(cron=cron, isc=isc):
                    try:
                        result = db(cron, isc=isc, ref=ref, block=False)
                    except (SyntaxError, ValueError):
                        continue
                    self.assertGreater(result, ref)

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
