#!/bin/bash
# Publishes the given npm package files (.tgz) from a GitHub Actions run,
# using npm trusted publishing: npm checks with GitHub which repository and
# workflow the run belongs to, so no token is stored anywhere.
# A version npm already has is skipped, so a failed run can simply be
# re-run.  See https://docs.npmjs.com/trusted-publishers
set -euo pipefail

# Only ever publish from a version tag that matches package.json and
# __version__
version=$(node -p "require('./package.json').version")
init=$(sed -n 's/^__version__ = "\(.*\)"$/\1/p' python/dateblock/__init__.py)
if [ "${GITHUB_REF_NAME:-}" != "v$version" ] || [ "$version" != "$init" ]; then
   echo "Not publishing: tag '${GITHUB_REF_NAME:-}', package.json $version, __version__ $init" >&2
   exit 1
fi

[ $# -gt 0 ] || { echo "Nothing to publish" >&2; exit 1; }
for file in "$@"; do
   if [ ! -f "$file" ]; then
      echo "Not publishing: $file wasn't built" >&2
      exit 1
   fi
   read -r name packed < <(tar -xOzf "$file" package/package.json | node -e '
      const p = JSON.parse(require("fs").readFileSync(0, "utf8"));
      console.log(p.name, p.version);')
   if [ "$packed" != "$version" ]; then
      echo "Not publishing: $file holds $name $packed, not $version" >&2
      exit 1
   fi
   if [ "$(npm view "$name@$version" version 2>/dev/null)" = "$version" ]; then
      echo "$name $version is already on npm; skipped"
      continue
   fi
   npm publish "$file"
done
