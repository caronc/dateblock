// Types for @caronc/dateblock

export interface NextOptions {
  /** Start from this moment (a Date, or milliseconds since the epoch) instead of now. */
  ref?: Date | number;
  /** Read the cron as a regular 5 field crontab entry (no seconds field). */
  isc?: boolean;
  /** Read the cron in this timezone (ie: "Asia/Tokyo") instead of the local one. */
  timeZone?: string;
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
 * Waits until the next moment that matches a cron string, and resolves to it.
 * Node.js keeps running other work while it waits.
 */
export function dateblock(cron: string, options?: DateblockOptions): Promise<Date>;
