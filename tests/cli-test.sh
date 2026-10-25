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

# Blocking returns once the next whole second arrives
start=$(date +%s)
"$DATEBLOCK" -s '/1' || fail "dateblock -s /1 exited with an error"
[ $(( $(date +%s) - start )) -le 2 ] || fail "dateblock -s /1 took too long"

[ $rc -eq 0 ] && echo "PASS: command line tool"
exit $rc
