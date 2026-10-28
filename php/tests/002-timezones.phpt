--TEST--
dateblock() reads the cron in the timezone of the reference
--EXTENSIONS--
dateblock
--INI--
date.timezone=America/New_York
--FILE--
<?php
$fmt = 'Y-m-d H:i:s T';

// The reference's own timezone is used, and kept in the result
$tokyo = new DateTimeImmutable('2026-01-01 08:00:00', new DateTimeZone('Asia/Tokyo'));
$next = dateblock('0 0 9', false, $tokyo);
echo $next->format($fmt), ' ', $next->getTimezone()->getName(), "\n";

// A DateTime works too, and is left as it was
$paris = new DateTime('2026-06-15 12:30:00', new DateTimeZone('Europe/Paris'));
echo dateblock('0 0 /6', false, $paris)->format($fmt), "\n";
echo $paris->format($fmt), "\n";

// Fixed offsets and abbreviations
$offset = new DateTimeImmutable('2026-01-01 00:00:00+05:30');
echo dateblock('0 30 2', false, $offset)->format('Y-m-d H:i:s P'), "\n";
$abbr = new DateTimeImmutable('2026-01-01 00:00:00 EST');
echo dateblock('0 0 1', false, $abbr)->format('Y-m-d H:i:s T'), "\n";

// A class based on DateTimeImmutable
class MyDate extends DateTimeImmutable {}
echo dateblock('0 0 12', false, new MyDate('2026-01-01', new DateTimeZone('UTC')))->format($fmt), "\n";

// Epoch values (and no reference) use PHP's default timezone, not the
// computer's: 2026-01-01 00:00:00 UTC is 2025-12-31 19:00 in New York
echo dateblock('0 0 20', false, 1767225600)->format($fmt), "\n";
echo dateblock('/1', false)->getTimezone()->getName(), "\n";

// Daylight saving: 2:30 doesn't exist on 2025-03-09 in New York, and the
// clock time stays the same either side of a change
$ny = new DateTimeZone('America/New_York');
echo dateblock('0 30 2', false, new DateTimeImmutable('2025-03-09 00:00', $ny))->format($fmt), "\n";
echo dateblock('0 0 /1', false, new DateTimeImmutable('2025-03-09 01:30', $ny))->format($fmt), "\n";
echo dateblock('0 0 12', false, new DateTimeImmutable('2025-11-01 13:00', $ny))->format($fmt), "\n";

// The 1 o'clock hour happens twice when the clocks go back: the second
// 1:00 is 10 minutes after the first 1:50, and the second 1:20 is followed
// by the second 1:30 (format('U') is used since getTimestamp() mixes the
// two up in PHP 8.0)
foreach (['01:50:00 EDT', '01:20:00 EST'] as $time) {
    $start = (new DateTimeImmutable("2025-11-02 $time"))->setTimezone($ny);
    $next = dateblock('0 /15 1', false, $start);
    echo $next->format($fmt), ' +', $next->format('U') - $start->format('U'), "s\n";
}
?>
--EXPECT--
2026-01-01 09:00:00 JST Asia/Tokyo
2026-06-15 18:00:00 CEST
2026-06-15 12:30:00 CEST
2026-01-01 02:30:00 +05:30
2026-01-01 01:00:00 EST
2026-01-01 12:00:00 UTC
2025-12-31 20:00:00 EST
America/New_York
2025-03-10 02:30:00 EDT
2025-03-09 03:00:00 EDT
2025-11-02 12:00:00 EST
2025-11-02 01:00:00 EST +600s
2025-11-02 01:30:00 EST +600s
