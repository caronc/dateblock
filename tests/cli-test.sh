#!/bin/sh
# Quick checks of the dateblock command line tool.
# Run by 'make check'; DATEBLOCK points at the tool to test.
export TZ=UTC
DATEBLOCK=${DATEBLOCK:-../src/dateblock}
rc=0

fail() {
   echo "FAIL: $*"
   rc=1
}

# --test reports when it would unblock without actually waiting
out=$("$DATEBLOCK" -t -c '* /5') || fail "dateblock -t exited with an error"
echo "$out" | grep -q '^Block Until  : ' || fail "dateblock -t output: $out"

# Bad syntax and impossible dates are both errors (not crashes)
"$DATEBLOCK" -t -c '* 99' >/dev/null 2>&1 && fail "dateblock accepted minute 99"
"$DATEBLOCK" -t -c '0 0 0 31 2 *' >/dev/null 2>&1
[ $? -eq 1 ] || fail "dateblock did not reject February 31st"

# Every way of writing an option gives the same answer
want=$("$DATEBLOCK" -t -n 5 | grep '^Block Until')
for form in "-tn5" "-tn 5" "-t -n5" "-t --minute 5" "-t --minute=5" \
            "-t --min 5" "-t --min=5" "--test --minute=5"; do
   # shellcheck disable=SC2086
   got=$("$DATEBLOCK" $form | grep '^Block Until')
   [ "$got" = "$want" ] || fail "'dateblock $form' gave: $got"
done

# A bad command line is refused (exit code 255), never guessed at
for bad in "--bogus" "-q" "-t --d 5" "-t -n 5 -n 6" "-t -n" "-t -n -t" \
           "-t --minute" "-t --minute=" "-t --test=1 -n 5" "-t5" "-t --=5"; do
   # shellcheck disable=SC2086
   "$DATEBLOCK" $bad >/dev/null 2>&1
   code=$?
   [ $code -eq 255 ] || fail "'dateblock $bad' exited with $code (not 255)"
done
"$DATEBLOCK" -h 2>&1 | grep -q '^  -z \[ --timezone \] arg' || fail "--help is missing --timezone"

# Huge or odd input is refused cleanly (no crash).  Linux allows at most
# 128 KB in a single argument, so 100 KB is about as big as it gets.
"$DATEBLOCK" -t -c "$(head -c 100000 /dev/zero | tr '\0' 5)" >/dev/null 2>&1
[ $? -eq 1 ] || fail "a 100 KB cron string was not refused cleanly"
"$DATEBLOCK" -t -n 5 $(seq 1 20000) >/dev/null 2>&1
[ $? -eq 0 ] || fail "20000 command arguments were not handled"
"$DATEBLOCK" -t -n "$(printf '\001\377')" >/dev/null 2>&1
[ $? -eq 1 ] || fail "control characters were not refused cleanly"

# --timezone reads the schedule in that zone and shows its times there
if [ -f /usr/share/zoneinfo/Asia/Tokyo ]; then
   out=$("$DATEBLOCK" -t -z Asia/Tokyo -c '0 0 9') || fail "dateblock -z exited with an error"
   echo "$out" | grep -q '^Block Until  : .* 09:00:00 JST (' || fail "dateblock -z output: $out"
else
   echo "SKIP: no timezone data for the --timezone checks"
fi
"$DATEBLOCK" -t -z Nowhere/Land -n 5 >/dev/null 2>&1
[ $? -eq 1 ] || fail "dateblock accepted an unknown timezone"
"$DATEBLOCK" -t -z ../../../etc/passwd -n 5 >/dev/null 2>&1
[ $? -eq 1 ] || fail "dateblock accepted a path as a timezone"

# A bad schedule says which field is wrong and why (exit code 1)
out=$("$DATEBLOCK" -t -c '* 99' 2>&1)
[ $? -eq 1 ] || fail "a bad minute did not exit with 1"
echo "$out" | grep -q "minute '99': 99 is outside of 0-59" || fail "a bad minute was not explained: $out"
out=$("$DATEBLOCK" -t -w 8 2>&1)
[ $? -eq 1 ] || fail "-w 8 did not exit with 1"
echo "$out" | grep -q "day of week '8': 8 is outside of 0-7" || fail "-w 8 was not explained: $out"

# Month and weekday names, and shortcuts such as @daily, mean the numbers
# they stand for
block_until() { "$DATEBLOCK" -t "$@" | grep '^Block Until'; }
same() {
   [ -n "$1" ] && [ "$1" = "$2" ] || fail "$3 gave '$1', not '$2'"
}
same "$(block_until -c @daily)" "$(block_until -c '0 0 0')" "@daily"
same "$(block_until -c @hourly)" "$(block_until -c '0 0')" "@hourly"
same "$(block_until -i -c @weekly)" "$(block_until -i -c '0 0 * * 0')" "-i @weekly"
same "$(block_until -o 9 -w Mon-Fri)" "$(block_until -o 9 -w 1-5)" "-w Mon-Fri"
same "$(block_until -c '0 0 0 1 Jul')" "$(block_until -c '0 0 0 1 7')" "Jul"
out=$("$DATEBLOCK" -t -c '@reboot' 2>&1)
[ $? -eq 1 ] || fail "@reboot did not exit with 1"
echo "$out" | grep -q "'@reboot' is not a time" || fail "@reboot was not explained: $out"
out=$("$DATEBLOCK" -t -i -c '@daily +30' 2>&1)
[ $? -eq 1 ] || fail "-i '@daily +30' did not exit with 1"
echo "$out" | grep -q "nothing may follow '@daily'" || fail "-i '@daily +30' was not explained: $out"
out=$("$DATEBLOCK" -t -w Sat-Sun 2>&1)
[ $? -eq 1 ] || fail "-w Sat-Sun did not exit with 1"
echo "$out" | grep -q "'Sat-Sun' goes backwards" || fail "-w Sat-Sun was not explained: $out"

# '*' mixed into a list is refused
"$DATEBLOCK" -t -c '0 *,5' >/dev/null 2>&1
[ $? -eq 1 ] || fail "dateblock accepted '*,5'"

# A command after the options runs once the time is reached, and its exit
# code is dateblock's
"$DATEBLOCK" -s /1 -- sh -c 'exit 7'
[ $? -eq 7 ] || fail "the command's exit code was not passed on"

# Arguments arrive exactly as given (no shell), and options after the
# command belong to it
out=$("$DATEBLOCK" -s /1 printf '%s|' 'a b' -n '*' '$HOME')
[ "$out" = 'a b|-n|*|$HOME|' ] || fail "command arguments were changed: $out"
out=$("$DATEBLOCK" -s/1 echo -n 5)
[ "$out" = "5" ] || fail "options after the command were not left to it: $out"
out=$("$DATEBLOCK" --second /1 -- echo -t)
[ "$out" = "-t" ] || fail "'--' did not end dateblock's options: $out"

# A command that can't be found (127) or run (126)
"$DATEBLOCK" -s /1 no-such-command-dateblock 2>/dev/null
[ $? -eq 127 ] || fail "a missing command did not exit with 127"
tmp=$(mktemp)
"$DATEBLOCK" -s /1 "$tmp" 2>/dev/null
[ $? -eq 126 ] || fail "a command that can't be run did not exit with 126"

# --test shows the command without running it, and a bad schedule never
# runs it
out=$("$DATEBLOCK" -t -n 5 -- touch "$tmp.ran")
[ ! -e "$tmp.ran" ] || fail "--test ran the command"
echo "$out" | grep -q '^Command      : touch ' || fail "--test did not show the command: $out"
"$DATEBLOCK" -c '* 99' touch "$tmp.ran" 2>/dev/null
[ $? -eq 1 ] && [ ! -e "$tmp.ran" ] || fail "a bad schedule ran the command"
rm -f "$tmp" "$tmp.ran"

# Blocking returns once the next whole second arrives
start=$(date +%s)
"$DATEBLOCK" -s '/1' || fail "dateblock -s /1 exited with an error"
[ $(( $(date +%s) - start )) -le 2 ] || fail "dateblock -s /1 took too long"

[ $rc -eq 0 ] && echo "PASS: command line tool"
exit $rc
