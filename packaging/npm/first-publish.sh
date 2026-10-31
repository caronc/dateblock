#!/bin/bash
# Dateblock blocks (sleeps) until a cron-style moment in time.
# Copyright (C) 2013-2026 Chris Caron <lead2gold@gmail.com>
#
# This file is part of Dateblock.  Dateblock is free software; you can
# redistribute it and/or modify it under the terms of the GNU General Public
# License as published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, see <https://www.gnu.org/licenses/>.
#
# Run once, by hand, before the first release with ready-built add-ons.
# npm only lets a package get a trusted publisher (so the GitHub workflow
# can publish it) once the package exists.  This publishes an empty 0.0.0
# of each ready-built package in node/platforms.json, from your own npm
# login; nothing ever installs it (@caronc/dateblock asks for its own
# version).  Packages already on npm are skipped.
# Usage (from the top of the source tree, after 'npm login'):
#   packaging/npm/first-publish.sh
set -euo pipefail

npm whoami >/dev/null || { echo "Run 'npm login' first" >&2; exit 1; }

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

for key in $(node -p "Object.keys(require('./node/platforms.json')).join(' ')"); do
   name="@caronc/dateblock-$key"
   if npm view "$name" name >/dev/null 2>&1; then
      echo "$name is already on npm; skipped"
      continue
   fi
   folder="$work/$key"
   mkdir -p "$folder"
   node -e '
      const [name, folder] = process.argv.slice(1);
      const project = require("./package.json");
      require("fs").writeFileSync(`${folder}/package.json`, JSON.stringify({
        name,
        version: "0.0.0",
        description: "A placeholder; install @caronc/dateblock instead",
        homepage: project.homepage,
        repository: project.repository,
        license: project.license,
        author: project.author,
      }, null, 2) + "\n");
      require("fs").writeFileSync(`${folder}/README.md`,
        `# ${name}\n\nA placeholder.  Install ` +
        "[@caronc/dateblock](https://www.npmjs.com/package/@caronc/dateblock) " +
        "instead; it installs the ready-built add-on it needs.\n");
   ' "$name" "$folder"
   (cd "$folder" && npm publish --access public)
done

echo
echo "Now, on npmjs.com, add a trusted publisher to each of these packages"
echo "(Settings, Trusted Publisher): GitHub Actions, repository"
echo "caronc/dateblock, workflow npm.yml, and allow publishing with"
echo "'npm publish'."
