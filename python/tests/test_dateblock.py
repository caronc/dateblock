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
# this program; if not, see <https://www.gnu.org/licenses/>.
"""Tests for the dateblock Python module (run with: python3 -m unittest)."""
import asyncio
import locale
import os
import random
import time as _time
import tracemalloc
import unittest
from unittest import mock
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


# How far Python's own clock can lag behind the precise one dateblock
# waits on.  Before Python 3.13, time.time() and datetime.now() on Windows
# only tick about every 15.6 ms; elsewhere this is about a nanosecond.
_CLOCK_SLACK = _time.get_clock_info("time").resolution


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
        self.assertLessEqual(result, datetime.now(timezone.utc) +
                             timedelta(seconds=_CLOCK_SLACK))

    def test_blocking_wakes_right_on_the_second(self):
        # A few seconds away, so the wait goes through several steps (the
        # system timer and the final second-by-second checks).  It must
        # never wake early; a busy computer may take a moment longer.
        target = int(_time.time()) + 3
        result = db("/1", ref=target - 1)
        woke = _time.time() + _CLOCK_SLACK
        self.assertEqual(result, datetime.fromtimestamp(target))
        self.assertGreaterEqual(woke, target)
        self.assertLess(woke - target, 2.0)

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

    def test_month_and_weekday_names(self):
        # 2000-01-01 is a Saturday
        ref = date(2000, 1, 1)
        self.assertEqual(db("0 0 9 * * Mon-Fri", ref=ref, block=False),
                         datetime(2000, 1, 3, 9, 0, 0))
        self.assertEqual(db("0 0 0 1 mar", ref=ref, block=False),
                         datetime(2000, 3, 1, 0, 0, 0))
        self.assertEqual(db("0 9 * * SUN", isc=True, ref=ref, block=False),
                         datetime(2000, 1, 2, 9, 0, 0))

    def test_shortcuts(self):
        ref = date(2000, 1, 1)
        for cron, isc, expect in (
                ("@hourly", False, datetime(2000, 1, 1, 1, 0, 0)),
                ("@daily", False, datetime(2000, 1, 2, 0, 0, 0)),
                ("@weekly", True, datetime(2000, 1, 2, 0, 0, 0)),
                ("@monthly", False, datetime(2000, 2, 1, 0, 0, 0)),
                ("@yearly", True, datetime(2001, 1, 1, 0, 0, 0)),
                ("@daily +30", False, datetime(2000, 1, 1, 0, 0, 30))):
            self.assertEqual(db(cron, isc=isc, ref=ref, block=False), expect,
                             cron)

    def test_bad_names_and_shortcuts_raise_syntax_error(self):
        for cron, says in (
                ("0 0 0 * * Monday", "'Monday' is not a name"),
                ("0 0 0 * * Sat-Sun", "'Sat-Sun' goes backwards"),
                ("0 Mon", "'Mon' is not a number"),
                ("@reboot", "'@reboot' is not a time"),
                ("@daily 5", "only a drift (ie: '+20') may follow")):
            with self.assertRaises(SyntaxError) as raised:
                db(cron, block=False)
            self.assertIn(says, str(raised.exception))

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

    def test_syntax_error_says_what_is_wrong(self):
        with self.assertRaises(SyntaxError) as raised:
            db("* 99", block=False)
        self.assertIn("minute '99': 99 is outside of 0-59", str(raised.exception))
        with self.assertRaises(SyntaxError) as raised:
            db("0 0 *,13 * 1", isc=True, block=False)
        self.assertIn("day of month '*,13'", str(raised.exception))

    def test_overlong_cron_raises_syntax_error(self):
        with self.assertRaises(SyntaxError) as raised:
            db("*" * 70000, block=False)
        self.assertIn("too long", str(raised.exception))

    def test_longest_cron_allowed(self):
        longest = "0" + " " * 65535
        db(longest, block=False)
        # One more character is refused, even a 2 byte one
        for extra in (" ", "\u00e9"):
            with self.assertRaises(SyntaxError) as raised:
                db(longest + extra, block=False)
            self.assertIn("too long", str(raised.exception))

    def test_huge_cron_is_refused_without_copying_it(self):
        # 20 MB of 'é' (40 MB as UTF-8); only its start is ever converted
        huge = "é" * 20_000_000
        tracemalloc.start()
        try:
            with self.assertRaises(SyntaxError) as raised:
                db(huge, block=False)
            peak = tracemalloc.get_traced_memory()[1]
        finally:
            tracemalloc.stop()
        self.assertIn("too long", str(raised.exception))
        self.assertLess(peak, 2_000_000)

    def test_cron_must_be_text_without_nul(self):
        with self.assertRaises(ValueError):
            db("0 0\x00 9", block=False)
        for cron in (b"0 0 9", 5, None):
            with self.assertRaises(TypeError):
                db(cron, block=False)

    def test_names_read_the_same_in_any_language(self):
        # In Turkish 'I' doesn't lower to 'i'
        before = locale.setlocale(locale.LC_ALL)
        for name in ("tr_TR.UTF-8", "tr_TR.utf8"):
            try:
                locale.setlocale(locale.LC_ALL, name)
                break
            except locale.Error:
                continue
        else:
            self.skipTest("no Turkish locale installed")
        try:
            ref = date(2000, 1, 1)
            self.assertEqual(db("@MIDNIGHT", ref=ref, block=False),
                             datetime(2000, 1, 2, 0, 0, 0))
            self.assertEqual(db("0 0 9 * * FRI", ref=ref, block=False),
                             datetime(2000, 1, 7, 9, 0, 0))
        finally:
            locale.setlocale(locale.LC_ALL, before)

    def test_aware_reference_with_a_broken_timestamp(self):
        # A datetime subclass can make timestamp() return anything
        class Odd(datetime):
            def timestamp(self):
                return self.value

        for value in (float("inf"), float("nan"), 1e300, -1e300):
            ref = Odd(2025, 1, 1, tzinfo=timezone.utc)
            ref.value = value
            with self.assertRaises(OverflowError):
                db("*/1", ref=ref, block=False)

    def test_tzinfo_returning_something_odd_raises_type_error(self):
        # A tzinfo's fromutc() can return anything, and Python doesn't
        # check it
        class Odd(tzinfo):
            def utcoffset(self, dt):
                return timedelta(0)

            def dst(self, dt):
                return timedelta(0)

            def fromutc(self, dt):
                return self.result

        for result in (None, 42, "2025-01-01"):
            zone = Odd()
            zone.result = result
            with self.assertRaises(TypeError):
                db("*/1", ref=datetime(2025, 1, 1, tzinfo=zone), block=False)

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
        # A Python datetime stops at the year 9999
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

    def test_dates_before_1970_and_after_3000(self):
        # Windows' own date functions only work from 1970 to the year 3000
        self.assertEqual(db("0 0 0 29 2", ref=date(1896, 3, 1), block=False),
                         datetime(1904, 2, 29))
        self.assertEqual(db("0 0 0 29 2", ref=date(3999, 3, 1), block=False),
                         datetime(4000, 2, 29))
        self.assertEqual(db("*/1", ref=datetime(1601, 1, 1), block=False),
                         datetime(1601, 1, 1, 0, 0, 1))
        plus9 = timezone(timedelta(hours=9))
        self.assertEqual(
            db("0 0 9", ref=datetime(1950, 1, 1, tzinfo=plus9), block=False),
            datetime(1950, 1, 1, 9, tzinfo=plus9))
        self.assertEqual(
            db("0 0 9", ref=datetime(3500, 1, 1, tzinfo=plus9), block=False),
            datetime(3500, 1, 1, 9, tzinfo=plus9))

    def test_huge_drift_searches_back_before_1970(self):
        # A 68 year drift from 2024 starts its search in 1956 (and finds
        # 1957-01-01 there, plus the drift)
        ref = datetime(2024, 2, 29, 23, 59, 59, tzinfo=timezone.utc)
        self.assertEqual(db("0 0 0 1 1 * 2147483647", ref=ref, block=False),
                         datetime(2025, 1, 19, 3, 14, 7, tzinfo=timezone.utc))
        naive = datetime(2024, 2, 29, 23, 59, 59)
        self.assertGreater(db("0 0 0 1 1 * 2147483647", ref=naive,
                              block=False), naive)

    def test_bad_argument_types(self):
        with self.assertRaises(TypeError):
            db(5, block=False)
        with self.assertRaises(TypeError):
            db("*/1", ref="yesterday", block=False)

    def test_package_details(self):
        self.assertRegex(dateblock.__version__, r"^\d+\.\d+\.\d+$")
        self.assertEqual(dateblock.__title__, "dateblock")
        self.assertEqual(dateblock.__author__, "Chris Caron")
        self.assertEqual(dateblock.__license__, "GPL-2.0-or-later")
        self.assertIn("Chris Caron", dateblock.__copyright__)

    def test_constants(self):
        self.assertEqual(dateblock.SEC_MAX, 59)
        self.assertEqual(dateblock.DOW_MIN, 0)
        self.assertEqual(dateblock.MONTH_MAX, 12)


class WaitTests(unittest.TestCase):
    """await dateblock.wait(...), the asyncio version."""

    def test_wait_returns_what_dateblock_works_out(self):
        for cron, ref, isc in (("0 0 9", datetime(2025, 1, 1), False),
                               ("0 9 * * 1-5", 999434323, True),
                               ("@daily +30", date(2024, 2, 28), False)):
            with self.subTest(cron=cron):
                self.assertEqual(
                    asyncio.run(dateblock.wait(cron, ref=ref, isc=isc)),
                    db(cron, ref=ref, isc=isc, block=False))

    def test_wait_keeps_the_reference_timezone(self):
        plus14 = timezone(timedelta(hours=14))
        result = asyncio.run(dateblock.wait("/1", ref=datetime.now(plus14)))
        self.assertIs(result.tzinfo, plus14)

    def test_wait_wakes_right_on_the_second(self):
        # It must never wake early; a busy computer may take a moment longer
        target = int(_time.time()) + 3
        result = asyncio.run(dateblock.wait("/1", ref=target - 1))
        woke = _time.time()
        self.assertEqual(result, datetime.fromtimestamp(target))
        self.assertGreaterEqual(woke, target)
        self.assertLess(woke - target, 2.0)

    def _woke_at(self, cron, ref, start):
        """Run wait() on a fake clock that starts at 'start'.

        - Returns (result, the fake time when the wait ended).
        """
        now = [start]

        async def sleep(seconds):
            now[0] += seconds

        with mock.patch.object(dateblock._wait.time, "time", lambda: now[0]), \
                mock.patch.object(dateblock._wait.asyncio, "sleep", sleep):
            result = asyncio.run(dateblock.wait(cron, ref=ref))
        return result, now[0]

    # New York, 2025-11-02: the clocks go back at 02:00 EDT, so 01:30
    # happens twice (05:30 UTC, then 06:30 UTC).  From 01:45 EDT the next
    # 01:30 is the second one.
    FIRST_0130 = 1762061400
    SECOND_0130 = FIRST_0130 + 3600

    @needs_zoneinfo
    def test_wait_ends_at_the_repeated_hour_in_a_zone(self):
        zone = ZoneInfo("America/New_York")
        start = self.FIRST_0130 + 15 * 60
        result, woke = self._woke_at(
            "0 30 1", datetime.fromtimestamp(start, zone), start)
        self.assertEqual(result.timestamp(), self.SECOND_0130)
        self.assertEqual(woke, self.SECOND_0130)

    @unittest.skipUnless(hasattr(_time, "tzset"), "needs time.tzset()")
    def test_wait_ends_at_the_repeated_hour_in_local_time(self):
        # A naive result can't say which 01:30 it is; wait() must still
        # end at the second one
        saved = os.environ.get("TZ")
        try:
            os.environ["TZ"] = "America/New_York"
            _time.tzset()
            # Small systems may have no timezone data for it
            if (_time.localtime(self.FIRST_0130).tm_isdst != 1 or
                    _time.localtime(self.SECOND_0130).tm_isdst != 0):
                self.skipTest("no timezone data for America/New_York")
            start = self.FIRST_0130 + 15 * 60
            result, woke = self._woke_at("0 30 1", start, start)
        finally:
            if saved is None:
                os.environ.pop("TZ", None)
            else:
                os.environ["TZ"] = saved
            _time.tzset()
        self.assertEqual(result, datetime(2025, 11, 2, 1, 30))
        self.assertEqual(woke, self.SECOND_0130)

    def test_other_tasks_run_while_waiting(self):
        # The wait is 2-3 seconds away; this task keeps ticking until it
        # ends.  A wait that blocked the loop would leave only one tick.
        async def ticks_while_waiting():
            waiting = asyncio.ensure_future(
                dateblock.wait("/1", ref=int(_time.time()) + 2))
            ticks = 0
            while not waiting.done():
                await asyncio.sleep(0.1)
                ticks += 1
            await waiting
            return ticks

        self.assertGreaterEqual(asyncio.run(ticks_while_waiting()), 10)

    def test_cancelling_stops_the_wait(self):
        async def far_away():
            # At least a day away; give up after a moment
            tomorrow = datetime.now() + timedelta(days=1)
            await asyncio.wait_for(dateblock.wait("0 0 9", ref=tomorrow), 0.2)

        start = _time.monotonic()
        with self.assertRaises(asyncio.TimeoutError):
            asyncio.run(far_away())
        self.assertLess(_time.monotonic() - start, 2)

    def test_bad_cron_raises_the_same_errors(self):
        for cron, error in (("* 99", SyntaxError), ("0 0 0 31 2 *", ValueError),
                            (b"0 0 9", TypeError), ("0\x00", ValueError)):
            with self.subTest(cron=cron):
                with self.assertRaises(error):
                    asyncio.run(dateblock.wait(cron))

    def test_long_wait_follows_the_real_clock(self):
        # A wait of up to a year sleeps at most a minute at a time.  The
        # clock then jumps past the moment (ie: the computer slept), and
        # the wait ends on its next check instead of sleeping on.
        start = 1750000000  # mid June 2025, months from a new year
        now = [start]
        slept = []

        async def sleep(seconds):
            slept.append(seconds)
            now[0] += seconds + 400 * 86400

        when = db("@yearly", ref=start, block=False)
        with mock.patch.object(dateblock._wait.time, "time", lambda: now[0]), \
                mock.patch.object(dateblock._wait.asyncio, "sleep", sleep):
            result = asyncio.run(dateblock.wait("@yearly", ref=start))

        self.assertEqual(result, when)
        self.assertEqual(len(slept), 1)
        self.assertLessEqual(slept[0], 60)


class UpcomingTests(unittest.TestCase):
    """dateblock.upcoming(...), the next few times without waiting."""

    def test_lists_the_next_few_times(self):
        # 2026-01-01 is a Thursday
        times = dateblock.upcoming("0 9 * * Mon-Fri", 5, isc=True,
                                   ref=datetime(2026, 1, 1))
        self.assertIsInstance(times, tuple)
        self.assertEqual(times, (
            datetime(2026, 1, 1, 9), datetime(2026, 1, 2, 9),
            datetime(2026, 1, 5, 9), datetime(2026, 1, 6, 9),
            datetime(2026, 1, 7, 9)))

    def test_count_defaults_and_limits(self):
        self.assertEqual(dateblock.COUNT_DEFAULT, 1)
        self.assertEqual(dateblock.COUNT_MAX, 1000)
        # Unless told otherwise, just the one dateblock() works out
        ref = datetime(2026, 1, 1, 12)
        self.assertEqual(dateblock.upcoming("@daily", ref=ref),
                         (db("@daily", ref=ref, block=False),))
        self.assertEqual(len(dateblock.upcoming("/1", dateblock.COUNT_MAX)),
                         dateblock.COUNT_MAX)
        for count in (0, -1, dateblock.COUNT_MAX + 1, 10**100, -10**100):
            with self.assertRaises(ValueError):
                dateblock.upcoming("* 5", count)
        # A huge count isn't spelled out in the message
        with self.assertRaisesRegex(ValueError, r"^count must be from 1 to 1000$"):
            dateblock.upcoming("* 5", 10**100)
        for count in ("5", 1.5, None):
            with self.assertRaises(TypeError):
                dateblock.upcoming("* 5", count)

    def test_each_time_follows_the_one_before(self):
        cron = "*/20 5 1-3 * * * +7"
        at = datetime(2026, 1, 1)
        for when in dateblock.upcoming(cron, 6, ref=at):
            self.assertEqual(when, db(cron, ref=at, block=False))
            at = when

    def test_errors_match_dateblock(self):
        with self.assertRaises(SyntaxError):
            dateblock.upcoming("* 99")
        with self.assertRaises(ValueError):
            dateblock.upcoming("0 0 0 31 2 *")
        with self.assertRaises(TypeError):
            dateblock.upcoming(b"* 5")

    def test_ends_early_past_the_year_9999(self):
        for ref in (datetime(9990, 6, 1),
                    datetime(9990, 6, 1, tzinfo=timezone.utc)):
            with self.subTest(ref=ref):
                times = dateblock.upcoming("@yearly", 1000, ref=ref)
                self.assertEqual(len(times), 9)
                self.assertEqual(times[-1].year, 9999)

    def test_a_timezone_that_breaks_later_is_still_raised(self):
        for error in (ValueError, OverflowError):
            class FailsFrom2027(tzinfo):
                def utcoffset(self, dt):
                    return timedelta(0)

                def dst(self, dt):
                    return timedelta(0)

                def fromutc(self, dt):
                    if dt.year >= 2027:
                        raise error("broken zone")
                    return dt.replace(tzinfo=self)

            with self.subTest(error=error):
                ref = datetime(2025, 6, 1, tzinfo=FailsFrom2027())
                with self.assertRaisesRegex(error, "broken zone"):
                    dateblock.upcoming("@yearly", 5, ref=ref)

    def test_a_timezone_that_breaks_near_the_year_9999_is_still_raised(self):
        # Only the very last day can run out of dates; failing about two
        # days before the end is the timezone's own fault
        class FailsFromDec30(tzinfo):
            def utcoffset(self, dt):
                return timedelta(0)

            def dst(self, dt):
                return timedelta(0)

            def fromutc(self, dt):
                if dt >= datetime(9999, 12, 30, 1, tzinfo=self):
                    raise OverflowError("broken zone")
                return dt.replace(tzinfo=self)

        ref = datetime(9999, 12, 29, 23, tzinfo=FailsFromDec30())
        with self.assertRaisesRegex(OverflowError, "broken zone"):
            dateblock.upcoming("@hourly", 5, ref=ref)

    @needs_zoneinfo
    def test_ends_early_past_the_year_9999_in_a_named_zone(self):
        # West of UTC the UTC time runs out first; east of it the local one
        for name in ("America/New_York", "Asia/Tokyo"):
            with self.subTest(zone=name):
                ref = datetime(9990, 6, 1, tzinfo=ZoneInfo(name))
                times = dateblock.upcoming("@yearly", 1000, ref=ref)
                self.assertEqual(len(times), 9)
                self.assertEqual(times[-1].year, 9999)

    @needs_zoneinfo
    def test_keeps_both_copies_of_a_repeated_hour_in_a_zone(self):
        # New York, 2025-11-02: the clocks go back, so 01:30 happens twice
        zone = ZoneInfo("America/New_York")
        times = dateblock.upcoming("0 30 1", 3,
                                   ref=datetime(2025, 11, 1, 12, tzinfo=zone))
        self.assertEqual([t.timestamp() for t in times],
                         [1762061400, 1762065000, 1762151400])
        self.assertTrue(all(t.tzinfo is zone for t in times))

    @unittest.skipUnless(hasattr(_time, "tzset"), "needs time.tzset()")
    def test_keeps_both_copies_of_a_repeated_hour_in_local_time(self):
        # The second 01:30 has fold=1, so its timestamp() is right too
        saved = os.environ.get("TZ")
        try:
            os.environ["TZ"] = "America/New_York"
            _time.tzset()
            if (_time.localtime(1762061400).tm_isdst != 1 or
                    _time.localtime(1762065000).tm_isdst != 0):
                self.skipTest("no timezone data for America/New_York")
            times = dateblock.upcoming("0 30 1", 3,
                                       ref=datetime(2025, 11, 1, 12))
            stamps = [t.timestamp() for t in times]
            folds = [t.fold for t in times]
            single = db("0 30 1", ref=1762061400, block=False)
        finally:
            if saved is None:
                os.environ.pop("TZ", None)
            else:
                os.environ["TZ"] = saved
            _time.tzset()
        self.assertEqual(stamps, [1762061400, 1762065000, 1762151400])
        self.assertEqual(folds, [0, 1, 0])
        self.assertEqual(single.fold, 1)


if __name__ == '__main__':
    unittest.main()
