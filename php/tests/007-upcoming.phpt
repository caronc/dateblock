--TEST--
dateblock_upcoming() lists the next few times without waiting
--EXTENSIONS--
dateblock
--INI--
date.timezone=UTC
--FILE--
<?php
// 2026-01-01 00:00:00 UTC (a Thursday)
$ref = 1767225600;
$fmt = 'Y-m-d H:i:s T D';

echo DATEBLOCK_COUNT_DEFAULT, ' ', DATEBLOCK_COUNT_MAX, "\n";

foreach (dateblock_upcoming('0 9 * * Mon-Fri', 5, $ref, true) as $when) {
    echo $when->format($fmt), "\n";
}
// Unless told otherwise, just the one dateblock() works out
$one = dateblock_upcoming('@daily', ref: $ref);
echo count($one), ' ', $one[0] == dateblock('@daily', false, $ref) ? 'same' : 'DIFFERENT', "\n";

// Each one is the same as dateblock() from the one before it
$list = dateblock_upcoming('*/20 5 1-3 * * * +7', 6, $ref);
$at = $ref;
$same = [];
foreach ($list as $when) {
    $same[] = $when == dateblock('*/20 5 1-3 * * * +7', false, $at) ? 'same' : 'DIFFERENT';
    $at = $when;
}
echo implode(' ', $same), "\n";

// New York, 2025-11-02: the clocks go back, so 01:30 happens twice
$ny = new DateTimeImmutable('2025-11-01 12:00', new DateTimeZone('America/New_York'));
foreach (dateblock_upcoming('0 30 1', 3, $ny) as $when) {
    echo $when->format($fmt . ' U'), "\n";
}

// Near the largest year the engine handles, the list just ends early
// (about 20 years before the year 2147483647)
$far = dateblock_upcoming('@yearly', 1000, 67767975570829464);
echo count($far) > 0 && count($far) < 1000 ? 'ended early' : count($far), "\n";

foreach ([['* 5', 0], ['* 5', 1001], ['0 0 0 31 2 *', 5], ['* 99', 5]]
         as [$cron, $count]) {
    try {
        dateblock_upcoming($cron, $count, $ref);
    } catch (Throwable $e) {
        echo get_class($e), ': ', $e->getMessage(), "\n";
    }
}

$f = new ReflectionFunction('dateblock_upcoming');
foreach ($f->getParameters() as $p) {
    echo $p->getPosition(), ' ', $p->getType(), ' $', $p->getName();
    if ($p->isOptional()) {
        echo ' = ', $p->getDefaultValueConstantName() ?? var_export($p->getDefaultValue(), true);
    }
    echo "\n";
}
echo $f->getReturnType(), "\n";
?>
--EXPECT--
1 1000
2026-01-01 09:00:00 UTC Thu
2026-01-02 09:00:00 UTC Fri
2026-01-05 09:00:00 UTC Mon
2026-01-06 09:00:00 UTC Tue
2026-01-07 09:00:00 UTC Wed
1 same
same same same same same same
2025-11-02 01:30:00 EDT Sun 1762061400
2025-11-02 01:30:00 EST Sun 1762065000
2025-11-03 01:30:00 EST Mon 1762151400
ended early
ValueError: dateblock_upcoming(): Argument #2 ($count) must be between 1 and 1000
ValueError: dateblock_upcoming(): Argument #2 ($count) must be between 1 and 1000
RuntimeException: No moment in time matches this cron (none in the next 400 years).
ValueError: dateblock_upcoming(): Argument #1 ($cron) is not a valid cron: minute '99': 99 is outside of 0-59
0 string $cron
1 int $count = DATEBLOCK_COUNT_DEFAULT
2 DateTimeInterface|int|float|null $ref = NULL
3 bool $isc = false
array
