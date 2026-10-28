--TEST--
dateblock() describes its own arguments
--EXTENSIONS--
dateblock
--FILE--
<?php
$f = new ReflectionFunction('dateblock');
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
0 string $cron
1 bool $block = true
2 DateTimeInterface|int|float|null $ref = NULL
3 bool $isc = false
DateTimeImmutable
