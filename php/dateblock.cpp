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
// The PHP extension: adds the dateblock() function (see dateblock.stub.php
// for how it is called).
#include <limits.h>
#include <stdio.h>
#include <math.h>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include "Date.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "ext/standard/info.h"
// These two headers don't say their functions are C ones themselves
BEGIN_EXTERN_C()
#include "ext/date/php_date.h"
#include "ext/spl/spl_exceptions.h"
END_EXTERN_C()
#include "zend_exceptions.h"
#include "dateblock_arginfo.h"

// Calls $obj->name(...$args), or the function 'name' when 'obj' is NULL.
//  - Returns true with the result in 'ret' (release it with
//    zval_ptr_dtor()).
//  - Returns false if the call failed or threw; 'ret' is then left empty
//    (and an exception may be set).
static bool Call(zval *obj, const char *name, zval *ret,
                 uint32_t argc = 0, zval *argv = NULL)
{
   zval fname;
   ZVAL_STRING(&fname, name);
   ZVAL_UNDEF(ret);
   bool ok = (call_user_function(NULL, obj, &fname, ret, argc, argv) == SUCCESS)
             && !EG(exception) && Z_TYPE_P(ret) != IS_UNDEF;
   zval_ptr_dtor(&fname);
   if (!ok)
   {
      zval_ptr_dtor(ret);
      ZVAL_UNDEF(ret);
   }
   return ok;
}

// True when 'zv' is a DateTime or DateTimeImmutable (or a class based on
// one of them)
static bool IsDate(zval *zv)
{
   return Z_TYPE_P(zv) == IS_OBJECT &&
          instanceof_function(Z_OBJCE_P(zv), php_date_get_interface_ce());
}

// new DateTimeImmutable("@t") moved into the timezone 'tz' (a
// DateTimeZone): the moment 't' as a date in that timezone.  This is used
// instead of setTimestamp(), which PHP 8.0 gets wrong in the hour that
// repeats when the clocks go back.
static bool AtMoment(time_t t, zval *tz, zval *ret)
{
   zval at, utc;
   char epoch[32];
   snprintf(epoch, sizeof(epoch), "@%lld", (long long)t);
   ZVAL_STRING(&at, epoch);
   bool ok = Call(NULL, "date_create_immutable", &utc, 1, &at);
   zval_ptr_dtor(&at);
   ok = ok && IsDate(&utc) && Call(&utc, "setTimezone", ret, 1, tz);
   zval_ptr_dtor(&utc);
   if (ok && (!IsDate(ret) || Z_PHPDATE_P(ret)->time == NULL))
   {
      zval_ptr_dtor(ret);
      ZVAL_UNDEF(ret);
      ok = false;
   }
   return ok;
}

// True when a date's year fits in a struct tm (the engine's limit)
static bool YearFits(timelib_sll year)
{
   return year - 1900 >= INT_MIN && year - 1900 <= INT_MAX - 1900;
}

// A PHP timezone, seen through the engine's Date::TimeZone hook.  The
// local time comes from PHP itself, so its timezone database and daylight
// saving rules are used (not the computer's).
class PhpZone : public Date::TimeZone
{
   public:
     PhpZone(zval *tz) : m_tz(tz), m_outOfRange(false) {}

     // True once a moment was too far away to be shown as a date (its year
     // doesn't fit); the search then gives up
     bool OutOfRange() const { return m_outOfRange; }

     bool ToLocal(time_t t, struct tm& tmOut) const
     {
        if (EG(exception))
        {
           return false;
        }

        zval moved;
        if (!AtMoment(t, m_tz, &moved))
        {
           return false;
        }
        timelib_time *local = Z_PHPDATE_P(&moved)->time;
        bool ok = YearFits(local->y);
        m_outOfRange = m_outOfRange || !ok;
        if (ok)
        {
           tmOut.tm_year = (int)(local->y - 1900);
           tmOut.tm_mon = (int)local->m - 1;
           tmOut.tm_mday = (int)local->d;
           tmOut.tm_hour = (int)local->h;
           tmOut.tm_min = (int)local->i;
           tmOut.tm_sec = (int)local->s;
        }
        zval_ptr_dtor(&moved);
        return ok;
     }

   private:
     zval *m_tz;
     mutable bool m_outOfRange;
};

// Turns the C++ exception being handled into a PHP one.  Call it from
// inside a catch block.  A PHP exception that is already set is kept.
static void ThrowCronError()
{
   try
   {
      throw;
   }
   catch (const std::invalid_argument &e)
   {
      if (!EG(exception))
         zend_argument_value_error(1, "is not a valid cron: %s", e.what());
   }
   catch (const std::out_of_range &)
   {
      if (!EG(exception))
         zend_argument_value_error(3,
               "is outside of the supported time range");
   }
   catch (const std::runtime_error &e)
   {
      if (!EG(exception))
         zend_throw_exception(spl_ce_RuntimeException, e.what(), 0);
   }
   catch (const std::bad_alloc &)
   {
      if (!EG(exception))
         zend_throw_error(NULL, "dateblock ran out of memory");
   }
   catch (const std::exception &e)
   {
      if (!EG(exception))
         zend_throw_error(NULL, "%s", e.what());
   }
   catch (...)
   {
      if (!EG(exception))
         zend_throw_error(NULL, "dateblock hit an unexpected error");
   }
}

// The moment to start from, as a DateTimeImmutable in the timezone the
// cron is read in:
//  - null:              now, in PHP's default timezone
//  - int or float:      seconds since the epoch, in PHP's default timezone
//  - DateTimeInterface: that moment, in its own timezone
// Returns false with an exception set on failure.
static bool StartFrom(zval *ref, zval *start)
{
   if (ref != NULL && IsDate(ref))
   {
      return Call(NULL, "DateTimeImmutable::createFromInterface", start,
                  1, ref);
   }

   if (ref != NULL && Z_TYPE_P(ref) != IS_NULL &&
       Z_TYPE_P(ref) != IS_LONG && Z_TYPE_P(ref) != IS_DOUBLE)
   {
      zend_argument_type_error(3,
            "must be of type DateTimeInterface|int|float|null, %s given",
            zend_zval_type_name(ref));
      return false;
   }

   zval now;
   if (!Call(NULL, "date_create_immutable", &now) || !IsDate(&now))
   {
      if (!EG(exception))
         zend_throw_error(NULL, "The current time could not be read");
      zval_ptr_dtor(&now);
      return false;
   }
   if (ref == NULL || Z_TYPE_P(ref) == IS_NULL)
   {
      ZVAL_COPY_VALUE(start, &now);
      return true;
   }

   // Seconds since the epoch; a fraction is dropped (rounded down).  A
   // float is only turned into a time_t once it is known to fit (NaN,
   // infinity and huge values don't).  The largest time_t rounds up to
   // 2**63 as a double, which itself doesn't fit, hence '<'.
   bool fits = true;
   time_t t = 0;
   if (Z_TYPE_P(ref) == IS_LONG)
   {
      t = (time_t)Z_LVAL_P(ref);
   }
   else
   {
      double epoch = floor(Z_DVAL_P(ref));
      fits = isfinite(epoch) &&
             epoch >= (double)std::numeric_limits<time_t>::min() &&
             epoch < (double)std::numeric_limits<time_t>::max();
      if (fits)
         t = (time_t)epoch;
   }
   zval tz;
   ZVAL_UNDEF(&tz);
   bool ok = fits && Call(&now, "getTimezone", &tz);
   ok = ok && AtMoment(t, &tz, start);
   zval_ptr_dtor(&tz);
   zval_ptr_dtor(&now);
   if (!ok && !EG(exception))
   {
      zend_argument_value_error(3, "is outside of the supported time range");
   }
   return ok;
}

ZEND_FUNCTION(dateblock)
{
   char *cron;
   size_t cron_len;
   zend_bool block = 1;
   zval *ref = NULL;
   zend_bool isc = 0;

   ZEND_PARSE_PARAMETERS_START(1, 4)
      Z_PARAM_STRING(cron, cron_len)
      Z_PARAM_OPTIONAL
      Z_PARAM_BOOL(block)
      Z_PARAM_ZVAL(ref)
      Z_PARAM_BOOL(isc)
   ZEND_PARSE_PARAMETERS_END();

   zval start;
   if (!StartFrom(ref, &start))
   {
      RETURN_THROWS();
   }

   // The cron is read in the timezone of the starting moment.  The moment
   // itself is read straight from the date: PHP 8.0's getTimestamp() works
   // it out again from the clock time, which picks the wrong one of the
   // two 1:30s when the clocks go back.
   zval tz;
   time_t when = 0;
   bool found = false;
   timelib_time *at = Z_PHPDATE_P(&start)->time;
   if (at != NULL && !YearFits(at->y))
   {
      zend_argument_value_error(3, "is outside of the supported time range");
   }
   else if (at != NULL && at->sse_uptodate &&
            Call(&start, "getTimezone", &tz))
   {
      // A C++ exception must never reach PHP; it would end the process
      PhpZone zone(&tz);
      try
      {
         when = Date::CronAfter((time_t)at->sse, std::string(cron, cron_len),
                                isc != 0, zone);
         found = true;
      }
      catch (...)
      {
         // A search that ran past the years a date can hold is a reference
         // too far away, not a cron that never happens
         if (zone.OutOfRange() && !EG(exception))
            zend_argument_value_error(3,
                  "is outside of the supported time range");
         else
            ThrowCronError();
      }

      // Build the result (in the same timezone) before sleeping, so a
      // problem shows up right away instead of once the time arrives
      found = found && AtMoment(when, &tz, return_value);
      zval_ptr_dtor(&tz);
   }
   zval_ptr_dtor(&start);
   if (!found)
   {
      if (!EG(exception))
         zend_throw_error(NULL, "The time found could not be shown as a date");
      RETURN_THROWS();
   }

   if (block)
   {
      // A signal can wake us early; go back to sleep until the time comes
      while (Date::BlockUntil(when) != 0)
      {
      }
   }
}

PHP_RINIT_FUNCTION(dateblock)
{
   (void)type;
   (void)module_number;
#if defined(ZTS) && defined(COMPILE_DL_DATEBLOCK)
   ZEND_TSRMLS_CACHE_UPDATE();
#endif
   return SUCCESS;
}

PHP_MINFO_FUNCTION(dateblock)
{
   (void)zend_module;
   php_info_print_table_start();
   php_info_print_table_row(2, "dateblock support", "enabled");
   php_info_print_table_row(2, "Version", PHP_DATEBLOCK_VERSION);
   php_info_print_table_end();
}

static const zend_module_dep dateblock_deps[] = {
   ZEND_MOD_REQUIRED("date")
   ZEND_MOD_REQUIRED("spl")
   ZEND_MOD_END
};

zend_module_entry dateblock_module_entry = {
   STANDARD_MODULE_HEADER_EX,
   NULL,
   dateblock_deps,
   "dateblock",
   ext_functions,
   NULL,
   NULL,
   PHP_RINIT(dateblock),
   NULL,
   PHP_MINFO(dateblock),
   PHP_DATEBLOCK_VERSION,
   STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_DATEBLOCK
# ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
# endif
ZEND_GET_MODULE(dateblock)
#endif
