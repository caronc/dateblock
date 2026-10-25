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

#include <iostream>
#include <sstream>
#include <string>
#include <stdexcept>
#include <boost/program_options.hpp>
#include "Date.h"
#include "dateblock.h"

namespace po = boost::program_options;
using namespace std;

//------------------------------------------------
//
//  Function: ProgramDetails
//
//------------------------------------------------
void ProgramDetails()
{
   cerr << "Dateblock v" << PACKAGE_VERSION  << " "
        << "Copyright (C) 2013-2026 " << AUTHOR << endl << endl
        << "Dateblock comes with ABSOLUTELY NO WARRANTY" << endl
        << "This is free software, and you are welcome to redistribute it" << endl
        << "under certain conditions; see man pages for details." << endl << endl

        << "This tool works similar to how sleep works except instead." << endl
        << "of delaying the system 'for' a specific time, it delays the" << endl
        << "system 'until' a specific time. Dateblock could be compared" << endl
        << "to how a cron works with its crontab definitions." << endl
        << "Dateblock will block until a specified criteria is met." << endl
        << "Syntax: "
        << "dateblock [options]" << endl << endl;
}

void ProgramExamples()
{
   cerr << "A variety of syntax is accepted by this tool such as:" << endl

     << "  x               (Value)     where 'x' is represented"
     << " numerically." << endl
     << "  */x (or /x)     (Modulus)   where 'x' is represented"
     << " numerically." << endl

     << "  x-y (or y-x)    (Range)     where 'x' and 'y' are are"
     << " represented numerically." << endl

     << "  x,y             (Separator) where 'x' and 'y' are are"
     << " represented numerically." << endl << endl;
   cerr << " Note: All variations of the syntax mentioned above can be mixed"
     << " if separated" << endl << "       using the 'comma' (Separator)"
     << " operator.  ie: */a,b,c-d,/e is valid." << endl
     << "       However: x-y-z is not valid, nor is /x/y or /x-y. All"
     << " values must be" << endl << "       within the range of it's time type."
     << " Thus 400-4000 would never work since" << endl << "       no time"
     << " constraint even resides within that range."
     << endl << endl;

   cerr << " The cron (--cron|-c) switch allows one to specify the standard cron"
     <<  " formatting:" << endl;
   cerr
     << "     drift time ------------------------------------------+" << endl
     << "     day of week (0 - 6) (Sunday=0) -------------------+  |" << endl
     << "     month (1 - 12) --------------------------------+  |  |" << endl
     << "     day of month (1 - 31) ----------------------+  |  |  |" << endl
     << "     hour (0 - 23) ---------------------------+  |  |  |  |" << endl
     << "     min (0 - 59) -------------------------+  |  |  |  |  |" << endl
     << "     sec (0 - 59) -----------------------  |  |  |  |  |  |" << endl
     << "                                        |  |  |  |  |  |  |" << endl
     << "                                        -  -  -  -  -  -  -" << endl
     << "                                        *  *  *  *  *  *  *" << endl;
   cerr << "  Substitute a asterix (*) as a placeholder for arguments"
     << " you are not" << endl
     << "  interested in. Asterixes are automatically placed in strings missing all 6"
     << endl << "  arguments (separated by white space)." << endl << endl;
   cerr << "Drifts: " << endl
        << endl << "  Drifting is an option that allows you to adjust the calculated results by some"
        << endl << "   additional time.  Lets say you wanted the application to wake up on the 1st"
        << endl << "   minute of each 10 min interval (1, 11, 21, 31, 41, 51). Specifying the cron"
        << endl << "   (minute) '*/1' would not work. In fact the cron of '*/1' would unblock at"
        << endl << "   every minute. You could however achieve the previous example using a drift"
        << endl << "   value of '60' (seconds) and a cron (minute) entry of '*/10'." << endl << endl;
   cerr << "  Some things to consider about drifting:" << endl
     << "   - Drifting is always calculated 'after' a specified cron." << endl
     << "   - You can not specify a drifting value on it's own without a cron entry to go"
     << endl << "     with it."
     << endl << "   - A drift value is always specified in seconds."
     << endl << "   - You can not drift longer then the interval calculated by the cron itself."
     << endl << "     In the event this occurs, only the remainder (modulus) is kept; the rest is"
     << endl << "     is considered overflow and will simply be ignored.  Drifting behaves this"
     << endl << "     way to prevent missing a segment of time that would have otherwise been"
     << endl << "     calculated." << endl
     << endl << "     For example... if you specify a a cron of '*/10' (which would equate to"
     << endl << "     unblocking every 10 seconds), and specified a drift of of 11 (seconds), the"
     << endl << "     overflow would leave you with an actual drift of '1' (not 11)." << endl << endl;
   cerr << "Examples: " << endl;

   cerr << "$> dateblock -n /10" << endl
     << "    This would block until a minute divisible by 10 was reached."
     <<  "  Minutes" << endl << "     divisible by 10 are: 0,10,20,30,40"
     << " and 50."
     << endl << endl;

   cerr << "$> dateblock -c \"/5 * 3,4 * * *\"" << endl
     << "    This would block until a second is divisible by 5 was reached and"
     << " only on" << endl
     << "    hours 3 and 4." << endl
     << "    This can also be written as such: $> dateblock -s /5 -o 3,4"
     << endl << endl;

      cerr << "$> dateblock -c \"* 5\"" << endl
     << "    This would block until the 5 minute of every hour. This can also be written"
     << endl << "    as such:" << endl
     << "          1. $> dateblock -n 5" << endl
     << "          2. $> dateblock -c \"* 5 * * * *\""
     << endl << endl;

   cerr << "$> dateblock -o /5 -d 1-14,20" << endl
     << "    This would block until a hour divisible by 5 was reached on"
     << " the first half" << endl << "     of the month as well as the"
     << " 20th of the month) Hours divisible by 5 are: 0,"
     << endl <<  "     5,10,15 and 20."
     << endl << endl;
}

int main(int argc, char **argv)
{

   Date dObjStart;
   Date dObjFinish;

   string sSecOffset="*";
   string sMinOffset="*";
   string sHourOffset="*";
   string sDomOffset="*";
   string sMonthOffset="*";
   string sDowOffset="*";
   string sDriftOffset="*";
   string sCronStr="";

   // Declare the supported options.
   po::options_description poAllOptions("Allowed options");
   po::variables_map poVariablesMap;
   poAllOptions.add_options()
       ("help,h", "Show this help screen.")
       ("test,t", "Test (Do not block for the specified period)")
       ("verbose,v", "Verbose mode")
       ("isc,i", "ISC mode; this only works when paired with --cron (-c) "
        "switch.")
       ("second,s", po::value<string>(), "Second (0-59)")
       ("minute,n", po::value<string>(), "Minute (0-59)")
       ("hour,o", po::value<string>(), "Hour (0-23)")
       ("dom,d", po::value<string>(), "Day of Month (1-31)")
       ("month,m", po::value<string>(), "Month (1-12) {Jan=1,...,Dec=12}")
       ("dow,w", po::value<string>(), "Day of Week (0-6) {Sun=0,...,Sat=6}")
       ("cron,c", po::value<string>(), "Cron string formatting")
       ("drift,x", po::value<string>(), "Additional drift time (in seconds).")
   ;

   // Parse all the options and ensure the critical ones have been defined.
   // It's up to the application mainline to ensure application-specific
   // arguments have been specified on the command-line.
   try
   {
      po::store(
            po::parse_command_line(argc, argv, poAllOptions),
            poVariablesMap);
   }
   catch(exception& e)
   {
      cerr << "Error parsing command line: " << e.what() << endl;
      cerr << "Exiting." << endl;
      return -1;
   }
   po::notify(poVariablesMap);
   if (poVariablesMap.count("help"))
   {
      ProgramDetails();
      cerr << poAllOptions << endl;
      ProgramExamples();
      return 0;
   }
   else if (! ( (poVariablesMap.count("cron")) ||
         (poVariablesMap.count("second")) ||
         (poVariablesMap.count("minute")) ||
         (poVariablesMap.count("hour")) ||
         (poVariablesMap.count("dom")) ||
         (poVariablesMap.count("month")) ||
         (poVariablesMap.count("dow"))
         )
      )
   {
      cerr << "Error: You must specify at least one option." << endl;
      ProgramDetails();
      cerr << poAllOptions << endl;
      ProgramExamples();
      return 1;
   }

   if (!poVariablesMap.count("cron"))
   {
      // Store Second
      if ((poVariablesMap.count("second")))
      {
         sSecOffset = poVariablesMap["second"].as<string>();
      }

      // Store Minute
      if ((poVariablesMap.count("minute")))
      {
         sMinOffset = poVariablesMap["minute"].as<string>();
      }

      // Store Hour
      if ((poVariablesMap.count("hour")))
      {
         sHourOffset = poVariablesMap["hour"].as<string>();
      }

      // Store DOM of Month
      if ((poVariablesMap.count("dom")))
      {
         sDomOffset = poVariablesMap["dom"].as<string>();
      }

      // Store Month
      if ((poVariablesMap.count("month")))
      {
         sMonthOffset = poVariablesMap["month"].as<string>();
      }
      // Store DOM of Week
      if ((poVariablesMap.count("dow")))
      {
         sDowOffset = poVariablesMap["dow"].as<string>();
      }
      // Store Drift
      if ((poVariablesMap.count("drift")))
      {
         sDriftOffset = poVariablesMap["drift"].as<string>();
      }

      // cout << "Executing Cron("
      //   << sSecOffset
      //   << "," << sMinOffset
      //   << "," << sHourOffset
      //   << "," << sDomOffset
      //   << "," << sMonthOffset
      //   << "," << sDowOffset << ");" << endl;
      try
      {
         if(!Date::CronValid(sSecOffset,
                               sMinOffset,
                               sHourOffset,
                               sDomOffset,
                               sMonthOffset,
                               sDowOffset,
                               sDriftOffset))
         {
            cerr << "Error: Syntax Invalid : '"
               << sSecOffset
               << " " << sMinOffset
               << " " << sHourOffset
               << " " << sDomOffset
               << " " << sMonthOffset
               << " " << sDowOffset
               << " " << sDriftOffset
               << "'" << endl;
               ProgramExamples();
            return 1;
         }

         dObjFinish = dObjStart.Cron(sSecOffset,
                        sMinOffset,
                        sHourOffset,
                        sDomOffset,
                        sMonthOffset,
                        sDowOffset,
                        sDriftOffset);
      }
      catch (const runtime_error& e)
      {
         cerr << "Error: No matching date/time could be found: "
              << e.what() << endl;
         return 1;
      }
      catch (const exception& e)
      {
         cerr << "Error: " << e.what() << endl;
         return 1;
      }
   }
   else
   {
      // ISC Format
      bool useISC = (poVariablesMap.count("isc") > 0);

      // Parse the old school cron format
      sCronStr = poVariablesMap["cron"].as<string>();
      //cout << "Executing Cron(\""
      //     << sCronStr << "\");" << endl;
      try
      {
         if(!Date::CronValid(sCronStr, useISC))
         {
            cerr << "Error: Syntax Invalid : '"
               << sCronStr << "'" << endl;
               ProgramExamples();
            return 1;
         }

         dObjFinish = dObjStart.Cron(sCronStr, useISC);
      }
      catch (const runtime_error& e)
      {
         cerr << "Error: No matching date/time could be found: "
              << e.what() << endl;
         return 1;
      }
      catch (const exception& e)
      {
         cerr << "Error: " << e.what() << endl;
         return 1;
      }
   }


   if (poVariablesMap.count("verbose") || poVariablesMap.count("test"))
   {
      cout << "Current Time : "<< dObjStart.Str("%Y-%m-%d %H:%M:%S (%a)") << endl;
      cout << "Block Until  : "<< dObjFinish.Str("%Y-%m-%d %H:%M:%S (%a)") << endl;
   }

   if (!(poVariablesMap.count("test")))
   {
      // Keep sleeping if a signal happens to wake us up early
      while (Date::BlockUntil(dObjFinish) > 0);
      Date dObjNow;
      if (poVariablesMap.count("verbose"))
      {
         cout << "UnBlock Time : "
              << dObjNow.Str("%Y-%m-%d %H:%M:%S") << endl;
      }
   }
   return 0;
}
