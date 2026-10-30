'use strict';
// Tests for the Node.js package (run with 'npm test')
const test = require('node:test');
const assert = require('node:assert');
const fs = require('node:fs');
const path = require('node:path');
const { dateblock, next, upcoming, COUNT_DEFAULT, COUNT_MAX } = require('..');

// 2026-01-01 00:00:00 UTC
const REF = Date.UTC(2026, 0, 1);
const utc = (cron, options = {}) =>
  next(cron, { ref: REF, timeZone: 'UTC', ...options }).toISOString();

test('works out the next matching moment', () => {
  assert.strictEqual(utc('0 */5'), '2026-01-01T00:05:00.000Z');
  assert.ok(next('0 */5') instanceof Date);
});

test('drift, range steps and the month field', () => {
  assert.strictEqual(utc('0 0 /6 +4'), '2026-01-01T00:00:04.000Z');
  assert.strictEqual(utc('0 0 /6 * * * 4'), '2026-01-01T00:00:04.000Z');
  assert.strictEqual(utc('0 0 1-10/3 * * *'), '2026-01-01T01:00:00.000Z');
  assert.strictEqual(utc('0 0 0 * 3'), '2026-03-01T00:00:00.000Z');
});

test('month and weekday names, and shortcuts such as @daily', () => {
  // 2026-01-01 is a Thursday
  assert.strictEqual(utc('0 0 9 * * Sat,Sun'), '2026-01-03T09:00:00.000Z');
  assert.strictEqual(utc('0 0 0 1 Mar'), '2026-03-01T00:00:00.000Z');
  assert.strictEqual(utc('0 9 * * mon-fri', { isc: true }), '2026-01-01T09:00:00.000Z');
  assert.strictEqual(utc('@daily'), '2026-01-02T00:00:00.000Z');
  assert.strictEqual(utc('@weekly', { isc: true }), '2026-01-04T00:00:00.000Z');
  assert.strictEqual(utc('@daily +30'), '2026-01-01T00:00:30.000Z');
});

test('regular crontab entries with isc', () => {
  assert.strictEqual(utc('*/15 * * * *', { isc: true }), '2026-01-01T00:15:00.000Z');
  assert.strictEqual(utc('0 0 13 * 5', { isc: true }), '2026-01-02T00:00:00.000Z');
});

test('leap days, including the century rule', () => {
  assert.strictEqual(utc('0 0 0 29 2'), '2028-02-29T00:00:00.000Z');
  assert.strictEqual(utc('0 0 0 29 2', { ref: Date.UTC(2097, 2, 1) }), '2104-02-29T00:00:00.000Z');
  assert.strictEqual(utc('0 0 0 29 2 1'), '2044-02-29T00:00:00.000Z');
});

test('the reference can be a Date or milliseconds (fractions dropped)', () => {
  assert.strictEqual(utc('0 */5', { ref: new Date(REF) }), '2026-01-01T00:05:00.000Z');
  assert.strictEqual(utc('/1', { ref: REF + 750 }), '2026-01-01T00:00:01.000Z');
  assert.strictEqual(next('/1', { ref: REF + 750 }).getMilliseconds(), 0);
});

test('the cron is read in the timezone asked for', () => {
  // 08:00 in Tokyo is 23:00 UTC the day before
  const tokyo = next('0 0 9', { ref: Date.UTC(2025, 11, 31, 23), timeZone: 'Asia/Tokyo' });
  assert.strictEqual(tokyo.toISOString(), '2026-01-01T00:00:00.000Z');
});

test('no timezone means the local one', () => {
  const local = Intl.DateTimeFormat().resolvedOptions().timeZone;
  assert.strictEqual(next('0 0 9', { ref: REF }).getTime(),
                     next('0 0 9', { ref: REF, timeZone: local }).getTime());
});

test('daylight saving changes in New York', () => {
  const ny = (cron, ref) => next(cron, { ref, timeZone: 'America/New_York' }).toISOString();
  // 2:30 doesn't exist on 2025-03-09; the next 2:30 is the day after
  assert.strictEqual(ny('0 30 2', Date.parse('2025-03-09T05:00:00Z')), '2025-03-10T06:30:00.000Z');
  // The hour after the clocks go forward is 3:00
  assert.strictEqual(ny('0 0 /1', Date.parse('2025-03-09T06:30:00Z')), '2025-03-09T07:00:00.000Z');
  // 12:00 either side of the clocks going back
  assert.strictEqual(ny('0 0 12', Date.parse('2025-11-01T17:00:00Z')), '2025-11-02T17:00:00.000Z');
  // 1:00-1:59 happens twice: the second 1:00 follows the first 1:50, and
  // the second 1:30 follows the second 1:20
  assert.strictEqual(ny('0 /15 1', Date.parse('2025-11-02T05:50:00Z')), '2025-11-02T06:00:00.000Z');
  assert.strictEqual(ny('0 /15 1', Date.parse('2025-11-02T06:20:00Z')), '2025-11-02T06:30:00.000Z');
});

test('a cron that is not written correctly is a SyntaxError', () => {
  for (const [cron, isc, message] of [
    ['* 99', false, "minute '99': 99 is outside of 0-59"],
    ['0 *,5', false, "'*' can't be mixed with other entries"],
    ['5-', false, 'a number is missing at the end'],
    ['0\u0000 5', false, 'is not understood'],
    ['5'.repeat(100000), false, 'the cron is too long'],
    ['*/15 * * * * *', true, 'there are too many fields'],
    ['0 0 0 * * Monday', false, "'Monday' is not a name"],
    ['@reboot', false, "'@reboot' is not a time"],
  ]) {
    assert.throws(() => next(cron, { isc }),
      (e) => e instanceof SyntaxError && e.message.includes(message), cron.slice(0, 20));
  }
});

test('the longest cron allowed, and one character more', () => {
  const longest = '0' + ' '.repeat(65535);
  assert.ok(next(longest) instanceof Date);
  // A 2 byte character past the limit is still refused (only the start of
  // a huge string is copied)
  for (const extra of [' ', '\u00e9', '\u{1f600}']) {
    assert.throws(() => next(longest + extra),
      (e) => e instanceof SyntaxError && e.message.includes('the cron is too long'));
  }
});

test('a cron that never happens is a RangeError', () => {
  assert.throws(() => next('0 0 0 30 2'),
    { name: 'RangeError', message: /No moment in time matches this cron/ });
});

test('bad options are refused', () => {
  assert.throws(() => next(5), TypeError);
  assert.throws(() => next('/1', 'x'), TypeError);
  assert.throws(() => next('/1', { ref: 'tomorrow' }), TypeError);
  assert.throws(() => next('/1', { isc: 1 }), TypeError);
  assert.throws(() => next('/1', { timeZone: 5 }), TypeError);
  assert.throws(() => next('/1', { timeZone: 'Nowhere/Land' }), RangeError);
  assert.throws(() => next('/1', { ref: NaN }), RangeError);
  assert.throws(() => next('/1', { ref: Infinity }), RangeError);
  assert.throws(() => next('/1', { ref: new Date(NaN) }), RangeError);
  assert.throws(() => next('/1', { ref: 1e300 }), RangeError);
});

test('the edge of the dates JavaScript can hold', () => {
  // The last second a Date can hold has nothing after it
  assert.throws(() => next('/1', { ref: 8.64e15, timeZone: 'UTC' }),
    { name: 'RangeError', message: /outside of the supported range/ });
  assert.strictEqual(next('/1', { ref: -8.64e15, timeZone: 'UTC' }).getTime(), -8.64e15 + 1000);
});

test('upcoming() lists the next few times', () => {
  const list = (cron, options = {}) =>
    upcoming(cron, { ref: REF, timeZone: 'UTC', ...options }).map((d) => d.toISOString());
  // 2026-01-01 is a Thursday
  assert.deepStrictEqual(list('0 9 * * Mon-Fri', { isc: true, count: 5 }), [
    '2026-01-01T09:00:00.000Z', '2026-01-02T09:00:00.000Z', '2026-01-05T09:00:00.000Z',
    '2026-01-06T09:00:00.000Z', '2026-01-07T09:00:00.000Z',
  ]);
  assert.strictEqual(COUNT_DEFAULT, 1);
  assert.strictEqual(COUNT_MAX, 1000);
  // Unless told otherwise, just the one next() works out
  assert.deepStrictEqual(upcoming('@daily', { ref: REF }), [next('@daily', { ref: REF })]);
  assert.strictEqual(upcoming('/1', { count: COUNT_MAX }).length, COUNT_MAX);

  // Each one is what next() gives from the one before it
  let at = REF;
  for (const when of upcoming('*/20 5 1-3 * * * +7', { ref: REF, count: 6 })) {
    assert.strictEqual(when.getTime(), next('*/20 5 1-3 * * * +7', { ref: at }).getTime());
    at = when;
  }
});

test('upcoming() keeps both copies of a repeated hour', () => {
  // New York, 2025-11-02: the clocks go back, so 01:30 happens twice
  const times = upcoming('0 30 1', {
    ref: Date.UTC(2025, 10, 1, 16), timeZone: 'America/New_York', count: 3,
  });
  assert.deepStrictEqual(times.map((d) => d.toISOString()), [
    '2025-11-02T05:30:00.000Z', '2025-11-02T06:30:00.000Z', '2025-11-03T06:30:00.000Z',
  ]);
});

test('upcoming() ends early at the edge of the dates JavaScript can hold', () => {
  const times = upcoming('@yearly', { ref: 8.64e15 - 20 * 366 * 86400000, timeZone: 'UTC', count: 1000 });
  assert.ok(times.length > 0 && times.length < 1000);
  assert.strictEqual(times[times.length - 1].toISOString(), '+275760-01-01T00:00:00.000Z');
});

test('upcoming() reads each option once', () => {
  const reads = {};
  const options = {};
  const values = { ref: Date.UTC(2026, 0, 1), isc: false, timeZone: 'UTC', count: 3 };
  for (const [name, value] of Object.entries(values)) {
    Object.defineProperty(options, name, {
      enumerable: true,
      get() {
        reads[name] = (reads[name] || 0) + 1;
        return value;
      },
    });
  }
  assert.equal(upcoming('@daily', options).length, 3);
  assert.deepEqual(reads, { ref: 1, isc: 1, timeZone: 1, count: 1 });
});

test('upcoming() refuses a bad count, and reports the first time as next() does', () => {
  for (const count of [0, 1001, 1.5, -1, NaN, Infinity]) {
    assert.throws(() => upcoming('* 5', { count }), RangeError);
  }
  assert.throws(() => upcoming('* 5', { count: '5' }), TypeError);
  assert.throws(() => upcoming('* 99'), SyntaxError);
  assert.throws(() => upcoming('0 0 0 31 2 *'), RangeError);
});

test('dateblock() waits until the moment arrives', async () => {
  const started = Date.now();
  const when = await dateblock('/1');
  assert.ok(Date.now() >= when.getTime());
  assert.ok(Date.now() - started <= 2000);

  // A reference in the past has nothing to wait for
  const quick = Date.now();
  await dateblock('0 0 0', { ref: 86400000 });
  assert.ok(Date.now() - quick < 1000);
});

test('dateblock() wakes on time after the computer sleeps', async (t) => {
  // While a computer sleeps the clock moves on but timers don't count, so
  // the clock (Date.now) and the timers are mocked separately
  let clock = Date.UTC(2026, 0, 1);
  t.mock.method(Date, 'now', () => clock);
  t.mock.timers.enable({ apis: ['setTimeout'] });
  let found;
  dateblock('0 0 9', { timeZone: 'UTC' }).then((when) => { found = when; });

  // Asleep from midnight until 9:00:30; the next check (within a minute of
  // timer time) sees the moment has passed
  clock = Date.UTC(2026, 0, 1, 9, 0, 30);
  t.mock.timers.tick(60 * 1000);
  await new Promise((resolve) => setImmediate(resolve));
  assert.strictEqual(found?.toISOString(), '2026-01-01T09:00:00.000Z');
});

test('dateblock() can be stopped with an AbortSignal', async () => {
  const controller = new AbortController();
  const waiting = dateblock('0 0 0 1 1', { signal: controller.signal });
  controller.abort(new Error('stopped'));
  await assert.rejects(waiting, { message: 'stopped' });

  await assert.rejects(dateblock('/1', { signal: AbortSignal.abort() }), { name: 'AbortError' });
  await assert.rejects(dateblock('/1', { signal: 'no' }), TypeError);
  await assert.rejects(dateblock('* 99'), SyntaxError);
});

test('ES module import works too', async () => {
  const esm = await import('../index.mjs');
  assert.strictEqual(esm.next, next);
  assert.strictEqual(esm.dateblock, dateblock);
  assert.strictEqual(esm.upcoming, upcoming);
  assert.strictEqual(esm.COUNT_MAX, COUNT_MAX);
});

test('the version matches the rest of dateblock', () => {
  const init = path.join(__dirname, '..', '..', 'python', 'dateblock', '__init__.py');
  const version = /^__version__ = "(.*)"$/m.exec(fs.readFileSync(init, 'utf8'))[1];
  assert.strictEqual(require('../../package.json').version, version);
});
