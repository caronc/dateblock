[![Paypal](https://img.shields.io/badge/paypal-donate-green.svg)](https://www.paypal.com/donate/?hosted_button_id=CR6YF7KLQWQ5E)
[![PyPI](https://img.shields.io/pypi/v/dateblock.svg)](https://pypi.org/project/dateblock/)
[![Python](https://img.shields.io/pypi/pyversions/dateblock.svg)](https://pypi.org/project/dateblock/)

Dateblock
=========
Dateblock allows you to block (exactly how a Linux/Unix Cron might) _until_ a
specific point of time unlike 'sleep' which blocks _for_ a set period of time.
The difference is very subtle but can prove to be very useful.

It offers a few new features that the Internet Systems Consortium (ICS) version
does not (refering to /etc/crontab style). Dateblock specifically offers the ability
to wake/unblock on a _specific second_ and/or _drift_ for certain period afterwards
your calculated time is met (kind of like combining _sleep_ and _ICS Cron_ into one).

Python bindings are additionally included supporting all of the standard datetime
formats and objects!

## Quick Start
The `dateblock` command is a C++ tool; install it from the distribution
packages or build it from source (see [Installation](#installation)).
```bash
# When would the next 5th minute start?  (--test shows without waiting)
dateblock -t -n /5

# Wait for 9:00 in Tokyo, whatever timezone this computer is in
dateblock -z Asia/Tokyo -c "0 0 9"

# Run a backup at the next quarter hour (dateblock exits with its exit code)
dateblock -n /15 -- ./backup.sh --full
```

The Python library is on PyPI (Linux, macOS and Windows):
```bash
pip install dateblock
```

Packages for Fedora, RHEL/CentOS/Alma/Rocky 9 and 10, Debian and Arch Linux
can be built from this repository; see [Installation](#installation).

__Syntax__:
dateblock [options] [--] [command [arguments...]]

__Allowed options__:
```
  -h [ --help ]         Show this help screen.
  -t [ --test ]         Test (Do not block for the specified period)
  -v [ --verbose ]      Verbose mode
  -i [ --isc ]          ISC Mode
  -s [ --second ] arg   Second (0-59)
  -n [ --minute ] arg   Minute (0-59)
  -o [ --hour ] arg     Hour (0-23)
  -d [ --dom ] arg      Day of Month (1-31)
  -m [ --month ] arg    Month (1-12) {Jan=1,...,Dec=12}
  -w [ --dow ] arg      Day of Week (0-6) {Sun=0,...,Sat=6}
  -c [ --cron ] arg     Cron string formatting
  -x [ --drift ] arg    Additional drift time (in seconds).
  -z [ --timezone ] arg Timezone to read the schedule in (ie:
                        America/Toronto); defaults to this computer's.

  Anything after the options (or after --) is a command to run once the
  time is reached.
```

A variety of syntax is accepted by this tool such as:
```
  x               (Value)     where 'x' is represented numerically.
  */x (or /x)     (Modulus)   where 'x' is represented numerically.
  x-y (or y-x)    (Range)     where 'x' and 'y' are are represented numerically.
  x,y             (Separator) where 'x' and 'y' are are represented numerically.
```

__Note__: With the exception of the drift option (+), all variations of the syntax
      mentioned above can be mixed if separated using the 'comma' (Separator)
      operator.  ie: */a,b,c-d,/e is valid.
      However: x-y-z is not valid, nor is /x/y, /x-y or a '*' mixed with
      other entries (ie: *,5). All values must be
      within the range of it's time type. Thus 400-4000 would never work since
      no time constraint even resides within that range.

The __--cron__(__-c__) switch allows one to specify the standard cron formatting:
```
     drift time ----------------------------------------------+
     day of week (0 - 6) (Sunday=0) -----------------------+  |
     month (1 - 12) ------------------------------------+  |  |
     day of month (1 - 31) --------------------------+  |  |  |
     hour (0 - 23) -------------------------------+  |  |  |  |
     min (0 - 59) -----------------------------+  |  |  |  |  |
     sec (0 - 59) ---------------------------  |  |  |  |  |  |
                                            |  |  |  |  |  |  |
                                            -  -  -  -  -  -  -
                                            *  *  *  *  *  *  *
```
Substitute an asterisk (*) as a placeholder for arguments you are not
interested in. Asterisks are automatically placed in strings missing all 6
arguments (separated by white space).

The __--isc__ (__-i__) switch makes the cron interpretation equivalent to
what it is today (ISC stands for Internet Systems Consortium). Hence:
```
         ISC Format (no second or drift)

     +----------------------------- min (0 - 59)
     |  +----------------------- hour (0 - 23)
     |  |  +----------------- day of month (1 - 31)
     |  |  |  +----------- month (1 - 12)
     |  |  |  |  +----- day of week (0 - 6) (Sunday=0)
     |  |  |  |  |
     -  -  -  -  -
     *  *  *  *  *
```

The __--test__(__-t__) switch is your greatest ally.  Use this to simply build
and test your crons to find out when they __would have__ blocked until without
actually blocking.  If you pair the the test with the __--ics__ (__-i__), then
you can use this tool to help construct regular crons for _/etc/crontab_ if
you're new to the idea and still learning how the crontabs work.

### Timezones:
The schedule is read in this computer's timezone unless __--timezone__
(__-z__) names another one from the timezone database (such as
`America/Toronto` or `Asia/Tokyo`). Daylight saving follows that timezone,
and dateblock unblocks at that exact moment no matter where the computer
is. The times shown by __--test__ and __--verbose__ are in that timezone too.
```bash
# 9:00 in Tokyo
dateblock -t -z Asia/Tokyo -c "0 0 9"
Current Time : 2026-10-24 22:05:00 JST (Sat)
Block Until  : 2026-10-25 09:00:00 JST (Sun)
```

### Running a command:
Put a command (and its arguments) after the options, and dateblock runs it
once the time is reached:
```bash
dateblock -n /15 -- ./backup.sh --full
dateblock -i -c "0 9 * * 1-5" notify-send "Stand-up"
```
- dateblock _becomes_ the command (the way `nice` and `timeout` do), so the
  exit code, signals and output are all the command's own.
- The command starts at `--`, or at the first argument that isn't one of
  dateblock's options; everything after that is passed to the command as
  is (no shell is involved), so `dateblock -n 5 ls -l` runs `ls -l`.
- With __--test__ (__-t__) the command is only shown, not run.
- If the schedule is invalid the command is never run (exit code 1). A
  command that can't be found exits with 127, and one that can't be run
  with 126 (just like a shell).

To run it again and again, put dateblock in a loop:
```bash
while dateblock -n /15; do ./backup.sh; done
```

### Drifting:
Drifting is an option that allows you to adjust the calculated results by some
additional time.  Lets say you wanted the application to wake up on the 1st
minute of each 10 min interval (1, 11, 21, 31, 41, 51). Specifying the cron
(minute) '*/1' would not work. In fact the cron of '*/1' would unblock at
every minute. You could however achieve the previous example using a drift
value of '60' (seconds) and a cron (minute) entry of '*/10'.

In a way, drifting is a little like executing a cron and then sleeping for
an additional period right afterwards.  However this is a much more elegant
solution with a more precise blocking period.

Some things to consider about drifting:

- Drifting is always calculated 'after' a specified cron.
- You can not specify a drifting value on it's own without a cron entry to go
  with it.
- A drift value is always specified in seconds.
- You can not drift longer then the interval calculated by the cron itself.
  In the event this occurs, only the remainder (modulus) is kept; the rest is
  is considered overflow and will simply be ignored.  Drifting behaves this
  way to prevent missing a segment of time that would have otherwise been
  calculated. For example... lets say you wanted to unblock every hour, your
  cron might look like this: ```dateblock --hour=/1```. With this cron in
  mind, you could never add a drift of a value larger then 1 hour minus 
  1 second (3659 seconds)... Why? Because the _--hour_ entry has already
  pre-defined the time (slice) bounds of 1, therefore a drift value of the
  same (or more) would cause the calculation to spill into _the next_ hour. An
  hour it was pre-calculated to wake on anyway.

  Let's say you wanted to wake ever 2 hours. Well your cron would look like this
  ```dateblock --hour=/2```.  If you wanted to add a drift into this picture,
  you'd have up to 2 hours minus 1 second (7319) to work with.

  However... if you specify a a cron of '*/10' (which would equate to
  unblocking every 10 seconds), your drift window is only a maximum of 9. So
  if you specified a drift of of 11 (seconds), the overflow would leave you
  with an actual drift of '1' (not 11). If you specified 10, then you would
  not drift at all (would be the same as not setting anything at all). This
  is how the modulus works when you overflow the drift value.

- You can use the shortcut character of a _plus_ (_+_) inline on a cron to immediately
  invoke the drift and spare you from writing the extra entries. Hence:
  ```dateblock -c '* */5 +60'``` is the same as writing ```dateblock -c '* */5 * * * * 60'```
  Once you invoke the __+__ character however, any entries found afterwards
  will be treated as an error.

### Examples
The following would block until a minute divisible by 10 was reached.  Minutes
divisible by 10 are: 0,10,20,30,40 and 50.
```bash
dateblock -n /10
```

The following would block until a second is divisible by 5 was reached and
only on hours 3 and 4. This can also be written as such:
```bash
dateblock -s /5 -o 3,4

# you can rewrite the above like this too:
dateblock -c "/5 * 3,4 * * *"
```

The following would block until the 5 minute of every hour:
```bash
dateblock -c "* 5"

# you can rewrite the above like this too:
dateblock -n 5
dateblock -c "* 5 * * * *"
```

The following would block until a hour divisible by 5 was reached on the first
half of the month as well as the 20th of the month) Hours divisible by 5 are:
0, 5,10,15 and 20.
```bash
dateblock -o /5 -d 1-14,20
```

Drifting using the cron was added in v1.0.0 (but was always available using
__-x__ or __--drift__). The advantage you get from using it inline with the
cron string is the ability to chain them with multiple drift times using
a comma and or range operator. The modulus one will not work here.

The following will block until the 5:20 minute mark of each hour (20 seconds
past)
```bash
dateblock -c "* 5 * * * * 20"

# you can rewrite the above like this too:
dateblock -n 5 -x 20

# You can skip to the drift portion by just denoting a plus (+) as the last
# entry specified.  This is interpreted as a drift time.
dateblock -c "* 5 +20"
```
The expression is considered invalid if more entries are found after the drift
period. However this is still valid:
```bash
dateblock -c "* 5 * * * * +20"
```

### DateBlock Python Bindings
The DateBlock python bindings are really easy to use too:
```python
from dateblock import dateblock
from datetime import datetime, date, time, timedelta

# Returns the date object it will unblock at
result = dateblock("* /20", block=False)

print("Blocking until %s" % result.strftime("%Y-%m-%d %H:%M:%S"))
dateblock("* /20")
print("Unblocked at %s" % datetime.now().strftime("%Y-%m-%d %H:%M:%S"))

# Perhaps you want to calculate a time relative to somewhere in the past?
# No problem; lets get a relative time 4 days ago (from now)
relative = datetime.now() - timedelta(days=4)
result = dateblock("* /20", ref=relative, block=False)

# To use the isc switch with the bindings, just set isc=True (by default
# it is off). Below would block for the start of each minute (not second
# had you not set the isc flag)
result = dateblock("/1", isc=True)

# Dateblock supports date, time and epoch time too!
#
# Date: (yyyy, mm, dd)
result = dateblock("/5", ref=date(2017, 4, 22))

# Time: (hh, mm, ss)
result = dateblock("/5", ref=time(10, 5, 0))

# Epoch: seconds
result = dateblock("/5", ref=13424236)

# Timezones: an aware datetime (or time) reads the cron in its own
# timezone, and the result comes back in that same timezone.  This waits
# for 9:00 in Tokyo, whatever timezone this computer is in.
from zoneinfo import ZoneInfo
tokyo = ZoneInfo("Asia/Tokyo")
result = dateblock("0 0 9", ref=datetime.now(tokyo))
```

A few things worth knowing:
- A naive `ref` (or none, a `date`, epoch or `timedelta`) uses this
  computer's timezone and returns a naive `datetime`.
- A timezone aware `ref` uses its own timezone, including its daylight
  saving rules, and returns an aware `datetime` with the same `tzinfo`.
  Blocking always ends at that exact moment.
- An invalid cron string raises `SyntaxError`; a cron that can never happen
  (such as February 31st) raises `ValueError`.
- While blocking, other Python threads keep running and Ctrl-C
  (`KeyboardInterrupt`) works as expected.

## Installation
### From PyPI (the Python library)
```bash
pip install dateblock
```
This gives you the Python library on Linux, macOS and Windows (the C++ code
is compiled straight into it, so nothing else is needed). The `dateblock`
command is the C++ tool; install it from the packages or build it from
source as shown below.

### From Source
Assuming you have a C++ compiler and the standard development tools that
usually go with it (make, autoconf, automake and libtool), then the
following will install everything for you.

```bash
./autogen.sh
./configure
make
make check
sudo make install
```

This installs:
- `libdateblock.so` (the shared library holding all of the date logic) and
  its headers in `include/dateblock/`, with a `dateblock.pc` file for
  `pkg-config`.
- The `dateblock` command line tool.

To build the Python bindings from your checkout (this compiles the C++ code
straight into the Python module):
```bash
pip install .
```

### Building with Docker
Don't want to install the build tools? If you have
[Docker](https://www.docker.com/) (or [Podman](https://podman.io/)) and
GNU make, you can build and test everything inside a container straight
from a fresh checkout; no `./configure` needed:

```bash
# Compile and run all of the tests
make docker-build

# Build packages (each one is tested while it builds)
make docker-rpm                 # Fedora 44 RPMs
make docker-rpm DISTRO=el9      # EL9 RPMs (or DISTRO=el10)
make docker-rpm-all             # the RPMs for Fedora 44, EL9 and EL10
make docker-deb                 # Debian 13 .deb packages
make docker-arch                # Arch Linux packages
make docker-all                 # every one of the above

# Build for 64-bit ARM instead (ie: Raspberry Pi); runs under emulation,
# so it needs qemu-user-static and is much slower (no Arch Linux image)
make docker-rpm ARCH=aarch64

# The Python source package and Linux wheels (needs uv or pipx)
make docker-pypi
make docker-pypi PYPI_ARCHS="x86_64 aarch64"

# Rebuild the man page after editing packaging/man/dateblock.md
make docker-man

# Open a shell inside the build container to poke around
make docker-shell

# Clean up: 'make clean' removes dist/ and the other build files in the
# checkout; 'make release-clean' also removes the build images
make clean
make release-clean
```

- Results are written to `dist/<distro>/` (`dist/<distro>-aarch64/` for
  ARM, `dist/pypi/` for Python).
- Your checkout is mounted read-only, so the container never changes it.
- Podman is used if Docker isn't installed; set `CONTAINER=podman` to
  prefer it.

See [packaging/README.md](https://github.com/caronc/dateblock/blob/master/packaging/README.md) for the details on each
package format, and [INSTALL.md](https://github.com/caronc/dateblock/blob/master/INSTALL.md) for building from source.
