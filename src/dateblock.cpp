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
 this program; if not, see <https://www.gnu.org/licenses/>.
*/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <iostream>
#include <map>
#include <string>
#include <stdexcept>
#include <vector>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include "Date.h"
#include "dateblock.h"

using namespace std;

// The options dateblock understands
struct Option
{
   char shortName;       // ie: 'n' for -n
   const char *longName; // ie: "minute" for --minute
   bool takesValue;
   const char *help;     // lines after the first are indented to line up
};

static const Option OPTIONS[] = {
   {'h', "help", false, "Show this help screen."},
   {'t', "test", false, "Test (Do not block for the specified period)"},
   {'v', "verbose", false, "Verbose mode"},
   {'i', "isc", false, "ISC mode; this only works when paired with --cron\n"
                       "(-c) switch."},
   {'s', "second", true, "Second (0-59)"},
   {'n', "minute", true, "Minute (0-59)"},
   {'o', "hour", true, "Hour (0-23)"},
   {'d', "dom", true, "Day of Month (1-31)"},
   {'m', "month", true, "Month (1-12) {Jan=1,...,Dec=12}"},
   {'w', "dow", true, "Day of Week (0-6) {Sun=0,...,Sat=6}"},
   {'c', "cron", true, "Cron string formatting"},
   {'x', "drift", true, "Additional drift time (in seconds)."},
   {'z', "timezone", true, "Timezone to read the schedule in (ie:\n"
                           "America/Toronto); defaults to this computer's."},
};
static const size_t OPTION_COUNT = sizeof(OPTIONS) / sizeof(OPTIONS[0]);

// What was given on the command line
struct CommandLine
{
   map<string, string> given;   // long name -> value ("" for a flag)
   vector<char *> command;      // the command to run once the time is reached

   bool Has(const char *name) const { return given.count(name) > 0; }
   string Get(const char *name) const
   {
      map<string, string>::const_iterator itr = given.find(name);
      return (itr == given.end()) ? string() : itr->second;
   }
};

// Lists the options (for --help)
static void PrintOptions()
{
   cerr << "Allowed options:" << endl;
   for (size_t i = 0; i < OPTION_COUNT; i++)
   {
      string label = string("  -") + OPTIONS[i].shortName + " [ --" +
                     OPTIONS[i].longName + " ]" +
                     (OPTIONS[i].takesValue ? " arg" : "");
      label.resize(label.size() < 24 ? 24 : label.size() + 1, ' ');
      cerr << label;
      for (const char *c = OPTIONS[i].help; *c; c++)
      {
         cerr << *c;
         if (*c == '\n')
            cerr << string(24, ' ');
      }
      cerr << endl;
   }
   cerr << endl;
}

// True if 'arg' can be the value of the option before it: it may not look
// like an option itself (a lone '-' is fine), so a forgotten value is
// reported instead of quietly swallowing the next option
static bool IsValue(const char *arg)
{
   return arg != NULL && (arg[0] != '-' || arg[1] == '\0');
}

// Records option 'opt' with 'value'; each option may only be given once
static bool Store(CommandLine& out, const Option& opt, const string& value,
                  string& error)
{
   if (out.Has(opt.longName))
   {
      error = string("option '--") + opt.longName +
              "' was given more than once";
      return false;
   }
   out.given[opt.longName] = value;
   return true;
}

// The option called --'name', which may be shortened as long as only one
// option starts with it (ie: --min for --minute); NULL (with 'error' set)
// if there is no such option, or more than one
static const Option *FindLong(const string& name, string& error)
{
   vector<const Option *> matches;
   for (size_t i = 0; i < OPTION_COUNT; i++)
   {
      if (name == OPTIONS[i].longName)
         return &OPTIONS[i];
      if (!name.empty() &&
          string(OPTIONS[i].longName).compare(0, name.size(), name) == 0)
         matches.push_back(&OPTIONS[i]);
   }
   if (matches.size() == 1)
      return matches[0];

   if (matches.empty())
   {
      error = "unrecognised option '--" + name + "'";
   }
   else
   {
      error = "option '--" + name + "' is ambiguous (it could be";
      for (size_t i = 0; i < matches.size(); i++)
         error += string(i ? ", --" : " --") + matches[i]->longName;
      error += ")";
   }
   return NULL;
}

// Reads argv into 'out'.
//  - Short options can be combined (-tv) and take their value attached
//    (-n5) or as the next argument (-n 5).
//  - Long options take theirs as --name=value or --name value.
//  - A value given as the next argument may not start with '-'; it would
//    be another option (ie: '-n -t' is missing the minute).
//  - The command to run starts at '--', or at the first argument that
//    isn't one of our options or an option's value.  Everything from
//    there on belongs to the command (like sudo or nice), so its own
//    options (ie: ls -l) are never taken for ours.
// Returns false (with 'error' set) if something isn't understood.
static bool ParseCommandLine(int argc, char **argv, CommandLine& out,
                             string& error)
{
   for (int i = 1; i < argc; i++)
   {
      string arg = (argv[i] != NULL) ? argv[i] : "";
      if (arg == "--" || arg.size() < 2 || arg[0] != '-')
      {
         for (int c = (arg == "--") ? i + 1 : i; c < argc; c++)
            out.command.push_back(argv[c]);
         return true;
      }

      if (arg[1] == '-')
      {
         // --name or --name=value
         string name = arg.substr(2), value;
         size_t equals = name.find('=');
         bool attached = (equals != string::npos);
         if (attached)
         {
            value = name.substr(equals + 1);
            name.resize(equals);
         }
         const Option *opt = FindLong(name, error);
         if (opt == NULL)
            return false;
         if (!opt->takesValue && attached)
         {
            error = string("option '--") + opt->longName +
                    "' does not take a value";
            return false;
         }
         if (opt->takesValue && attached && value.empty())
         {
            error = string("option '--") + opt->longName +
                    "' needs a value after the '='";
            return false;
         }
         if (opt->takesValue && !attached)
         {
            if (i + 1 >= argc || !IsValue(argv[i + 1]))
            {
               error = string("option '--") + opt->longName +
                       "' needs a value";
               return false;
            }
            value = argv[++i];
         }
         if (!Store(out, *opt, value, error))
            return false;
         continue;
      }

      // -abc: flags, until one that takes a value (the rest of the
      // argument, or the next one)
      for (size_t c = 1; c < arg.size(); c++)
      {
         const Option *opt = NULL;
         for (size_t o = 0; o < OPTION_COUNT && opt == NULL; o++)
            if (OPTIONS[o].shortName == arg[c])
               opt = &OPTIONS[o];
         if (opt == NULL)
         {
            error = string("unrecognised option '-") + arg[c] + "'";
            return false;
         }
         if (!opt->takesValue)
         {
            if (!Store(out, *opt, "", error))
               return false;
            continue;
         }

         string value;
         if (c + 1 < arg.size())
            value = arg.substr(c + 1);
         else if (i + 1 < argc && IsValue(argv[i + 1]))
            value = argv[++i];
         else
         {
            error = string("option '-") + opt->shortName + "' needs a value";
            return false;
         }
         if (!Store(out, *opt, value, error))
            return false;
         break;
      }
   }
   return true;
}

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
        << "Give it a command (and its arguments) after the options, and" << endl
        << "dateblock runs it once the time is reached, exiting with the" << endl
        << "command's own exit code." << endl << endl
        << "Syntax: "
        << "dateblock [options] [--] [command [arguments...]]" << endl << endl;
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
     << "       However: x-y-z is not valid, nor is /x/y, /x-y or '*' mixed"
     << " with" << endl << "       other entries (ie: *,5). All"
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

   cerr << "$> dateblock -z Asia/Tokyo -c \"0 0 9\"" << endl
     << "    This would block until 9:00 in Tokyo, whatever timezone this"
     << endl << "    computer is in (daylight saving follows that timezone)."
     << endl << endl;

   cerr << "$> dateblock -n /15 -- ./backup.sh --full" << endl
     << "    This would block until the next quarter hour, and then run"
     << endl << "    './backup.sh --full' (exiting with its exit code)."
     << endl << endl;

   cerr << "$> dateblock -o /5 -d 1-14,20" << endl
     << "    This would block until a hour divisible by 5 was reached on"
     << " the first half" << endl << "     of the month as well as the"
     << " 20th of the month) Hours divisible by 5 are: 0,"
     << endl <<  "     5,10,15 and 20."
     << endl << endl;
}

// True if 'name' (ie: America/Toronto) is in this computer's timezone
// database.  Anything else would quietly be treated as UTC, so it is
// checked first.  Names may not be paths of their own.
static bool KnownTimeZone(const string& name)
{
   if (name.empty() || name[0] == '/' || name.find("..") != string::npos)
   {
      return false;
   }
   const char *dirs[] = {getenv("TZDIR"), "/usr/share/zoneinfo",
                         "/usr/lib/zoneinfo", "/usr/share/lib/zoneinfo"};
   for (unsigned i = 0; i < sizeof(dirs)/sizeof(dirs[0]); i++)
   {
      struct stat st;
      if (dirs[i] != NULL &&
          stat((string(dirs[i]) + "/" + name).c_str(), &st) == 0 &&
          S_ISREG(st.st_mode))
      {
         return true;
      }
   }
   return false;
}

// Everything dateblock does; main() only adds a safety net around it
static int Run(int argc, char **argv)
{
   CommandLine cl;
   string error;
   if (!ParseCommandLine(argc, argv, cl, error))
   {
      cerr << "Error parsing command line: " << error << endl;
      cerr << "Exiting." << endl;
      return 255;
   }
   vector<char *>& command = cl.command;

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
   string sShown;     // the schedule as typed, for error messages
   bool useISC = false;

   if (cl.Has("help"))
   {
      ProgramDetails();
      PrintOptions();
      ProgramExamples();
      return 0;
   }
   else if (!(cl.Has("cron") || cl.Has("second") || cl.Has("minute") ||
              cl.Has("hour") || cl.Has("dom") || cl.Has("month") ||
              cl.Has("dow")))
   {
      cerr << "Error: You must specify at least one option." << endl;
      ProgramDetails();
      PrintOptions();
      ProgramExamples();
      return 1;
   }

   // Read the schedule (and show the times) in another timezone.  Only
   // this program's own TZ changes; the moment it unblocks at is the same
   // everywhere.
   string sTimeFmt = "%Y-%m-%d %H:%M:%S";
   if (cl.Has("timezone"))
   {
      string zone = cl.Get("timezone");
      if (!KnownTimeZone(zone))
      {
         cerr << "Error: Unknown timezone '" << zone << "'" << endl;
         return 1;
      }
      setenv("TZ", zone.c_str(), 1);
      tzset();
      dObjStart.Set();
      sTimeFmt += " %Z";
   }

   if (!cl.Has("cron"))
   {
      // Store Second
      if (cl.Has("second"))
      {
         sSecOffset = cl.Get("second");
      }

      // Store Minute
      if (cl.Has("minute"))
      {
         sMinOffset = cl.Get("minute");
      }

      // Store Hour
      if (cl.Has("hour"))
      {
         sHourOffset = cl.Get("hour");
      }

      // Store DOM of Month
      if (cl.Has("dom"))
      {
         sDomOffset = cl.Get("dom");
      }

      // Store Month
      if (cl.Has("month"))
      {
         sMonthOffset = cl.Get("month");
      }
      // Store DOM of Week
      if (cl.Has("dow"))
      {
         sDowOffset = cl.Get("dow");
      }
      // Store Drift
      if (cl.Has("drift"))
      {
         sDriftOffset = cl.Get("drift");
      }

      sShown = sSecOffset + " " + sMinOffset + " " + sHourOffset + " " +
               sDomOffset + " " + sMonthOffset + " " + sDowOffset + " " +
               sDriftOffset;
   }
   else
   {
      // The whole schedule as one string (a regular crontab entry with
      // --isc)
      useISC = cl.Has("isc");
      sCronStr = cl.Get("cron");
      sShown = sCronStr;
   }

   try
   {
      if (cl.Has("cron"))
      {
         dObjFinish = dObjStart.Cron(sCronStr, useISC);
      }
      else
      {
         dObjFinish = dObjStart.Cron(sSecOffset, sMinOffset, sHourOffset,
                                     sDomOffset, sMonthOffset, sDowOffset,
                                     sDriftOffset);
      }
   }
   catch (const invalid_argument& e)
   {
      cerr << "Error: Syntax Invalid : '" << sShown << "'" << endl
           << "       " << e.what() << endl
           << "Run 'dateblock --help' to see the syntax that is accepted."
           << endl;
      return 1;
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

   if (cl.Has("verbose") || cl.Has("test"))
   {
      cout << "Current Time : "<< dObjStart.Str(sTimeFmt + " (%a)") << endl;
      cout << "Block Until  : "<< dObjFinish.Str(sTimeFmt + " (%a)") << endl;
      if (!command.empty())
      {
         cout << "Command      :";
         for (size_t i = 0; i < command.size(); i++)
         {
            cout << " " << command[i];
         }
         cout << endl;
      }
   }

   if (!(cl.Has("test")))
   {
      // Keep sleeping if a signal happens to wake us up early
      while (Date::BlockUntil(dObjFinish) > 0);
      Date dObjNow;
      if (cl.Has("verbose"))
      {
         cout << "UnBlock Time : "
              << dObjNow.Str(sTimeFmt) << endl;
      }

      if (!command.empty())
      {
         // Become the command (like nice or timeout do): from here on the
         // exit code, signals and output are all its own.  Nothing goes
         // through a shell, so arguments are passed exactly as given.
         cout.flush();
         cerr.flush();
         command.push_back(NULL);
         execvp(command[0], &command[0]);

         // Only reached if it could not be run (126 and 127 are what the
         // shell uses for these)
         int err = errno;
         if (err == ENOENT)
         {
            cerr << "dateblock: " << command[0] << ": command not found"
                 << endl;
            return 127;
         }
         cerr << "dateblock: " << command[0] << ": " << strerror(err) << endl;
         return 126;
      }
   }
   return 0;
}

int main(int argc, char **argv)
{
   // Anything unexpected (ie: running out of memory) is reported and ends
   // dateblock with exit code 1, instead of crashing it
   try
   {
      return Run(argc, argv);
   }
   catch (const exception& e)
   {
      cerr << "Error: " << e.what() << endl;
   }
   catch (...)
   {
      cerr << "Error: something unexpected went wrong." << endl;
   }
   return 1;
}
