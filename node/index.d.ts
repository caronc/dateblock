// Types for @caronc/dateblock

export interface NextOptions {
  /** Start from this moment (a Date, or milliseconds since the epoch) instead of now. */
  ref?: Date | number;
  /** Read the cron as a regular 5 field crontab entry (no seconds field). */
  isc?: boolean;
  /** Read the cron in this timezone (ie: "Asia/Tokyo") instead of the local one. */
  timeZone?: string;
}

export interface UpcomingOptions extends NextOptions {
  /** How many to return (1 to COUNT_MAX; COUNT_DEFAULT unless given). */
  count?: number;
}

export interface DateblockOptions extends NextOptions {
  /** Aborting it stops the wait; the promise is rejected with the signal's reason. */
  signal?: AbortSignal;
}

/**
 * Works out the next moment that matches a cron string, without waiting.
 *
 * @throws {SyntaxError} the cron is not written correctly
 * @throws {RangeError} the cron never happens, or the timezone or reference is invalid
 */
export function next(cron: string, options?: NextOptions): Date;

/**
 * Works out the next few moments that match a cron string, without waiting.
 * Returns them earliest first; the list is shorter than count when no later
 * moment can be found.
 *
 * @throws {SyntaxError} the cron is not written correctly
 * @throws {RangeError} the cron never happens, the count is out of range, or
 *   the timezone or reference is invalid
 */
export function upcoming(cron: string, options?: UpcomingOptions): Date[];

/** How many times upcoming() returns unless told otherwise (1: just the next one). */
export const COUNT_DEFAULT: number;

/** The most times upcoming() returns (1000). */
export const COUNT_MAX: number;

/**
 * Waits until the next moment that matches a cron string, and resolves to it.
 * Node.js keeps running other work while it waits.
 */
export function dateblock(cron: string, options?: DateblockOptions): Promise<Date>;
