[![npm](https://img.shields.io/npm/v/@caronc/dateblock.svg)](https://www.npmjs.com/package/@caronc/dateblock)
[![Node.js](https://img.shields.io/node/v/@caronc/dateblock.svg)](https://www.npmjs.com/package/@caronc/dateblock)
[![Paypal](https://img.shields.io/badge/paypal-donate-green.svg)](https://www.paypal.com/donate/?hosted_button_id=CR6YF7KLQWQ5E)

@caronc/dateblock
=================
Wait _until_ a cron-style moment in time, instead of _for_ a number of
milliseconds.

```javascript
import { dateblock } from '@caronc/dateblock';

// Wait for the next quarter hour (0, 15, 30 and 45 minutes past)
await dateblock('* /15');
```

- Seconds and a "drift" (extra wait) on top of regular cron.
- Reads regular 5-field crontab entries too.
- Any timezone, with daylight saving handled for you.
- Waiting never blocks Node.js: `dateblock()` returns a promise.
- TypeScript types are included; works with `import` and `require()`.

This is the Node.js package of
[dateblock](https://github.com/caronc/dateblock), which also has a command
line tool and Python and PHP libraries.

## Installation
```bash
npm install @caronc/dateblock
```
npm compiles the C++ code while installing, so it needs a C++ compiler and
Python (see [node-gyp](https://github.com/nodejs/node-gyp#installation)).
npm 12 and newer only run that step once you allow it, so the first time:
```bash
npm install-scripts approve @caronc/dateblock
npm rebuild @caronc/dateblock
```

It needs Node.js 20 or newer, on Linux, macOS or Windows.  Like the rest of
dateblock, it needs a 64-bit `time_t`, so it won't build on older 32-bit
systems (see
[INSTALL.md](https://github.com/caronc/dateblock/blob/master/INSTALL.md)).

## Usage
```javascript
import { dateblock, next } from '@caronc/dateblock';
// or, in CommonJS: const { dateblock, next } = require('@caronc/dateblock');

// When is the next 10 minute mark?  (a Date; nothing waits)
const when = next('* /10');

// Wait for it; the promise gives the Date it waited for
await dateblock('* /10');

// Work it out from another moment (a Date, or milliseconds since the epoch)
next('* /10', { ref: new Date('2026-01-01T12:34:56Z') });

// A regular crontab entry: 9:00 every weekday
next('0 9 * * 1-5', { isc: true });

// 9:00 in Tokyo, whatever timezone this computer is in
await dateblock('0 0 9', { timeZone: 'Asia/Tokyo' });

// Stop waiting early with an AbortSignal
const controller = new AbortController();
setTimeout(() => controller.abort(), 5000);
await dateblock('0 0 9', { signal: controller.signal });
```

Run a job again and again:
```javascript
while (true) {
  await dateblock('* /15');
  await backup();
}
```

### next(cron, options)
Works out the next moment that matches `cron`, without waiting, and returns
it as a `Date`.

| Option | What it does |
|---|---|
| `ref` | Start from this `Date` (or milliseconds since the epoch) instead of now. |
| `isc` | `true` to read a regular 5-field crontab entry (see below). |
| `timeZone` | Read the cron in this timezone (ie: `"America/Toronto"`) instead of the local one. |

### dateblock(cron, options)
Waits until the next moment that matches `cron`.  It takes the same options
as `next()`, plus:

| Option | What it does |
|---|---|
| `signal` | An `AbortSignal`; aborting it stops the wait (the promise is rejected with the signal's reason). |

It returns a promise for the `Date` it waited for.  Long waits (even years)
are fine.  The clock is checked at least once a minute, so a computer that
was asleep (or had its clock changed) still wakes within a minute of the
right moment.

### Errors
- A cron string that isn't written correctly throws a `SyntaxError` saying
  what is wrong.
- A cron that can never happen (such as February 31st), an unknown timezone
  or a reference too far away throws a `RangeError`.
- Options of the wrong type throw a `TypeError`.

`dateblock()` throws these right away, before it starts waiting.

## The cron string
Up to 7 fields, separated by spaces.  Fields left off the end count as `*`
(anything).
```
     drift time ----------------------------------------------+
     day of week (0 - 7) (Sunday=0 or 7) ------------------+  |
     month (1 - 12) ------------------------------------+  |  |
     day of month (1 - 31) --------------------------+  |  |  |
     hour (0 - 23) -------------------------------+  |  |  |  |
     min (0 - 59) -----------------------------+  |  |  |  |  |
     sec (0 - 59) ---------------------------  |  |  |  |  |  |
                                            |  |  |  |  |  |  |
                                            *  *  *  *  *  *  *
```

Each field can be:

| Written | Means |
|---|---|
| `*` | anything |
| `5` | exactly 5 |
| `/5` or `*/5` | every 5th (0, 5, 10...) |
| `1-5` | 1 to 5 |
| `1-30/5` | every 5th from 1 to 30 (1, 6, 11...) |
| `1,5,20-25` | any of these, mixed together |
| `Mon-Fri`, `Jan,Jul` | names for the day of the week and the month (any case) |
| `7` (day of the week) | Sunday, as `0` is (so `Fri-7` is Friday to Sunday) |

A shortcut can stand for the whole cron: `@hourly`, `@daily` (or
`@midnight`), `@weekly` (Sundays), `@monthly` and `@yearly` (or
`@annually`), each at the start of the hour or day.  Only a drift may
follow one, ie: `'@daily +30'` (and not with `isc: true`, since a crontab
entry has no drift).

Things worth knowing:
- Fields smaller than the smallest one you give start at their lowest
  value: `'* 5'` is exactly 5 minutes past (second 0), not every second of
  that minute.
- The day of the month and the day of the week must both match.
- **Drift** is extra time (in seconds) added once the time is found:
  `'* /10 * * * * 60'` waits for 1, 11, 21... minutes past.  `+` jumps
  straight to the drift field, so `'* /10 +60'` is the same.  A drift
  longer than the schedule's own interval only keeps the remainder, so no
  match is skipped.

| Cron | Waits for |
|---|---|
| `'/20'` | the next second divisible by 20 (0, 20, 40) |
| `'* /10'` | every 10 minutes |
| `'* 5'` | 5 minutes past each hour |
| `'0 30 9'` | 9:30 every day |
| `'0 0 9 * * 1-5'` | 9:00 every weekday |
| `'0 0 0 1 1'` | midnight on January 1st |
| `'/5 * 3,4'` | every 5 seconds, during 3:00-4:59 |
| `'* 5 +20'` | 5 minutes and 20 seconds past each hour |

### Regular crontab entries (`isc: true`)
With `isc: true` the cron is read the way `/etc/crontab` reads it: 5 fields
(minute, hour, day of month, month, day of week), seconds always 0, no
drift.  The day of the month _or_ the day of the week has to match when
both are given, as crontab does.
```javascript
next('*/15 * * * *', { isc: true });   // every 15 minutes
next('0 9 * * 1-5', { isc: true });    // 9:00 every weekday
```

## Timezones
Without `timeZone`, the cron is read in the same timezone JavaScript's
`Date` uses.  Any name from the timezone database works (`"Europe/Paris"`,
`"Asia/Tokyo"`, `"UTC"`...).  Daylight saving follows that timezone: a time
skipped when the clocks go forward isn't matched, and a repeated hour when
they go back is matched both times.

## Support
- Questions and bugs: [GitHub issues](https://github.com/caronc/dateblock/issues)
- The full project (command line tool, Python, PHP):
  [github.com/caronc/dateblock](https://github.com/caronc/dateblock)
- Like it?  [Sponsor](https://github.com/sponsors/caronc) or
  [donate](https://www.paypal.com/donate/?hosted_button_id=CR6YF7KLQWQ5E).

## License
GPL-2.0-or-later.  See
[COPYING](https://github.com/caronc/dateblock/blob/master/COPYING).
