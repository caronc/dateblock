/*
 Dateblock blocks (sleeps) until a cron-style moment in time.
 Copyright (C) 2013-2026 Chris Caron <lead2gold@gmail.com>

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
#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <datetime.h>

#include <math.h>
#include <string.h>
#include <limits>
#include <new>
#include <stdexcept>
#include "Date.h"

// Turns seconds since the epoch (from Python) into a time_t.  NaN,
// infinity and values a time_t can't hold set an OverflowError and return
// false.  The largest time_t rounds up to 2**63 as a double, which itself
// doesn't fit, hence '>='.
static bool EpochToTime(double epoch, time_t &out)
{
   epoch = floor(epoch);
   if (!isfinite(epoch) ||
       epoch < (double) std::numeric_limits<time_t>::min() ||
       epoch >= (double) std::numeric_limits<time_t>::max())
   {
      PyErr_SetString(PyExc_OverflowError,
            "ref is outside of the supported time range.");
      return false;
   }
   out = (time_t) epoch;
   return true;
}

// Fills in 'out' from a naive Python reference value (timezone aware
// ones go through CronInZone() instead):
//  - None:               now
//  - datetime:           as is
//  - date:               midnight of that day
//  - time:               that time, today
//  - timedelta:          offset from now
//  - int or float:       seconds since the epoch
// Returns false (with a TypeError set) for anything else.
// Throws std::out_of_range for a moment that can't be a local date.
static bool PyToDate(PyObject *obj, Date &out)
{
   out.Set();
   if (obj == NULL || obj == Py_None)
   {
      return true;
   }

   if (PyDateTime_Check(obj))
   {
      out.Set(PyDateTime_GET_YEAR(obj),
              PyDateTime_GET_MONTH(obj),
              PyDateTime_GET_DAY(obj),
              PyDateTime_DATE_GET_HOUR(obj),
              PyDateTime_DATE_GET_MINUTE(obj),
              PyDateTime_DATE_GET_SECOND(obj));
      return true;
   }

   if (PyDate_Check(obj))
   {
      out.Set(PyDateTime_GET_YEAR(obj),
              PyDateTime_GET_MONTH(obj),
              PyDateTime_GET_DAY(obj),
              0, 0, 0);
      return true;
   }

   if (PyTime_Check(obj))
   {
      out.Set(out.Year(), out.Month(), out.DOM(),
              PyDateTime_TIME_GET_HOUR(obj),
              PyDateTime_TIME_GET_MINUTE(obj),
              PyDateTime_TIME_GET_SECOND(obj));
      return true;
   }

   if (PyDelta_Check(obj))
   {
      // timedelta is relative to now. Date precision is one second, so we
      // keep only whole seconds (floor), which matches datetime handling.
      long long deltaSeconds =
         (long long) PyDateTime_DELTA_GET_DAYS(obj) * 86400LL +
         (long long) PyDateTime_DELTA_GET_SECONDS(obj);
      long long target = (long long) out.Time() + deltaSeconds;
      if (target < (long long) std::numeric_limits<time_t>::min() ||
          target > (long long) std::numeric_limits<time_t>::max())
      {
         PyErr_SetString(PyExc_OverflowError,
               "timedelta reference overflows the supported time range.");
         return false;
      }
      out.Set((time_t) target);
      return true;
   }

   if (PyLong_Check(obj) && !PyBool_Check(obj))
   {
      long long epoch = PyLong_AsLongLong(obj);
      if (epoch == -1 && PyErr_Occurred())
      {
         return false;
      }
      out.Set((time_t) epoch);
      return true;
   }

   if (PyFloat_Check(obj))
   {
      time_t epoch;
      if (!EpochToTime(PyFloat_AS_DOUBLE(obj), epoch))
      {
         return false;
      }
      out.Set(epoch);
      return true;
   }

   PyErr_SetString(PyExc_TypeError,
         "ref must be a datetime, date, time, timedelta, epoch seconds or None.");
   return false;
}

// Returns a new (naive, local time) datetime object for 'date'.  When the
// clocks go back, the second of the two identical clock times gets
// fold=1 (as datetime.fromtimestamp() gives it), so its timestamp() is
// still right.  Python is asked which one it is; if it can't say (ie: a
// year Windows' clock functions don't cover), fold stays 0.  Any other
// error (ie: out of memory) is raised.
static PyObject *DateToPy(const Date &date)
{
   int fold = 0;
   PyObject *args = Py_BuildValue("(L)", (long long)date.Time());
   PyObject *local = args ? PyDateTime_FromTimestamp(args) : NULL;
   Py_XDECREF(args);
   if (local == NULL)
   {
      if (!PyErr_ExceptionMatches(PyExc_OverflowError) &&
          !PyErr_ExceptionMatches(PyExc_ValueError) &&
          !PyErr_ExceptionMatches(PyExc_OSError))
      {
         return NULL;
      }
      PyErr_Clear();
   }
   else
   {
      if (PyDateTime_Check(local) &&
          PyDateTime_GET_YEAR(local) == date.Year() &&
          PyDateTime_GET_MONTH(local) == date.Month() &&
          PyDateTime_GET_DAY(local) == date.DOM() &&
          PyDateTime_DATE_GET_HOUR(local) == date.Hour() &&
          PyDateTime_DATE_GET_MINUTE(local) == date.Min() &&
          PyDateTime_DATE_GET_SECOND(local) == date.Sec())
      {
         fold = PyDateTime_DATE_GET_FOLD(local);
      }
      Py_DECREF(local);
   }
   return PyDateTime_FromDateAndTimeAndFold(date.Year(),
                                            date.Month(),
                                            date.DOM(),
                                            date.Hour(),
                                            date.Min(),
                                            date.Sec(),
                                            0, fold);
}

// 1970-01-01 00:00:00 UTC (set up when the module loads)
static PyObject *g_epoch = NULL;

// True if 't' (seconds since the epoch) is so close to the first or last
// moment a datetime can hold (years 1 and 9999), or past it, that its local
// time may not fit.  A timezone's offset is always under 24 hours, so only
// the last (or first) day can be affected.
static bool PastEdge(time_t t)
{
   const long long first = -62135596800LL;  // 0001-01-01 00:00:00 UTC
   const long long last = 253402300799LL;   // 9999-12-31 23:59:59 UTC
   const long long margin = 86400;
   return (long long) t < first + margin || (long long) t > last - margin;
}

// The moment 't' (seconds since the epoch) as an aware datetime in
// 'tzinfo', like datetime.fromtimestamp(t, tzinfo).  It is worked out as
// (1970-01-01 UTC + timedelta(seconds=t)).astimezone(tzinfo) instead,
// which needs nothing from the operating system: fromtimestamp() can't go
// before 1970 or past the year 3000 on Windows, this works for any year
// a datetime can hold.  Returns NULL with an error set on failure.
static PyObject *FromTimestamp(time_t t, PyObject *tzinfo)
{
   long long days = (long long) t / 86400;
   long long secs = (long long) t % 86400;
   if (secs < 0)
   {
      secs += 86400;
      days--;
   }
   if (days > 999999999 || days < -999999999)
   {
      PyErr_SetString(PyExc_OverflowError,
            "The time is outside of the supported range.");
      return NULL;
   }
   PyObject *delta = PyDelta_FromDSU((int) days, (int) secs, 0);
   if (delta == NULL)
   {
      return NULL;
   }
   PyObject *utc = PyNumber_Add(g_epoch, delta);
   Py_DECREF(delta);
   if (utc == NULL)
   {
      return NULL;
   }
   PyObject *local = PyObject_CallMethod(utc, "astimezone", "O", tzinfo);
   Py_DECREF(utc);

   // A tzinfo's own fromutc() decides what comes back, and Python doesn't
   // check it; anything but a datetime can't be read as one
   if (local != NULL && !PyDateTime_Check(local))
   {
      PyErr_Format(PyExc_TypeError,
            "the tzinfo returned %.100s instead of a datetime",
            Py_TYPE(local)->tp_name);
      Py_DECREF(local);
      return NULL;
   }
   return local;
}

// A Python tzinfo, seen through the engine's Date::TimeZone hook.
//  - The local time comes from FromTimestamp(t, tzinfo), so any
//    tzinfo works (zoneinfo, datetime.timezone, dateutil, pytz) and its
//    own daylight saving rules are used.
//  - Only use it while holding the GIL.
//  - A Python error stops the search and stays set so it can be raised.
class PyZone : public Date::TimeZone
{
   public:
     PyZone(PyObject *tzinfo) : m_tzinfo(tzinfo), m_pastEdge(false) {}

     // True once a moment failed because it is too close to the limits of
     // a datetime (see PastEdge); the search then gives up
     bool PastEdge() const { return m_pastEdge; }

     bool ToLocal(time_t t, struct tm& tmOut) const
     {
        if (PyErr_Occurred())
        {
           return false;
        }

        PyObject *dt = FromTimestamp(t, m_tzinfo);
        if (dt == NULL)
        {
           m_pastEdge = ::PastEdge(t);
           return false;
        }
        tmOut.tm_year = PyDateTime_GET_YEAR(dt) - 1900;
        tmOut.tm_mon = PyDateTime_GET_MONTH(dt) - 1;
        tmOut.tm_mday = PyDateTime_GET_DAY(dt);
        tmOut.tm_hour = PyDateTime_DATE_GET_HOUR(dt);
        tmOut.tm_min = PyDateTime_DATE_GET_MINUTE(dt);
        tmOut.tm_sec = PyDateTime_DATE_GET_SECOND(dt);
        Py_DECREF(dt);
        return true;
     }

   private:
     PyObject *m_tzinfo;
     mutable bool m_pastEdge;
};

// Turns the C++ exception being handled into a Python one.  Call it from
// inside a catch block.  A Python error that is already set (ie: raised
// by a tzinfo) is kept as it is.
static void SetCronError()
{
   try
   {
      throw;
   }
   catch (const std::invalid_argument &e)
   {
      if (!PyErr_Occurred())
         PyErr_Format(PyExc_SyntaxError,
               "The cron is not formatted correctly: %s", e.what());
   }
   catch (const std::out_of_range &)
   {
      if (!PyErr_Occurred())
         PyErr_SetString(PyExc_OverflowError,
               "ref is outside of the supported time range.");
   }
   catch (const std::runtime_error &e)
   {
      if (!PyErr_Occurred())
         PyErr_Format(PyExc_ValueError,
               "No date/time matches the cron specified (%s).", e.what());
   }
   catch (const std::bad_alloc &)
   {
      PyErr_NoMemory();
   }
   catch (const std::exception &e)
   {
      if (!PyErr_Occurred())
         PyErr_SetString(PyExc_RuntimeError, e.what());
   }
   catch (...)
   {
      if (!PyErr_Occurred())
         PyErr_SetString(PyExc_RuntimeError,
               "dateblock hit an unexpected error.");
   }
}

// The next match for a naive reference (or none), read in the computer's
// own timezone.  Returns a naive datetime and sets 'when', or NULL with
// an error set.
static PyObject *CronLocal(const char *cron, size_t length, bool isc,
                           PyObject *ref, time_t &when)
{
   // A C++ exception must never reach Python; it would abort the
   // interpreter.  Even creating a Date can throw (if the current time
   // can't be shown as a local date), so everything happens in here.
   try
   {
      Date dObjStart;
      if (!PyToDate(ref, dObjStart))
      {
         return NULL;
      }
      Date dObjFinish(dObjStart.Cron(std::string(cron, length), isc));
      when = dObjFinish.Time();
      return DateToPy(dObjFinish);
   }
   catch (...)
   {
      SetCronError();
      return NULL;
   }
}

// The moment a timezone aware datetime stands for, in seconds since the
// epoch.  Returns false with an error set on failure.
static bool AwareEpoch(PyObject *ref, time_t &epoch)
{
   PyObject *ts = PyObject_CallMethod(ref, "timestamp", NULL);
   if (ts == NULL)
   {
      return false;
   }
   // timestamp() can be anything a datetime subclass makes it return
   double seconds = PyFloat_AsDouble(ts);
   Py_DECREF(ts);
   return !PyErr_Occurred() && EpochToTime(seconds, epoch);
}

// The next match after 'epoch', read in 'tzinfo'.  Returns an aware
// datetime (with that tzinfo) and sets 'when', or NULL with an error set.
// 'pastEdge' (if given) is then set when it failed because the moments got
// too close to the limits of a datetime.
static PyObject *CronInZone(const char *cron, size_t length, bool isc,
                            time_t epoch, PyObject *tzinfo, time_t &when,
                            bool *pastEdge = NULL)
{
   PyZone zone(tzinfo);
   try
   {
      when = Date::CronAfter(epoch, std::string(cron, length), isc, zone);
   }
   catch (...)
   {
      SetCronError();
      if (pastEdge != NULL)
         *pastEdge = zone.PastEdge();
      return NULL;
   }
   PyObject *result = FromTimestamp(when, tzinfo);
   if (result == NULL && pastEdge != NULL)
      *pastEdge = PastEdge(when);
   return result;
}

// For a timezone aware datetime or time, returns a new reference to the
// datetime to start from (a time means that time today, in its own
// timezone) and sets 'tzinfo' (also a new reference).  Returns NULL with
// 'tzinfo' NULL for anything else, and NULL with an error set on failure.
static PyObject *AwareRef(PyObject *ref, PyObject **tzinfo)
{
   *tzinfo = NULL;
   if (ref == NULL || !(PyDateTime_Check(ref) || PyTime_Check(ref)))
   {
      return NULL;
   }

   PyObject *tz = PyObject_GetAttrString(ref, "tzinfo");
   if (tz == NULL || tz == Py_None)
   {
      Py_XDECREF(tz);
      return NULL;
   }

   PyObject *start;
   if (PyTime_Check(ref))
   {
      // Today's date over there
      PyObject *now = PyObject_CallMethod(
            (PyObject *) PyDateTimeAPI->DateTimeType, "now", "O", tz);
      PyObject *day = now ? PyObject_CallMethod(now, "date", NULL) : NULL;
      start = day ? PyObject_CallMethod(
            (PyObject *) PyDateTimeAPI->DateTimeType, "combine", "OO",
            day, ref) : NULL;
      Py_XDECREF(day);
      Py_XDECREF(now);
   }
   else
   {
      Py_INCREF(ref);
      start = ref;
   }
   if (start == NULL)
   {
      Py_DECREF(tz);
      return NULL;
   }

   // A tzinfo that gives no offset means the value is naive after all
   PyObject *offset = PyObject_CallMethod(start, "utcoffset", NULL);
   if (offset == NULL || offset == Py_None)
   {
      Py_XDECREF(offset);
      Py_DECREF(tz);
      if (PyErr_Occurred() || start == ref)
      {
         Py_DECREF(start);
         return NULL;
      }
      // An aware time with an odd tzinfo; carry on with today's naive time
      return start;
   }
   Py_DECREF(offset);
   *tzinfo = tz;
   return start;
}

PyDoc_STRVAR(dateblock_doc,
"dateblock(cron, block=True, ref=None, isc=False)\n"
"--\n\n"
"Block (sleep) until the next moment that matches a cron string.\n\n"
"- cron:  a dateblock cron string ('sec min hour dom month dow drift'),\n"
"         or a regular crontab string when isc=True.\n"
"- block: set to False to only work out the time (no sleeping).\n"
"- ref:   calculate relative to this datetime, date, time, timedelta\n"
"         or epoch value instead of now.\n"
"- isc:   treat the cron string as a regular 5 field crontab entry.\n\n"
"A timezone aware datetime or time as ref reads the cron in its own\n"
"timezone (with its daylight saving rules) and returns an aware datetime\n"
"in that same timezone.  Anything else uses this computer's timezone and\n"
"returns a naive datetime.  Either way it unblocks at that exact moment.");

// The cron string (a str) as the engine reads it.  Only its start is
// turned into UTF-8 (just past the longest one allowed); that is enough
// for a huge one to be refused, without first building a UTF-8 copy of
// all of it.  Returns a new reference that owns '*text' (release it once
// done), or NULL with an error set.
static PyObject *CronText(PyObject *cron, const char **text, size_t *length)
{
   PyObject *head = PyUnicode_Substring(cron, 0, Date::T_CRON_LENGTH_MAX + 1);
   if (head == NULL)
   {
      return NULL;
   }
   Py_ssize_t size = 0;
   *text = PyUnicode_AsUTF8AndSize(head, &size);
   if (*text == NULL)
   {
      Py_DECREF(head);
      return NULL;
   }
   if (memchr(*text, '\0', size) != NULL)
   {
      Py_DECREF(head);
      PyErr_SetString(PyExc_ValueError, "embedded null character");
      return NULL;
   }
   *length = static_cast<size_t>(size);
   return head;
}

// The first match of 'cron' after 'ref' (or now).  'ref' sets the
// timezone (see AwareRef()); 'zone' gets a new reference to the tzinfo
// used, or NULL for the computer's own.  Returns the datetime and sets
// 'when' to that moment, or returns NULL with an error set.
static PyObject *FirstMatch(const char *str, size_t length, bool isc,
                            PyObject *ref, PyObject **zone, time_t &when)
{
   PyObject *tzinfo = NULL;
   PyObject *start = AwareRef(ref, &tzinfo);
   if (start == NULL && PyErr_Occurred())
   {
      return NULL;
   }

   PyObject *result = NULL;
   time_t epoch;
   if (tzinfo == NULL)
   {
      result = CronLocal(str, length, isc, start ? start : ref, when);
   }
   else if (AwareEpoch(start, epoch))
   {
      result = CronInZone(str, length, isc, epoch, tzinfo, when);
   }
   Py_XDECREF(start);
   if (result == NULL)
   {
      Py_XDECREF(tzinfo);
      return NULL;
   }
   *zone = tzinfo;
   return result;
}

// Works out the next match of 'cron' (a str) after 'ref' (or now).
// Returns the datetime to give back and sets 'when' to that moment, or
// returns NULL with an error set.
static PyObject *NextMatch(PyObject *cron, PyObject *ref, bool isc,
                           time_t &when)
{
   const char *str = NULL;
   size_t length = 0;
   PyObject *head = CronText(cron, &str, &length);
   if (head == NULL)
   {
      return NULL;
   }
   PyObject *tzinfo = NULL;
   PyObject *result = FirstMatch(str, length, isc, ref, &tzinfo, when);
   Py_DECREF(head);
   Py_XDECREF(tzinfo);
   return result;
}

// Block for a specified cron period and return the date calculated.
// Pass block=False to just perform the calculation.
static PyObject *dateblock(PyObject *self, PyObject *args, PyObject *kwds)
{
   PyObject *cron = NULL;
   int block = 1;
   int isc = 0;
   PyObject *pDateObj = NULL;

   static const char *kwlist[] = {"cron", "block", "ref", "isc", NULL};

   if (!PyArg_ParseTupleAndKeywords(args, kwds, "U|pOp",
            const_cast<char **>(kwlist), &cron, &block, &pDateObj, &isc))
   {
      // Couldn't parse content
      return NULL;
   }

   // Work out the moment (and build the result first; a moment Python
   // can't represent, past the year 9999, raises right away instead of
   // after sleeping until then)
   time_t when = 0;
   PyObject *result = NextMatch(cron, pDateObj, isc != 0, when);
   if (result == NULL)
   {
      return NULL;
   }

   if (block)
   {
      // Release the GIL while we sleep so other Python threads keep
      // running.  If a signal wakes us early, give Python a chance to run
      // its handlers (ie: Ctrl-C raises KeyboardInterrupt) and then go
      // back to sleep.
      while (true)
      {
         unsigned long left;
         Py_BEGIN_ALLOW_THREADS
         left = Date::BlockUntil(when);
         Py_END_ALLOW_THREADS

         if (left == 0)
         {
            break;
         }
         if (PyErr_CheckSignals() < 0)
         {
            Py_DECREF(result);
            return NULL;
         }
      }
   }

   // Return Finish Date/Time
   return result;
}

// Used by dateblock.wait(): the same as dateblock(cron, block=False, ...)
// but returns (datetime, seconds since the epoch), so the waiting can be
// done with asyncio without reading the moment back from the datetime
// (a naive one can't tell the two copies of a repeated hour apart).
static PyObject *next_match(PyObject *self, PyObject *args, PyObject *kwds)
{
   PyObject *cron = NULL;
   int isc = 0;
   PyObject *pDateObj = NULL;

   static const char *kwlist[] = {"cron", "ref", "isc", NULL};

   if (!PyArg_ParseTupleAndKeywords(args, kwds, "U|Op",
            const_cast<char **>(kwlist), &cron, &pDateObj, &isc))
   {
      return NULL;
   }

   time_t when = 0;
   PyObject *result = NextMatch(cron, pDateObj, isc != 0, when);
   if (result == NULL)
   {
      return NULL;
   }
   return Py_BuildValue("(NL)", result, static_cast<long long>(when));
}

PyDoc_STRVAR(upcoming_doc,
"upcoming(cron, count=COUNT_DEFAULT, ref=None, isc=False)\n"
"--\n\n"
"The next few moments that match a cron string, without waiting.\n\n"
"- cron:  a dateblock cron string, or a regular crontab string when\n"
"         isc=True.\n"
"- count: how many to return (1 to COUNT_MAX; just the next one\n"
"         unless given).\n"
"- ref:   start from this datetime, date, time, timedelta or epoch\n"
"         value instead of now.\n"
"- isc:   treat the cron string as a regular 5 field crontab entry.\n\n"
"Returns a tuple of datetimes, earliest first, the same kind\n"
"dateblock() returns (aware in ref's own timezone, otherwise naive).\n"
"The tuple is shorter than count when no later moment can be shown\n"
"(ie: past the year 9999).  Errors are the same as dateblock()'s, and\n"
"a count out of range raises ValueError.  One exception: on the very\n"
"last day of the year 9999 (or first of the year 1), an error from a\n"
"custom tzinfo can't be told apart from running out of dates, so it\n"
"just ends the tuple.");

// True if the error just raised means the list has simply run out: the
// next moment is too far away to be shown (ie: past the year 9999), or no
// later moment matches.
//  - In local time no code but Python's own runs, so its range errors
//    (ValueError, OverflowError) are enough to tell.
//  - In a timezone, the tzinfo's own code runs too and may raise either
//    one, so the search must also have reached a datetime's limits.  In
//    that last day a broken tzinfo's error looks the same as running out,
//    so it ends the list too.
static bool ListEnds(bool inZone, bool pastEdge)
{
   if (inZone && !pastEdge)
   {
      return false;
   }
   return PyErr_ExceptionMatches(PyExc_OverflowError) ||
          PyErr_ExceptionMatches(PyExc_ValueError);
}

// The next 'count' matches, as a tuple (see upcoming_doc)
static PyObject *upcoming(PyObject *self, PyObject *args, PyObject *kwds)
{
   PyObject *cron = NULL;
   PyObject *pCount = NULL;
   PyObject *pDateObj = NULL;
   int isc = 0;

   static const char *kwlist[] = {"cron", "count", "ref", "isc", NULL};

   if (!PyArg_ParseTupleAndKeywords(args, kwds, "U|OOp",
            const_cast<char **>(kwlist), &cron, &pCount, &pDateObj, &isc))
   {
      return NULL;
   }

   // Any whole number is accepted here, so even a huge one gets the same
   // ValueError as any other count out of range
   long count = Date::T_COUNT_DEFAULT;
   if (pCount != NULL)
   {
      PyObject *number = PyNumber_Index(pCount);
      if (number == NULL)
      {
         return NULL;
      }
      int overflow = 0;
      count = PyLong_AsLongAndOverflow(number, &overflow);
      if (count == -1 && PyErr_Occurred())
      {
         Py_DECREF(number);
         return NULL;
      }
      Py_DECREF(number);
      if (overflow)
      {
         // Too big to be worth showing
         PyErr_Format(PyExc_ValueError, "count must be from 1 to %d",
                      (int)Date::T_COUNT_MAX);
         return NULL;
      }
      if (count < 1 || count > Date::T_COUNT_MAX)
      {
         PyErr_Format(PyExc_ValueError, "count must be from 1 to %d (not %ld)",
                      (int)Date::T_COUNT_MAX, count);
         return NULL;
      }
   }

   const char *str = NULL;
   size_t length = 0;
   PyObject *head = CronText(cron, &str, &length);
   if (head == NULL)
   {
      return NULL;
   }

   // The first one is found as dateblock() finds it; its errors are raised
   PyObject *tzinfo = NULL;
   time_t when = 0;
   PyObject *item = FirstMatch(str, length, isc != 0, pDateObj, &tzinfo,
                               when);
   PyObject *found = item ? PyList_New(0) : NULL;
   if (found != NULL && PyList_Append(found, item) < 0)
   {
      Py_CLEAR(found);
   }
   Py_XDECREF(item);

   // Each of the rest follows the moment before it (in seconds, so the
   // two copies of a repeated hour stay apart), in the same timezone
   while (found != NULL && PyList_GET_SIZE(found) < count)
   {
      time_t next = 0;
      bool pastEdge = false;
      if (tzinfo != NULL)
      {
         item = CronInZone(str, length, isc != 0, when, tzinfo, next,
                           &pastEdge);
      }
      else
      {
         PyObject *at = PyLong_FromLongLong((long long)when);
         item = at ? CronLocal(str, length, isc != 0, at, next) : NULL;
         Py_XDECREF(at);
      }
      if (item == NULL)
      {
         if (ListEnds(tzinfo != NULL, pastEdge))
         {
            PyErr_Clear();
            break;
         }
         Py_CLEAR(found);
         break;
      }
      if (PyList_Append(found, item) < 0)
      {
         Py_CLEAR(found);
      }
      Py_DECREF(item);
      when = next;
   }
   Py_DECREF(head);
   Py_XDECREF(tzinfo);
   if (found == NULL)
   {
      return NULL;
   }
   PyObject *result = PyList_AsTuple(found);
   Py_DECREF(found);
   return result;
}

// Python Method Table.
static PyMethodDef DateblockMethods[] =
{
    {"dateblock", (PyCFunction)(void (*)(void))dateblock,
                  METH_VARARGS | METH_KEYWORDS, dateblock_doc},
    {"upcoming", (PyCFunction)(void (*)(void))upcoming,
                 METH_VARARGS | METH_KEYWORDS, upcoming_doc},
    {"_next", (PyCFunction)(void (*)(void))next_match,
              METH_VARARGS | METH_KEYWORDS,
              "_next(cron, ref=None, isc=False) -> (datetime, epoch)"},
    // sentinel
    {NULL, NULL, 0, NULL}
};

// Imported by the dateblock package (python/dateblock/__init__.py)
static struct PyModuleDef dateblockmodule =
{
    PyModuleDef_HEAD_INIT,
    "dateblock._dateblock",
    "Cron-like blocking (sleep until a moment in time)",
    -1,
    DateblockMethods
};

// Register the method table.
PyMODINIT_FUNC PyInit__dateblock(void)
{
   PyDateTime_IMPORT;
   if (PyDateTimeAPI == NULL)
   {
      return NULL;
   }

   PyObject *epoch = PyDateTimeAPI->DateTime_FromDateAndTime(
         1970, 1, 1, 0, 0, 0, 0, PyDateTimeAPI->TimeZone_UTC,
         PyDateTimeAPI->DateTimeType);
   if (epoch == NULL)
   {
      return NULL;
   }

   PyObject *m = PyModule_Create(&dateblockmodule);
   if (m == NULL)
   {
      Py_DECREF(epoch);
      return NULL;
   }

   // Create Constants
   if (PyModule_AddIntConstant(m, "SEC_MIN", Date::T_SEC_MIN) < 0 ||
       PyModule_AddIntConstant(m, "SEC_MAX", Date::T_SEC_MAX) < 0 ||
       PyModule_AddIntConstant(m, "MIN_MIN", Date::T_MIN_MIN) < 0 ||
       PyModule_AddIntConstant(m, "MIN_MAX", Date::T_MIN_MAX) < 0 ||
       PyModule_AddIntConstant(m, "HOUR_MIN", Date::T_HOUR_MIN) < 0 ||
       PyModule_AddIntConstant(m, "HOUR_MAX", Date::T_HOUR_MAX) < 0 ||
       PyModule_AddIntConstant(m, "DOM_MIN", Date::T_DOM_MIN) < 0 ||
       PyModule_AddIntConstant(m, "DOM_MAX", Date::T_DOM_MAX) < 0 ||
       PyModule_AddIntConstant(m, "MONTH_MIN", Date::T_MONTH_MIN) < 0 ||
       PyModule_AddIntConstant(m, "MONTH_MAX", Date::T_MONTH_MAX) < 0 ||
       PyModule_AddIntConstant(m, "YEAR_MIN", Date::T_YEAR_MIN) < 0 ||
       PyModule_AddIntConstant(m, "YEAR_MAX", Date::T_YEAR_MAX) < 0 ||
       PyModule_AddIntConstant(m, "DOW_MIN", Date::T_DOW_MIN) < 0 ||
       PyModule_AddIntConstant(m, "DOW_MAX", Date::T_DOW_MAX) < 0 ||
       PyModule_AddIntConstant(m, "DRIFT_MIN", Date::T_DRIFT_MIN) < 0 ||
       PyModule_AddIntConstant(m, "DRIFT_MAX", Date::T_DRIFT_MAX) < 0 ||
       PyModule_AddIntConstant(m, "COUNT_DEFAULT", Date::T_COUNT_DEFAULT) < 0 ||
       PyModule_AddIntConstant(m, "COUNT_MAX", Date::T_COUNT_MAX) < 0)
   {
      Py_DECREF(epoch);
      Py_DECREF(m);
      return NULL;
   }

   // Only kept once the module is ready (replacing any from an earlier
   // import, so it isn't leaked)
   Py_XDECREF(g_epoch);
   g_epoch = epoch;
   return m;
}
