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
 this program; if not, write to the Free Software Foundation, Inc.,
 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*/
#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <datetime.h>

#include <math.h>
#include <stdexcept>
#include "Date.h"

// Fills in 'out' from a Python reference value:
//  - None:               now
//  - datetime:           as is (timezone aware values are converted to
//                        local time)
//  - date:               midnight of that day
//  - time:               that time, today
//  - int or float:       seconds since the epoch
// Returns false (with a TypeError set) for anything else.
static bool PyToDate(PyObject *obj, Date &out)
{
   out.Set();
   if (obj == NULL || obj == Py_None)
   {
      return true;
   }

   if (PyDateTime_Check(obj))
   {
      PyObject *tzinfo = PyObject_GetAttrString(obj, "tzinfo");
      if (tzinfo == NULL)
      {
         return false;
      }
      bool aware = (tzinfo != Py_None);
      Py_DECREF(tzinfo);

      if (aware)
      {
         // Let Python work out the exact moment, then view it locally
         PyObject *ts = PyObject_CallMethod(obj, "timestamp", NULL);
         if (ts == NULL)
         {
            return false;
         }
         double epoch = PyFloat_AsDouble(ts);
         Py_DECREF(ts);
         if (epoch == -1.0 && PyErr_Occurred())
         {
            return false;
         }
         out.Set((time_t) floor(epoch));
         return true;
      }

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
      out.Set((time_t) floor(PyFloat_AS_DOUBLE(obj)));
      return true;
   }

   PyErr_SetString(PyExc_TypeError,
         "ref must be a datetime, date, time, epoch seconds or None.");
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

PyDoc_STRVAR(dateblock_doc,
"dateblock(cron, block=True, ref=None, isc=False)\n"
"--\n\n"
"Block (sleep) until the next moment that matches a cron string.\n\n"
"- cron:  a dateblock cron string ('sec min hour dom month dow drift'),\n"
"         or a regular crontab string when isc=True.\n"
"- block: set to False to only work out the time (no sleeping).\n"
"- ref:   calculate relative to this datetime, date, time or epoch value\n"
"         instead of now.\n"
"- isc:   treat the cron string as a regular 5 field crontab entry.\n\n"
"Returns the matching moment as a datetime (in local time).");

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

   // Reference Time (defaults to now)
   Date dObjStart;
   if (!PyToDate(pDateObj, dObjStart))
   {
      return NULL;
   }

   if (!Date::CronValid(str, isc != 0))
   {
      // Raise SyntaxError Exception
      PyErr_SetString(PyExc_SyntaxError,
            "The cron specified is not formatted correctly.");
      return NULL;
   }

   Date dObjFinish;
   try
   {
      dObjFinish = dObjStart.Cron(str, isc != 0);
   }
   catch (const std::runtime_error &e)
   {
      PyErr_Format(PyExc_ValueError,
            "No date/time matches the cron specified (%s).", e.what());
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
         left = Date::BlockUntil(dObjFinish);
         Py_END_ALLOW_THREADS

         if (left == 0)
         {
            break;
         }
         if (PyErr_CheckSignals() < 0)
         {
            return NULL;
         }
      }
   }

   // Return Finish Date/Time
   return DateToPy(dObjFinish);
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
