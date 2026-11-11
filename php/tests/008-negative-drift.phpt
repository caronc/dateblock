--TEST--
A negative drift wakes up that many seconds before the matched time
--EXTENSIONS--
dateblock
--INI--
date.timezone=UTC
--FILE--
<?php
// 2026-11-10 00:00:00 UTC
$ref = 1794268800;
$fmt = 'Y-m-d H:i:s T';

// 6 days (518400 seconds) before 9:00 on December 25th, written each way
// a drift can be given
foreach (['0 0 9 25 12 * -518400', '0 0 9 25 12 -518400',
          '0 0 9 25 12 +-518400'] as $cron) {
    echo dateblock($cron, false, $ref)->format($fmt), "\n";
}

// Christmas is still to come, but its early time has passed: next year
echo dateblock('0 0 9 25 12 * -518400', false,
               gmmktime(9, 0, 0, 12, 19, 2026))->format($fmt), "\n";
echo dateblock('0 0 9 25 12 * -518400', false,
               gmmktime(0, 0, 0, 12, 22, 2026))->format($fmt), "\n";

// Negative and positive drifts mixed: the earliest still to come wins
echo dateblock('0 0 10 20 12 * -3600,3600', false,
               gmmktime(9, 30, 0, 12, 20, 2026))->format($fmt), "\n";

// Read in the timezone of the reference
$tokyo = new DateTimeImmutable('2026-11-10', new DateTimeZone('Asia/Tokyo'));
echo dateblock('0 0 9 25 12 -518400', false, $tokyo)->format($fmt), "\n";

// Each listed time follows the one before it
foreach (dateblock_upcoming('@daily -3600', 3, $ref) as $when) {
    echo $when->format($fmt), "\n";
}

// Below the lowest drift is refused
try {
    dateblock('* * * * * * -2147483648', false);
} catch (ValueError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
2026-12-19 09:00:00 UTC
2026-12-19 09:00:00 UTC
2026-12-19 09:00:00 UTC
2027-12-19 09:00:00 UTC
2027-12-19 09:00:00 UTC
2026-12-20 11:00:00 UTC
2026-12-19 09:00:00 JST
2026-11-10 23:00:00 UTC
2026-11-11 23:00:00 UTC
2026-11-12 23:00:00 UTC
dateblock(): Argument #1 ($cron) is not a valid cron: drift '-2147483648': -2147483648 is outside of -2147483647 to 2147483647
