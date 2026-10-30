--TEST--
dateblock() reads month and weekday names, and shortcuts such as @daily
--EXTENSIONS--
dateblock
--INI--
date.timezone=UTC
--FILE--
<?php
// 2026-01-01 00:00:00 UTC (a Thursday)
$ref = 1767225600;
$fmt = 'Y-m-d H:i:s D';

echo dateblock('0 0 9 * * Sat,Sun', false, $ref)->format($fmt), "\n";
echo dateblock('0 0 0 1 Mar', false, $ref)->format($fmt), "\n";
echo dateblock('0 9 * * mon-fri', false, $ref, true)->format($fmt), "\n";
echo dateblock('@daily', false, $ref)->format($fmt), "\n";
echo dateblock('@weekly', false, $ref, isc: true)->format($fmt), "\n";
echo dateblock('@daily +30', false, $ref)->format($fmt), "\n";

foreach (['0 0 0 * * Monday', '0 0 0 * * Sat-Sun', '@reboot'] as $cron) {
    try {
        dateblock($cron, false, $ref);
    } catch (Throwable $e) {
        echo get_class($e), ': ', $e->getMessage(), "\n";
    }
}
?>
--EXPECT--
2026-01-03 09:00:00 Sat
2026-03-01 00:00:00 Sun
2026-01-01 09:00:00 Thu
2026-01-02 00:00:00 Fri
2026-01-04 00:00:00 Sun
2026-01-01 00:00:30 Thu
ValueError: dateblock(): Argument #1 ($cron) is not a valid cron: day of week 'Monday': 'Monday' is not a name (use Sun, Mon, Tue, Wed, Thu, Fri or Sat)
ValueError: dateblock(): Argument #1 ($cron) is not a valid cron: day of week 'Sat-Sun': 'Sat-Sun' goes backwards (list them instead, ie: 'Sat,Sun', or use 7 for Sunday)
ValueError: dateblock(): Argument #1 ($cron) is not a valid cron: '@reboot' is not a time (it means when the computer starts)
