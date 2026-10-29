// Dateblock blocks (sleeps) until a cron-style moment in time.
// Copyright (C) 2013-2026 Chris Caron <lead2gold@gmail.com>
//
// This file is part of Dateblock.  Dateblock is free software; you can
// redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation; either version 2 of
// the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along with
// this program; if not, see <https://www.gnu.org/licenses/>.
'use strict';

// The C++ add-on, compiled by npm when the package is installed.  npm 12
// and newer only run a package's install script once it is approved, so
// say how when it is missing.
let addon;
try {
  addon = require('../build/Release/dateblock.node');
} catch (error) {
  if (error.code !== 'MODULE_NOT_FOUND') {
    throw error;
  }
  throw new Error(
    "@caronc/dateblock wasn't built when it was installed (npm 12 and newer " +
    'skip install scripts until they are approved).  Run:\n' +
    '  npm install-scripts approve @caronc/dateblock\n' +
    '  npm rebuild @caronc/dateblock\n' +
    '(building needs a C++ compiler and Python; see ' +
    'https://github.com/nodejs/node-gyp#installation)', { cause: error });
}
const { cronAfter } = addon;

// How often a long wait checks the clock again (in milliseconds).  Timers
// stop counting while the computer sleeps and don't notice the clock being
// changed, so a long wait is made of short timers that each look at the
// real time when they wake.
const CHECK_EVERY = 60 * 1000;

// Returns a function that gives [year, month, day, hour, minute, second]
// for a moment (in seconds since the epoch) as the clocks in 'timeZone'
// show it, or null for a moment a Date can't hold.  Intl knows every
// timezone's daylight saving rules.  Throws a RangeError for an unknown
// timezone.
function zoneReader(timeZone) {
  const format = new Intl.DateTimeFormat('en-US', {
    timeZone,
    hourCycle: 'h23',
    era: 'short',
    year: 'numeric',
    month: 'numeric',
    day: 'numeric',
    hour: 'numeric',
    minute: 'numeric',
    second: 'numeric',
  });

  return (seconds) => {
    const date = new Date(seconds * 1000);
    if (Number.isNaN(date.getTime())) {
      return null;
    }
    const part = {};
    for (const { type, value } of format.formatToParts(date)) {
      part[type] = value;
    }
    // Years before 1 AD are shown as 1 BC, 2 BC... (year 0 is 1 BC)
    let year = Number(part.year);
    if (part.era === 'BC' || part.era === 'B') {
      year = 1 - year;
    }
    return [year, Number(part.month), Number(part.day),
            Number(part.hour) % 24, Number(part.minute), Number(part.second)];
  };
}

// The options both functions take, checked
function readOptions(cron, options) {
  if (typeof cron !== 'string') {
    throw new TypeError('cron must be a string');
  }
  if (options === undefined || options === null) {
    options = {};
  } else if (typeof options !== 'object') {
    throw new TypeError('options must be an object');
  }

  const { ref, isc = false, timeZone, signal } = options;
  if (typeof isc !== 'boolean') {
    throw new TypeError('options.isc must be true or false');
  }

  // The moment to start from, in milliseconds since the epoch
  let start;
  if (ref === undefined || ref === null) {
    start = Date.now();
  } else if (ref instanceof Date) {
    start = ref.getTime();
  } else if (typeof ref === 'number') {
    start = ref;
  } else {
    throw new TypeError('options.ref must be a Date or milliseconds since the epoch');
  }
  if (!Number.isFinite(start) || Math.abs(start) > 8.64e15) {
    throw new RangeError('options.ref is outside of the supported time range');
  }

  if (timeZone !== undefined && typeof timeZone !== 'string') {
    throw new TypeError('options.timeZone must be a string (ie: "Asia/Tokyo")');
  }
  // No timezone means the one JavaScript's Date uses
  const zone = timeZone === undefined ?
    Intl.DateTimeFormat().resolvedOptions().timeZone : timeZone;

  return { isc, start, zone, signal };
}

// Works out the next moment that matches 'cron', without waiting.
//  - options.ref:      start from this Date (or milliseconds since the
//                      epoch) instead of now
//  - options.isc:      true to read a regular 5 field crontab entry
//  - options.timeZone: read the cron in this timezone (ie: "Asia/Tokyo")
//                      instead of the local one
// Returns a Date.  Throws a SyntaxError for a cron that isn't written
// correctly, and a RangeError for one that never happens, an unknown
// timezone or a reference too far away.
function next(cron, options) {
  const { isc, start, zone } = readOptions(cron, options);
  const toLocal = zoneReader(zone);

  const result = cronAfter(Math.floor(start / 1000), cron, isc, toLocal);
  if (result.error === 'syntax') {
    throw new SyntaxError(`The cron is not formatted correctly: ${result.message}`);
  }
  if (result.error === 'never' || result.error === 'range') {
    throw new RangeError(result.message);
  }
  if (result.error) {
    throw new Error(result.message);
  }

  const when = new Date(result.when * 1000);
  if (Number.isNaN(when.getTime())) {
    throw new RangeError('The time found is outside of the supported range.');
  }
  return when;
}

// Waits until 'when' (milliseconds since the epoch), unless 'signal' is
// aborted first
function waitUntil(when, signal) {
  return new Promise((resolve, reject) => {
    let timer;
    const onAbort = () => {
      clearTimeout(timer);
      reject(signal.reason);
    };
    const tick = () => {
      const left = when - Date.now();
      if (left <= 0) {
        if (signal) {
          signal.removeEventListener('abort', onAbort);
        }
        resolve();
      } else {
        timer = setTimeout(tick, Math.min(left, CHECK_EVERY));
      }
    };

    if (signal) {
      if (signal.aborted) {
        reject(signal.reason);
        return;
      }
      signal.addEventListener('abort', onAbort, { once: true });
    }
    tick();
  });
}

// Waits until the next moment that matches 'cron'.  Takes the same
// options as next(), plus:
//  - options.signal: an AbortSignal; aborting it stops the wait (the
//                    promise is rejected with the signal's reason)
// Returns a promise for the Date it waited for.  Node.js keeps running
// other work while it waits.
async function dateblock(cron, options) {
  const when = next(cron, options);
  const { signal } = options || {};
  if (signal !== undefined && signal !== null && !(signal instanceof AbortSignal)) {
    throw new TypeError('options.signal must be an AbortSignal');
  }
  await waitUntil(when.getTime(), signal);
  return when;
}

module.exports = { dateblock, next };
