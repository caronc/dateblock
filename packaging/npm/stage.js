// Dateblock blocks (sleeps) until a cron-style moment in time.
// Copyright (C) 2013-2026 Chris Caron <lead2gold@gmail.com>
//
// This file is part of Dateblock.  Dateblock is free software; you can
// redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation; either version 2 of
// the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along with
// this program; if not, see <https://www.gnu.org/licenses/>.
'use strict';

// Lays out the npm packages in a folder each, ready for 'npm pack':
//   node packaging/npm/stage.js main <folder>
//     @caronc/dateblock itself: the JavaScript, the C++ source (to build
//     it on computers with no ready-built add-on) and an optional
//     dependency on each ready-built package.
//   node packaging/npm/stage.js platform <folder>
//     The ready-built add-on for this computer (ie:
//     @caronc/dateblock-linux-x64-gnu), from node/build/Release (run
//     'npm run build' first).
// Run it from the top of the source tree.
const fs = require('node:fs');
const path = require('node:path');

const top = path.join(__dirname, '..', '..');
const project = JSON.parse(fs.readFileSync(path.join(top, 'package.json'), 'utf8'));
const platforms = require(path.join(top, 'node', 'platforms.json'));
const platformKey = require(path.join(top, 'node', 'platform.js'));

// The details every package shares
const shared = {};
for (const field of ['version', 'homepage', 'bugs', 'repository', 'license',
                     'author', 'funding', 'engines', 'publishConfig']) {
  shared[field] = project[field];
}

function copy(file, folder, as = file) {
  const to = path.join(folder, as);
  fs.mkdirSync(path.dirname(to), { recursive: true });
  fs.copyFileSync(path.join(top, file), to);
}

function writeJson(folder, data) {
  fs.writeFileSync(path.join(folder, 'package.json'),
                   JSON.stringify(data, null, 2) + '\n');
}

function stageMain(folder) {
  for (const file of project.files) {
    copy(file, folder);
  }
  // npm shows the package's README.md; use the one written for npm users
  copy('node/README.md', folder, 'README.md');

  const main = { ...project };
  // Building is only needed where there is no ready-built add-on, and is
  // never done on install (that would need approving on npm 12)
  main.scripts = { build: project.scripts.build };
  main.optionalDependencies = {};
  for (const key of Object.keys(platforms)) {
    main.optionalDependencies[`${project.name}-${key}`] = project.version;
  }
  writeJson(folder, main);
}

function stagePlatform(folder) {
  const key = platformKey();
  const platform = platforms[key];
  if (!platform) {
    throw new Error(`${key} is not listed in node/platforms.json`);
  }
  copy('node/build/Release/dateblock.node', folder, 'dateblock.node');
  copy('COPYING', folder);
  fs.writeFileSync(path.join(folder, 'README.md'),
    `# ${project.name}-${key}\n\n` +
    `The ready-built add-on of [${project.name}]` +
    `(https://www.npmjs.com/package/${project.name}) for ${platform.name}.\n` +
    `Install ${project.name} instead; npm picks this package for you.\n\n` +
    `Its source is at ${project.homepage}.\n`);

  const data = {
    name: `${project.name}-${key}`,
    description: `The ready-built ${project.name} add-on for ${platform.name}`,
    ...shared,
    os: [platform.os],
    cpu: [platform.cpu],
  };
  if (platform.libc) {
    data.libc = [platform.libc];
  }
  data.files = ['dateblock.node', 'COPYING'];
  writeJson(folder, data);
}

const [mode, folder] = process.argv.slice(2);
if (!folder || !['main', 'platform'].includes(mode)) {
  console.error('Usage: node packaging/npm/stage.js main|platform <folder>');
  process.exit(2);
}
fs.rmSync(folder, { recursive: true, force: true });
fs.mkdirSync(folder, { recursive: true });
(mode === 'main' ? stageMain : stagePlatform)(folder);
