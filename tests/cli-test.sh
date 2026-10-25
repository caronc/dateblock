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
