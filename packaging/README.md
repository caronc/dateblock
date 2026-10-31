## Packaging
This directory holds everything needed to package dateblock for various
distributions.

| Directory  | Contents                                                     |
|------------|--------------------------------------------------------------|
| `redhat/`  | RPM spec file (Fedora, RHEL/CentOS/Alma/Rocky 9 and 10)      |
| `debian/`  | Debian packaging (copied in as `debian/` when building)      |
| `arch/`    | Arch Linux `PKGBUILD`                                        |
| `man/`     | The `dateblock(1)` man page and its source                   |
| `docker/`  | One `Dockerfile.<distro>` build environment per distribution |

The Python package (PyPI) is built from `pyproject.toml` and `setup.py` at
the top of the source tree; see [PyPI](#pypi-pip-install-dateblock) below.
The PHP extension lives in `php/` and is described by `composer.json` at the
top of the source tree; see [PIE](#pie-pie-install-caroncdateblock) below.
The Node.js package lives in `node/` (its build file is
`node/binding.gyp`) and is described by `package.json` at the top of the
source tree; see
[npm](#npm-npm-install-caroncdateblock) below.

Each `Dockerfile.<distro>` installs its build requirements straight from the
matching packaging file (the spec's `BuildRequires`, `debian/control` or the
`PKGBUILD`'s `makedepends`), so the containers always match what the package
build needs.

### Building packages
The easiest way is with the `docker-*` targets from the top of the source
tree. They only need GNU make and Docker (or Podman); no `./configure`
required. Every package's own tests run as part of its build.

| Command                       | Builds                                     |
|-------------------------------|--------------------------------------------|
| `make docker-rpm`             | Fedora 44 RPMs                             |
| `make docker-rpm DISTRO=el9`  | EL9 RPMs (also `DISTRO=el10`)              |
| `make docker-rpm-all`         | RPMs for Fedora 44, EL9 and EL10           |
| `make docker-deb`             | Debian 13 (trixie) `.deb` packages         |
| `make docker-arch`            | Arch Linux `.pkg.tar.zst` packages         |
| `make docker-all`             | All of the above                           |

The results are written to `dist/<distro>/`. Set `CONTAINER=podman` to use
Podman when both are installed.

### Packages produced

| Contents            | RPM                 | Debian              | Arch               |
|---------------------|---------------------|---------------------|--------------------|
| Command line tool   | `dateblock`         | `dateblock`         | `dateblock`        |
| Shared library      | `dateblock-libs`    | `libdateblock1`     | `dateblock`        |
| Headers, `.pc` file | `dateblock-devel`   | `libdateblock-dev`  | `dateblock`        |
| Python bindings     | `python3-dateblock` | `python3-dateblock` | `python-dateblock` |
| PHP extension       | `php-dateblock`     | `php-dateblock`     | `php-dateblock`    |

Arch Linux packages the library and headers together with the tool, as is
usual there.

The PHP extension is built for each distribution's own PHP and turned on
when installed (`/etc/php.d/40-dateblock.ini` on Fedora and EL, through
`phpenmod` on Debian).  Arch Linux installs `/etc/php/conf.d/dateblock.ini`
with the line commented out, as is usual there.  Repositories that offer
several PHP versions side by side (ie: Remi's, or `php8.4-*` packages) name
their builds after the PHP version; the package here is the base one they
start from.

### Building without Docker
On a machine running the matching distribution:

* **RPM** (needs `rpm-build` and the spec's `BuildRequires`; on EL9/EL10 the
  CRB repository must be enabled):
   ```bash
   ./autogen.sh && ./configure && make dist
   rpmbuild -ba --define "_sourcedir $PWD" packaging/redhat/dateblock.spec
   ```
   Or, with [mock](https://github.com/rpm-software-management/mock) (the
   AlmaLinux configurations already enable CRB):
   ```bash
   rpmbuild -bs --define "_sourcedir $PWD" --define "_srcrpmdir $PWD" \
      packaging/redhat/dateblock.spec
   mock -r alma+epel-9-x86_64 dateblock-2.0.0-*.src.rpm
   ```

* **Debian** (needs `dpkg-dev` and the `Build-Depends` from
  `debian/control`):
   ```bash
   ./autogen.sh && ./configure && make dist
   mv dateblock-2.0.0.tar.gz ../dateblock_2.0.0.orig.tar.gz
   cd .. && tar -xzf dateblock_2.0.0.orig.tar.gz && cd dateblock-2.0.0
   cp -a /path/to/checkout/packaging/debian debian
   dpkg-buildpackage -us -uc
   ```

* **Arch Linux** (needs `base-devel`); this downloads the tagged release
  from GitHub:
   ```bash
   cd packaging/arch && makepkg -s
   ```

## PyPI (pip install dateblock)
The Python package is built from `pyproject.toml` and `setup.py` at the top
of the source tree. The PyPI wheels have the C++ code compiled straight into
the Python module, so they need nothing else installed.  They hold only the
Python library; the `dateblock` command is the C++ tool (built from source
or installed from the distribution packages).

Build the source package and the Linux wheels (manylinux and musllinux,
Python 3.9 to 3.14) with:
```bash
make docker-pypi                              # this machine's CPU (x86_64)
make docker-pypi PYPI_ARCHS="x86_64 aarch64"  # plus 64-bit ARM (Raspberry Pi)
```
This needs Docker (or Podman) plus [uv](https://docs.astral.sh/uv/) or pipx.
ARM wheels are built under emulation, so they need `qemu-user-static` and
take a while (about 30 minutes for the full set). The files are written to
`dist/pypi/`, and `twine check` makes sure the PyPI page will render.

| Wheels for             | Where they can be built                           |
|------------------------|---------------------------------------------------|
| Linux x86_64           | locally, with `make docker-pypi`                  |
| Linux ARM64 (ie: Pi)   | locally under emulation (`PYPI_ARCHS` above)      |
| macOS (Intel and ARM)  | only on a Mac (or GitHub's macOS runners)         |
| Windows                | only on Windows (or GitHub's Windows runners)     |

Anyone on a platform without a wheel can still `pip install dateblock`; pip
builds it from the source package instead, which needs a C++ compiler and
the Python headers.

## PIE (pie install caronc/dateblock)
[PIE](https://github.com/php/pie) is PHP's extension installer (it replaces
PECL).  It reads `composer.json`, downloads the source for a version tag
from GitHub, builds `php/` with `phpize` (the C++ code is compiled straight
into the extension) and turns it on.  Nothing is uploaded for it:
[Packagist](https://packagist.org/packages/caronc/dateblock) picks up each new version tag by itself.

PIE needs a C++ compiler and the PHP development files on the computer it
runs on, and a 64-bit `time_t` (older 32-bit systems are refused, as
everywhere else; see `INSTALL.md`).  Windows isn't supported (PIE needs
ready-built Windows files for that).

## npm (npm install @caronc/dateblock)
The Node.js add-on is published as several npm packages:
- `@caronc/dateblock` itself: the JavaScript, plus the C++ source (in
  `node/` and `src/`) so it can still be built on computers that have no
  ready-built add-on.
- One ready-built add-on per platform listed in `node/platforms.json`,
  ie: `@caronc/dateblock-linux-x64-gnu`.  Each holds just
  `dateblock.node`.  `@caronc/dateblock` lists them all as optional
  dependencies, and npm only installs the one that matches the computer
  (by its `os`, `cpu` and `libc`).  `node/index.js` loads it.

Nothing runs while installing (no install script, and no `binding.gyp` at
the top of the package, which npm would take as one), so npm 12 has
nothing to ask to approve.  Elsewhere, users build it once with
`npm explore @caronc/dateblock -- npm run build`.

`packaging/npm/build.sh` builds and tests the add-on on the computer it
runs on, packs `@caronc/dateblock` and that computer's ready-built
package, then installs them in an empty project to check that:
- the ready-built add-on is used, with nothing built while installing;
- without it, the error says how to build it, and that build works.

Build the Linux ones in a container with:
```bash
make docker-npm                  # glibc (AlmaLinux 8), into dist/npm-gnu/
make docker-npm NPM_LIBC=musl    # musl (Alpine), into dist/npm-musl/
```
The glibc one is built on AlmaLinux 8 (glibc 2.28, the oldest Node.js 20
runs on), so it runs wherever Node.js does; the musl one has the C++
runtime built in.  `ARCH=aarch64` builds for 64-bit ARM (under emulation).
Like everywhere else, it needs a 64-bit `time_t`; older 32-bit systems
(including 32-bit Linux builds of Node.js) are refused (see `INSTALL.md`).
The package's README is `node/README.md` (written for npm users; npm shows
it on the package page), not the project's own README.

The GitHub workflow (`.github/workflows/npm.yml`) builds and checks every
ready-built add-on on its own platform (Linux in the containers above,
macOS and Windows on their runners) on every push.  Nothing is kept (no
artifacts).  When a version tag is pushed, and every check passed, it
builds them again and publishes each one straight from its own job, then
`@caronc/dateblock` itself last (so it is never on npm without them).  It
uses trusted publishing (no token is stored), and skips versions npm
already has, so a failed run can simply be re-run.

npm only lets a package get a trusted publisher once it exists, so a new
package is published once by hand first.  `@caronc/dateblock` already is.
For the ready-built ones (and any platform added to `node/platforms.json`
later):
```bash
npm login
packaging/npm/first-publish.sh
```
It publishes an empty `0.0.0` placeholder of each one that isn't on npm
yet (nothing ever installs it; `@caronc/dateblock` asks for its own
version).  Then, on npmjs.com, add a trusted publisher to each package's
settings: GitHub Actions, repository `caronc/dateblock`, workflow
`npm.yml`.  Under "Allowed actions", also allow publishing with
`npm publish` (only staged publishing is allowed otherwise, and the
workflow's publish would be refused).

## Man Page
The man page is generated with [Ronn-NG](https://github.com/apjanke/ronn-ng).
- Edit `man/dateblock.md`, following the
  [Ronn format](https://github.com/apjanke/ronn-ng/blob/main/man/ronn-format.7.ronn).
- Then run `make docker-man` from the top of the source tree. It rebuilds
  `man/dateblock.1` (installed with the tool) and `man/dateblock.1.html`.
  Commit all three files.
