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
 this program; if not, write to the Free Software Foundation, Inc.,
 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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
#include <set>
#include <vector>
#include <ctype.h>
#include <sstream>
#include <stdexcept>
#include "Date.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <sys/time.h>
#endif

#ifdef DEBUG
#include <iostream>
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

   // Splits a whitespace separated cron string into its fields.
   //  - Missing fields are filled in with '*'.
   //  - A '+N' entry (dateblock format only) jumps straight to the drift
   //    field; nothing may follow it.
   //  - Returns false if the string has more fields than allowed.
   // Thread safe localtime() (Windows spells it localtime_s)
   bool LocalTime(time_t tVal, struct tm& tmOut)
   {
   #ifdef _WIN32
      return localtime_s(&tmOut, &tVal) == 0;
   #else
      return localtime_r(&tVal, &tmOut) != NULL;
   #endif
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

   bool SplitCronString(const string& strIn, bool isISC,
                        vector<string>& tokens)
   {
      // ISC only processes the first 5 fields, where as
      // otherwise we process the first 7
      size_t token_max = isISC?Date::ISC_CRON_FIELD_COUNT:
                               Date::DBL_CRON_FIELD_COUNT;
      bool ok = true;
      bool drifted = false;
      string token;
      istringstream in(strIn);

      tokens.clear();
      while (in >> token)
      {
         if (drifted || tokens.size() >= token_max)
         {
            // More entries then expected found
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
         }
         tokens.push_back(token);
      }

      // Placeholders
      while (tokens.size() < token_max)
         tokens.push_back("*");

      return ok;
   }

   // Reads the number starting at 'pos' (moving 'pos' past it).  Fails if
   // there is no number there or it is outside of 'lo' to 'hi'.
   bool ReadNumber(const string& str, size_t& pos, int lo, int hi,
                   long long& out)
   {
      size_t start = pos;
      out = 0;
      while (pos < str.size() && isdigit((unsigned char)str[pos]))
      {
         out = out * 10 + (str[pos] - '0');
         if (out > hi)
            return false;
         pos++;
      }
      return pos > start && out >= lo;
   }

   // The calendar (leap years and weekdays included) repeats every 400
   // years.  If nothing matches within that long, nothing ever will (ie:
   // the 31st of February).
   const time_t SEARCH_HORIZON = (time_t)146097 * 86400;

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

   // Moves 't' forward to a local date/time worked out by mktime().  It
   // always moves forward, even if the clock change around a daylight
   // saving switch makes mktime() land a little early.
   time_t JumpTo(time_t t, struct tm target)
   {
      target.tm_isdst = -1;
      time_t next = mktime(&target);
      return (next > t) ? next : t + 3600;
   }

   // The first moment after 'after' whose local time matches 's'.
   //  - Moves forward one real moment at a time (jumping over whole
   //    seconds, minutes, hours, days and months that can't match), so a
   //    daylight saving change never makes it skip a match.
   //  - Throws std::runtime_error if nothing matches.
   time_t NextMatch(const Schedule& s, time_t after)
   {
      const time_t max = numeric_limits<time_t>::max();
      time_t t = after + 1;
      time_t limit = (after > max - SEARCH_HORIZON) ? max : after + SEARCH_HORIZON;
      struct tm tm;

      while (t <= limit && LocalTime(t, tm))
      {
         if (!Has(s.month, tm.tm_mon + 1))
         {
            struct tm next = tm;
            next.tm_mon++;
            next.tm_mday = 1;
            next.tm_hour = next.tm_min = next.tm_sec = 0;
            t = JumpTo(t, next);
            continue;
         }

         if (!DayMatches(s, tm) || NextIn(s.hour, tm.tm_hour, 23) < 0)
         {
            struct tm next = tm;
            next.tm_mday++;
            next.tm_hour = next.tm_min = next.tm_sec = 0;
            t = JumpTo(t, next);
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
   m_time = right;
   LocalTime(m_time, m_tmObj);
   return (*this);
}

//------------------------------------------------
//
//  Function: Assignment '=' (uses struct tm object)
//
//------------------------------------------------
const Date & Date::operator=(const struct tm &right)
{
   memcpy(&m_tmObj,&right,sizeof(struct tm));
   m_time = mktime(&m_tmObj);
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
   m_time = tVal;
   LocalTime(m_time, m_tmObj);
}

//------------------------------------------------
//
//  Function: Set (uses struct tm object)
//
//------------------------------------------------
void Date::Set(const struct tm &tmObj)
{
   memcpy(&m_tmObj,&tmObj,sizeof(struct tm));
   m_time = mktime(&m_tmObj);
}

//------------------------------------------------
//
//  Function: Operator ==
//
//------------------------------------------------
const bool Date::operator==(const Date& right) const
{
   return (m_time == right.m_time);
}

//------------------------------------------------
//
//  Function: Operator !=
//
//------------------------------------------------
const bool Date::operator!=(const Date& right) const
{
   return (m_time != right.m_time);
}

//------------------------------------------------
//
//  Function: Operator <
//
//------------------------------------------------
const bool Date::operator<(const Date& right) const
{
   return (m_time < right.m_time);
}

//------------------------------------------------
//
//  Function: Operator <=
//
//------------------------------------------------
const bool Date::operator<=(const Date& right) const
{
   return (m_time <= right.m_time);
}

//------------------------------------------------
//
//  Function: Operator >
//
//------------------------------------------------
const bool Date::operator>(const Date& right) const
{
   return (m_time > right.m_time);
}

//------------------------------------------------
//
//  Function: Operator >=
//
//------------------------------------------------
const bool Date::operator>=(const Date& right) const
{
   return (m_time >= right.m_time);
}

//------------------------------------------------
//
//  Function: MaxDOMsThisMonth
//
//------------------------------------------------
const int Date::MaxDOMsThisMonth() const
{
   return DaysInMonth(m_tmObj.tm_year + 1900, m_tmObj.tm_mon);
}

//------------------------------------------------
//
//  Function: MaxDOMsNextMonth
//
//------------------------------------------------
const int Date::MaxDOMsNextMonth() const
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
const int Date::MaxDOMsPrevMonth() const
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
const bool Date::AddSec(const int seconds)
{
   // Aquire Time Now
   struct tm tmTime;
   time_t tTmp=mktime(&m_tmObj)+seconds;
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
const bool Date::AddMin(const int minutes)
{
   // Aquire Time Now
   struct tm tmTime;
   time_t tTmp=mktime(&m_tmObj)+(time_t)minutes*60;
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
const bool Date::AddHour(const int hours)
{
   // Aquire Time Now
   struct tm tmTime;
   time_t tTmp=mktime(&m_tmObj)+(time_t)hours*3600;
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
const bool Date::AddDOM(const int days)
{
   // Aquire Time Now
   struct tm tmTime;
   time_t tTmp=mktime(&m_tmObj)+(time_t)days*86400;
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
const bool Date::AddMonth(const int months)
{
   struct tm tmTime;

   memcpy(&tmTime,&m_tmObj,sizeof(struct tm));
   // Temporarily Store the Current Day
   int tmpDays = tmTime.tm_mday;

   tmTime.tm_mday = T_DOM_MIN; // Store Min Day (temporary)

   int mCnt = months;
   if(months > 0)
   {
      // Add Months
      while(mCnt > 0)
      {
         if(tmTime.tm_mon == 11)
         {
            tmTime.tm_mon = 0;
            tmTime.tm_year++;
         }
         else
         {
            tmTime.tm_mon++;
         }
         mCnt--;
      }
   }
   else
   {
      // Subtract Months
      while(mCnt < 0)
      {
         if(tmTime.tm_mon ==0)
         {
            tmTime.tm_mon=11;
            tmTime.tm_year--;
         }
         else
         {
            tmTime.tm_mon--;
         }
         mCnt++;
      }
   }

   m_time=mktime(&tmTime);
   memcpy(&m_tmObj,&tmTime,sizeof(struct tm));
   return SetDOM(tmpDays);
}

//--------------------------------------------------
//
//  Function: AddYear
//
//------------------------------------------------
const bool Date::AddYear(const int years)
{
   struct tm tmTime;
   memcpy(&tmTime,&m_tmObj,sizeof(struct tm));
   tmTime.tm_year+=years;
   m_time=mktime(&tmTime);
   memcpy(&m_tmObj,&tmTime,sizeof(struct tm));
   return true;
}

//------------------------------------------------
//
//  Function: SetSec
//
//------------------------------------------------
const bool Date::SetSec(const int seconds)
{
   bool ok = false;
   if(seconds >= T_SEC_MIN && seconds <= T_SEC_MAX)
   {
      m_tmObj.tm_sec = seconds;
      m_time=mktime(&m_tmObj);
      ok=true;
   }
   return ok;
}

//------------------------------------------------
//
//  Function: SetMin
//
//------------------------------------------------
const bool Date::SetMin(const int minutes)
{
   bool ok = false;
   if(minutes >= T_MIN_MIN && minutes <= T_MIN_MAX)
   {
      m_tmObj.tm_min = minutes;
      m_time=mktime(&m_tmObj);
      ok=true;
   }
   return ok;
}

//------------------------------------------------
//
//  Function: SetHour
//
//------------------------------------------------
const bool Date::SetHour(const int hours)
{
   bool ok = false;
   if(hours >= T_HOUR_MIN && hours <= T_HOUR_MAX)
   {
      m_tmObj.tm_hour = hours;
      m_time=mktime(&m_tmObj);
      ok=true;
   }
   return ok;
}

//------------------------------------------------
//
//  Function: SetDOM
//
//------------------------------------------------
const bool Date::SetDOM(const int days)
{
   bool ok = false;
   if(days >= T_DOM_MIN && days <= T_DOM_MAX)
   {
      // Apply Correction If Nessisary
      int MaxDays=MaxDOMsThisMonth();
      if(days > MaxDays)
      {
         m_tmObj.tm_mday = MaxDays;
      }
      else
      {
         m_tmObj.tm_mday = days;
      }
      m_time=mktime(&m_tmObj);
      ok=true;
   }
   return ok;
}

//------------------------------------------------
//
//  Function: SetMonth
//
//------------------------------------------------
const bool Date::SetMonth(const int months)
{
   bool ok = false;
   if(months >= T_MONTH_MIN && months <= T_MONTH_MAX)
   {
      m_tmObj.tm_mon = months - 1;
      m_time=mktime(&m_tmObj);
      ok=true;
   }
   return ok;
}

//------------------------------------------------
//
//  Function: SetYear
//
//------------------------------------------------
const bool Date::SetYear(const int years)
{
   bool ok = false;
   if(years >= T_YEAR_MIN && years <= T_YEAR_MAX)
   {
      m_tmObj.tm_year = years - 1900;
      m_time=mktime(&m_tmObj);
      ok=true;
   }
   return ok;
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
   const int values[COMBO_COUNT] = {
      lSecOffset, lMinOffset, lHourOffset, lDomOffset,
      lMonthOffset, lDowOffset, lDriftOffset };
   const int lows[COMBO_COUNT] = {
      T_SEC_MIN, T_MIN_MIN, T_HOUR_MIN, T_DOM_MIN,
      T_MONTH_MIN, T_DOW_MIN, T_DRIFT_MIN };
   const int highs[COMBO_COUNT] = {
      T_SEC_MAX, T_MIN_MAX, T_HOUR_MAX, T_DOM_MAX,
      T_MONTH_MAX, T_DOW_MAX, T_DRIFT_MAX };

   set<int> lists[COMBO_COUNT];
   for (int i = 0; i < COMBO_COUNT; i++)
   {
      bool set = (values[i] >= lows[i] && values[i] <= highs[i]);
      lists[i].insert(set ? values[i] : T_NO_ENTRY);
   }
   return CronFromLists(lists, false, true, true);
}

//------------------------------------------------
//
//  Function: CronFromLists
//
//------------------------------------------------
const Date Date::CronFromLists(const set<int> lists[COMBO_COUNT],
                               bool isISC, bool domStar, bool dowStar) const
{
   // Each field is a bit mask of the values it allows
   const int lows[COMBO_DRIFT] = {
      T_SEC_MIN, T_MIN_MIN, T_HOUR_MIN, T_DOM_MIN, T_MONTH_MIN, T_DOW_MIN };
   const int highs[COMBO_DRIFT] = {
      T_SEC_MAX, T_MIN_MAX, T_HOUR_MAX, T_DOM_MAX, T_MONTH_MAX, T_DOW_MAX };
   uint64_t masks[COMBO_DRIFT];
   bool given[COMBO_DRIFT];

   for (int i = 0; i < COMBO_DRIFT; i++)
   {
      // A list holding only T_NO_ENTRY means the field was not given
      given[i] = !(lists[i].size() == 1 && *lists[i].begin() == T_NO_ENTRY);
      masks[i] = 0;
      for (set<int>::const_iterator itr = lists[i].begin();
           itr != lists[i].end(); itr++)
         if (*itr >= lows[i] && *itr <= highs[i])
            masks[i] |= (uint64_t)1 << *itr;
      if (!given[i])
         masks[i] = AllOf(lows[i], highs[i]);
   }

   Schedule s;
   if (isISC)
   {
      // A crontab entry has no seconds; it runs at the start of the minute.
      // When both day fields are restricted, either one may match.
      masks[COMBO_SEC] = 1;
      s.domOrDow = !domStar && !dowStar;
   }
   else
   {
      // Fields smaller than the smallest one given reset to their lowest
      // value, so '-n 5' means 5 minutes past the hour at second 0.
      //  - Day of month and day of week count as the same size.
      //  - With nothing given at all we unblock at the start of each minute.
      bool dayGiven = given[COMBO_DOM] || given[COMBO_DOW];
      int smallest = given[COMBO_SEC]   ? 0 :
                     given[COMBO_MIN]   ? 1 :
                     given[COMBO_HOUR]  ? 2 :
                     dayGiven           ? 3 :
                     given[COMBO_MONTH] ? 4 : 1;
      if (smallest > 0)
         masks[COMBO_SEC] = (uint64_t)1 << T_SEC_MIN;
      if (smallest > 1 && !given[COMBO_MIN])
         masks[COMBO_MIN] = (uint64_t)1 << T_MIN_MIN;
      if (smallest > 2 && !given[COMBO_HOUR])
         masks[COMBO_HOUR] = (uint64_t)1 << T_HOUR_MIN;
      if (smallest > 3 && !dayGiven)
         masks[COMBO_DOM] = (uint64_t)1 << T_DOM_MIN;
      s.domOrDow = false;
   }
   s.sec = masks[COMBO_SEC];
   s.min = masks[COMBO_MIN];
   s.hour = masks[COMBO_HOUR];
   s.dom = masks[COMBO_DOM];
   s.month = masks[COMBO_MONTH];
   s.dow = masks[COMBO_DOW];

   // A drift is added after the match.  Matching from 'now - drift' keeps
   // only the part of the drift that fits inside the schedule's interval,
   // so no match is ever skipped.  With several drifts the earliest wins.
   bool found = false;
   time_t best = 0;
   const set<int>& drifts = lists[COMBO_DRIFT];
   for (set<int>::const_iterator itr = drifts.begin();
        itr != drifts.end(); itr++)
   {
      time_t drift = (*itr > 0) ? *itr : 0;
      time_t when = NextMatch(s, m_time - drift) + drift;
      if (!found || when < best)
      {
         best = when;
         found = true;
      }
   }
   return Date(best);
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
   static Date tmpDate;
   // Calculate Possible Combinations
   set<int> comboList[COMBO_COUNT]; // Build Combination List
   return(
         tmpDate.ParseCronString(
            sSecOffset,
            comboList[COMBO_SEC],T_SEC_MIN,T_SEC_MAX) &&
         tmpDate.ParseCronString(
            sMinOffset,
            comboList[COMBO_MIN],T_MIN_MIN,T_MIN_MAX) &&
         tmpDate.ParseCronString(
            sHourOffset,
            comboList[COMBO_HOUR],T_HOUR_MIN,T_HOUR_MAX) &&
         tmpDate.ParseCronString(
            sDomOffset,
            comboList[COMBO_DOM],T_DOM_MIN,T_DOM_MAX) &&
         tmpDate.ParseCronString(
            sMonthOffset,
            comboList[COMBO_MONTH],T_MONTH_MIN,T_MONTH_MAX) &&
         tmpDate.ParseCronString(
            sDowOffset,
            comboList[COMBO_DOW],T_DOW_MIN,T_DOW_MAX) &&
         tmpDate.ParseCronString(
            sDriftOffset,
            comboList[COMBO_DRIFT],T_DRIFT_MIN,T_DRIFT_MAX));
}

//------------------------------------------------
//
//  Function: CronValid
//            (using whitespace as a delimiter)
//
//------------------------------------------------
bool Date::CronValid(const string& strIn, bool isISC)
{
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

   vector<string> v_tokens;
   if (!SplitCronString(strIn, isISC, v_tokens))
   {
      // More entries then expected found
      return false;
   }

   // Convert to expected format
   return isISC?
         // Yes? Ok then skip the 'seconds' field (defaults to '*')
         CronValid("*", v_tokens[0], v_tokens[1], v_tokens[2],
                        v_tokens[3], v_tokens[4], "*"):
         // No? Then handle all of the arguments
         CronValid(v_tokens[0], v_tokens[1], v_tokens[2],
                   v_tokens[3], v_tokens[4], v_tokens[5],
                   v_tokens[6]);
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

   set<int> comboList[COMBO_COUNT];
   if (!(ParseCronString(sSecOffset,comboList[COMBO_SEC],T_SEC_MIN,T_SEC_MAX) &&
    ParseCronString(sMinOffset,comboList[COMBO_MIN],T_MIN_MIN,T_MIN_MAX) &&
    ParseCronString(sHourOffset,comboList[COMBO_HOUR],T_HOUR_MIN,T_HOUR_MAX) &&
    ParseCronString(sDomOffset,comboList[COMBO_DOM],T_DOM_MIN,T_DOM_MAX) &&
    ParseCronString(sMonthOffset,comboList[COMBO_MONTH],T_MONTH_MIN,T_MONTH_MAX) &&
    ParseCronString(sDowOffset,comboList[COMBO_DOW],T_DOW_MIN,T_DOW_MAX) &&
    ParseCronString(sDriftOffset,comboList[COMBO_DRIFT],T_DRIFT_MIN,T_DRIFT_MAX)))
   {
      // Invalid syntax; CronValid() is what reports it
      return *this;
   }
   return CronFromLists(comboList, false, true, true);
}

//------------------------------------------------
//
//  Function: Cron
//            (using whitespace as a delimiter)
//------------------------------------------------
const Date Date::Cron(const string& strIn, bool isISC) const
{
   //         Normal Format
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
   //         ISC Format (no second)
   //
   //     +----------------------------- min (0 - 59)
   //     |  +----------------------- hour (0 - 23)
   //     |  |  +----------------- day of month (1 - 31)
   //     |  |  |  +----------- month (1 - 12)
   //     |  |  |  |  +----- day of week (0 - 6) (Sunday=0)
   //     |  |  |  |  |
   //     -  -  -  -  -
   //     *  *  *  *  *

   // Any fields beyond what is allowed are simply ignored here;
   // CronValid() is what reports them as an error
   vector<string> v_tokens;
   SplitCronString(strIn, isISC, v_tokens);

   #ifdef DEBUG
   cerr << "DEBUG Date::Cron("
      << v_tokens[0] << ","
      << v_tokens[1] << ","
      << v_tokens[2] << ","
      << v_tokens[3] << ","
      << v_tokens[4] << ","
      << v_tokens[5] << ","
      << v_tokens[6] << "),"
      << " ISC=" << ((string) (isISC?"y":"n"))
      << endl;
   #endif

   if (!isISC)
   {
      return Cron(v_tokens[0], v_tokens[1], v_tokens[2], v_tokens[3],
                  v_tokens[4], v_tokens[5], v_tokens[6]);
   }

   // ISC: no seconds or drift field
   set<int> comboList[COMBO_COUNT];
   comboList[COMBO_SEC].insert(T_NO_ENTRY);
   comboList[COMBO_DRIFT].insert(T_NO_ENTRY);
   if (!(ParseCronString(v_tokens[0],comboList[COMBO_MIN],
                    T_MIN_MIN,T_MIN_MAX,true) &&
    ParseCronString(v_tokens[1],comboList[COMBO_HOUR],
                    T_HOUR_MIN,T_HOUR_MAX,true) &&
    ParseCronString(v_tokens[2],comboList[COMBO_DOM],
                    T_DOM_MIN,T_DOM_MAX,true) &&
    ParseCronString(v_tokens[3],comboList[COMBO_MONTH],
                    T_MONTH_MIN,T_MONTH_MAX,true) &&
    ParseCronString(v_tokens[4],comboList[COMBO_DOW],
                    T_DOW_MIN,T_DOW_MAX,true)))
   {
      // Invalid syntax; CronValid() is what reports it
      return *this;
   }
   // crontab(5) treats a day field starting with '*' as unrestricted
   return CronFromLists(comboList, true,
                        v_tokens[2][0] == '*', v_tokens[4][0] == '*');
}

bool Date::ParseCronString(const string& strIn, set<int> &comboListOut,
                             int minVal, int maxVal,
                             bool stepFromLowest) const
{
   // Each comma separated entry is one of:
   //   *            any value
   //   x            a value
   //   x-y (y-x)    every value from x to y
   //   x-y/z        every z'th value from x to y
   //   */x (/x)     every value divisible by x, or with stepFromLowest
   //                every x'th value from the lowest one (crontab)
   // Anything else (ie: '5-', 'x-y-z', '/x/y') is invalid.
   vector<string> tokens;
   Tokenize(strIn, ',', tokens);

   for (vector<string>::const_iterator itr = tokens.begin();
        itr != tokens.end(); itr++)
   {
      const string& token = *itr;
      if (token == "*")
         continue;

      size_t pos = 0;
      long long first, last, step = 1;
      if (token[0] == '/' || token.compare(0, 2, "*/") == 0)
      {
         // Modulus
         pos = (token[0] == '/') ? 1 : 2;
         if (!ReadNumber(token, pos, minVal, maxVal, step) || step == 0)
            return false;
         first = stepFromLowest ? minVal : 0;
         last = maxVal;
      }
      else
      {
         if (!ReadNumber(token, pos, minVal, maxVal, first))
            return false;
         last = first;
         if (pos < token.size() && token[pos] == '-')
         {
            // Range
            pos++;
            if (!ReadNumber(token, pos, minVal, maxVal, last))
               return false;
            if (first > last)
            {
               long long tmp = first;
               first = last;
               last = tmp;
            }

            // Step through the range
            if (pos < token.size() && token[pos] == '/')
            {
               pos++;
               if (!ReadNumber(token, pos, 1, maxVal, step))
                  return false;
            }
         }
      }
      if (pos != token.size())
         return false;

      // Refuse lists that would take up an absurd amount of memory
      long long count = (last - first) / step + 1;
      if (count + (long long)comboListOut.size() > CRON_MAX_FIELD_VALUES)
         return false;

      for (long long value = first; value <= last; value += step)
         if (value >= minVal)
            comboListOut.insert((int)value);
   }

   // A list holding only T_NO_ENTRY means the field was not given
   if (comboListOut.empty())
      comboListOut.insert(T_NO_ENTRY);
   return true;
}

size_t Date::Tokenize(const string& strIn,
                char delimiter,
                vector<string> &tokens) const
{
   size_t startPos = 0;
   tokens.clear();
   while (startPos <= strIn.size())
   {
      size_t nextPos = strIn.find(delimiter, startPos);
      if (nextPos == string::npos)  // Last token
         nextPos = strIn.size();

      if (nextPos > startPos)
      {
         string token(strIn, startPos, nextPos - startPos);
         tokens.insert(tokens.end(), token);
      }

      // + 1 because it is the size of the seperator (,)
      startPos = nextPos + 1;
   }
   return tokens.size();
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
#if !defined(_WIN32) && !defined(__APPLE__)
   struct timespec target;
   target.tv_sec = until.Time();
   target.tv_nsec = 0;

   int rc = clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &target, NULL);
   if (rc == EINTR)
   {
      // A signal woke us up early; report how long is left
      time_t now = Now();
      return (until.Time() > now) ? (unsigned long)(until.Time() - now) : 0;
   }
   return 0;
#else
   // macOS and Windows can't sleep until an exact time, so sleep for
   // whatever is left instead.  Sleep at most a second at a time so that
   // Ctrl-C is noticed quickly; the caller loops until we return 0.
   struct timespec now;
   NowPrecise(now);
   if (now.tv_sec >= until.Time())
   {
      return 0;
   }

   double left = (double)(until.Time() - now.tv_sec) - now.tv_nsec / 1e9;
   double chunk = (left > 1.0) ? 1.0 : left;
#ifdef _WIN32
   Sleep((DWORD)(chunk * 1000.0) + 1);
#else
   struct timespec rel;
   rel.tv_sec = (time_t)chunk;
   rel.tv_nsec = (long)((chunk - (double)rel.tv_sec) * 1e9);
   nanosleep(&rel, NULL);
#endif

   time_t after = Now();
   return (until.Time() > after) ? (unsigned long)(until.Time() - after) : 0;
#endif
}
