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
#include <limits>
#include <new>
#include <stdexcept>
#include "Date.h"

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
      // NaN, infinity and huge values can't be turned into a time_t.  The
      // largest time_t rounds up to 2**63 as a double, which itself
      // doesn't fit, hence '>='.
      double epoch = floor(PyFloat_AS_DOUBLE(obj));
      if (!isfinite(epoch) ||
          epoch < (double) std::numeric_limits<time_t>::min() ||
          epoch >= (double) std::numeric_limits<time_t>::max())
      {
         PyErr_SetString(PyExc_OverflowError,
               "ref is outside of the supported time range.");
         return false;
      }
      out.Set((time_t) epoch);
      return true;
   }

   PyErr_SetString(PyExc_TypeError,
         "ref must be a datetime, date, time, timedelta, epoch seconds or None.");
   return false;
}

// Returns a new (naive, local time) datetime object for 'date'
static PyObject *DateToPy(const Date &date)
{
   return PyDateTime_FromDateAndTime(date.Year(),
                                     date.Month(),
                                     date.DOM(),
                                     date.Hour(),
                                     date.Min(),
                                     date.Sec(),
                                     0);
}

// 1970-01-01 00:00:00 UTC (set up when the module loads)
static PyObject *g_epoch = NULL;

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
     PyZone(PyObject *tzinfo) : m_tzinfo(tzinfo) {}

     bool ToLocal(time_t t, struct tm& tmOut) const
     {
        if (PyErr_Occurred())
        {
           return false;
        }

        PyObject *dt = FromTimestamp(t, m_tzinfo);
        if (dt == NULL)
        {
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
}

// The next match for a naive reference (or none), read in the computer's
// own timezone.  Returns a naive datetime and sets 'when', or NULL with
// an error set.
static PyObject *CronLocal(const char *cron, bool isc, PyObject *ref,
                           time_t &when)
{
   // A C++ exception must never reach Python; it would abort the
   // interpreter
   Date dObjFinish;
   try
   {
      Date dObjStart;
      if (!PyToDate(ref, dObjStart))
      {
         return NULL;
      }
      dObjFinish = dObjStart.Cron(cron, isc);
   }
   catch (...)
   {
      SetCronError();
      return NULL;
   }
   when = dObjFinish.Time();
   return DateToPy(dObjFinish);
}

// The next match for a timezone aware datetime, read in its own timezone.
// Returns an aware datetime (with the same tzinfo) and sets 'when', or
// NULL with an error set.
static PyObject *CronInZone(const char *cron, bool isc, PyObject *ref,
                            PyObject *tzinfo, time_t &when)
{
   PyObject *ts = PyObject_CallMethod(ref, "timestamp", NULL);
   if (ts == NULL)
   {
      return NULL;
   }
   double epoch = floor(PyFloat_AsDouble(ts));
   Py_DECREF(ts);
   if (PyErr_Occurred())
   {
      return NULL;
   }

   try
   {
      PyZone zone(tzinfo);
      when = Date::CronAfter((time_t) epoch, cron, isc, zone);
   }
   catch (...)
   {
      SetCronError();
      return NULL;
   }
   return FromTimestamp(when, tzinfo);
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

// Block for a specified cron period and return the date calculated.
// Pass block=False to just perform the calculation.
static PyObject *dateblock(PyObject *self, PyObject *args, PyObject *kwds)
{
   const char *str = NULL;
   int block = 1;
   int isc = 0;
   PyObject *pDateObj = NULL;

   static const char *kwlist[] = {"cron", "block", "ref", "isc", NULL};

   if (!PyArg_ParseTupleAndKeywords(args, kwds, "s|pOp",
            const_cast<char **>(kwlist), &str, &block, &pDateObj, &isc))
   {
      // Couldn't parse content
      return NULL;
   }

   // Work out the moment (and build the result first; a moment Python
   // can't represent, past the year 9999, raises right away instead of
   // after sleeping until then)
   time_t when = 0;
   PyObject *tzinfo = NULL;
   PyObject *start = AwareRef(pDateObj, &tzinfo);
   if (start == NULL && PyErr_Occurred())
   {
      return NULL;
   }

   PyObject *result;
   if (tzinfo != NULL)
   {
      result = CronInZone(str, isc != 0, start, tzinfo, when);
   }
   else
   {
      result = CronLocal(str, isc != 0, start ? start : pDateObj, when);
   }
   Py_XDECREF(start);
   Py_XDECREF(tzinfo);
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

// Python Method Table.
static PyMethodDef DateblockMethods[] =
{
    {"dateblock", (PyCFunction)(void (*)(void))dateblock,
                  METH_VARARGS | METH_KEYWORDS, dateblock_doc},
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

   g_epoch = PyDateTimeAPI->DateTime_FromDateAndTime(
         1970, 1, 1, 0, 0, 0, 0, PyDateTimeAPI->TimeZone_UTC,
         PyDateTimeAPI->DateTimeType);
   if (g_epoch == NULL)
   {
      return NULL;
   }

   PyObject *m = PyModule_Create(&dateblockmodule);
   if (m == NULL)
   {
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
       PyModule_AddIntConstant(m, "DRIFT_MAX", Date::T_DRIFT_MAX) < 0)
   {
      Py_DECREF(m);
      return NULL;
   }
   return m;
}
