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
#include <time.h>
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

/*
 * Smaller one-off checks; returns 0 if they all pass
 */
static int Fail(const char *what)
{
   cerr << "FATAL: " << what << endl;
   return 1;
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

   return CheckExtras();
}
