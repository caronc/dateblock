/*  Date Cron Unit Tester by Chris Caron <lead2gold@gmail.com>
 *
 * Since there are thousands of possible cron combinations. The best way to
 * test the function out is to redirect the output to a file (after building
 * the test plan).  Scan the file and make sure you agree with it's results and
 * then keep it as reference.
 *
 * Future checks should be redirected to a seperate/different file.  You can
 * then use diff or meld to view the differences.  In a perfect world there
 * won't be any! :)
 *
 * One thing that is guarenteed about a cron, is that the time generated should
 * never be equal or less then the 'start' time.  This one thing 'IS' checked
 * using this tool
*/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <iostream>
#include <stdexcept>
#include <stdlib.h>
#include <time.h>
#include <limits>
#include <string>
#include "Date.h"
using namespace std;
struct plan {
    int start_time[6];
    int finish_time[6];
    char cron[7][16];
    /* in seconds; this is how far in time to advance prior to the next check.
     * Once the finish_time is reached then the test is finished */
    int increment;
};

/*
 * Test plans are as easy as pie;  just add a time range you want to test
 * below:
 */
plan test_plan[] =
{
/*      Start Time             Finish Time                Cron to Test             Increment */

   /*
    * Test a recent crash we had on Mar 5th, 2012 around 18:30Z; the command it died on was:
    * dateblock -n 30 -o 13 -d 1-14 -m3,11;
    */
   { {2012, 3, 1, 13, 30, 0}, {2012, 3, 6, 0, 0, 0}, {"*", "30", "13", "1-14", "3,11", "*", "*"}, 1800},
   /* Test if the minute switch input matches the current time already */
   { {2000, 2, 28, 0, 0, 0}, {2000, 2, 29, 0, 0, 0}, {"*", "0", "*", "*", "*", "*", "*"}, 86400},
   /* Test if the minute switch input matches the current time already */
   { {2000, 2, 28, 0, 0, 0}, {2000, 2, 29, 0, 0, 0}, {"0", "0", "*", "*", "*", "*", "*"}, 86400},
   /* Test every second of every day for 1 day on a leap year */
   { {2000, 2, 28, 0, 0, 0}, {2000, 2, 29, 0, 0, 0}, {"1", "*", "*", "*", "*", "*", "*"}, 1},
   /* Test Day of Week (Friday) where Seconds are always set to 3 and Hour
    * always 23
    */
   { {2011, 3, 1, 23, 15, 2 }, {2012, 1, 1, 0, 0, 0}, {"3", "*", "23", "*", "*", "5", "*"}, 86400},
   /* Test where Seconds are divisible by 5, minutes divisible by 10 and hours
    * divisible by 10; run test for 1 day (small increments) */
   { {2011, 1, 1, 0, 0, 0 }, {2011, 1, 2, 0, 0, 0}, {"/5", "/10", "/10", "*", "*", "*", "*"}, 60},
   /* Test Blocking every second for 1 full day (Feb 29th) */
   { {2004, 2, 29, 0, 0, 0 }, {2004, 3, 1 , 0, 0, 0}, {"/1", "*", "*", "*", "*", "*", "*"}, 1},
   /* Test Blocking every minute for 1 full day (Feb 29th) */
   { {2004, 2, 29, 0, 0, 0 }, {2004, 3, 1 , 0, 0, 0}, {"*", "/1", "*", "*", "*", "*", "*"}, 40},
   /* Test Blocking every minute for 1 full day  (except force seconds to 0)
    * (Feb 29th) */
   { {2004, 2, 29, 0, 0, 0 }, {2004, 3, 1 , 0, 0, 0}, {"0", "/1", "*", "*", "*", "*", "*"}, 40},
   /* Test Blocking every hour for 1 full day (Feb 29th) */
   { {2004, 2, 29, 0, 0, 0 }, {2004, 3, 1 , 0, 0, 0}, {"*", "*", "/1", "*", "*", "*", "*"}, 40},
   /* Test Blocking every hour for 1 full day  (except force minutes and
    * seconds to 0) (Feb 29th) */
   { {2004, 2, 29, 0, 0, 0 }, {2004, 3, 1 , 0, 0, 0}, {"0", "0", "/1", "*", "*", "*", "*"}, 40},
   /* Test Drift Times */
   { {2034, 8, 11 , 0, 0, 0 }, {2034, 8, 12 , 0, 0, 0}, {"*", "/10", "*", "*", "*", "*", "480"}, 60},
   { {2014, 2, 11 , 1, 0, 0 }, {2014, 2, 14 , 0, 0, 0}, {"*", "*", "/2", "*", "*", "*", "45"}, 60}
};

// The computer's own timezone, through the Date::TimeZone hook.  Every
// answer must match the regular (mktime() based) search.
class SystemZone : public Date::TimeZone
{
   public:
     bool ToLocal(time_t t, struct tm& tmOut) const
     {
        return localtime_r(&t, &tmOut) != NULL;
     }
};

// A timezone a fixed number of seconds ahead of UTC (no daylight saving)
class FixedZone : public Date::TimeZone
{
   public:
     FixedZone(long offset) : m_offset(offset) {}
     bool ToLocal(time_t t, struct tm& tmOut) const
     {
        time_t shifted = t + m_offset;
        return gmtime_r(&shifted, &tmOut) != NULL;
     }
   private:
     long m_offset;
};

/*
 * Smaller one-off checks; returns 0 if they all pass
 */
static int Fail(const char *what)
{
   cerr << "FATAL: " << what << endl;
   return 1;
}

// Works out 'cron' from the given start time and compares the result
//  - isISC: read 'cron' as a regular crontab entry
//  - expect: "YYYY-MM-DD HH:MM:SS", or "" if no match should be found
static bool Expect(const char *cron, bool isISC, int year, int mon, int day,
                   int hour, int min, int sec, const char *expect)
{
   string got;
   try
   {
      got = Date(year, mon, day, hour, min, sec).Cron(cron, isISC).Str();
   }
   catch (const runtime_error&)
   {
   }
   if (got == expect)
      return true;
   cerr << "FATAL: '" << cron << "'" << (isISC ? " (isc)" : "")
        << " gave '" << got << "', expected '" << expect << "'" << endl;
   return false;
}

// True if working out 'cron' throws std::invalid_argument
static bool Refused(const Date& start, const char *cron, bool isISC)
{
   try
   {
      start.Cron(cron, isISC);
   }
   catch (const invalid_argument&)
   {
      return true;
   }
   cerr << "FATAL: '" << cron << "'" << (isISC ? " (isc)" : "")
        << " was accepted" << endl;
   return false;
}

// Works out 'cron' both ways around each clock change in 'tz' and
// compares them; returns false (after printing why) if they differ
static bool SameInZone(const char *tz, int year)
{
   setenv("TZ", tz, 1);
   tzset();
   const char *crons[] = {
      "0 0 0", "0 30 2", "0 0 /1", "0 /15 1", "0 0 0 * * 0", "0 0 0 1",
      "0 30 0 * * *", "0 0 1 8-14 3 0 3600,7200", "/20 59 23",
      "0 0 2 * * * 1800"};
   const char *isc[] = {"30 2 * * *", "0 0 * * *", "*/15 0-3 * * 0"};
   SystemZone zone;
   for (int month = 1; month <= 12; month++)
   {
      // Every 50 minutes for a whole month (lands on every minute-of-hour
      // offset in steps of 10)
      Date start(year, month, 1, 0, 0, 0);
      Date finish(start);
      finish.AddMonth(1);
      for (; start < finish; start.AddSec(3000))
      {
         for (unsigned c = 0; c < sizeof(crons)/sizeof(crons[0]); c++)
         {
            if (Date::CronAfter(start.Time(), crons[c], false, zone) !=
                  start.Cron(crons[c]).Time())
            {
               cerr << "FATAL: " << tz << " '" << crons[c] << "' from "
                    << start.Str("%F %T %Z") << " differs" << endl;
               return false;
            }
         }
         for (unsigned c = 0; c < sizeof(isc)/sizeof(isc[0]); c++)
         {
            if (Date::CronAfter(start.Time(), isc[c], true, zone) !=
                  start.Cron(isc[c], true).Time())
            {
               cerr << "FATAL: " << tz << " '" << isc[c] << "' (isc) from "
                    << start.Str("%F %T %Z") << " differs" << endl;
               return false;
            }
         }
      }
   }
   return true;
}

static int CheckTimeZones()
{
   // A fixed +09:00 zone: 09:00 there is 00:00 UTC
   FixedZone tokyo(9 * 3600);
   time_t ref = 1735689600;  // 2025-01-01 00:00:00 UTC (09:00 in +09:00)
   if (Date::CronAfter(ref, "0 0 9", false, tokyo) != ref + 86400 ||
       Date::CronAfter(ref, "0 0 10", false, tokyo) != ref + 3600 ||
       Date::CronAfter(ref, "0 9 * * *", true, tokyo) != ref + 86400)
      return Fail("Cron in a fixed +09:00 zone is wrong");

   // Days, weekdays and leap days follow the zone's calendar, not UTC's:
   // 2028-02-29 00:00 in +14:00 is still Feb 28th in UTC
   FixedZone kiribati(14 * 3600);
   if (Date::CronAfter(ref, "0 0 0 29 2", false, kiribati) !=
         (time_t)1835395200 - 14 * 3600 ||
       Date::CronAfter(ref, "0 0 0 * * 1", false, kiribati) !=
         (time_t)1736121600 - 14 * 3600)
      return Fail("Days in a +14:00 zone are wrong");

   // Bad syntax still throws
   bool refused = false;
   try
   {
      Date::CronAfter(ref, "* 99", false, tokyo);
   }
   catch (const invalid_argument&)
   {
      refused = true;
   }
   if (!refused)
      return Fail("CronAfter() with bad syntax did not throw");

   // The hook agrees with the regular search through every clock change
   const char *saved = getenv("TZ");
   string savedTZ = saved ? saved : "";
   int rc = 0;
   setenv("TZ", "America/New_York", 1);
   tzset();
   if (Date(2025, 7, 1, 12, 0, 0).Str("%Z") != "EDT")
      cout << "SKIP: timezone hook checks" << endl;
   else if (!SameInZone("America/New_York", 2025) ||
            !SameInZone("Australia/Lord_Howe", 2025) ||
            !SameInZone("America/Santiago", 2025))
      rc = Fail("The timezone hook disagrees around a clock change");

   if (saved)
      setenv("TZ", savedTZ.c_str(), 1);
   else
      unsetenv("TZ");
   tzset();
   return rc;
}

static int CheckLeapYears()
{
   // The next Feb 29th, including across 2100 (not a leap year)
   if (!Expect("0 0 0 29 2", false, 2025, 6, 1, 0, 0, 0, "2028-02-29 00:00:00") ||
       !Expect("0 0 0 29 2", false, 2097, 3, 1, 0, 0, 0, "2104-02-29 00:00:00") ||
       !Expect("0 0 0 29 2", false, 2399, 3, 1, 0, 0, 0, "2400-02-29 00:00:00"))
      return Fail("Feb 29th was not found");

   // Feb 29th on a given weekday can be decades away
   if (!Expect("0 0 0 29 2 1", false, 2025, 6, 1, 0, 0, 0, "2044-02-29 00:00:00") ||
       !Expect("0 0 0 29 2 1", false, 2081, 3, 1, 0, 0, 0, "2112-02-29 00:00:00") ||
       !Expect("0 0 0 29 2 2", false, 2096, 3, 1, 0, 0, 0, "2124-02-29 00:00:00"))
      return Fail("Feb 29th on a weekday was not found");

   // The last day of February in leap and regular years
   if (!Expect("0 0 0 28,29 2", false, 2024, 2, 28, 12, 0, 0, "2024-02-29 00:00:00") ||
       !Expect("0 0 0 28,29 2", false, 2023, 2, 28, 12, 0, 0, "2024-02-28 00:00:00"))
      return Fail("Feb 28th/29th was not found");

   // 1900 was not a leap year, 2000 was
   if (!Expect("0 0 0 29 2", false, 1896, 3, 1, 0, 0, 0, "1904-02-29 00:00:00") ||
       !Expect("0 0 0 29 2", false, 1999, 3, 1, 0, 0, 0, "2000-02-29 00:00:00"))
      return Fail("The century leap year rule is wrong");

   // Dateblock format: Feb 29th AND one of the listed weekdays
   if (!Expect("0 0 0 29 2 0-6", false, 2025, 3, 1, 0, 0, 0, "2028-02-29 00:00:00") ||
       !Expect("0 0 0 29 2 1,3,5", false, 2025, 3, 1, 0, 0, 0, "2036-02-29 00:00:00") ||
       !Expect("0 0 0 1-7 * 1", false, 2025, 6, 3, 0, 0, 0, "2025-07-07 00:00:00"))
      return Fail("Leap day AND weekday is wrong");

   // crontab: Feb 29th OR a listed weekday in February; a '*' day of the
   // week means only Feb 29th counts
   if (!Expect("0 0 29 2 1", true, 2025, 3, 1, 0, 0, 0, "2026-02-02 00:00:00") ||
       !Expect("0 0 29 2 0,6", true, 2024, 2, 26, 0, 0, 0, "2024-02-29 00:00:00") ||
       !Expect("0 0 29 2 *", true, 2025, 3, 1, 0, 0, 0, "2028-02-29 00:00:00") ||
       !Expect("0 0 29,30,31 2 *", true, 2025, 3, 1, 0, 0, 0, "2028-02-29 00:00:00") ||
       !Expect("0 0 1-7 * 1", true, 2025, 6, 3, 0, 0, 0, "2025-06-04 00:00:00"))
      return Fail("Leap day OR weekday (isc) is wrong");

   // An impossible day still matches through its weekday with crontab
   if (!Expect("0 0 30 2 1", true, 2025, 3, 1, 0, 0, 0, "2026-02-02 00:00:00"))
      return Fail("Feb 30th OR Monday (isc) should match a Monday");

   // Days that never happen
   if (!Expect("0 0 0 30 2", false, 2024, 1, 1, 0, 0, 0, "") ||
       !Expect("0 0 30 2 *", true, 2024, 1, 1, 0, 0, 0, "") ||
       !Expect("0 0 0 31 4,6,9,11", false, 2024, 1, 1, 0, 0, 0, ""))
      return Fail("An impossible day was found");
   return 0;
}

static int CheckDayMatching()
{
   // Dateblock format: the day of month AND the day of week must match
   if (!Expect("0 0 0 13 * 5", false, 2025, 6, 1, 0, 0, 0, "2025-06-13 00:00:00") ||
       !Expect("0 0 0 1 * 1", false, 2025, 6, 3, 0, 0, 0, "2025-09-01 00:00:00") ||
       !Expect("0 0 0 1,15 * 0,6", false, 2025, 1, 1, 0, 0, 0, "2025-02-01 00:00:00"))
      return Fail("Day of month AND day of week is wrong");

   // crontab: when both are restricted, either one may match
   if (!Expect("0 0 1 * 1", true, 2025, 6, 3, 0, 0, 0, "2025-06-09 00:00:00") ||
       !Expect("0 0 13 * 5", true, 2025, 6, 1, 0, 0, 0, "2025-06-06 00:00:00") ||
       !Expect("0 0 1,15 * 0,6", true, 2025, 1, 2, 0, 0, 0, "2025-01-04 00:00:00"))
      return Fail("Day of month OR day of week (isc) is wrong");

   // crontab: a day field starting with '*' doesn't count as restricted
   if (!Expect("0 0 13 * *", true, 2025, 6, 1, 0, 0, 0, "2025-06-13 00:00:00") ||
       !Expect("0 0 * * 5", true, 2025, 6, 1, 0, 0, 0, "2025-06-06 00:00:00") ||
       !Expect("0 0 */2 * 1", true, 2025, 6, 3, 0, 0, 0, "2025-06-09 00:00:00"))
      return Fail("A '*' day field (isc) is wrong");
   return 0;
}

static int CheckUnsetFields()
{
   // Fields smaller than the smallest one given reset to their lowest value
   if (!Expect("* 5", false, 2025, 1, 15, 3, 10, 0, "2025-01-15 04:05:00") ||
       !Expect("* * 3", false, 2025, 1, 15, 3, 10, 0, "2025-01-16 03:00:00") ||
       !Expect("* * * * 3", false, 2025, 1, 15, 0, 0, 0, "2025-03-01 00:00:00") ||
       !Expect("* * * * * 1", false, 2025, 1, 15, 0, 0, 0, "2025-01-20 00:00:00"))
      return Fail("Unset smaller fields were not reset");

   // Fields between the given ones match anything
   if (!Expect("/5 * 3,4", false, 2025, 1, 15, 3, 10, 0, "2025-01-15 03:10:05") ||
       !Expect("0 * * 15", false, 2025, 1, 15, 3, 10, 0, "2025-01-15 03:11:00"))
      return Fail("Unset fields in between should match anything");

   // crontab entries always run at second 0, every other field as written
   if (!Expect("* 3 * * *", true, 2025, 6, 1, 3, 10, 30, "2025-06-01 03:11:00") ||
       !Expect("0 0 * 3 *", true, 2025, 1, 15, 0, 0, 0, "2025-03-01 00:00:00"))
      return Fail("crontab fields were not read as written");
   return 0;
}

static int CheckSteps()
{
   // Every z'th value of a range (x-y/z), in both formats
   if (!Expect("0 1-10/3", false, 2025, 1, 15, 3, 2, 0, "2025-01-15 03:04:00") ||
       !Expect("0 10-1/3", false, 2025, 1, 15, 3, 8, 0, "2025-01-15 03:10:00") ||
       !Expect("0 1-10/3", false, 2025, 1, 15, 3, 10, 0, "2025-01-15 04:01:00") ||
       !Expect("1-10/3 * * * *", true, 2025, 1, 15, 3, 2, 0, "2025-01-15 03:04:00"))
      return Fail("Range steps (x-y/z) are wrong");

   // Dateblock format: '*/x' is every value divisible by x
   if (!Expect("0 0 0 */5", false, 2025, 6, 2, 0, 0, 0, "2025-06-05 00:00:00") ||
       !Expect("0 0 0 1 */3", false, 2025, 2, 1, 0, 0, 0, "2025-03-01 00:00:00"))
      return Fail("'*/x' (dateblock) should count from 0");

   // crontab: '*/x' counts from the field's lowest value (days 1, 6, 11...
   // and months 1, 4, 7, 10)
   if (!Expect("0 0 */5 * *", true, 2025, 6, 2, 0, 0, 0, "2025-06-06 00:00:00") ||
       !Expect("0 0 1 */3 *", true, 2025, 2, 1, 0, 0, 0, "2025-04-01 00:00:00") ||
       !Expect("*/20 * * * *", true, 2025, 1, 15, 3, 2, 0, "2025-01-15 03:20:00"))
      return Fail("'*/x' (isc) should count from the lowest value");

   // Steps that make no sense are refused
   if (Date::CronValid("1-10/0") || Date::CronValid("1-10/") ||
       Date::CronValid("1-10/3/2") || Date::CronValid("1-10/61"))
      return Fail("A broken range step was accepted");
   return 0;
}

static int CheckHardening()
{
   // Lists that would never end or would use up the memory are refused
   if (Date::CronValid("* * * * * * 0-2147483647") ||
       Date::CronValid("* * * * * * /1") ||
       !Date::CronValid("* * * * * * 0-9999"))
      return Fail("Huge drift lists should be refused");

   // Values at the very top of the range don't overflow
   if (!Expect("0 * * * * * 2147483647-2147483647", false,
               2025, 1, 15, 0, 0, 0, "2025-01-15 00:00:07") ||
       !Expect("0 * * * * * /2147483647", false,
               2025, 1, 15, 0, 0, 0, "2025-01-15 00:00:07"))
      return Fail("The largest drift was not handled");

   // Broken entries are refused instead of quietly matching anything
   const char *broken[] = {"5-", "5-,7", "/", "*/", "-5", "5-7-9", "/5/2",
                           "5/2", "**", "5*", "99", "1-99"};
   for (unsigned i = 0; i < sizeof(broken)/sizeof(broken[0]); i++)
   {
      if (Date::CronValid(broken[i]))
      {
         cerr << "FATAL: '" << broken[i] << "' was accepted" << endl;
         return 1;
      }
   }

   // Empty list entries and a '+' with no drift are refused
   const char *empty[] = {",", "5,", ",5", "5,,6", "* 5 +", "+"};
   for (unsigned i = 0; i < sizeof(empty)/sizeof(empty[0]); i++)
   {
      if (Date::CronValid(empty[i]))
      {
         cerr << "FATAL: '" << empty[i] << "' was accepted" << endl;
         return 1;
      }
   }

   // Adding a huge number of months or years is instant and never
   // overflows the year
   clock_t added = clock();
   Date dAdd(2025, 1, 31, 0, 0, 0);
   if (!dAdd.AddMonth(-25) || dAdd.Str() != "2022-12-31 00:00:00")
      return Fail("AddMonth(-25) is wrong");
   if (!dAdd.AddMonth(14) || dAdd.Str() != "2024-02-29 00:00:00")
      return Fail("AddMonth(14) should stop at the end of February");
   Date dHuge(2025, 1, 1, 0, 0, 0);
   dHuge.AddMonth(2147483647);
   dHuge.AddMonth(-2147483647 - 1);
   if (dHuge.AddYear(2147483647))
      return Fail("AddYear() should refuse a year that doesn't fit");
   if ((clock() - added) / CLOCKS_PER_SEC > 1)
      return Fail("AddMonth() with a huge value took too long");

   // '*' mixed with other entries ('*,5') is unclear and refused; steps
   // such as '*/5,3' are fine
   const char *mixed[] = {"0 *,5", "0 5,*", "0 0 *,13 * 1", "*,*", "1-5,*"};
   for (unsigned i = 0; i < sizeof(mixed)/sizeof(mixed[0]); i++)
   {
      if (Date::CronValid(mixed[i]) || Date::CronValid(mixed[i], true) ||
          !Refused(Date(2025, 6, 1, 0, 0, 0), mixed[i], false))
      {
         cerr << "FATAL: '" << mixed[i] << "' was accepted" << endl;
         return 1;
      }
   }
   if (!Date::CronValid("0 */5,3") ||
       !Expect("0 */20,3", false, 2025, 6, 1, 0, 0, 30, "2025-06-01 00:03:00"))
      return Fail("A step next to a value should still work");

   // More comma entries than a field can hold are refused, even when
   // they repeat the same value
   string many = "0";
   for (int i = 0; i < 10000; i++)
      many += ",0";
   if (Date::CronValid("* " + many) || !Date::CronValid("* 0,0,0"))
      return Fail("A huge repeated list was accepted");

   // Moments that can't be shown as a local date are refused, not left
   // holding garbage
   bool outOfRange = false;
   try
   {
      Date dMax(numeric_limits<time_t>::max());
   }
   catch (const out_of_range&)
   {
      outOfRange = true;
   }
   if (!outOfRange)
      return Fail("Date(time_t max) should throw out_of_range");

   // The largest year that fits works; one more is refused and the date
   // stays as it was
   Date dKeep(2025, 1, 1, 0, 0, 0);
   if (dKeep.AddYear(2147483000) || dKeep.Str() != "2025-01-01 00:00:00")
      return Fail("AddYear() past the largest year was accepted");
   if (!dKeep.SetYear(2147483647) || dKeep.Year() != 2147483647 ||
       dKeep.AddYear(1) || dKeep.AddMonth(12) || dKeep.Year() != 2147483647)
      return Fail("The largest year was not handled");

   // Every value of every field is still quick to work out
   clock_t started = clock();
   if (!Expect("0-59 0-59 0-23 1-31 1-12 0-6", false,
               2025, 1, 15, 0, 0, 0, "2025-01-15 00:00:01") ||
       !Expect("/1 /1 /1 * * * 0-999", false,
               2025, 1, 15, 0, 0, 0, "2025-01-15 00:00:01"))
      return Fail("Full field lists were not worked out");
   if ((clock() - started) / CLOCKS_PER_SEC > 2)
      return Fail("Full field lists took too long");
   return 0;
}

static int CheckDaylightSaving()
{
   // New York: clocks go forward 2025-03-09 02:00, back 2025-11-02 02:00
   const char *saved = getenv("TZ");
   string savedTZ = saved ? saved : "";
   setenv("TZ", "America/New_York", 1);
   tzset();

   int rc = 0;
   if (Date(2025, 7, 1, 12, 0, 0).Str("%Z") != "EDT")
   {
      // No time zone data on this machine
      cout << "SKIP: daylight saving checks" << endl;
   }
   else if (Date(2025, 3, 9, 0, 0, 0).Str() != "2025-03-09 00:00:00")
      rc = Fail("Setting a time on a clock change day is off");
   else if (!Expect("0 30 2", false, 2025, 3, 9, 0, 0, 0, "2025-03-10 02:30:00"))
      rc = Fail("2:30 doesn't exist on 2025-03-09 and should be skipped");
   else if (!Expect("0 0 /1", false, 2025, 3, 9, 1, 30, 0, "2025-03-09 03:00:00"))
      rc = Fail("The hour after the clocks go forward was missed");
   else if (!Expect("0 0 1 8-14 3 0 3600,7200", false, 2025, 1, 1, 0, 0, 0,
                    "2025-03-09 03:00:00"))
      rc = Fail("A drift across the clocks going forward is wrong");
   else
   {
      // Changing the month or year keeps the clock time across daylight
      // saving (12:00 stays 12:00)
      Date dMonth(2025, 7, 15, 12, 0, 0);
      Date dSet(2025, 7, 15, 12, 0, 0);
      Date dYear(2025, 1, 15, 12, 0, 0);
      if (!dMonth.AddMonth(5) || dMonth.Str() != "2025-12-15 12:00:00" ||
          !dSet.SetMonth(12) || dSet.Str() != "2025-12-15 12:00:00" ||
          !dYear.SetMonth(7) || dYear.Str() != "2025-07-15 12:00:00")
         rc = Fail("A month change moved the clock time");
   }
   if (rc == 0 && Date(2025, 7, 1, 12, 0, 0).Str("%Z") == "EDT")
   {
      // 01:00-01:59 happens twice; the second 01:00 is 10 minutes after
      // the first 01:50
      Date start(2025, 11, 2, 1, 50, 0);
      Date next = start.Cron("0 /15 1");
      if (start.Str("%Z") == "EDT" && next.Time() - start.Time() != 600)
         rc = Fail("The repeated hour when the clocks go back was missed");
   }

   if (saved)
      setenv("TZ", savedTZ.c_str(), 1);
   else
      unsetenv("TZ");
   tzset();
   return rc;
}

// The earliest of each drift worked out on its own
static string EachDrift(const Date& start, const string& cron,
                        const char *drifts[], unsigned count)
{
   string best;
   for (unsigned i = 0; i < count; i++)
   {
      Date when = start.Cron(cron + " " + drifts[i]);
      string got = when.Str("%Y%m%d%H%M%S");
      if (best.empty() || got < best)
         best = got;
   }
   return best;
}

static int CheckDriftLists()
{
   // A drift list gives the same answer as trying each drift by itself
   const char *crons[] = {
      "/1 * * * * *", "0 /10 * * * *", "30 15 */2 * * *",
      "0 0 0 29 2 *", "0 0 0 29 2 1", "0 0 12 13 * 5", "0 0 0 1-7 * 1",
      "/7 /11 3,15 * 2,8 *"};
   const char *drifts[] = {"0", "1", "59", "60", "61", "3599", "3600",
                           "86400", "90061", "2678400", "31622400",
                           "883612800", "2147483647"};
   const unsigned count = sizeof(drifts)/sizeof(drifts[0]);
   const int starts[][6] = {
      {2025, 1, 1, 0, 0, 0}, {2024, 2, 29, 23, 59, 59},
      {2028, 2, 28, 12, 0, 0}, {2025, 6, 13, 0, 0, 0}};

   for (unsigned c = 0; c < sizeof(crons)/sizeof(crons[0]); c++)
   {
      for (unsigned t = 0; t < sizeof(starts)/sizeof(starts[0]); t++)
      {
         Date start(starts[t][0], starts[t][1], starts[t][2],
                    starts[t][3], starts[t][4], starts[t][5]);

         // Every drift, then a few smaller groups of them
         for (unsigned first = 0; first < count; first += 4)
         {
            string list;
            for (unsigned i = first; i < count; i++)
               list += (list.empty() ? "" : ",") + string(drifts[i]);

            string got = start.Cron(string(crons[c]) + " " + list)
                              .Str("%Y%m%d%H%M%S");
            string expect = EachDrift(start, crons[c], drifts + first,
                                      count - first);
            if (got != expect)
            {
               cerr << "FATAL: '" << crons[c] << " " << list << "' from "
                    << start.Str() << " gave " << got << ", expected "
                    << expect << endl;
               return 1;
            }
         }
      }
   }

   // Many drifts on a schedule that rarely matches stay quick (each drift
   // used to search on its own)
   clock_t started = clock();
   if (!Expect("0 0 0 29 2 1 0-9999", false, 2025, 6, 1, 0, 0, 0,
               "2044-02-29 00:00:00") ||
       !Expect("59 59 23 29 2 1 0-9999", false, 2081, 3, 1, 0, 0, 0,
               "2112-02-29 23:59:59"))
      return Fail("A long drift list on Feb 29th was wrong");
   if ((clock() - started) / CLOCKS_PER_SEC > 1)
      return Fail("A long drift list took too long");
   return 0;
}

static int CheckExtras()
{
   // Setting a date while 'today' is the 31st must not spill into the
   // next month
   Date dObj(2001, 1, 31, 12, 0, 0);
   dObj.Set(2000, 4, 3, 1, 2, 3);
   if (dObj.Str() != "2000-04-03 01:02:03")
      return Fail("Set() rolled over into the wrong month");

   // Month lengths (including leap years)
   if (Date(2000, 2, 1).MaxDOMsThisMonth() != 29)
      return Fail("Feb 2000 should have 29 days");
   if (Date(1900, 2, 1).MaxDOMsThisMonth() != 28)
      return Fail("Feb 1900 should have 28 days");
   if (Date(2001, 12, 1).MaxDOMsNextMonth() != 31)
      return Fail("Jan 2002 should have 31 days");
   if (Date(2001, 1, 1).MaxDOMsPrevMonth() != 31)
      return Fail("Dec 2000 should have 31 days");

   // Formatting goes through strftime()
   Date dFmt(2026, 10, 24, 14, 5, 9);
   if (dFmt.Str("%I %p %j %h %x %X") != "02 PM 297 Oct 10/24/26 14:05:09")
      return Fail("Str() formatting is wrong");

   // A cron that can never match reports an error instead of exiting
   bool thrown = false;
   try
   {
      dFmt.Cron("0 0 0 31 2 *");
   }
   catch (const runtime_error&)
   {
      thrown = true;
   }
   if (!thrown)
      return Fail("Feb 31st should not be found");

   // ...but an impossible combination is skipped when another one works
   if (dFmt.Cron("0 0 0 30,31 2,3 *").Str() != "2027-03-30 00:00:00")
      return Fail("Mar 30th should have been found");

   // Bad syntax throws instead of quietly returning the start time
   if (!Refused(dFmt, "* 99", false) || !Refused(dFmt, "* 5 +20 1", false) ||
       !Refused(dFmt, "* * * * * * * *", false) ||
       !Refused(dFmt, "5,,6", false) || !Refused(dFmt, "99 * * * *", true) ||
       !Refused(dFmt, "* * * * * *", true))
      return Fail("Cron() with bad syntax did not throw");
   bool refused = false;
   try
   {
      dFmt.Cron("0", "99", "*");
   }
   catch (const invalid_argument&)
   {
      refused = true;
   }
   if (!refused)
      return Fail("Cron() with a bad minute field did not throw");

   // BlockUntil() also takes a struct tm (local time)
   struct tm past = Date(2000, 1, 1, 0, 0, 0).Tm();
   if (Date::BlockUntil(past) != 0 ||
       Date::BlockUntil(Date(2000, 1, 1, 0, 0, 0).Tm()) != 0)
      return Fail("BlockUntil() a struct tm in the past should not wait");

   Date soon;
   soon.AddSec(1);
   struct tm next = soon.Tm();
   next.tm_isdst = -1;
   time_t blockStart = time(NULL);
   while (Date::BlockUntil(next) > 0);
   if (Date().Time() < soon.Time() || time(NULL) - blockStart > 2)
      return Fail("BlockUntil() a struct tm woke up at the wrong time");

   struct tm huge = past;
   huge.tm_year = 2147483647 - 1900;
   huge.tm_mon = 12;
   bool tooBig = false;
   try
   {
      Date::BlockUntil(huge);
   }
   catch (const out_of_range&)
   {
      tooBig = true;
   }
   if (!tooBig)
      return Fail("BlockUntil() a struct tm past the largest year should throw");

   // Inline drift; nothing may follow it
   if (!Date::CronValid("* 5 +20") || Date::CronValid("* 5 +20 1"))
      return Fail("Inline drift validation is wrong");
   if (Date(2026, 10, 24, 14, 5, 9).Cron("* 5 +20").Str() !=
         "2026-10-24 14:05:20")
      return Fail("Inline drift calculation is wrong");

   return 0;
}

int main(int argc, char **argv)
{
   /* Create Reference Time */
   Date dObjStart;
   Date dObjFinish;
   for (unsigned i=0; i < sizeof(test_plan)/sizeof(test_plan[0]); i++)
   {
      // Start Time
      dObjStart.Set(test_plan[i].start_time[0],
                    test_plan[i].start_time[1],
                    test_plan[i].start_time[2],
                    test_plan[i].start_time[3],
                    test_plan[i].start_time[4],
                    test_plan[i].start_time[5]);
      // Finish Time
      dObjFinish.Set(test_plan[i].finish_time[0],
                     test_plan[i].finish_time[1],
                     test_plan[i].finish_time[2],
                     test_plan[i].finish_time[3],
                     test_plan[i].finish_time[4],
                     test_plan[i].finish_time[5]);

      while ( dObjStart < dObjFinish)
      {
         if(!dObjStart.CronValid(test_plan[i].cron[0],
                      test_plan[i].cron[1],
                      test_plan[i].cron[2],
                      test_plan[i].cron[3],
                      test_plan[i].cron[4],
                      test_plan[i].cron[5],
                      test_plan[i].cron[6]))
         {
            // Houston! We have a serious problem
            cerr << "FATAL: Cron "
                 << test_plan[i].cron[0] << " "
                 << test_plan[i].cron[1] << " "
                 << test_plan[i].cron[2] << " "
                 << test_plan[i].cron[3] << " "
                 << test_plan[i].cron[4] << " "
                 << test_plan[i].cron[5] << " "
                 << test_plan[i].cron[6] << " is invalid." << endl;
            return 1;
         }
         Date dObjRef(dObjStart.Cron(test_plan[i].cron[0],
                      test_plan[i].cron[1],
                      test_plan[i].cron[2],
                      test_plan[i].cron[3],
                      test_plan[i].cron[4],
                      test_plan[i].cron[5],
                      test_plan[i].cron[6]));

         cout << dObjStart.Str("%Y/%m/%d %H:%M:%S (%a)") << " "
              << test_plan[i].cron[0] << " "
              << test_plan[i].cron[1] << " "
              << test_plan[i].cron[2] << " "
              << test_plan[i].cron[3] << " "
              << test_plan[i].cron[4] << " "
              << test_plan[i].cron[5] << " "
              << test_plan[i].cron[6] << ": "
              << dObjRef.Str("%Y/%m/%d %H:%M:%S (%a)") << endl;
         // The timezone hook gives the same answer
         string joined = string(test_plan[i].cron[0]);
         for (int f = 1; f < 7; f++)
            joined += string(" ") + test_plan[i].cron[f];
         if (Date::CronAfter(dObjStart.Time(), joined, false, SystemZone()) !=
               dObjRef.Time())
         {
            cerr << "FATAL: the timezone hook disagrees on '" << joined
                 << "' from " << dObjStart.Str() << endl;
            return 1;
         }

         if(dObjRef <= dObjStart)
         {
            // Houston! We have a serious problem
            cerr << "FATAL: " << dObjStart.Str("%Y/%m/%d %H:%M:%S (%a)") << " "
                 << test_plan[i].cron[0] << " "
                 << test_plan[i].cron[1] << " "
                 << test_plan[i].cron[2] << " "
                 << test_plan[i].cron[3] << " "
                 << test_plan[i].cron[4] << " "
                 << test_plan[i].cron[5] << " "
                 << test_plan[i].cron[6] << " is greater than or equal to "
                 << dObjRef.Str("%Y/%m/%d %H:%M:%S (%a)") << endl;
            return 1;
         }
         dObjStart.AddSec(test_plan[i].increment);
      }
   }

   if (CheckExtras() || CheckLeapYears() || CheckDayMatching() ||
       CheckUnsetFields() || CheckSteps() || CheckHardening() ||
       CheckDriftLists() || CheckDaylightSaving() || CheckTimeZones())
      return 1;
   return 0;
}
