--TEST--
dateblock() explains bad input with an exception
--EXTENSIONS--
dateblock
--INI--
date.timezone=UTC
--FILE--
<?php
function attempt(callable $call)
{
    try {
        $call();
        echo "no exception\n";
    } catch (Throwable $e) {
        echo get_class($e), ': ', $e->getMessage(), "\n";
    }
}

// A cron that isn't written correctly
attempt(fn() => dateblock('* 99', false));
attempt(fn() => dateblock('0 *,5', false));
attempt(fn() => dateblock('5-', false));
attempt(fn() => dateblock("0\0 5", false));
attempt(fn() => dateblock(str_repeat('5', 100000), false));
// The longest cron allowed works; one byte more is refused
attempt(fn() => dateblock('0' . str_repeat(' ', 65535), false)->format('s'));
attempt(fn() => dateblock('0' . str_repeat(' ', 65535) . "\u{e9}", false));
attempt(fn() => dateblock('*/15 * * * * *', false, null, true));

// A cron that never happens
attempt(fn() => dateblock('0 0 0 30 2', false));

// A reference of the wrong type, or out of range
attempt(fn() => dateblock('/1', false, 'tomorrow'));
attempt(fn() => dateblock('/1', false, []));
attempt(fn() => dateblock('/1', false, new stdClass()));
attempt(fn() => dateblock('/1', false, NAN));
attempt(fn() => dateblock('/1', false, INF));
attempt(fn() => dateblock('/1', false, 1e300));
attempt(fn() => dateblock('/1', false, PHP_INT_MAX));
attempt(fn() => dateblock('/1', false, PHP_INT_MIN));
attempt(fn() => dateblock('/1', false, 1e18));
attempt(fn() => dateblock('/1', false, -1e18));
attempt(fn() => dateblock('/1', false, new DateTimeImmutable('-292277022657-01-27 08:29:52 UTC')));

// The wrong number of arguments
attempt(fn() => dateblock());
?>
--EXPECT--
ValueError: dateblock(): Argument #1 ($cron) is not a valid cron: minute '99': 99 is outside of 0-59
ValueError: dateblock(): Argument #1 ($cron) is not a valid cron: minute '*,5': '*' can't be mixed with other entries (use '*' on its own)
ValueError: dateblock(): Argument #1 ($cron) is not a valid cron: second '5-': the end of the range: a number is missing at the end
ValueError: dateblock(): Argument #1 ($cron) is not a valid cron: second '0\x00': '\x00' is not understood (each entry is *, x, x-y, x-y/z or */x)
ValueError: dateblock(): Argument #1 ($cron) is not a valid cron: the cron is too long (more than 65536 characters)
no exception
ValueError: dateblock(): Argument #1 ($cron) is not a valid cron: the cron is too long (more than 65536 characters)
ValueError: dateblock(): Argument #1 ($cron) is not a valid cron: there are too many fields; a crontab entry has 5 (minute, hour, day of month, month and day of week)
RuntimeException: No moment in time matches this cron (none in the next 400 years).
TypeError: dateblock(): Argument #3 ($ref) must be of type DateTimeInterface|int|float|null, string given
TypeError: dateblock(): Argument #3 ($ref) must be of type DateTimeInterface|int|float|null, array given
TypeError: dateblock(): Argument #3 ($ref) must be of type DateTimeInterface|int|float|null, stdClass given
ValueError: dateblock(): Argument #3 ($ref) is outside of the supported time range
ValueError: dateblock(): Argument #3 ($ref) is outside of the supported time range
ValueError: dateblock(): Argument #3 ($ref) is outside of the supported time range
ValueError: dateblock(): Argument #3 ($ref) is outside of the supported time range
ValueError: dateblock(): Argument #3 ($ref) is outside of the supported time range
ValueError: dateblock(): Argument #3 ($ref) is outside of the supported time range
ValueError: dateblock(): Argument #3 ($ref) is outside of the supported time range
ValueError: dateblock(): Argument #3 ($ref) is outside of the supported time range
ArgumentCountError: dateblock() expects at least 1 argument, 0 given
