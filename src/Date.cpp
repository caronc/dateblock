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

// How many passes Cron() may take before giving up on finding a match
#ifndef CRON_ASSERT_LOOP_LIMIT
#define CRON_ASSERT_LOOP_LIMIT 400
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

   // Start on the 1st so that changing the month can't roll over into
   // the next one (ie: today is the 31st and the new month has 30 days)
   m_tmObj.tm_mday = T_DOM_MIN;

   // Now Set Defaults
   SetYear(year);
   SetMonth(mon);
   SetDOM(day);
   SetHour(hour);
   SetMin(min);
   SetSec(sec);
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
   Date dObjFinish(*this);
   Date dObjRef(*this);

   #ifdef CRON_ASSERT_LOOP_LIMIT
      // Only allow looping for a specific time before failing
      unsigned cron_limit = CRON_ASSERT_LOOP_LIMIT;
   #endif

   #ifdef DEBUG
   cerr << "DEBUG Date::Cron("
       << lSecOffset
       << "," << lMinOffset
       << "," << lHourOffset
       << "," << lDomOffset
       << "," << lMonthOffset
       << "," << lDowOffset
       << "," << lDriftOffset << ");" << endl;
   #endif

   bool bSetSec=(lSecOffset >= Date::T_SEC_MIN &&
                 lSecOffset <= Date::T_SEC_MAX);
   bool bSetMin=(lMinOffset >= Date::T_MIN_MIN &&
                 lMinOffset <= Date::T_MIN_MAX);
   bool bSetHour=(lHourOffset >= Date::T_HOUR_MIN &&
                  lHourOffset <= Date::T_HOUR_MAX);
   bool bSetDom=(lDomOffset >= Date::T_DOM_MIN &&
                 lDomOffset <= Date::T_DOM_MAX);
   bool bSetMonth=(lMonthOffset >= Date::T_MONTH_MIN &&
                   lMonthOffset <= Date::T_MONTH_MAX);
   bool bSetDow=(lDowOffset >= Date::T_DOW_MIN &&
                 lDowOffset <= Date::T_DOW_MAX);
   bool bSetDrift=(lDriftOffset >= Date::T_DRIFT_MIN &&
                 lDriftOffset <= Date::T_DRIFT_MAX);

   if (!bSetSec)
        lSecOffset=Date::T_SEC_MIN;     // Force Default
   if (!bSetMin)
        lMinOffset=Date::T_MIN_MIN;     // Force Default
   if (!bSetHour)
        lHourOffset=Date::T_HOUR_MIN;   // Force Default
   if (!bSetDom)
        lDomOffset=Date::T_DOM_MIN;     // Force Default
   if (!bSetMonth)
        lMonthOffset=Date::T_MONTH_MIN; // Force Default
   if (!bSetDow)
        lDowOffset=Date::T_DOW_MIN;     // Force Default
   if (!bSetDrift)
        lDriftOffset=0;                 // Force Default

   // No need to set drift default because if one wasn't detected then
   // we use whatever was passed in as an option instead

   if (!( bSetMin || bSetHour || bSetDom || bSetMonth || bSetDow ))
   {
      // This function requires that at least something has been set
      // so at worst case scenario (and to avoid an infinite loop
      // we assume that seconds have been set
      bSetSec=true;
   }

   // A Drift introduces a small problem, it adds time to the final
   // result. We need to factor this into our calculation a head of time.
   // If a drift has been specified, we have to assume the current time
   // is actually in the past by the same amount.
   if (lDriftOffset > 0)
   {
      dObjRef.AddSec(-lDriftOffset);
      dObjFinish.AddSec(-lDriftOffset);
   }
   #ifdef DEBUG
   cerr << "DEBUG Date::Cron"
        << " SetSec: " << bSetSec
        << ", SetMin: " << bSetMin
        << ", SetHour: " << bSetHour
        << ", SetDom: " << bSetDom
        << ", SetMonth: " << bSetMonth
        << ", SetDow: " << bSetDow
        << ", Drift: " << lDriftOffset
        << endl;
   #endif
   // Forever Loop (unless cron is matched)
   while(1)
   {
   #ifdef CRON_ASSERT_LOOP_LIMIT
      if(cron_limit == 0){
         // No moment in time matches this cron (ie: February 31st)
         ostringstream msg;
         msg << "CRON_ASSERT_LOOP_LIMIT ("
             << CRON_ASSERT_LOOP_LIMIT
             << ") reached."
             << " CRON='"
             << lSecOffset
             << " " << lMinOffset
             << " " << lHourOffset
             << " " << lDomOffset
             << " " << lMonthOffset
             << " " << lDowOffset
             << " " << lDriftOffset
             << "'"
             << ", REF=" << dObjRef.Str();
         throw runtime_error(msg.str());
      }
      // Decrement Limit Counter
      cron_limit--;
   #endif
      #ifdef DEBUG
      cerr << "DEBUG Date::Cron LOOP"
              #ifdef CRON_ASSERT_LOOP_LIMIT
              << "[" << (CRON_ASSERT_LOOP_LIMIT-cron_limit) << "]"
              #endif
              << " in=" << this->Str()
              << ", ref=" << dObjRef.Str()
              << ", calc=" << dObjFinish.Str()
              << endl;
      #endif
      if (bSetSec)
      {
         if(dObjFinish.Sec() > lSecOffset)
         {
            dObjFinish.AddMin(1); // Add 1 minute
            dObjFinish.SetSec(lSecOffset);
            // Continue to parse until the second matches
            #ifdef DEBUG
            cerr << "DEBUG Date::Cron stage=1, min++, sec=" << lSecOffset << endl;
            #endif
            continue;
         }
         else if(dObjFinish.Sec() < lSecOffset)
         {
            dObjFinish.AddSec((lSecOffset-dObjFinish.Sec()));
            #ifdef DEBUG
            cerr << "DEBUG Date::Cron stage=1, min++" << endl;
            #endif
            continue;
         }
         // else we match correctly!
      }

      if (bSetMin)
      {
         if(dObjFinish.Min() > lMinOffset)
         {
            dObjFinish.AddHour(1); // + 1 hour
            dObjFinish.SetMin(lMinOffset);
            // Continue to parse until the minute matches
            #ifdef DEBUG
            cerr << "DEBUG Date::Cron stage=1, hour++, min=" << lMinOffset << endl;
            #endif
            continue;
         }
         else if(dObjFinish.Min() < lMinOffset)
         {
            dObjFinish.AddMin((lMinOffset-dObjFinish.Min()));
            #ifdef DEBUG
            cerr << "DEBUG Date::Cron stage=1, hour++" << endl;
            #endif
            continue;
         }
         // else we match correctly!
      }

      if (bSetHour)
      {
         if(dObjFinish.Hour() > lHourOffset)
         {
            dObjFinish.AddDOM(1); // + 1 day
            dObjFinish.SetHour(lHourOffset);
            // Continue to parse until the hour matches
            #ifdef DEBUG
            cerr << "DEBUG Date::Cron stage=1, dom++, hour=" << lHourOffset << endl;
            #endif
            continue;
         }
         else if(dObjFinish.Hour() < lHourOffset)
         {
            dObjFinish.AddHour(lHourOffset-dObjFinish.Hour());
            #ifdef DEBUG
            cerr << "DEBUG Date::Cron stage=1, dom++" << endl;
            #endif
            continue;
         }
         // else we match correctly!
      }

      if (bSetDow)
      {
         if(dObjFinish.DOW() != lDowOffset)
         {
            dObjFinish.AddDOM(1); // Add 1 day
            // Continue to parse until the DOW matches
            #ifdef DEBUG
            cerr << "DEBUG Date::Cron stage=1, dom++" << endl;
            #endif
            continue;
         }
      }

      if (bSetDom)
      {
         if(dObjFinish.MaxDOMsThisMonth() < lDomOffset)
         {
            // Don't handle values like 31 in February, instead, just add a month
            // and move along
            dObjFinish.AddMonth(1);
            #ifdef DEBUG
            cerr << "DEBUG Date::Cron stage=1, month++" << endl;
            #endif
            continue;
         }
         else if(dObjFinish.DOM() > lDomOffset)
         {
            dObjFinish.AddMonth(1); // + 1 month
            dObjFinish.SetDOM(lDomOffset);
            // Continue to parse until the DOM matches
            #ifdef DEBUG
            cerr << "DEBUG Date::Cron stage=1, month++, dom=" << lDomOffset << endl;
            #endif
            continue;
         }
         else if(dObjFinish.DOM() < lDomOffset)
         {
            dObjFinish.SetDOM(lDomOffset);
            #ifdef DEBUG
            cerr << "DEBUG Date::Cron stage=1, dom=" << lDomOffset << endl;
            #endif
            continue;
         }
         // else we match correctly!
      }

      if (bSetMonth)
      {
         if(dObjFinish.Month() > lMonthOffset)
         {
            dObjFinish.AddYear(1); // + 1 year
            dObjFinish.SetMonth(lMonthOffset);
            // Continue to parse until the Month matches
            #ifdef DEBUG
            cerr << "DEBUG Date::Cron stage=1, year++, month=" << lMonthOffset << endl;
            #endif
            continue;
         }
         else if(dObjFinish.Month() < lMonthOffset)
         {
            dObjFinish.SetMonth(lMonthOffset);
            #ifdef DEBUG
            cerr << "DEBUG Date::Cron stage=1, month=" << lMonthOffset << endl;
            #endif
            continue;
         }
         // else we match correctly!
      }

      // Reset Items backwards based on what is set
      if(bSetMonth)
      {
         if (!bSetHour) dObjFinish.SetHour(Date::T_HOUR_MIN);  // reset hours
         if (!bSetMin) dObjFinish.SetMin(Date::T_MIN_MIN);     // reset minutes
         if (!bSetSec) dObjFinish.SetSec(Date::T_SEC_MIN);     // reset seconds

         if( dObjFinish < dObjRef )
         {
            /* This happens when we're caculating on the same
             * month we are currently in */
            dObjFinish.AddMonth(1);
            #ifdef DEBUG
            cerr << "DEBUG Date::Cron stage=2, month++" << endl;
            #endif
            continue;
         }

      }
      else if ( bSetDom || bSetDow )
      {
         if (!bSetHour) dObjFinish.SetHour(Date::T_HOUR_MIN);  // reset hours
         if (!bSetMin) dObjFinish.SetMin(Date::T_MIN_MIN);     // reset minutes
         if (!bSetSec) dObjFinish.SetSec(Date::T_SEC_MIN);     // reset seconds

         if( dObjFinish < dObjRef )
         {
            /* This happens when we're caculating on the same
             * day we are currently on */
            dObjFinish.AddDOM(1);
            #ifdef DEBUG
            cerr << "DEBUG Date::Cron stage=2, dom++" << endl;
            #endif
            continue;
         }
      }
      else if (bSetHour)
      {
         if (!bSetMin) dObjFinish.SetMin(Date::T_MIN_MIN);     // reset minutes
         if (!bSetSec) dObjFinish.SetSec(Date::T_SEC_MIN);     // reset seconds
         if( dObjFinish < dObjRef )
         {
            /* This happens when we're caculating on the same
             * hour we are currently on */
            dObjFinish.AddHour(1);
            #ifdef DEBUG
            cerr << "DEBUG Date::Cron stage=2, hour++" << endl;
            #endif
            continue;
         }
      }
      else if (bSetMin)
      {
         if (!bSetSec) dObjFinish.SetSec(Date::T_SEC_MIN);     // reset seconds
         if( dObjFinish < dObjRef )
         {
            /* This happens when we're caculating on the same
             * minute we are currently on */
            dObjFinish.AddMin(1);
            #ifdef DEBUG
            cerr << "DEBUG Date::Cron stage=2, min++" << endl;
            #endif
            continue;
         }
      }
      else if (bSetSec)
      {
         if( dObjFinish < dObjRef )
         {
            /* This happens when we're caculating on the same
             * minute we are currently on */
            dObjFinish.AddSec(1);
            #ifdef DEBUG
            cerr << "DEBUG Date::Cron stage=2, sec++" << endl;
            #endif
            continue;
         }
      }

      // Accomodate Drift time if nessisary
      if (lDriftOffset > 0)
         dObjFinish.AddSec(lDriftOffset);

      // We don't use our reference object for the final check; instead we use
      // *this object! This is VERY IMPORTANT!
      //
      // We use the real time. This is very important because dObjRef will
      // correctly equate in this equation when there is a drift time involved
      if( dObjFinish == *this )
      {
         #ifdef DEBUG
         cerr << "DEBUG Date::Cron Countermeasures"
              << " ref==finish (" << this->m_time
              << "==" << dObjFinish.m_time << ")"
              << endl;
         #endif
         // times can not be identical .. move along
         //
         //  This Adjustment 'MUST' be done otherwise it is
         //  possible for this loop to return true over and
         //  over again if called consecutively within the same
         //  second.
         //
         //  ie. while [ 1 -eq 1 ]; do ./dateblock -s /5; done
         //
         //  should only be called once every 5 seconds reguardless
         //  if execution time is less then a second
         //
         //  Based on the arguments pased, we only need to adjust the
         //  smallest of them. Hence if Seconds and Years were specified,
         //  we only need to adjust seconds.
         //
         //  This forces the time to be invalid, so the calculation process
         //  begins over again.
         if(bSetSec)dObjFinish.AddSec(1);
         else if(bSetMin)dObjFinish.AddMin(1);
         else if(bSetHour)dObjFinish.AddHour(1);
         else if(bSetDom)dObjFinish.AddDOM(1);
         else if(bSetMonth)dObjFinish.AddMonth(1);
         else if(bSetDow)dObjFinish.AddDOM(1);

         // Remove Drift Accomodation (if set)
         if (lDriftOffset > 0)
            dObjFinish.AddSec(-lDriftOffset);

         #ifdef DEBUG
         cerr << "DEBUG Date::Cron Countermeasures"
              << " new_ref=" << dObjFinish.m_time
              << ", difference=+" << (dObjFinish.m_time - this->m_time)
              << " sec(s)"
              << endl;
         #endif
         continue;
      }

      // If the code reaches here, then everything matched
      // correctly.  There is no need to stay in the loop
      break;

   } // end infinite while loop

   // Store new Time
   return dObjFinish;
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

   set<int> comboList[COMBO_COUNT]; // Build Combination List
   Date dObjFinish(*this);

   // Calculate Possible Seconds Combiniation
   if(ParseCronString(sSecOffset,comboList[COMBO_SEC],T_SEC_MIN,T_SEC_MAX) &&
    ParseCronString(sMinOffset,comboList[COMBO_MIN],T_MIN_MIN,T_MIN_MAX) &&
    ParseCronString(sHourOffset,comboList[COMBO_HOUR],T_HOUR_MIN,T_HOUR_MAX) &&
    ParseCronString(sDomOffset,comboList[COMBO_DOM],T_DOM_MIN,T_DOM_MAX) &&
    ParseCronString(sMonthOffset,comboList[COMBO_MONTH],T_MONTH_MIN,T_MONTH_MAX) &&
    ParseCronString(sDowOffset,comboList[COMBO_DOW],T_DOW_MIN,T_DOW_MAX) &&
    ParseCronString(sDriftOffset,comboList[COMBO_DRIFT],T_DRIFT_MIN,T_DRIFT_MAX))
   {
      // Prepare Iterators
      set<int>::const_iterator itrSec,
                               itrMin,
                               itrHour,
                               itrDom,
                               itrMonth,
                               itrDow,
                               itrDrift;

      // Date Object to Hold on to the lowest reference point
      Date dObjRef(dObjFinish);
      bool bFound = false;
      string sError;

      // Big On7 unfortunately.  Perhaps this section could be
      // more smarter and only handle obviously newer times and skip
      // the obvious short ones.
      itrSec  = comboList[COMBO_SEC].begin();
      for(;itrSec != comboList[COMBO_SEC].end();itrSec++)
      {
         itrMin  = comboList[COMBO_MIN].begin();
         for(;itrMin != comboList[COMBO_MIN].end();itrMin++)
         {
            itrHour = comboList[COMBO_HOUR].begin();
            for(;itrHour != comboList[COMBO_HOUR].end();itrHour++)
            {
               itrDom  = comboList[COMBO_DOM].begin();
               for(;itrDom != comboList[COMBO_DOM].end();itrDom++)
               {
                  itrMonth= comboList[COMBO_MONTH].begin();
                  for(;itrMonth != comboList[COMBO_MONTH].end();itrMonth++)
                  {
                     itrDow  = comboList[COMBO_DOW].begin();
                     for(;itrDow != comboList[COMBO_DOW].end();itrDow++)
                     {
                        itrDrift  = comboList[COMBO_DRIFT].begin();
                        for(;itrDrift != comboList[COMBO_DRIFT].end();itrDrift++)
                        {
                           // Build Reference Point; a combination that
                           // can never happen (ie: Feb 30th) is skipped
                           // as long as another one works out
                           try
                           {
                              dObjRef=Cron(*itrSec,
                                           *itrMin,
                                           *itrHour,
                                           *itrDom,
                                           *itrMonth,
                                           *itrDow,
                                           *itrDrift);
                           }
                           catch (const runtime_error& e)
                           {
                              sError = e.what();
                              continue;
                           }

                           // Store Value if nessisary (the first match
                           // is always stored)
                           if(!bFound || dObjRef < dObjFinish)
                           {
                              dObjFinish = dObjRef;
                              bFound = true;
                           }

                        } // Drift
                     } // Day of Week
                  } // Month
               } // Day of Month
            } // Hours
         } // Minutes
      } // Seconds

      if (!bFound && !sError.empty())
      {
         // Not one of the combinations could be matched
         throw runtime_error(sError);
      }
   } // Valid Parsing

   return dObjFinish;
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

   // Convert cron to the next relative date
   return isISC?
         // Yes? Ok then skip the 'seconds' field (defaults to '*')
         Cron("*", v_tokens[0], v_tokens[1], v_tokens[2], v_tokens[3],
               v_tokens[4], "*"):
         // No? Then handle all of the arguments
         Cron(v_tokens[0], v_tokens[1], v_tokens[2], v_tokens[3], v_tokens[4],
              v_tokens[5], v_tokens[6]);
}

bool Date::ParseCronString(const string& strIn, set<int> &comboListOut,
                             int minVal, int maxVal) const
{
   // Valid characters for strIn are:
   //   0-9,/*
   // - Return immediately if there are invalid characters
   // - Delimit Comma's into array
   // - store unique values into comboList

   vector<string> tokens;
   if(Tokenize(strIn,',',tokens) > 0)
   {
      // Iterate Through Vector Array and Parse Entries
      vector<string>::const_iterator itr;
      size_t startPos;
      size_t nextPos;
      int rangeStart=-1;
      bool modFlag=false;

      for(itr=tokens.begin(); itr!=tokens.end(); itr++)
      {
         // cout << "DEBUG: Parsing '" << *itr <<  "' Size: "
         //    << itr->size() << endl;
         startPos=0; // Reset Pointer
         nextPos=0; // Reset Pointer
         while(startPos < itr->size())
         {
            for ( ;  nextPos < itr->size() ; nextPos++ )
               if(!isdigit((*itr)[nextPos]))
                  break;

          //   cout << "DEBUG: NonNumerical: " << (*itr)[nextPos] << endl;
            if(nextPos < itr->size()) // A non-Numerical Value was found
            {
               // Handle It's Value
               // If '-' then begin storing range
               // If '*' then do nothing
               if((*itr)[nextPos] == '*')
               {
                  // Update positions to look past dash
                  startPos = ++nextPos;
                  continue;
               }
               else if((*itr)[nextPos] == '-' && rangeStart < 0)
               {
                  if(modFlag == true)
                  {
                     // failure
                     return false;
                  }

                  // Store Range
                  istringstream myStream(
                        itr->substr(startPos,nextPos-startPos));
                  if(!(myStream >> rangeStart))
                  {
                     // failure
                     return false;
                  }

                  // Handle Invalid range
                  if(rangeStart < minVal || rangeStart > maxVal)
                  {
                     // failure
                     return false;
                  }

                  // Update positions to look past dash
                  startPos = ++nextPos;
                  continue;

               }
               else if((*itr)[nextPos] == '/' && modFlag == false)
               {
                  // Mod Val
                  modFlag=true;

                  // Update positions to look past dash
                  startPos = ++nextPos;
                  continue;
               }
               else
               {
                  // failure
                  return false;
               }
            }

            if(nextPos > startPos)
            {
               // Store Number
               int val;
               istringstream myStream(itr->substr(startPos,nextPos-startPos));
               if(!(myStream >> val))
               {
                  // failure
                  return false;
               }

               // Handle Invalid range
               if(val < minVal || val > maxVal)
               {
                  // failure
                  return false;
               }

               if(modFlag == true)
               {
                  // Calculate Mod
                  int newVal=0;
                  if (val == 0)
                  {
                     // failure
                     return false;
                  }
                  for(; (newVal <= maxVal); newVal+=val)
                  {
                     if(newVal < minVal)continue;
                     comboListOut.insert(newVal);
                  }
                  // reset mod flag
                  modFlag=false;
               }
               else if(rangeStart < 0)
               {
                  // Store Value Otherwise
                  if(val >= minVal && val <= maxVal)
                     comboListOut.insert(val);
               }
               else
               {
                  if(rangeStart > val)
                  {
                     // Swap
                     int tmp;
                     tmp=rangeStart;
                     rangeStart=val;
                     val=tmp;
                  }

                  // Store Range
                  for(; (rangeStart <= val); rangeStart++)
                     if(rangeStart >= minVal && rangeStart <= maxVal)
                        comboListOut.insert(rangeStart);

                  // Reset Range
                  rangeStart=-1;
               }
            }

            // Increment Position
            startPos = nextPos + 1;
         }
      }
   }

   // At the very least; Token list must contain a '-1'
   // This is used for Basic Cron processing.  '-1' signifies
   // that no combo value was handled
   if(comboListOut.size() <=0)comboListOut.insert(-1);

   // Debug List
   // set<int>::const_iterator itr;
   // cout << "Size: " << comboListOut.size() << endl;
   // for(itr=comboListOut.begin(); itr != comboListOut.end(); itr++)
   //    cout << "Val: " << *itr << endl;

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
