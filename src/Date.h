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
#ifndef DATEBLOCK_DATE_H
#define DATEBLOCK_DATE_H

#include <time.h>
#include <string>
#include <set>
#include <vector>

class Date
{
   public:
     // Lets Cron work in a timezone other than the computer's own (see
     // CronAfter() below).
     //  - ToLocal() fills in tm_year, tm_mon, tm_mday, tm_hour, tm_min and
     //    tm_sec (as localtime() would) for the moment 't'.
     //  - It returns false if it can't; the cron search then gives up.
     //  - The day of the week is worked out for you.
     class TimeZone
     {
        public:
          virtual ~TimeZone() {}
          virtual bool ToLocal(time_t t, struct tm& tmOut) const = 0;
     };

     // Some Min/Max Restrictions
     static const int T_SEC_MIN;
     static const int T_SEC_MAX;
     static const int T_MIN_MIN;
     static const int T_MIN_MAX;
     static const int T_HOUR_MIN;
     static const int T_HOUR_MAX;
     static const int T_MONTH_MIN;
     static const int T_MONTH_MAX;
     static const int T_YEAR_MIN;
     static const int T_YEAR_MAX;
     static const int T_DOM_MIN;
     static const int T_DOM_MAX;
     static const int T_DOW_MIN;
     static const int T_DOW_MAX;
     static const int T_DRIFT_MIN;
     static const int T_DRIFT_MAX;
     static const int T_NO_ENTRY;

     // ISC Cron Fields (5 of them)
     static const int ISC_CRON_FIELD_COUNT;

     // Dateblock Cron Fields (7 of them at this time) with the extension of
     // both seconds and drift)
     static const int DBL_CRON_FIELD_COUNT;

     // The constructors, Set() and '=' throw std::out_of_range for a
     // moment that can't be shown as a local date (ie: a time_t hundreds
     // of millions of years away).  The Add*() and Set*() functions below
     // return false instead and leave the date as it was.
     Date();
     Date(int year,
          int month = 1,
          int day = 1,
          int hour = 0,
          int minute = 0,
          int second = 0);
     Date(const time_t localTime);
     Date(const struct tm& tmPtr);
     Date(const Date& dateObjIn);
     ~Date();

     bool operator==(const Date&) const;
     bool operator<(const Date&) const;
     bool operator<=(const Date&) const;
     bool operator>(const Date&) const;
     bool operator>=(const Date&) const;
     bool operator!=(const Date& right) const;

     // Assignments
     const Date & operator=(const Date&);
     const Date & operator=(const time_t &);
     const Date & operator=(const struct tm &);
     void Set(); // get Now
     void Set(int year,
              int month = 1,
              int day = 1,
              int hour = 0,
              int minute = 0,
              int second = 0);
     void Set(const Date&);
     void Set(const time_t &);
     void Set(const struct tm &);

     // Add (or, when negative, take away) time:
     //  - AddSec(), AddMin() and AddHour() count real elapsed time
     //  - AddDOM(), AddMonth() and AddYear() move the calendar date and keep
     //    the clock time, even across daylight saving (12:00 stays 12:00)
     //  - past the end of the new month means its last day (Jan 31st plus a
     //    month, or Feb 29th plus a year, lands on Feb 28th)
     //  - T_YEAR_MIN and T_YEAR_MAX only limit what Set() and SetYear()
     //    accept; adding can go before year 0 (year -1 is 2 BC)
     bool AddSec(const int seconds = 1);
     bool AddMin(const int minutes = 1);
     bool AddHour(const int hours = 1);
     bool AddDOM(const int days = 1);
     bool AddMonth(const int months = 1);
     bool AddYear(const int years = 1);

     bool SetSec(const int seconds = 0);
     bool SetMin(const int minutes = 0);
     bool SetHour(const int hours = 0);
     bool SetDOM(const int days = 1);
     // Past the end of the new month means its last day (Jan 31st set to
     // February is the 28th or 29th, Feb 29th set to 2025 is the 28th), as
     // AddMonth() does
     bool SetMonth(const int months = 1);
     bool SetYear(const int years);

     time_t Time()const;
     const struct tm& Tm()const;

     // returns a string based on passed in format (uses strftime codes)
     const std::string Str(const std::string& format="%Y-%m-%d %H:%M:%S") const;

     int Sec() const; // returns 0-59
     int Min() const; // returns 0-59
     int Hour() const; // returns 0-23
     int DOM() const; // returns 1-31
     int Month() const; // returns 1-12 Jan=1, Dec=12
     int Year() const; // returns the full year (ie: 2025)
     int DOW() const; // returns 0-6 Sun=0, Sat=6

     int MaxDOMsThisMonth() const; // returns 28-31
     int MaxDOMsNextMonth() const; // returns 28-31
     int MaxDOMsPrevMonth() const; // returns 28-31

     // A more advanced cron that calculates cron syntax such as:
     // 0-4,6      (which would handle 0,1,2,3,4,6)
     // 1,2,6,7,8  (which would handle 1,2,6,7,8)
     // */10       (which would handle 'every increment of 10',
     //             in the minute column; this would be similar
     //             to writing: 0,10,20,30,40,50)
     //
     // Valid ranges are no different then whats specified
     // above.

     // CronValid Tests Cron Strings and returns true
     // if they are parseable.
     //
     // sSecOffset, sMinOffset, and sHourOffset can all be set
     // to '*' as their default value;  They are not defined
     // as defaults below to remove any ambiguity with the other
     // CronValid() function
     static bool CronValid(const std::string& sSecOffset,
                           const std::string& sMinOffset,
                           const std::string& sHourOffset,
                           const std::string& sDomOffset   = "*",
                           const std::string& sMonthOffset = "*",
                           const std::string& sDowOffset   = "*",
                           const std::string& sDriftOffset = "*");

     static bool CronValid(const std::string& sCronStr="*  *  *  *  *  *  *",
                            bool isISC=false);

     // Cron
     //  -1 means not set.
     //  Valid Ranges are as follows:
     //     Second (0-59) Default=0
     //     Minute (0-59)
     //     Hour (0-23)
     //     DOM of Month (1-31)
     //     Month (1-11) {Jan=1,...,Dec=12}
     //     DOM of Week (0-6) {Sun=0,...,Sat=6}
     //     Drift Offset identifies how many seconds to adjust
     //     the final results by.
     //  Returns a Date Object of the next moment in time
     //  that matches specified cron entry
     //  By default lSecoffset is always set to 0 if 'nothing'
     //    else was specified.
     //  Days a month doesn't have are simply skipped in that month: days
     //  20-31 of February and March match Feb 20-28 (29 in a leap year)
     //  and Mar 20-31.  Only when nothing at all can ever match (ie: only
     //  the 30th and 31st of February) is std::runtime_error thrown.  All
     //  of the Cron() functions below behave the same way.
     //  The Cron() functions that take strings also throw
     //  std::invalid_argument if the cron is not formatted correctly; its
     //  what() says which field is wrong and why (ie: "minute '99': 99 is
     //  outside of 0-59").  CronValid() checks one without an exception.
     //  A cron (or a single field) longer than 64 KiB is always refused.
     const Date Cron(int lSecOffset   = T_NO_ENTRY,
                     int lMinOffset   = T_NO_ENTRY,
                     int lHourOffset  = T_NO_ENTRY,
                     int lDomOffset   = T_NO_ENTRY,
                     int lMonthOffset = T_NO_ENTRY,
                     int lDowOffset   = T_NO_ENTRY,
                     int lDriftOffset = T_NO_ENTRY) const;

     // sSecOffset, sMinOffset, and sHourOffset can all be set
     // to '*' as their default value;  They are not defined
     // as defaults below to remove any ambiguity with the other
     // Cron() function
     const Date Cron(const std::string& sSecOffset,
                     const std::string& sMinOffset,
                     const std::string& sHourOffset,
                     const std::string& sDomOffset   = "*",
                     const std::string& sMonthOffset = "*",
                     const std::string& sDowOffset   = "*",
                     const std::string& sDriftOffset = "*") const;

     // Support the ISC Cron Formatting (same as above, but without
     // the seconds) (UNIX/Linux Crontab equivalent)
     //
     // day of week (0 - 6) (Sunday=0) -----------------------+
     // month (1 - 12) ------------------------------------+  |
     // day of month (1 - 31) --------------------------+  |  |
     // hour (0 - 23) -------------------------------+  |  |  |
     // min (0 - 59) -----------------------------+  |  |  |  |
     //                                           |  |  |  |  |
     //                                           -  -  -  -  -
     //                                   String: *  *  *  *  *
     //
     // Standard Formatting
     // drift time --------------------------------------------+
     // day of week (0 - 6) (Sunday=0) ---------------------+  |
     // month (1 - 12) ----------------------------------+  |  |
     // day of month (1 - 31) ------------------------+  |  |  |
     // hour (0 - 23) -----------------------------+  |  |  |  |
     // min (0 - 59) ---------------------------+  |  |  |  |  |
     // seconds(0 - 59) ---------------------+  |  |  |  |  |  |
     //                                      |  |  |  |  |  |  |
     //                                      -  -  -  -  -  -  -
     //                              String: *  *  *  *  *  *  *
     const Date Cron(const std::string& sCronStr="*  *  *  *  *  *  *",
                     bool isISC = false) const;

     // The same as Cron() above, but read in 'zone' instead of the
     // computer's own timezone.
     //  - 'after' and the result are seconds since the epoch, so they mean
     //    the same moment everywhere.
     //  - Daylight saving follows 'zone'.
     static time_t CronAfter(time_t after, const std::string& sCronStr,
                             bool isISC, const TimeZone& zone);

     // Sleeps until the wall clock reaches 'until'.
     //  - Returns 0 once that moment arrives (or right away if it has
     //    already passed).
     //  - Otherwise returns the number of seconds still left; call it again
     //    to keep waiting.  This happens when a signal wakes us up early,
     //    and (on macOS and Windows) after each WaitStep() of sleep.
     //  - The wait follows the wall clock, so it still ends on time if the
     //    computer sleeps or its clock is changed meanwhile.
     //  - A struct tm is a local date/time, read the same way mktime()
     //    reads it (set tm_isdst to -1 to let it work out daylight
     //    saving).  Throws std::out_of_range if it isn't a date that can
     //    be shown.
     static unsigned long BlockUntil(const Date& until);
     static unsigned long BlockUntil(time_t until);
     static unsigned long BlockUntil(const struct tm& until);

     // How long BlockUntil() sleeps (in seconds) on macOS and Windows
     // before it looks at the clock again; 0 once 'until' has arrived.
     // The system's own timer wakes it right on time; these regular
     // checks are a safety net in case it doesn't.
     //  - More than an hour and a minute away: an hour.
     //  - Otherwise: until a minute before 'until'.
     //  - The last minute: until the next whole second.
     static double WaitStep(time_t until, const struct timespec& now);

   private:
     struct tm m_tmObj;
     time_t m_time;
};

inline time_t Date::Time() const
{
   return m_time;
}

inline const struct tm& Date::Tm() const
{
   return m_tmObj;
}
inline int Date::Sec() const
{
   return m_tmObj.tm_sec;
}
inline int Date::Min() const
{
   return m_tmObj.tm_min;
}
inline int Date::Hour() const
{
   return m_tmObj.tm_hour;
}
inline int Date::DOM() const
{
   return m_tmObj.tm_mday;
}
inline int Date::Month() const
{
   return m_tmObj.tm_mon + 1;
}
inline int Date::DOW() const
{
   return m_tmObj.tm_wday;
}
inline int Date::Year() const
{
   return m_tmObj.tm_year + 1900;
}
#endif
