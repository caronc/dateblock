dateblock(1) -- block (sleep) until a cron-style moment in time
================================================================

## SYNOPSIS

`dateblock` [`-t`] [`-v`] [`-C` <COUNT>] [`-z` <ZONE>] `-c` <CRON> [`-i`] [[`--`] <COMMAND> [<ARGUMENTS>...]]<br>
`dateblock` [`-t`] [`-v`] [`-C` <COUNT>] [`-z` <ZONE>] [`-s` <SEC>] [`-n` <MIN>] [`-o` <HOUR>] [`-d` <DOM>] [`-m` <MONTH>] [`-w` <DOW>] [`-x` <DRIFT>] [[`--`] <COMMAND> [<ARGUMENTS>...]]<br>
`dateblock` `-h`

## DESCRIPTION

**dateblock** works like sleep(1), except that instead of waiting _for_ a
set period of time, it waits _until_ a specific point in time is reached.
That point in time is described the same way a crontab(5) entry is, so
**dateblock** can be thought of as running a single cron entry in the
foreground: it returns the moment the schedule is next matched.

On top of the regular cron fields, **dateblock** can also:

  * match on a specific _second_ (not just a minute).
  * _drift_; wait an extra number of seconds after the time is matched.

Use `--test` to see when **dateblock** would unblock without waiting, or
`--count` to list the next few times.

Given a <COMMAND>, **dateblock** runs it once the time is reached and exits
with the command's exit code; see [RUNNING A COMMAND][].

## OPTIONS

  `-h`, `--help`:
  Show the help screen (with examples) and exit.

  `-t`, `--test`:
  Do not block; print the current time and the time **dateblock** would
  unblock at, then exit.

  `-v`, `--verbose`:
  Print the current time and the time **dateblock** will unblock at before
  blocking, and the time it actually unblocked at afterwards.

  `-C`, `--count=`<COUNT>:
  Do not block; print the current time and the next <COUNT> times (1 to
  1000) the schedule matches, then exit. The first is the time
  **dateblock** would unblock at. A <COMMAND> is shown but not run. In the
  rare case that no later time exists (ie: past the last date the computer
  can show), the list ends early with a note on standard error, and the
  exit status is still `0`.

  `-c`, `--cron=`<CRON>:
  The full schedule as a single cron string; see [CRON STRING][]. When this
  is used the individual field options below are ignored.

  `-i`, `--isc`:
  Read the `--cron` string as a regular 5 field crontab(5) entry (minute,
  hour, day of month, month, day of week) with no second or drift fields.
  ISC stands for the Internet Systems Consortium cron.

  `-z`, `--timezone=`<ZONE>:
  Read the schedule in timezone <ZONE> (a name from the timezone database,
  such as `America/Toronto` or `Asia/Tokyo`) instead of this computer's
  own; see [TIMEZONES][]. The times shown by `--test` and `--verbose` are
  in that timezone too.

  `-s`, `--second=`<SEC>:
  The second(s) to unblock on (0-59).

  `-n`, `--minute=`<MIN>:
  The minute(s) to unblock on (0-59).

  `-o`, `--hour=`<HOUR>:
  The hour(s) to unblock on (0-23).

  `-d`, `--dom=`<DOM>:
  The day(s) of the month to unblock on (1-31).

  `-m`, `--month=`<MONTH>:
  The month(s) to unblock on (1-12, or Jan to Dec).

  `-w`, `--dow=`<DOW>:
  The day(s) of the week to unblock on (0-6, or Sun to Sat; Sunday is 0 or
  7).

  `-x`, `--drift=`<DRIFT>:
  Extra seconds to wait once the time is matched; see [DRIFTING][]. Use the
  7th field (or `+`<DRIFT>) instead when using `--cron`.

At least one field (or `--cron`) must be given. Fields that are not given
match any value, except for the ones _smaller_ than the smallest field given:
those are reset to their lowest value. So `-n 5` unblocks at exactly 5
minutes past the hour (second 0), and `-o 3` at exactly 3:00:00.

When both a day of the month and a day of the week are given, both have to
match: `-d 13 -w 5` is every Friday the 13th.

## FIELD SYNTAX

Every field (and every entry in a cron string) accepts:

  * `x`:
    a single value.
  * `*/x` (or `/x`):
    every value divisible by _x_ (modulus).
  * `x-y` (or `y-x`):
    every value from _x_ to _y_ (range).
  * `x-y/z`:
    every _z_'th value from _x_ to _y_, so `1-10/3` is 1, 4, 7 and 10.
  * `x,y`:
    both _x_ and _y_ (separator).
  * `*`:
    any value. It must be on its own; `*,5` is not valid.

The month and the day of the week can also be written as names, in any
case: `Jan` to `Dec` and `Sun` to `Sat` (ie: `Mon-Fri` or `Jan,Jul`). A
range of names can't go backwards: `Sat-Sun` is refused (write `Sat,Sun`
or `Sat-7`), since it would otherwise mean every day of the week.

As in crontab(5), a day of the week of `7` is Sunday too, so `Fri-7` is
Friday to Sunday.

These can be mixed when separated by commas, so `*/a,b,c-d,/e` is valid.
However `x-y-z`, `/x/y`, `/x-y`, an unfinished `x-`, an empty entry
(`5,,6`) and a `*` mixed with other entries are not. Every value
must be within the range of its field; `400-4000` never works for any field.

## CRON STRING

The `--cron` string has up to 7 fields separated by white space:

    drift time ---------------------------------------------+
    day of week (0 - 7) (Sunday=0 or 7) -----------------+  |
    month (1 - 12) -----------------------------------+  |  |
    day of month (1 - 31) -------------------------+  |  |  |
    hour (0 - 23) ------------------------------+  |  |  |  |
    min (0 - 59) ----------------------------+  |  |  |  |  |
    sec (0 - 59) -------------------------+  |  |  |  |  |  |
                                          |  |  |  |  |  |  |
                                          *  *  *  *  *  *  *

Missing fields at the end are filled in with `*`, so `* 5` is the same as
`* 5 * * * * *`.

An entry starting with a plus (`+`) jumps straight to the drift field, so
`* /5 +60` is the same as `* /5 * * * * 60`. Nothing may follow the `+`
entry.

The crontab(5) shortcuts can stand for the whole string, with or without
`--isc`. Without `--isc` a `+` drift may follow one (ie: `@daily +30`);
nothing else may, and with `--isc` nothing may follow it at all (a crontab
entry has no drift):

  * `@hourly`: the start of every hour.
  * `@daily` (or `@midnight`): every day at 00:00.
  * `@weekly`: every Sunday at 00:00.
  * `@monthly`: the 1st of every month at 00:00.
  * `@yearly` (or `@annually`): every January 1st at 00:00.

`@reboot` is refused, since it isn't a time.

With `--isc` the string is a regular crontab(5) entry instead:

    day of week (0 - 7) (Sunday=0 or 7) -----+
    month (1 - 12) ----------------------+   |
    day of month (1 - 31) -----------+   |   |
    hour (0 - 23) ---------------+   |   |   |
    min (0 - 59) ------------+   |   |   |   |
                             |   |   |   |   |
                             *   *   *   *   *

It works the same as a crontab(5) entry does:

  * It always matches at second 0. Every other field means exactly what it
    says, so `* 3 * * *` is every minute from 3:00 to 3:59.
  * When both day fields are restricted (neither starts with `*`), _either_
    one may match: `0 0 1 * 1` is the 1st of every month and every Monday.
  * `*/x` counts from the field's lowest value, so a day of month of `*/5`
    is the 1st, 6th, 11th and so on (without `--isc` it is the 5th, 10th,
    15th and so on).

## RUNNING A COMMAND

Anything after the options is a command for **dateblock** to run once the
time is reached:

    $ dateblock -n /15 -- ./backup.sh --full

  * **dateblock** _becomes_ the command (with execvp(3), the way nice(1)
    and timeout(1) do), so the exit code, signals and output are all the
    command's own.
  * The command starts at `--`, or at the first argument that is not one
    of **dateblock**'s options (or an option's value). Everything from
    there on is passed to the command exactly as given, without a shell,
    so `dateblock -n 5 ls -l` runs `ls -l`. Use `--` when the command
    itself starts with a dash.
  * With `--test` the command is shown (on the `Command` line) but not
    run. If the schedule is invalid or impossible the command is never
    run.

## TIMEZONES

The schedule is read in this computer's timezone (the `TZ` environment
variable, or the system setting), unless `--timezone` names another one.

  * Daylight saving follows that timezone. A time skipped when the clocks
    go forward (such as 2:30 in most of North America on the second Sunday
    of March) does not happen that day, and a time repeated when they go
    back matches the first time it comes around.
  * **dateblock** unblocks at that exact moment, no matter which timezone
    the computer itself is in.

## DRIFTING

A drift adds a number of seconds onto the time that was matched. For
example, to wake up on the 1st minute of every 10 minute interval (1, 11,
21, ..., 51) use a minute of `*/10` with a drift of 60 seconds.

  * A drift is always measured in seconds and is applied _after_ the
    schedule is matched.
  * A drift can not be given on its own; it needs a schedule to go with it.
  * A drift can not be longer than the interval of the schedule itself.
    Only the remainder is kept, so that no matching time is ever skipped.
    With a schedule of every 10 seconds (`*/10`), a drift of 11 really
    drifts 1 second, and a drift of 10 does nothing at all.
  * Inline drifts (in the cron string) accept the separator and range
    syntax, so `0 /5 +15,45` unblocks 15 and 45 seconds after every 5th
    minute. The modulus (`/x`) syntax does not apply to drifts.
  * A drift list can hold at most 10,000 values (ie: `+0-9999`).

## EXAMPLES

Block until the next minute divisible by 10 (0, 10, 20, 30, 40 and 50):

    $ dateblock -n /10

Block until the next second divisible by 5, but only during hours 3 and 4:

    $ dateblock -c "/5 * 3,4 * * *"
    $ dateblock -s /5 -o 3,4

Block until 5 minutes past the hour (all three are the same):

    $ dateblock -c "* 5"
    $ dateblock -c "* 5 * * * *"
    $ dateblock -n 5

Block until an hour divisible by 5 (0, 5, 10, 15 and 20) on the 1st to the
14th, or the 20th, of the month:

    $ dateblock -o /5 -d 1-14,20

Block until 20 seconds after 5 minutes past the hour:

    $ dateblock -c "* 5 * * * * 20"
    $ dateblock -c "* 5 +20"

Block until 9:00 on weekdays, written as a regular crontab entry:

    $ dateblock -i -c "0 9 * * 1-5"
    $ dateblock -i -c "0 9 * * Mon-Fri"

Block until the next midnight, then 30 seconds more:

    $ dateblock -c "@daily +30"

Block until 9:00 in Tokyo, whatever timezone this computer is in:

    $ dateblock -z Asia/Tokyo -c "0 0 9"
    $ dateblock -t -z Asia/Tokyo -c "0 0 9"
    Current Time : 2026-10-24 22:05:00 JST (Sat)
    Block Until  : 2026-10-25 09:00:00 JST (Sun)

See when a schedule would next run, without waiting:

    $ dateblock -t -c "0 30 2 * * 0"
    Current Time : 2026-10-24 13:05:00 (Sat)
    Block Until  : 2026-10-25 02:30:00 (Sun)

List the next 5 times a crontab entry runs (a quick way to check one):

    $ dateblock -C 5 -i -c "0 9 * * Mon-Fri"
    Current Time : 2026-10-30 16:53:37 (Fri)
    Next Times   : 2026-11-02 09:00:00 (Mon)
                   2026-11-03 09:00:00 (Tue)
                   2026-11-04 09:00:00 (Wed)
                   2026-11-05 09:00:00 (Thu)
                   2026-11-06 09:00:00 (Fri)

Run a backup at the next quarter hour; the exit code is the backup's:

    $ dateblock -n /15 -- ./backup.sh --full

Run it every 15 minutes, on the quarter hour:

    $ while dateblock -n /15; do ./backup.sh; done

## EXIT STATUS

With a <COMMAND>, **dateblock** exits with the command's own exit code
once it has run. Otherwise, or if the command never got to run:

  * `0`:
    The matching time was reached (or, with `--test` or `--count`, was
    worked out).
  * `1`:
    No options were given, the schedule is not valid, the `--timezone` is
    unknown, the `--count` is not a whole number from 1 to 1000, or no matching time exists (such as the 31st of February). A
    schedule counts as impossible when nothing matches within the next 400
    years, the length of time it takes the calendar to repeat itself.
  * `126`:
    The <COMMAND> was found but could not be run (ie: it is not
    executable).
  * `127`:
    The <COMMAND> could not be found.
  * `255`:
    The command line could not be parsed.

If **dateblock** is killed before the time is reached (ie: with Ctrl-C), it
exits with the usual status for that signal.

## SEE ALSO

sleep(1), crontab(5), date(1), nice(1), timeout(1), execvp(3)

## AUTHOR

Chris Caron &lt;lead2gold@gmail.com&gt;

## BUGS

If you find any bugs, please make them known at:
https://github.com/caronc/dateblock/issues

## DONATIONS
If you found Apprise useful at all, [please consider sponsoring][sponsorship] or [donating][donating]!

[sponsorship]: https://github.com/sponsors/caronc
[donating]: https://www.paypal.com/donate/?hosted_button_id=CR6YF7KLQWQ5E

## COPYRIGHT

Copyright (C) 2013-2026 Chris Caron. License GPLv2+: GNU GPL version 2 or
later (https://www.gnu.org/licenses/old-licenses/gpl-2.0.html).

This is free software: you are free to change and redistribute it. There is
NO WARRANTY, to the extent permitted by law.
