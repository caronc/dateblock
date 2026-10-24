# Installation
The following can be used to help install dateblock into your environment.
This tool is not currently compatible with Microsoft Windows.

## Requirements
You will require the following in order for this to work correctly:
* automake
* autoconf
* libtool
* make
* GNU C++ Compiler
* boost (the `program_options` library; only the command line tool uses it)
* python 3.9+ with setuptools (optional; for the Python bindings)

On Fedora and RHEL/CentOS/Alma/Rocky:
```bash
sudo dnf install gcc-c++ make autoconf automake libtool boost-devel \
                 python3-devel python3-setuptools python3-pip
```

## Build Procedure
The following will set a correct build environment
```bash
./autogen.sh
```

Then run the following:
```bash
# Configure the environment (optionally over-ride the prefix)
./configure

# Compile the code
make

# Run the tests
make check

# Install the package into your system (as root)
# some distributions just allow you to prefix
# the below command with _sudo_
make install
```

## Python Bindings
The Python bindings link against the `libdateblock` library built above, so
run `make` first:
```bash
pip install ./python
```

If you installed libdateblock to `/usr/local` (the default), you may need to
run `sudo ldconfig` (or add `/usr/local/lib` to `/etc/ld.so.conf.d/`) so the
library can be found when Python loads the module.

To build the bindings against an already installed libdateblock instead of
this source tree:
```bash
CPPFLAGS="$(pkg-config --cflags dateblock)" \
LDFLAGS="$(pkg-config --libs-only-L dateblock)" \
   pip install ./python
```

## RPM Packages
`dateblock.spec` builds on Fedora and on RHEL/CentOS/Alma/Rocky 9 and 10.
It produces the following packages:

| Package             | Contents                                         |
|---------------------|--------------------------------------------------|
| `dateblock`         | The `dateblock` command line tool                |
| `dateblock-libs`    | The `libdateblock.so.1` shared library           |
| `dateblock-devel`   | Headers, `libdateblock.so` and `dateblock.pc`    |
| `python3-dateblock` | The `dateblock` Python module                    |

Everything depends on `dateblock-libs`, so installing (for example)
`python3-dateblock` pulls in the shared library automatically.

The spec expects the GitHub release tarball (`v<version>` tag). To build from
your current checkout instead:
```bash
# Create the source tarball the spec expects
VERSION=$(rpmspec -q --srpm --qf '%{version}' dateblock.spec)
git archive --prefix=dateblock-$VERSION/ -o dateblock-$VERSION.tar.gz HEAD

# Build a source RPM, then build it for each distribution with mock
rpmbuild -bs --define "_sourcedir $PWD" --define "_srcrpmdir $PWD" \
   dateblock.spec
mock -r fedora-44-x86_64 dateblock-$VERSION-*.src.rpm
mock -r alma+epel-10-x86_64 dateblock-$VERSION-*.src.rpm
mock -r alma+epel-9-x86_64 dateblock-$VERSION-*.src.rpm
```

On RHEL 9 and 10 based systems some of the build dependencies
(`pyproject-rpm-macros` and `python3-wheel`) come from the CRB repository;
the AlmaLinux mock configurations already enable it.  Outside of mock, run
`sudo dnf config-manager --set-enabled crb` first.

You're done!
