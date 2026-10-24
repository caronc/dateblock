#!/bin/bash
# Uploads the given files to PyPI from a GitHub Actions run, using PyPI
# trusted publishing:
#  1. Ask GitHub for an identity token that says which repository and
#     workflow this run belongs to.
#  2. Trade it with PyPI for an upload token that only lasts a few minutes.
#  3. Upload with twine.
# Files PyPI already has are skipped, so a failed run can simply be re-run.
# See https://docs.pypi.org/trusted-publishers/
set -euo pipefail

# Only ever publish from a version tag that matches configure.ac
version=$(sed -nE 's/^AC_INIT\(\[dateblock\], *\[([^]]+)\].*/\1/p' configure.ac)
if [ "${GITHUB_REF_NAME:-}" != "v$version" ]; then
   echo "Not uploading: tag '${GITHUB_REF_NAME:-}' does not match v$version" >&2
   exit 1
fi

json_value() {
   # Prints the value of a "key":"value" pair from a JSON reply
   sed -E 's/.*"'"$1"'" *: *"([^"]+)".*/\1/'
}

oidc=$(curl -sSf -H "Authorization: bearer $ACTIONS_ID_TOKEN_REQUEST_TOKEN" \
   "$ACTIONS_ID_TOKEN_REQUEST_URL&audience=pypi" | json_value value)

token=$(curl -sSf -X POST https://pypi.org/_/oidc/mint-token \
   -d "{\"token\":\"$oidc\"}" | json_value token)
echo "::add-mask::$token"

pipx run twine upload --skip-existing -u __token__ -p "$token" "$@"
