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

# Only ever publish from a version tag that matches __version__
version=$(sed -n 's/^__version__ = "\(.*\)"$/\1/p' python/dateblock/__init__.py)
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

# The token goes in the environment, not on the command line (where other
# processes could see it)
TWINE_USERNAME=__token__ TWINE_PASSWORD="$token" \
   pipx run "$(grep '^twine==' .github/release-tools/requirements.txt)" \
   upload --skip-existing "$@"
