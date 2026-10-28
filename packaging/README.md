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
[Packagist](https://packagist.org/packages/caronc/dateblock) picks up each
new version tag by itself.

PIE needs a C++ compiler and the PHP development files on the computer it
runs on, and a 64-bit `time_t` (older 32-bit systems are refused, as
everywhere else; see `INSTALL.md`).  Windows isn't supported (PIE needs
ready-built Windows files for that).

## Man Page
The man page is generated with [Ronn-NG](https://github.com/apjanke/ronn-ng).
- Edit `man/dateblock.md`, following the
  [Ronn format](https://github.com/apjanke/ronn-ng/blob/main/man/ronn-format.7.ronn).
- Then run `make docker-man` from the top of the source tree. It rebuilds
  `man/dateblock.1` (installed with the tool) and `man/dateblock.1.html`.
  Commit all three files.
