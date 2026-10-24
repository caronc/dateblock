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

Arch Linux packages the library and headers together with the tool, as is
usual there.

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
   mock -r alma+epel-9-x86_64 dateblock-1.1.0-*.src.rpm
   ```

* **Debian** (needs `dpkg-dev` and the `Build-Depends` from
  `debian/control`):
   ```bash
   ./autogen.sh && ./configure && make dist
   mv dateblock-1.1.0.tar.gz ../dateblock_1.1.0.orig.tar.gz
   cd .. && tar -xzf dateblock_1.1.0.orig.tar.gz && cd dateblock-1.1.0
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
the Python module, so they need nothing else installed, and they include the
`dateblock` command.

Build the source package and the Linux wheels (manylinux and musllinux, for
x86_64, Python 3.9 to 3.14) with:
```bash
make docker-pypi
```
This needs Docker (or Podman) plus [uv](https://docs.astral.sh/uv/) or pipx.
The files are written to `dist/pypi/`, and `twine check` makes sure the PyPI
page will render. Upload them with your own PyPI credentials:
```bash
uvx twine upload dist/pypi/*
```

The GitHub workflow (`.github/workflows/wheels.yml`) builds the full set:
Linux (x86_64 and aarch64), macOS (Intel and Apple Silicon) and Windows. It
runs on every push. Publishing a GitHub release also uploads everything to
PyPI using [trusted publishing](https://docs.pypi.org/trusted-publishers/),
so no API token is stored anywhere. To set that up once:
1. On PyPI, open the dateblock project's **Settings > Publishing** and add a
   GitHub publisher: owner `caronc`, repository `dateblock`, workflow
   `wheels.yml`, environment `pypi`.
2. On GitHub, create an environment called `pypi` (repository **Settings >
   Environments**).

## Making a release
```bash
make release
```
This builds and tests everything (source tarball, every distribution package
and the PyPI files) and gathers them into `dist/release/` with a
`SHA256SUMS` file.

Before tagging a new version, update it in all of these places:
- `configure.ac` (`AC_INIT`); `setup.py` reads the version from here
- `packaging/redhat/dateblock.spec` (`Version:` plus a `%changelog` entry)
- `packaging/debian/changelog` (a new entry at the top)
- `packaging/arch/PKGBUILD` (`pkgver`; then run `updpkgsums` once the release
  is tagged)

## Man Page
The man page is generated with [Ronn-NG](https://github.com/apjanke/ronn-ng).
- Edit `man/dateblock.md`, following the
  [Ronn format](https://github.com/apjanke/ronn-ng/blob/main/man/ronn-format.7.ronn).
- Then run `make docker-man` from the top of the source tree. It rebuilds
  `man/dateblock.1` (installed with the tool) and `man/dateblock.1.html`.
  Commit all three files.
