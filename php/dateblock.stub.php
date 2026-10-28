<?php

/**
 * Block (sleep) until the next moment that matches a cron string.
 *
 * - $cron:  a dateblock cron string ('sec min hour dom month dow drift'),
 *           or a regular crontab string when $isc is true.
 * - $block: set to false to only work out the time (no sleeping).
 * - $ref:   work it out from this moment instead of now (a DateTime,
 *           DateTimeImmutable or seconds since the epoch).
 * - $isc:   treat the cron string as a regular 5 field crontab entry.
 *
 * The cron is read in the timezone of $ref (when it is a DateTime or
 * DateTimeImmutable), otherwise in PHP's default timezone
 * (date_default_timezone_get()).  The result is in that same timezone.
 *
 * @throws ValueError       the cron is not written correctly
 * @throws RuntimeException no moment in time ever matches the cron
 *
 * @generate-function-entries
 */
function dateblock(
    string $cron,
    bool $block = true,
    DateTimeInterface|int|float|null $ref = null,
    bool $isc = false
): DateTimeImmutable {}
