--TEST--
dateblock() works out the next matching moment
--EXTENSIONS--
dateblock
--INI--
date.timezone=UTC
--FILE--
<?php
// 2026-01-01 00:00:00 UTC
$ref = 1767225600;
$fmt = 'Y-m-d H:i:s T';

var_dump(extension_loaded('dateblock'));
var_dump(phpversion('dateblock') === trim(phpversion('dateblock')));

$next = dateblock('0 */5', false, $ref);
var_dump(get_class($next));
echo $next->format($fmt), "\n";

// Named arguments, a float reference (the fraction is dropped), inline
// drift, the drift field and a range step
echo dateblock('0 */5', block: false, ref: $ref + 0.75)->format($fmt), "\n";
echo dateblock('0 0 /6 +4', false, $ref)->format($fmt), "\n";
echo dateblock('0 0 /6 * * * 4', false, $ref)->format($fmt), "\n";
echo dateblock('0 0 1-10/3 * * *', false, $ref)->format($fmt), "\n";
echo dateblock('0 0 0 * 3', false, $ref)->format($fmt), "\n";

// Regular crontab entries (no seconds field)
echo dateblock('*/15 * * * *', false, $ref, true)->format($fmt), "\n";
echo dateblock('0 0 13 * 5', false, $ref, isc: true)->format($fmt), "\n";

// Leap days, including the century rule (2100 is not a leap year)
echo dateblock('0 0 0 29 2', false, $ref)->format($fmt), "\n";
echo dateblock('0 0 0 29 2', false, gmmktime(0, 0, 0, 3, 1, 2097))->format($fmt), "\n";
echo dateblock('0 0 0 29 2 1', false, $ref)->format($fmt), "\n";

// The largest year a date can hold still works
echo dateblock('/1', false, 67767976233316800)->format($fmt), "\n";

// No fraction of a second in the result
echo dateblock('/1', false)->format('u'), "\n";
?>
--EXPECT--
bool(true)
bool(true)
string(17) "DateTimeImmutable"
2026-01-01 00:05:00 UTC
2026-01-01 00:05:00 UTC
2026-01-01 00:00:04 UTC
2026-01-01 00:00:04 UTC
2026-01-01 01:00:00 UTC
2026-03-01 00:00:00 UTC
2026-01-01 00:15:00 UTC
2026-01-02 00:00:00 UTC
2028-02-29 00:00:00 UTC
2104-02-29 00:00:00 UTC
2044-02-29 00:00:00 UTC
2147483647-12-29 12:00:01 UTC
000000
