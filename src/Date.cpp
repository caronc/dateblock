/*
 Dateblock blocks (sleeps) until a cron-style moment in time.
 Copyright (C) 2013-2018 Chris Caron <lead2gold@gmail.com>

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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <time.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <stdint.h>
#include <limits>
#include <algorithm>
#include <vector>
#include <ctype.h>
#include <stdexcept>
#include "Date.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <sys/time.h>
#endif

// The most values one field may expand to.  Time fields never get close
// (a minute has 60 values); this stops a drift such as '0-2147483647' from
// using up all of the memory.
#ifndef CRON_MAX_FIELD_VALUES
#define CRON_MAX_FIELD_VALUES 10000
#endif

using namespace std;

namespace {
   // Number of days in a month (month is 0-11, year is the full year)
   int DaysInMonth(int year, int month)
   {
      static const int days[12] =
         {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
      bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
      return (month == 1 && leap) ? 29 : days[month];
   }

   // Some systems' own date functions give up on dates far from today:
   // Windows' only work from 1970 to the year 3000, and macOS' mktime()
   // refuses years before 1900.  LocalTime() and MakeTime() below work
   // around that whenever one of them fails.  These make other systems act
   // the same way, so the work-around can be tested anywhere:
   //  - DATEBLOCK_TEST_NARROW_TIME: like Windows
   //  - DATEBLOCK_TEST_OLD_MKTIME:  like macOS
   #ifdef DATEBLOCK_TEST_NARROW_TIME
   // 3000-12-31 23:59:59 UTC, the last moment Windows can convert
   const long long NARROW_LAST = 32535215999LL;
   #endif

   // The calendar (leap years and weekdays included) repeats every 400 years
   const long long CALENDAR_CYCLE = 146097LL * 86400;

   // The system's own thread safe localtime()
   bool SystemLocalTime(time_t t, struct tm& tmOut)
   {
   #ifdef _WIN32
      return localtime_s(&tmOut, &t) == 0;
   #else
   #ifdef DATEBLOCK_TEST_NARROW_TIME
      if (t < 0 || t > NARROW_LAST)
         return false;
   #endif
      return localtime_r(&t, &tmOut) != NULL;
   #endif
   }

   // The system's own mktime(), reporting failure.  mktime() returns -1
   // both on failure and for one real second (1969-12-31 23:59:59 UTC); it
   // only fills in tm_wday when it works, which tells the two apart.
   bool SystemMakeTime(struct tm& tmIo, time_t& tOut)
   {
   #ifdef DATEBLOCK_TEST_OLD_MKTIME
      if (tmIo.tm_year < 0)
         return false;
   #endif
      struct tm tmp = tmIo;
      tmp.tm_wday = -1;
      time_t t = mktime(&tmp);
      if (t == (time_t)-1 && tmp.tm_wday == -1)
         return false;
   #ifdef DATEBLOCK_TEST_NARROW_TIME
      if (t < 0 || t > NARROW_LAST)
         return false;
   #endif
      tmIo = tmp;
      tOut = t;
      return true;
   }

   // Thread safe localtime().  Fails for times whose year doesn't fit in an
   // int (Year() adds 1900 to tm_year).
   bool LocalTime(time_t tVal, struct tm& tmOut)
   {
      struct tm tmp;
      long long cycles = 0;
      if (!SystemLocalTime(tVal, tmp))
      {
         // The system gave up; work out the same moment a whole number of
         // 400 year cycles away (somewhere from 2000 to 2399) and move the
         // year back afterwards
         if (tVal < -(1LL << 62) || tVal > (1LL << 62))
            return false;
         long long fromBase = (long long)tVal - 946684800LL;  // 2000-01-01
         cycles = fromBase / CALENDAR_CYCLE;
         if (fromBase % CALENDAR_CYCLE < 0)
            cycles--;
         if (cycles == 0 ||
             !SystemLocalTime((time_t)(tVal - cycles * CALENDAR_CYCLE), tmp))
            return false;
      }
      long long year = (long long)tmp.tm_year + cycles * 400;
      if (year < INT_MIN || year > INT_MAX - 1900)
         return false;
      tmp.tm_year = (int)year;
      tmOut = tmp;
      return true;
   }

   // mktime() that reports failure.  'tmOut' and 'tOut' are only changed
   // when it works.  Years that don't fit in an int (once 1900 is added)
   // fail too.
   bool MakeTime(const struct tm& tmIn, struct tm& tmOut, time_t& tOut)
   {
      struct tm tmp = tmIn;
      long long cycles = 0;
      time_t t;
      if (!SystemMakeTime(tmp, t))
      {
         // The system gave up; the same idea as LocalTime(): move the year
         // into 2000 to 2399, then back again afterwards
         long long year = (long long)tmp.tm_year + 1900 + tmp.tm_mon / 12;
         cycles = (year - 2000) / 400;
         if ((year - 2000) % 400 < 0)
            cycles--;
         tmp.tm_year = (int)(tmp.tm_year - cycles * 400);
         if (cycles == 0 || !SystemMakeTime(tmp, t))
            return false;
      }
      long long year2 = (long long)tmp.tm_year + cycles * 400;
      if (year2 < INT_MIN || year2 > INT_MAX - 1900)
         return false;
      tmp.tm_year = (int)year2;
      tmOut = tmp;
      tOut = (time_t)(t + cycles * CALENDAR_CYCLE);
      return true;
   }

   // The same, for a date worked out from a calendar change (a new month,
   // year, hour...).  Daylight saving is worked out for the new date, so
   // the clock time stays as asked for (12:00 in July plus 5 months is
   // 12:00 in December, not 11:00).
   bool MakeLocalTime(struct tm tmIn, struct tm& tmOut, time_t& tOut)
   {
      tmIn.tm_isdst = -1;
      return MakeTime(tmIn, tmOut, tOut);
   }

   // The current time (to the nanosecond where available).  This reads the
   // same clock that BlockUntil() sleeps on; time() can lag it by a few
   // milliseconds, which is enough to think we are still in the previous
   // second.
   void NowPrecise(struct timespec& ts)
   {
   #if defined(_WIN32)
      timespec_get(&ts, TIME_UTC);
   #elif defined(__APPLE__)
      struct timeval tv;
      gettimeofday(&tv, NULL);
      ts.tv_sec = tv.tv_sec;
      ts.tv_nsec = tv.tv_usec * 1000;
   #else
      clock_gettime(CLOCK_REALTIME, &ts);
   #endif
   }

   // The current time in whole seconds
   time_t Now()
   {
      struct timespec ts;
      NowPrecise(ts);
      return ts.tv_sec;
   }

   // Seconds from 'now' until 'until' (0 if it has passed).  Very long
   // waits are capped instead of wrapping around to a small number (an
   // unsigned long is only 32 bits on Windows).
   unsigned long SecondsLeft(time_t until, time_t now)
   {
      if (until <= now)
         return 0;
      unsigned long long left = (unsigned long long)until - (unsigned long long)now;
      const unsigned long most = numeric_limits<unsigned long>::max();
      return (left > most) ? most : (unsigned long)left;
   }

   // Splits a whitespace separated cron string into its fields.
   //  - Missing fields are filled in with '*'.
   //  - A '+N' entry (dateblock format only) jumps straight to the drift
   //    field; nothing may follow it.
   //  - Returns false (with 'why' set) if the string has more fields than
   //    allowed, or a '+' without a drift.
   bool SplitCronString(const string& strIn, bool isISC,
                        vector<string>& tokens, string& why)
   {
      // ISC only processes the first 5 fields, where as
      // otherwise we process the first 7
      size_t token_max = isISC?Date::ISC_CRON_FIELD_COUNT:
                               Date::DBL_CRON_FIELD_COUNT;
      bool ok = true;
      bool drifted = false;
      size_t pos = 0;

      tokens.clear();
      while (true)
      {
         while (pos < strIn.size() && isspace((unsigned char)strIn[pos]))
            pos++;
         if (pos == strIn.size())
            break;
         size_t start = pos;
         while (pos < strIn.size() && !isspace((unsigned char)strIn[pos]))
            pos++;
         string token(strIn, start, pos - start);

         if (drifted)
         {
            why = "nothing may follow the drift (the '+' entry)";
            ok = false;
            break;
         }
         if (tokens.size() >= token_max)
         {
            why = isISC ?
               "there are too many fields; a crontab entry has 5 (minute, "
               "hour, day of month, month and day of week)" :
               "there are too many fields; a cron has at most 7 (second, "
               "minute, hour, day of month, month, day of week and drift)";
            ok = false;
            break;
         }

         if (!isISC && token[0] == '+')
         {
            // drift entry; fill in our blanks first
            while (tokens.size() < (token_max-1))
               tokens.push_back("*");
            token.erase(0, 1);
            drifted = true;
            if (token.empty())
            {
               why = "a '+' needs the drift after it (ie: '+20')";
               ok = false;
               break;
            }
         }
         tokens.push_back(token);
      }

      // Placeholders
      while (tokens.size() < token_max)
         tokens.push_back("*");

      return ok;
   }

   // 'text' for an error message: shortened if it is very long, and with
   // control characters written as \xNN so they can't upset a terminal or
   // a log file
   string Shown(const string& text)
   {
      string shown;
      for (size_t i = 0; i < text.size() && shown.size() < 40; i++)
      {
         unsigned char c = (unsigned char)text[i];
         if (c < 0x20 || c == 0x7f)
         {
            const char *hex = "0123456789abcdef";
            shown += string("\\x") + hex[c >> 4] + hex[c & 0xf];
         }
         else
            shown += (char)c;
      }
      return (shown.size() < text.size() && shown.size() >= 40) ?
             shown + "..." : shown;
   }

   // Reads the number starting at 'pos' (moving 'pos' past it), stopping
   // at 'end'.  Fails (with 'why' set) if there is no number there or it
   // is outside of 'lo' to 'hi'.
   bool ReadNumber(const string& str, size_t& pos, size_t end, int lo, int hi,
                   long long& out, string& why)
   {
      size_t start = pos;
      out = 0;
      while (pos < end && isdigit((unsigned char)str[pos]))
      {
         out = out * 10 + (str[pos] - '0');
         if (out > hi)
         {
            while (pos < end && isdigit((unsigned char)str[pos]))
               pos++;
            why = Shown(str.substr(start, pos - start)) + " is outside of " +
                  to_string(lo) + "-" + to_string(hi);
            return false;
         }
         pos++;
      }
      if (pos == start)
      {
         why = (pos < end) ?
            "'" + Shown(str.substr(pos, end - pos)) + "' is not a number" :
            "a number is missing at the end";
         return false;
      }
      if (out < lo)
      {
         why = to_string(out) + " is outside of " + to_string(lo) + "-" +
               to_string(hi);
         return false;
      }
      return true;
   }

   // The calendar (leap years and weekdays included) repeats every 400
   // years.  If nothing matches within that long, nothing ever will (ie:
   // the 31st of February).
   const time_t SEARCH_HORIZON = (time_t)CALENDAR_CYCLE;

   // What the local time has to look like to match.  Bit 'n' of a mask
   // set means value 'n' is allowed.
   struct Schedule
   {
      uint64_t sec, min, hour, dom, month, dow;
      // true:  the day of month OR the day of week has to match (crontab)
      // false: both have to match
      bool domOrDow;
   };

   bool Has(uint64_t mask, int value)
   {
      return value >= 0 && value < 64 && ((mask >> value) & 1);
   }

   // The smallest allowed value from 'from' to 'max', or -1 if there is none
   int NextIn(uint64_t mask, int from, int max)
   {
      for (int value = from; value <= max; value++)
         if (Has(mask, value))
            return value;
      return -1;
   }

   // A mask allowing every value from 'lo' to 'hi'
   uint64_t AllOf(int lo, int hi)
   {
      uint64_t mask = 0;
      for (int value = lo; value <= hi; value++)
         mask |= (uint64_t)1 << value;
      return mask;
   }

   bool DayMatches(const Schedule& s, const struct tm& tm)
   {
      bool dom = Has(s.dom, tm.tm_mday);
      bool dow = Has(s.dow, tm.tm_wday);
      return s.domOrDow ? (dom || dow) : (dom && dow);
   }

   // Days from 1970-01-01 to the given date (month is 1-12).  Works for
   // any year; see https://howardhinnant.github.io/date_algorithms.html
   long long DaysFromCivil(long long year, int month, int day)
   {
      year -= (month <= 2) ? 1 : 0;
      long long era = (year >= 0 ? year : year - 399) / 400;
      long long yoe = year - era * 400;
      long long doy = (153 * (month > 2 ? month - 3 : month + 9) + 2) / 5 +
                      day - 1;
      long long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
      return era * 146097 + doe - 719468;
   }

   // The clock time in 'tm' counted in seconds as if it were UTC.  Fields
   // past their end roll over (month 12 is January of the next year, day
   // 32 is the 1st of the next month and so on).
   long long WallSeconds(const struct tm& tm)
   {
      long long year = (long long)tm.tm_year + 1900 + tm.tm_mon / 12;
      int month = tm.tm_mon % 12;
      if (month < 0)
      {
         month += 12;
         year--;
      }
      long long days = DaysFromCivil(year, month + 1, 1) + tm.tm_mday - 1;
      return days * 86400 + tm.tm_hour * 3600LL + tm.tm_min * 60LL + tm.tm_sec;
   }

   // The local time at 't', in the computer's own timezone (zone NULL) or
   // in 'zone'
   bool ZoneLocal(const Date::TimeZone* zone, time_t t, struct tm& tmOut)
   {
      if (zone == NULL)
         return LocalTime(t, tmOut);

      struct tm tm;
      memset(&tm, 0, sizeof(tm));
      if (!zone->ToLocal(t, tm) ||
          tm.tm_mon < 0 || tm.tm_mon > 11 || tm.tm_mday < 1 ||
          tm.tm_mday > DaysInMonth(tm.tm_year + 1900, tm.tm_mon) ||
          tm.tm_hour < 0 || tm.tm_hour > 23 || tm.tm_min < 0 ||
          tm.tm_min > 59 || tm.tm_sec < 0 || tm.tm_sec > 60 ||
          tm.tm_year > INT_MAX - 1900)
         return false;

      // 1970-01-01 was a Thursday (4)
      long long days = DaysFromCivil(tm.tm_year + 1900LL, tm.tm_mon + 1,
                                     tm.tm_mday);
      tm.tm_wday = (int)(((days + 4) % 7 + 7) % 7);
      tm.tm_isdst = -1;
      tmOut = tm;
      return true;
   }

   // The moment the clocks in 'zone' show 'target' (like mktime()).
   //  - When that time happens twice (the clocks went back), the first.
   //  - When it never happens (the clocks went forward over it), the
   //    moment just after the jump.
   //  - Returns false if the zone can't tell us.
   bool ZoneMakeTime(const Date::TimeZone* zone, const struct tm& target,
                     time_t& out)
   {
      // Guess with the offset in use around that time, then again with
      // the offset at the first guess
      long long wall = WallSeconds(target);
      struct tm tm;
      if (!ZoneLocal(zone, (time_t)wall, tm))
         return false;
      long long offA = WallSeconds(tm) - wall;
      time_t a = (time_t)(wall - offA);
      if (!ZoneLocal(zone, a, tm))
         return false;
      long long offB = WallSeconds(tm) - a;
      time_t b = (time_t)(wall - offB);
      if (!ZoneLocal(zone, b, tm))
         return false;

      bool aShows = (offA == offB);
      bool bShows = (WallSeconds(tm) - b == offB);
      if (aShows && bShows)
         out = (a < b) ? a : b;
      else if (aShows || bShows)
         out = aShows ? a : b;
      else
         out = (a > b) ? a : b;
      return true;
   }

   // Moves 't' forward to a local date/time ('zone' NULL uses MakeTime()).
   // It always moves forward, even if the clock change around a daylight
   // saving switch makes the result land a little early.
   time_t JumpTo(const Date::TimeZone* zone, time_t t, struct tm target)
   {
      time_t next;
      struct tm made;
      if (zone == NULL)
      {
         target.tm_isdst = -1;
         if (!MakeTime(target, made, next))
            next = t;
      }
      else if (!ZoneMakeTime(zone, target, next))
      {
         next = t;
      }
      return (next > t) ? next : t + 3600;
   }

   // The first moment after 'after' whose local time matches 's'.
   //  - 'zone' NULL means the computer's own timezone.
   //  - Moves forward one real moment at a time (jumping over whole
   //    seconds, minutes, hours, days and months that can't match), so a
   //    daylight saving change never makes it skip a match.
   //  - Throws std::runtime_error if nothing matches.
   time_t NextMatch(const Schedule& s, time_t after,
                    const Date::TimeZone* zone)
   {
      const time_t max = numeric_limits<time_t>::max();
      if (after == max)
         throw runtime_error("The reference time is out of range.");
      time_t t = after + 1;
      time_t limit = (after > max - SEARCH_HORIZON) ? max : after + SEARCH_HORIZON;
      struct tm tm;

      while (t <= limit && ZoneLocal(zone, t, tm))
      {
         if (!Has(s.month, tm.tm_mon + 1))
         {
            struct tm next = tm;
            next.tm_mon++;
            next.tm_mday = 1;
            next.tm_hour = next.tm_min = next.tm_sec = 0;
            t = JumpTo(zone, t, next);
            continue;
         }

         if (!DayMatches(s, tm) || NextIn(s.hour, tm.tm_hour, 23) < 0)
         {
            struct tm next = tm;
            next.tm_mday++;
            next.tm_hour = next.tm_min = next.tm_sec = 0;
            t = JumpTo(zone, t, next);
            continue;
         }

         // Seconds left until the next minute and hour start.  A leap
         // second (tm_sec 60) still moves us forward.
         time_t toMinute = (tm.tm_sec < 60) ? 60 - tm.tm_sec : 1;
         time_t toHour = toMinute + (59 - tm.tm_min) * 60;

         if (!Has(s.hour, tm.tm_hour))
         {
            // One hour at a time; jumping further could step over a
            // matching hour when the clocks change
            t += toHour;
            continue;
         }

         if (!Has(s.min, tm.tm_min))
         {
            int min = NextIn(s.min, tm.tm_min, 59);
            t += (min < 0) ? toHour
                           : toMinute + (min - tm.tm_min - 1) * 60;
            continue;
         }

         int sec = NextIn(s.sec, tm.tm_sec, 59);
         if (sec < 0)
         {
            t += toMinute;
            continue;
         }
         if (sec == tm.tm_sec)
         {
            return t;
         }
         t += sec - tm.tm_sec;
      }

      throw runtime_error(
         "No moment in time matches this cron (none in the next 400 years).");
   }

   // The fields of a cron string, in order (the drift comes last)
   enum Field { F_SEC, F_MIN, F_HOUR, F_DOM, F_MONTH, F_DOW, F_DRIFT,
                F_COUNT };

   // The lowest and highest value of each field
   int FieldLow(int field)
   {
      const int lows[F_COUNT] = {
         Date::T_SEC_MIN, Date::T_MIN_MIN, Date::T_HOUR_MIN, Date::T_DOM_MIN,
         Date::T_MONTH_MIN, Date::T_DOW_MIN, Date::T_DRIFT_MIN };
      return lows[field];
   }

   int FieldHigh(int field)
   {
      const int highs[F_COUNT] = {
         Date::T_SEC_MAX, Date::T_MIN_MAX, Date::T_HOUR_MAX, Date::T_DOM_MAX,
         Date::T_MONTH_MAX, Date::T_DOW_MAX, Date::T_DRIFT_MAX };
      return highs[field];
   }

   // What a cron string allows, once parsed
   struct CronFields
   {
      uint64_t mask[F_DRIFT];   // bit 'n' set: value 'n' is allowed
      bool given[F_DRIFT];      // false: the field was '*' (or left out)
      vector<int> drifts;       // sorted, no repeats; empty: no drift
      bool isISC;               // read as a regular crontab entry
      bool domStar, dowStar;    // crontab: the day field started with '*'
   };

   // Reads the comma separated entries of one field.  Each entry is one of:
   //   *            any value (on its own only; '*,5' is unclear, so it
   //                is refused)
   //   x            a value
   //   x-y (y-x)    every value from x to y
   //   x-y/z        every z'th value from x to y
   //   */x (/x)     every value divisible by x, or with stepFromLowest
   //                every x'th value from the lowest one (crontab)
   // Anything else (ie: '5-', 'x-y-z', '/x/y', or an empty entry such as
   // '5,,6') is invalid.
   //  - Each entry is handed to 'add(first, last, step)', which may refuse
   //    it by returning false.
   //  - 'given' is false when the field is '*' (or empty).
   //  - Returns false (with 'why' set) if it is not formatted correctly.
   template <class Add>
   bool ReadField(const string& str, int lo, int hi, bool stepFromLowest,
                  Add& add, bool& given, string& why)
   {
      given = false;
      if (str.empty())
         return true;

      // More entries than a field can ever hold (ie: '0,0,0,...' repeated
      // a million times) are refused before anything is read
      if (count(str.begin(), str.end(), ',') >= CRON_MAX_FIELD_VALUES)
      {
         why = "it has more than " + to_string(CRON_MAX_FIELD_VALUES) +
               " entries";
         return false;
      }

      bool star = false;
      size_t entries = 0;
      size_t start = 0;
      while (true)
      {
         size_t end = str.find(',', start);
         if (end == string::npos)
            end = str.size();
         if (end == start)
         {
            why = "an entry is empty (two commas in a row, or a comma at "
                  "the start or end)";
            return false;
         }
         entries++;

         if (end - start == 1 && str[start] == '*')
         {
            star = true;
         }
         else
         {
            size_t pos = start;
            long long first, last, step = 1;
            if (str[start] == '/' ||
                (end - start > 1 && str[start] == '*' && str[start + 1] == '/'))
            {
               // Modulus
               pos += (str[start] == '/') ? 1 : 2;
               if (!ReadNumber(str, pos, end, lo, hi, step, why))
               {
                  why = "the step after '/': " + why;
                  return false;
               }
               if (step == 0)
               {
                  why = "a step of 0 is not allowed";
                  return false;
               }
               first = stepFromLowest ? lo : 0;
               last = hi;
            }
            else
            {
               if (!ReadNumber(str, pos, end, lo, hi, first, why))
                  return false;
               last = first;
               if (pos < end && str[pos] == '-')
               {
                  // Range
                  pos++;
                  if (!ReadNumber(str, pos, end, lo, hi, last, why))
                  {
                     why = "the end of the range: " + why;
                     return false;
                  }
                  if (first > last)
                  {
                     long long tmp = first;
                     first = last;
                     last = tmp;
                  }

                  // Step through the range
                  if (pos < end && str[pos] == '/')
                  {
                     pos++;
                     if (!ReadNumber(str, pos, end, 1, hi, step, why))
                     {
                        why = "the step after '/': " + why;
                        return false;
                     }
                  }
               }
            }
            if (pos != end)
            {
               why = "'" + Shown(str.substr(pos, end - pos)) +
                     "' is not understood (each entry is *, x, x-y, x-y/z "
                     "or */x)";
               return false;
            }
            if (!add(first, last, step))
            {
               why = "it has too many values (a field holds at most " +
                     to_string(CRON_MAX_FIELD_VALUES) + ")";
               return false;
            }
            given = true;
         }

         if (end == str.size())
            break;
         start = end + 1;
      }
      if (star && entries > 1)
      {
         why = "'*' can't be mixed with other entries (use '*' on its own)";
         return false;
      }
      return true;
   }

   // Sets the bit of every value an entry allows (time fields; they all
   // fit in 64 bits)
   struct AddToMask
   {
      uint64_t mask;
      int lo;
      bool operator()(long long first, long long last, long long step)
      {
         for (long long value = first; value <= last; value += step)
            if (value >= lo)
               mask |= (uint64_t)1 << value;
         return true;
      }
   };

   // Sorts 'values' and drops repeats
   void Tidy(vector<int>& values)
   {
      sort(values.begin(), values.end());
      values.erase(unique(values.begin(), values.end()), values.end());
   }

   // Collects every value an entry allows (the drift, which can be as large
   // as INT_MAX), so a list such as '0-2147483647' can't use up all of the
   // memory or time:
   //  - At most twice CRON_MAX_FIELD_VALUES values are collected in all,
   //    repeats included.  Small overlaps ('0-9999,5') are fine, but piles
   //    of them ('0,1-9999,2-9999,...') are refused right away.
   //  - Once repeats are dropped, at most CRON_MAX_FIELD_VALUES may be left
   //    (see ParseFields()).
   struct AddToList
   {
      vector<int>& values;
      bool operator()(long long first, long long last, long long step)
      {
         long long added = (last - first) / step + 1;
         if ((long long)values.size() + added > 2 * CRON_MAX_FIELD_VALUES)
            return false;
         for (long long value = first; value <= last; value += step)
            values.push_back((int)value);
         return true;
      }
   };

   // The name of each field, for error messages
   const char *FieldName(int field)
   {
      const char *names[F_COUNT] = {
         "second", "minute", "hour", "day of month", "month", "day of week",
         "drift" };
      return names[field];
   }

   // Parses every field (seconds to drift).  Returns false (with 'why' set
   // to the field and what is wrong with it) if any of them is not
   // formatted correctly.  'stepFromLowest' is crontab's reading of '*/x'.
   bool ParseFields(const string fields[F_COUNT], bool stepFromLowest,
                    CronFields& out, string& why)
   {
      for (int i = 0; i < F_DRIFT; i++)
      {
         AddToMask add = { 0, FieldLow(i) };
         if (!ReadField(fields[i], FieldLow(i), FieldHigh(i), stepFromLowest,
                        add, out.given[i], why))
         {
            why = string(FieldName(i)) + " '" + Shown(fields[i]) + "': " + why;
            return false;
         }
         out.mask[i] = add.mask;
         out.given[i] = out.given[i] && add.mask != 0;
      }

      out.drifts.clear();
      AddToList add = { out.drifts };
      bool given;
      if (!ReadField(fields[F_DRIFT], FieldLow(F_DRIFT), FieldHigh(F_DRIFT),
                     stepFromLowest, add, given, why))
      {
         why = "drift '" + Shown(fields[F_DRIFT]) + "': " + why;
         return false;
      }
      Tidy(out.drifts);
      if (out.drifts.size() > CRON_MAX_FIELD_VALUES)
      {
         why = "drift '" + Shown(fields[F_DRIFT]) + "': it has more than " +
               to_string(CRON_MAX_FIELD_VALUES) + " different values";
         return false;
      }
      return true;
   }

   // Splits and parses a whole cron string.  Throws std::invalid_argument
   // saying what is wrong (ie: "minute '99': 99 is outside of 0-59") if it
   // is not formatted correctly.
   //
   //         Dateblock Format
   //
   //     +------------------------------ second (0 - 59)
   //     |  +--------------------------- min (0 - 59)
   //     |  |  +----------------------- hour (0 - 23)
   //     |  |  |  +----------------- day of month (1 - 31)
   //     |  |  |  |  +----------- month (1 - 12)
   //     |  |  |  |  |  +----- day of week (0 - 6) (Sunday=0)
   //     |  |  |  |  |  |  +- drift time (defaults to zero)
   //     |  |  |  |  |  |  |
   //     -  -  -  -  -  -  -
   //     *  *  *  *  *  *  *
   //
   //         ISC Format (no second or drift)
   //
   //     +----------------------------- min (0 - 59)
   //     |  +----------------------- hour (0 - 23)
   //     |  |  +----------------- day of month (1 - 31)
   //     |  |  |  +----------- month (1 - 12)
   //     |  |  |  |  +----- day of week (0 - 6) (Sunday=0)
   //     |  |  |  |  |
   //     -  -  -  -  -
   //     *  *  *  *  *
   void ParseCron(const string& strIn, bool isISC, CronFields& out)
   {
      vector<string> tokens;
      string why;
      if (!SplitCronString(strIn, isISC, tokens, why))
         throw invalid_argument(why);

      out.isISC = isISC;
      if (!isISC)
      {
         if (!ParseFields(&tokens[0], false, out, why))
            throw invalid_argument(why);
         out.domStar = out.dowStar = true;
         return;
      }

      // ISC has no seconds or drift field ('*' means not given)
      const string fields[F_COUNT] = {
         "*", tokens[0], tokens[1], tokens[2], tokens[3], tokens[4], "*" };
      if (!ParseFields(fields, true, out, why))
         throw invalid_argument(why);

      // crontab(5) treats a day field starting with '*' as unrestricted
      out.domStar = (tokens[2][0] == '*');
      out.dowStar = (tokens[4][0] == '*');
   }

   // The first moment after 'after' that 'f' allows, read in 'zone' (NULL
   // means the computer's own timezone)
   time_t NextTime(const CronFields& f, time_t after,
                   const Date::TimeZone* zone)
   {
      uint64_t masks[F_DRIFT];
      for (int i = 0; i < F_DRIFT; i++)
         masks[i] = f.given[i] ? f.mask[i] : AllOf(FieldLow(i), FieldHigh(i));

      Schedule s;
      if (f.isISC)
      {
         // A crontab entry has no seconds; it runs at the start of the
         // minute.  When both day fields are restricted, either one may
         // match.
         masks[F_SEC] = 1;
         s.domOrDow = !f.domStar && !f.dowStar;
      }
      else
      {
         // Fields smaller than the smallest one given reset to their lowest
         // value, so '-n 5' means 5 minutes past the hour at second 0.
         //  - Day of month and day of week count as the same size.
         //  - With nothing given at all we unblock at the start of each
         //    minute.
         bool dayGiven = f.given[F_DOM] || f.given[F_DOW];
         int smallest = f.given[F_SEC]   ? 0 :
                        f.given[F_MIN]   ? 1 :
                        f.given[F_HOUR]  ? 2 :
                        dayGiven         ? 3 :
                        f.given[F_MONTH] ? 4 : 1;
         if (smallest > 0)
            masks[F_SEC] = (uint64_t)1 << Date::T_SEC_MIN;
         if (smallest > 1 && !f.given[F_MIN])
            masks[F_MIN] = (uint64_t)1 << Date::T_MIN_MIN;
         if (smallest > 2 && !f.given[F_HOUR])
            masks[F_HOUR] = (uint64_t)1 << Date::T_HOUR_MIN;
         if (smallest > 3 && !dayGiven)
            masks[F_DOM] = (uint64_t)1 << Date::T_DOM_MIN;
         s.domOrDow = false;
      }
      s.sec = masks[F_SEC];
      s.min = masks[F_MIN];
      s.hour = masks[F_HOUR];
      s.dom = masks[F_DOM];
      s.month = masks[F_MONTH];
      s.dow = masks[F_DOW];

      // A drift is added after the match.  Matching from 'now - drift' keeps
      // only the part of the drift that fits inside the schedule's interval,
      // so no match is ever skipped.  With several drifts the earliest wins.
      //  - Drifts are handled largest first.  One search from 'now -
      //    largest' finds a match that every drift reaching past it shares,
      //    so those need no search of their own; only the smallest of them
      //    can win.
      //  - The rest start searching after that match, so the whole drift
      //    list costs about one pass over the time it covers instead of one
      //    full search per drift.
      vector<time_t> drifts(f.drifts.begin(), f.drifts.end());
      if (drifts.empty())
         drifts.push_back(0);

      const time_t lowest = numeric_limits<time_t>::min();
      const time_t highest = numeric_limits<time_t>::max();
      time_t best = highest;
      vector<time_t>::iterator end = drifts.end();
      while (end != drifts.begin())
      {
         time_t largest = *(end - 1);
         if (after < lowest + largest)
            throw runtime_error("The reference time is out of range.");
         time_t match = NextMatch(s, after - largest, zone);

         // The smallest drift that still lands after now
         vector<time_t>::iterator use =
            upper_bound(drifts.begin(), end, after - match);
         if (match > highest - *use)
            throw runtime_error("The matching time is out of range.");
         time_t when = match + *use;
         if (when < best)
            best = when;

         // Nothing can be sooner than the very next second
         if (best == after + 1)
            break;
         end = use;
      }

      struct tm check;
      if (!ZoneLocal(zone, best, check))
         throw runtime_error("The matching time is out of range.");
      return best;
   }
}

// Limits
const int Date::T_SEC_MIN=0;
const int Date::T_SEC_MAX=59;
const int Date::T_MIN_MIN=0;
const int Date::T_MIN_MAX=59;
const int Date::T_HOUR_MIN=0;
const int Date::T_HOUR_MAX=23;
const int Date::T_DOM_MIN=1;
const int Date::T_DOM_MAX=31;
const int Date::T_MONTH_MIN=1;
const int Date::T_MONTH_MAX=12;
const int Date::T_YEAR_MIN=0;
const int Date::T_YEAR_MAX=INT_MAX;
const int Date::T_DOW_MIN=0;
const int Date::T_DOW_MAX=6;

// Fields
const int Date::ISC_CRON_FIELD_COUNT = 5;
const int Date::DBL_CRON_FIELD_COUNT = 7;

// Drift Defaults
const int Date::T_NO_ENTRY=-1;

// Presently our AddSec() function only handles integers as input and therefore
// we need to set the max to the maximum values of a 'signed integer' AddSec()
// will always take a signed value since we allow the decrementing of time by
// passing in a negative number
const int Date::T_DRIFT_MAX=INT_MAX;

// Minimum Drift Time
const int Date::T_DRIFT_MIN=0;

//------------------------------------------------
//
//  Function: Constructor (now)
//
//------------------------------------------------
Date::Date()
{
   Set();
}

//------------------------------------------------
//
//  Function: Constructor (accepts YYYY, MM, DD, hh, mm, ss)
//
//------------------------------------------------
Date::Date(int year, int mon, int day,
           int hour, int min, int sec)
{
   Set(year, mon, day, hour, min, sec);
}

//------------------------------------------------
//
//  Function: Constructor (uses Date object)
//
//------------------------------------------------
Date::Date(const Date & dObj)
{
   Set(dObj);
}

//------------------------------------------------
//
//  Function: Constructor (uses time_t type)
//
//------------------------------------------------
Date::Date(const time_t localTime)
{
   Set(localTime);
}

//------------------------------------------------
//
//  Function: Constructor (uses struct tm object)
//
//------------------------------------------------
Date::Date(const struct tm& tmPtr)
{
   Set(tmPtr);
}

//------------------------------------------------
//
//  Function: Deconstructor
//
//------------------------------------------------
Date::~Date()
{
}

//------------------------------------------------
//
//  Function: Assignment '=' (uses Date object)
//
//------------------------------------------------
const Date & Date::operator=(const Date& right)
{
   memcpy(&m_tmObj,&right.m_tmObj,sizeof(struct tm));
   m_time = right.m_time;
   return (*this);
}

//------------------------------------------------
//
//  Function: Assignment '=' (uses time_t type)
//
//------------------------------------------------
const Date & Date::operator=(const time_t &right)
{
   Set(right);
   return (*this);
}

//------------------------------------------------
//
//  Function: Assignment '=' (uses struct tm object)
//
//------------------------------------------------
const Date & Date::operator=(const struct tm &right)
{
   Set(right);
   return (*this);
}

//------------------------------------------------
//
//  Function: Set (now)
//
//------------------------------------------------
void Date::Set()
{
   m_time=Now();
   LocalTime(m_time, m_tmObj);
}

//------------------------------------------------
//
//  Function: Set (accepts YYYY, MM, DD, hh, mm, ss)
//
//------------------------------------------------
void Date::Set(int year, int mon, int day,
               int hour, int min, int sec)
{
   m_time=Now();
   LocalTime(m_time, m_tmObj);

   // Other values out of range keep today's value for that field
   struct tm tmObj = m_tmObj;
   if (year >= T_YEAR_MIN && year <= T_YEAR_MAX)
      tmObj.tm_year = year - 1900;
   if (mon >= T_MONTH_MIN && mon <= T_MONTH_MAX)
      tmObj.tm_mon = mon - 1;
   if (hour >= T_HOUR_MIN && hour <= T_HOUR_MAX)
      tmObj.tm_hour = hour;
   if (min >= T_MIN_MIN && min <= T_MIN_MAX)
      tmObj.tm_min = min;
   if (sec >= T_SEC_MIN && sec <= T_SEC_MAX)
      tmObj.tm_sec = sec;

   // A day out of range means the 1st; one past the end of the month
   // becomes the last day (ie: the 31st of April is the 30th)
   tmObj.tm_mday = (day >= T_DOM_MIN && day <= T_DOM_MAX) ? day : T_DOM_MIN;
   int maxDays = DaysInMonth(tmObj.tm_year + 1900, tmObj.tm_mon);
   if (tmObj.tm_mday > maxDays)
      tmObj.tm_mday = maxDays;

   // Set it all at once, and let mktime() work out daylight saving for
   // that date (today's setting may not apply to it)
   tmObj.tm_isdst = -1;
   Set(tmObj);
}

//------------------------------------------------
//
//  Function: Set (uses Date object)
//
//------------------------------------------------
void Date::Set(const Date& dObj)
{
   (*this) = dObj;
}

//------------------------------------------------
//
//  Function: Set (uses time_t type)
//  where tVal is the EPOCH Time (number of seconds
//  since 1970-01-01 00:00:01)
//
//------------------------------------------------
void Date::Set(const time_t &tVal)
{
   struct tm tmObj;
   if (!LocalTime(tVal, tmObj))
   {
      throw out_of_range("The time can't be shown as a local date.");
   }
   m_time = tVal;
   m_tmObj = tmObj;
}

//------------------------------------------------
//
//  Function: Set (uses struct tm object)
//
//------------------------------------------------
void Date::Set(const struct tm &tmObj)
{
   if (!MakeTime(tmObj, m_tmObj, m_time))
   {
      throw out_of_range("The date is outside of the supported range.");
   }
}

//------------------------------------------------
//
//  Function: Operator ==
//
//------------------------------------------------
bool Date::operator==(const Date& right) const
{
   return (m_time == right.m_time);
}

//------------------------------------------------
//
//  Function: Operator !=
//
//------------------------------------------------
bool Date::operator!=(const Date& right) const
{
   return (m_time != right.m_time);
}

//------------------------------------------------
//
//  Function: Operator <
//
//------------------------------------------------
bool Date::operator<(const Date& right) const
{
   return (m_time < right.m_time);
}

//------------------------------------------------
//
//  Function: Operator <=
//
//------------------------------------------------
bool Date::operator<=(const Date& right) const
{
   return (m_time <= right.m_time);
}

//------------------------------------------------
//
//  Function: Operator >
//
//------------------------------------------------
bool Date::operator>(const Date& right) const
{
   return (m_time > right.m_time);
}

//------------------------------------------------
//
//  Function: Operator >=
//
//------------------------------------------------
bool Date::operator>=(const Date& right) const
{
   return (m_time >= right.m_time);
}

//------------------------------------------------
//
//  Function: MaxDOMsThisMonth
//
//------------------------------------------------
int Date::MaxDOMsThisMonth() const
{
   return DaysInMonth(m_tmObj.tm_year + 1900, m_tmObj.tm_mon);
}

//------------------------------------------------
//
//  Function: MaxDOMsNextMonth
//
//------------------------------------------------
int Date::MaxDOMsNextMonth() const
{
   int month = m_tmObj.tm_mon + 1;
   int year = m_tmObj.tm_year + 1900;
   if (month > 11)
   {
      month = 0;
      year++;
   }
   return DaysInMonth(year, month);
}

//------------------------------------------------
//
//  Function: MaxDOMsPrevMonth
//
//------------------------------------------------
int Date::MaxDOMsPrevMonth() const
{
   int month = m_tmObj.tm_mon - 1;
   int year = m_tmObj.tm_year + 1900;
   if (month < 0)
   {
      month = 11;
      year--;
   }
   return DaysInMonth(year, month);
}


//--------------------------------------------------
//
//  Function: AddSec
//
//------------------------------------------------
bool Date::AddSec(const int seconds)
{
   // Aquire Time Now
   struct tm tmTime;
   time_t tTmp=m_time+seconds;
   if (!LocalTime(tTmp, tmTime))
   {
      return false;
   }

   m_time=tTmp;
   m_tmObj = tmTime;
   return true;
}

//--------------------------------------------------
//
//  Function: AddMin
//
//------------------------------------------------
bool Date::AddMin(const int minutes)
{
   // Aquire Time Now
   struct tm tmTime;
   time_t tTmp=m_time+(time_t)minutes*60;
   if (!LocalTime(tTmp, tmTime))
   {
      return false;
   }

   m_time=tTmp;
   m_tmObj = tmTime;
   return true;
}

//--------------------------------------------------
//
//  Function: AddHour
//
//------------------------------------------------
bool Date::AddHour(const int hours)
{
   // Aquire Time Now
   struct tm tmTime;
   time_t tTmp=m_time+(time_t)hours*3600;
   if (!LocalTime(tTmp, tmTime))
   {
      return false;
   }

   // Store new time_t type
   m_time = tTmp;
   // Store struct tm type
   m_tmObj = tmTime;
   return true;
}

//--------------------------------------------------
//
//  Function: AddDOM
//
//------------------------------------------------
bool Date::AddDOM(const int days)
{
   // Aquire Time Now
   struct tm tmTime;
   time_t tTmp=m_time+(time_t)days*86400;
   if (!LocalTime(tTmp, tmTime))
   {
      return false;
   }

   m_time=tTmp;
   m_tmObj = tmTime;
   return true;
}

//--------------------------------------------------
//
//  Function: AddMonth
//
//------------------------------------------------
bool Date::AddMonth(const int months)
{
   struct tm tmTime;

   memcpy(&tmTime,&m_tmObj,sizeof(struct tm));
   // Temporarily Store the Current Day
   int tmpDays = tmTime.tm_mday;

   // Work out the new month and year directly (no loop), refusing years
   // that don't fit in a struct tm
   long long total = (long long)tmTime.tm_mon + months;
   long long years = total / 12;
   int month = (int)(total % 12);
   if (month < 0)
   {
      month += 12;
      years--;
   }
   long long year = (long long)tmTime.tm_year + years;
   if (year < INT_MIN || year > INT_MAX - 1900)
   {
      return false;
   }
   tmTime.tm_mon = month;
   tmTime.tm_year = (int)year;

   // Past the end of the new month means its last day (Jan 31st plus a
   // month is Feb 28th or 29th)
   int maxDays = DaysInMonth((int)(year + 1900), month);
   tmTime.tm_mday = (tmpDays > maxDays) ? maxDays : tmpDays;
   return MakeLocalTime(tmTime, m_tmObj, m_time);
}

//--------------------------------------------------
//
//  Function: AddYear
//
//------------------------------------------------
bool Date::AddYear(const int years)
{
   struct tm tmTime;
   memcpy(&tmTime,&m_tmObj,sizeof(struct tm));
   long long year = (long long)tmTime.tm_year + years;
   if (year < INT_MIN || year > INT_MAX - 1900)
   {
      return false;
   }
   tmTime.tm_year = (int)year;
   return MakeLocalTime(tmTime, m_tmObj, m_time);
}

//------------------------------------------------
//
//  Function: SetSec
//
//------------------------------------------------
bool Date::SetSec(const int seconds)
{
   if(seconds < T_SEC_MIN || seconds > T_SEC_MAX)
   {
      return false;
   }
   struct tm tmTime = m_tmObj;
   tmTime.tm_sec = seconds;
   return MakeLocalTime(tmTime, m_tmObj, m_time);
}

//------------------------------------------------
//
//  Function: SetMin
//
//------------------------------------------------
bool Date::SetMin(const int minutes)
{
   if(minutes < T_MIN_MIN || minutes > T_MIN_MAX)
   {
      return false;
   }
   struct tm tmTime = m_tmObj;
   tmTime.tm_min = minutes;
   return MakeLocalTime(tmTime, m_tmObj, m_time);
}

//------------------------------------------------
//
//  Function: SetHour
//
//------------------------------------------------
bool Date::SetHour(const int hours)
{
   if(hours < T_HOUR_MIN || hours > T_HOUR_MAX)
   {
      return false;
   }
   struct tm tmTime = m_tmObj;
   tmTime.tm_hour = hours;
   return MakeLocalTime(tmTime, m_tmObj, m_time);
}

//------------------------------------------------
//
//  Function: SetDOM
//
//------------------------------------------------
bool Date::SetDOM(const int days)
{
   if(days < T_DOM_MIN || days > T_DOM_MAX)
   {
      return false;
   }
   // Past the end of the month means its last day
   int maxDays = MaxDOMsThisMonth();
   struct tm tmTime = m_tmObj;
   tmTime.tm_mday = (days > maxDays) ? maxDays : days;
   return MakeLocalTime(tmTime, m_tmObj, m_time);
}

//------------------------------------------------
//
//  Function: SetMonth
//
//------------------------------------------------
bool Date::SetMonth(const int months)
{
   if(months < T_MONTH_MIN || months > T_MONTH_MAX)
   {
      return false;
   }
   struct tm tmTime = m_tmObj;
   tmTime.tm_mon = months - 1;
   return MakeLocalTime(tmTime, m_tmObj, m_time);
}

//------------------------------------------------
//
//  Function: SetYear
//
//------------------------------------------------
bool Date::SetYear(const int years)
{
   if(years < T_YEAR_MIN || years > T_YEAR_MAX)
   {
      return false;
   }
   struct tm tmTime = m_tmObj;
   tmTime.tm_year = years - 1900;
   return MakeLocalTime(tmTime, m_tmObj, m_time);
}

//------------------------------------------------
//
//  Function: Str
//
//------------------------------------------------
const string Date::Str(const string& strFormat) const
{
   if (strFormat.empty())
   {
      return string();
   }

   // strftime() returns 0 when the buffer is too small, so keep growing
   // it until the result fits.  Give up (returning an empty string) for
   // formats that genuinely produce nothing or are absurdly large.
   vector<char> buf(256);
   while (buf.size() <= 65536)
   {
      size_t len = strftime(&buf[0], buf.size(), strFormat.c_str(), &m_tmObj);
      if (len > 0)
      {
         return string(&buf[0], len);
      }
      buf.resize(buf.size() * 2);
   }
   return string();
}

//------------------------------------------------
//
//  Function: Cron (Basic)
//
//------------------------------------------------
const Date Date::Cron(int lSecOffset,
                      int lMinOffset,
                      int lHourOffset,
                      int lDomOffset,
                      int lMonthOffset,
                      int lDowOffset,
                      int lDriftOffset) const
{
   // A value outside of its field's range means "not set"
   const int values[F_COUNT] = {
      lSecOffset, lMinOffset, lHourOffset, lDomOffset,
      lMonthOffset, lDowOffset, lDriftOffset };

   CronFields f;
   for (int i = 0; i < F_DRIFT; i++)
   {
      f.given[i] = (values[i] >= FieldLow(i) && values[i] <= FieldHigh(i));
      f.mask[i] = f.given[i] ? (uint64_t)1 << values[i] : 0;
   }
   if (lDriftOffset >= T_DRIFT_MIN && lDriftOffset <= T_DRIFT_MAX)
      f.drifts.push_back(lDriftOffset);
   f.isISC = false;
   f.domStar = f.dowStar = true;
   return Date(NextTime(f, m_time, NULL));
}

//------------------------------------------------
//
//  Function: CronValid (Advanced)
//
//------------------------------------------------
bool Date::CronValid(const string& sSecOffset,
                     const string& sMinOffset,
                     const string& sHourOffset,
                     const string& sDomOffset,
                     const string& sMonthOffset,
                     const string& sDowOffset,
                     const string& sDriftOffset)
{
   const string fields[F_COUNT] = {
      sSecOffset, sMinOffset, sHourOffset, sDomOffset,
      sMonthOffset, sDowOffset, sDriftOffset };
   CronFields f;
   string why;
   return ParseFields(fields, false, f, why);
}

//------------------------------------------------
//
//  Function: CronValid
//            (using whitespace as a delimiter)
//
//------------------------------------------------
bool Date::CronValid(const string& strIn, bool isISC)
{
   CronFields f;
   try
   {
      ParseCron(strIn, isISC, f);
   }
   catch (const invalid_argument&)
   {
      return false;
   }
   return true;
}

//------------------------------------------------
//
//  Function: Cron (Advanced)
//
//------------------------------------------------
const Date Date::Cron(const string& sSecOffset,
                      const string& sMinOffset,
                      const string& sHourOffset,
                      const string& sDomOffset,
                      const string& sMonthOffset,
                      const string& sDowOffset,
                      const string& sDriftOffset) const
{
   const string fields[F_COUNT] = {
      sSecOffset, sMinOffset, sHourOffset, sDomOffset,
      sMonthOffset, sDowOffset, sDriftOffset };
   CronFields f;
   string why;
   if (!ParseFields(fields, false, f, why))
   {
      throw invalid_argument(why);
   }
   f.isISC = false;
   f.domStar = f.dowStar = true;
   return Date(NextTime(f, m_time, NULL));
}

//------------------------------------------------
//
//  Function: Cron
//            (using whitespace as a delimiter)
//------------------------------------------------
const Date Date::Cron(const string& strIn, bool isISC) const
{
   CronFields f;
   ParseCron(strIn, isISC, f);
   return Date(NextTime(f, m_time, NULL));
}

//------------------------------------------------
//
//  Function: CronAfter
//
//------------------------------------------------
time_t Date::CronAfter(time_t after, const string& strIn, bool isISC,
                       const TimeZone& zone)
{
   CronFields f;
   ParseCron(strIn, isISC, f);
   return NextTime(f, after, &zone);
}

//------------------------------------------------
//
//  Function: BlockUntil
//
//  Desc: Sleeps until the wall clock reaches a specific moment.  Sleeping
//        until an exact time (rather than for a number of seconds) means
//        we wake up right on the second, even if the clock was adjusted
//        while we slept.
//
//------------------------------------------------
unsigned long Date::BlockUntil(const Date& until)
{
   return BlockUntil(until.Time());
}

unsigned long Date::BlockUntil(const struct tm& until)
{
   struct tm tmOut;
   time_t when;
   if (!MakeTime(until, tmOut, when))
   {
      throw out_of_range("The date is outside of the supported range.");
   }
   return BlockUntil(when);
}

unsigned long Date::BlockUntil(time_t until)
{
#if !defined(_WIN32) && !defined(__APPLE__)
   struct timespec target;
   target.tv_sec = until;
   target.tv_nsec = 0;

   int rc = clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &target, NULL);
   if (rc == 0 || rc == EINTR)
   {
      // Done, or a signal woke us up early; report how long is left
      return SecondsLeft(until, Now());
   }
   // Any other error: fall back to sleeping a second at a time below
#endif

   // macOS and Windows can't sleep until an exact time, so sleep for
   // whatever is left instead.  Sleep at most a second at a time so that
   // Ctrl-C is noticed quickly; the caller loops until we return 0.
   struct timespec now;
   NowPrecise(now);
   if (now.tv_sec >= until)
   {
      return 0;
   }

   double left = (double)(until - now.tv_sec) - now.tv_nsec / 1e9;
   double chunk = (left > 1.0) ? 1.0 : left;
#ifdef _WIN32
   Sleep((DWORD)(chunk * 1000.0) + 1);
#else
   struct timespec rel;
   rel.tv_sec = (time_t)chunk;
   rel.tv_nsec = (long)((chunk - (double)rel.tv_sec) * 1e9);
   nanosleep(&rel, NULL);
#endif

   return SecondsLeft(until, Now());
}
