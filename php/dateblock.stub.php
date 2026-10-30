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

/**
 * The next few moments that match a cron string, without waiting.
 *
 * - $cron:  a dateblock cron string, or a regular crontab string when
 *           $isc is true.
 * - $count: how many to return (1 to DATEBLOCK_COUNT_MAX).
 * - $ref:   start from this moment instead of now (a DateTime,
 *           DateTimeImmutable or seconds since the epoch).
 * - $isc:   treat the cron string as a regular 5 field crontab entry.
 *
 * Returns a list of DateTimeImmutable, earliest first, in the same
 * timezone dateblock() would use.  The list is shorter than $count when no
 * later moment can be found.
 *
 * @return list<DateTimeImmutable>
 * @throws ValueError       the cron is not written correctly, or $count
 *                          is out of range
 * @throws RuntimeException no moment in time ever matches the cron
 */
function dateblock_upcoming(
    string $cron,
    int $count = DATEBLOCK_COUNT_DEFAULT,
    DateTimeInterface|int|float|null $ref = null,
    bool $isc = false
): array {}

/**
 * How many times dateblock_upcoming() lists unless told otherwise.
 * @var int
 */
const DATEBLOCK_COUNT_DEFAULT = 1;

/**
 * The most times dateblock_upcoming() lists.
 * @var int
 */
const DATEBLOCK_COUNT_MAX = 1000;
