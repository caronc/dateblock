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
// The Node.js add-on: works out the next moment that matches a cron.  The
// waiting and the friendly API live in index.js; this only does the
// calculation:
//
//   cronAfter(after, cron, isc, toLocal)
//    - after:   seconds since the epoch to search from
//    - cron:    the cron string
//    - isc:     true for a regular 5 field crontab entry
//    - toLocal: function(seconds) returning [year, month, day, hour,
//               minute, second] for that moment in the chosen timezone, or
//               null if it can't
//
// It returns { when: seconds } or { error: kind, message }, where kind is
// one of 'syntax', 'never', 'range' or 'memory'.  If toLocal throws, that
// exception is passed on as it is.
#include <math.h>
#include <limits.h>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <node_api.h>
#include "Date.h"

namespace {

// A JavaScript timezone, seen through the engine's Date::TimeZone hook
class JsZone : public Date::TimeZone
{
   public:
     JsZone(napi_env env, napi_value fn)
        : m_env(env), m_fn(fn), m_outOfRange(false) {}

     // True once a moment was too far away to be shown as a date; the
     // search then gives up
     bool OutOfRange() const { return m_outOfRange; }

     bool ToLocal(time_t t, struct tm& tmOut) const
     {
        bool pending = false;
        if (napi_is_exception_pending(m_env, &pending) != napi_ok || pending)
        {
           return false;
        }

        // The search can ask thousands of times; free each call's values
        // as soon as it is done
        napi_handle_scope scope;
        if (napi_open_handle_scope(m_env, &scope) != napi_ok)
        {
           return false;
        }
        bool ok = Read(t, tmOut);
        napi_close_handle_scope(m_env, scope);
        return ok;
     }

   private:
     bool Read(time_t t, struct tm& tmOut) const
     {
        napi_value global, arg, result;
        if (napi_get_global(m_env, &global) != napi_ok ||
            napi_create_double(m_env, (double)t, &arg) != napi_ok ||
            napi_call_function(m_env, global, m_fn, 1, &arg, &result) != napi_ok)
        {
           return false;
        }

        bool isArray = false;
        uint32_t length = 0;
        if (napi_is_array(m_env, result, &isArray) != napi_ok || !isArray ||
            napi_get_array_length(m_env, result, &length) != napi_ok ||
            length != 6)
        {
           // null: the moment is outside what a date can show
           m_outOfRange = true;
           return false;
        }

        // year, month (1-12), day, hour, minute, second; whole numbers only
        static const double lowest[6] = {-1e12, 1, 1, 0, 0, 0};
        static const double highest[6] = {1e12, 12, 31, 23, 59, 60};
        double v[6];
        for (uint32_t i = 0; i < 6; i++)
        {
           napi_value item;
           if (napi_get_element(m_env, result, i, &item) != napi_ok ||
               napi_get_value_double(m_env, item, &v[i]) != napi_ok ||
               !isfinite(v[i]) || v[i] != floor(v[i]) ||
               v[i] < lowest[i] || v[i] > highest[i])
           {
              return false;
           }
        }
        if (v[0] - 1900 < (double)INT_MIN || v[0] - 1900 > (double)INT_MAX - 1900)
        {
           m_outOfRange = true;
           return false;
        }

        tmOut.tm_year = (int)(v[0] - 1900);
        tmOut.tm_mon = (int)v[1] - 1;
        tmOut.tm_mday = (int)v[2];
        tmOut.tm_hour = (int)v[3];
        tmOut.tm_min = (int)v[4];
        tmOut.tm_sec = (int)v[5];
        return true;
     }

     napi_env m_env;
     napi_value m_fn;
     mutable bool m_outOfRange;
};

// { error: kind, message }
napi_value Failure(napi_env env, const char *kind, const char *message)
{
   napi_value result, k, m;
   if (napi_create_object(env, &result) != napi_ok ||
       napi_create_string_utf8(env, kind, NAPI_AUTO_LENGTH, &k) != napi_ok ||
       napi_create_string_utf8(env, message, NAPI_AUTO_LENGTH, &m) != napi_ok ||
       napi_set_named_property(env, result, "error", k) != napi_ok ||
       napi_set_named_property(env, result, "message", m) != napi_ok)
   {
      return NULL;
   }
   return result;
}

// Turns the C++ exception being handled into a { error, message } result.
// Call it from inside a catch block.
napi_value CronFailure(napi_env env, const JsZone &zone)
{
   try
   {
      throw;
   }
   catch (const std::invalid_argument &e)
   {
      return Failure(env, "syntax", e.what());
   }
   catch (const std::out_of_range &e)
   {
      return Failure(env, "range", e.what());
   }
   catch (const std::runtime_error &e)
   {
      // A search that ran past the dates that can be shown is a reference
      // too far away, not a cron that never happens
      if (zone.OutOfRange())
         return Failure(env, "range", "The time is outside of the supported range.");
      return Failure(env, "never", e.what());
   }
   catch (const std::bad_alloc &)
   {
      return Failure(env, "memory", "dateblock ran out of memory.");
   }
   catch (const std::exception &e)
   {
      return Failure(env, "error", e.what());
   }
   catch (...)
   {
      return Failure(env, "error", "dateblock hit an unexpected error.");
   }
}

napi_value CronAfter(napi_env env, napi_callback_info info)
{
   size_t argc = 4;
   napi_value argv[4];
   if (napi_get_cb_info(env, info, &argc, argv, NULL, NULL) != napi_ok ||
       argc != 4)
   {
      napi_throw_type_error(env, NULL, "cronAfter() takes 4 arguments");
      return NULL;
   }

   // index.js checks these; this only makes sure nothing odd gets through
   double after;
   bool isc;
   size_t length;
   napi_valuetype fnType;
   if (napi_get_value_double(env, argv[0], &after) != napi_ok ||
       napi_get_value_string_utf8(env, argv[1], NULL, 0, &length) != napi_ok ||
       napi_get_value_bool(env, argv[2], &isc) != napi_ok ||
       napi_typeof(env, argv[3], &fnType) != napi_ok || fnType != napi_function)
   {
      napi_throw_type_error(env, NULL, "cronAfter() was given the wrong types");
      return NULL;
   }
   // A double holds every whole second a JavaScript Date can (and far
   // more); NaN, infinity and huge values are refused before converting
   if (!isfinite(after) || after != floor(after) ||
       after < (double)std::numeric_limits<time_t>::min() ||
       after >= (double)std::numeric_limits<time_t>::max())
   {
      return Failure(env, "range", "The time is outside of the supported range.");
   }

   // A C++ exception must never reach Node.js; it would end the process
   JsZone zone(env, argv[3]);
   try
   {
      // A huge cron is only copied up to just past the longest one
      // allowed; that is enough for it to be refused.  Node.js never cuts
      // a character in half, so 4 bytes (the longest UTF-8 character) are
      // asked for past the limit, so that more than the limit always
      // arrives.
      if (length > Date::T_CRON_LENGTH_MAX)
         length = Date::T_CRON_LENGTH_MAX + 4;
      std::string cron(length + 1, '\0');
      if (napi_get_value_string_utf8(env, argv[1], &cron[0], cron.size(),
                                     &length) != napi_ok)
      {
         return NULL;
      }
      cron.resize(length);

      time_t when = Date::CronAfter((time_t)after, cron, isc, zone);

      napi_value result, w;
      if (napi_create_object(env, &result) != napi_ok ||
          napi_create_double(env, (double)when, &w) != napi_ok ||
          napi_set_named_property(env, result, "when", w) != napi_ok)
      {
         return NULL;
      }
      return result;
   }
   catch (...)
   {
      // An exception thrown by toLocal() is passed on as it is
      bool pending = false;
      napi_is_exception_pending(env, &pending);
      if (pending)
      {
         return NULL;
      }
      return CronFailure(env, zone);
   }
}

}  // namespace

NAPI_MODULE_INIT()
{
   // cronAfter(), plus how many times upcoming() lists by default and at
   // most (shared with the other bindings through Date.h)
   napi_value fn, countDefault, countMax;
   if (napi_create_function(env, "cronAfter", NAPI_AUTO_LENGTH, CronAfter,
                            NULL, &fn) != napi_ok ||
       napi_set_named_property(env, exports, "cronAfter", fn) != napi_ok ||
       napi_create_int32(env, Date::T_COUNT_DEFAULT, &countDefault) != napi_ok ||
       napi_set_named_property(env, exports, "countDefault",
                               countDefault) != napi_ok ||
       napi_create_int32(env, Date::T_COUNT_MAX, &countMax) != napi_ok ||
       napi_set_named_property(env, exports, "countMax", countMax) != napi_ok)
   {
      return NULL;
   }
   return exports;
}
