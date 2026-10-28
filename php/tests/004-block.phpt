--TEST--
dateblock() sleeps until the moment arrives
--EXTENSIONS--
dateblock
--INI--
date.timezone=UTC
--FILE--
<?php
$start = microtime(true);
$next = dateblock('/1');
$now = microtime(true);
var_dump($now >= $next->getTimestamp());
var_dump($now - $start <= 2.0);

// A reference in the past doesn't block (its next match has passed)
$start = microtime(true);
dateblock('0 0 0', true, 86400);
var_dump(microtime(true) - $start < 1.0);
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
