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

## Packages (RPM, Debian and Arch Linux)
Recipes for building RPM (Fedora and RHEL/CentOS/Alma/Rocky 9 and 10),
Debian and Arch Linux packages live in [packaging/](packaging/). The easiest
way to build them is in a container (needs Docker or Podman):
```bash
make docker-rpm DISTRO=el9      # or f44 (the default) or el10
make docker-deb
make docker-arch
make docker-all                 # all of them
```

See [packaging/README.md](packaging/README.md) for the packages each one
produces and how to build them without Docker.

You're done!
