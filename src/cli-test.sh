#!/bin/sh
# Quick checks of the dateblock command line tool.
# Run by 'make check' from the build directory.
export TZ=UTC
rc=0

fail() {
   echo "FAIL: $*"
   rc=1
}

# --test reports when it would unblock without actually waiting
out=$(./dateblock -t -c '* /5') || fail "dateblock -t exited with an error"
echo "$out" | grep -q '^Block Until  : ' || fail "dateblock -t output: $out"

# Bad syntax and impossible dates are both errors (not crashes)
./dateblock -t -c '* 99' >/dev/null 2>&1 && fail "dateblock accepted minute 99"
./dateblock -t -c '0 0 0 31 2 *' >/dev/null 2>&1
[ $? -eq 1 ] || fail "dateblock did not reject February 31st"

# Blocking returns once the next whole second arrives
start=$(date +%s)
./dateblock -s '/1' || fail "dateblock -s /1 exited with an error"
[ $(( $(date +%s) - start )) -le 2 ] || fail "dateblock -s /1 took too long"

[ $rc -eq 0 ] && echo "PASS: command line tool"
exit $rc
