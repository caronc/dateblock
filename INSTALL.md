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
The easiest way is from PyPI, which needs nothing else installed:
```bash
pip install dateblock
```

To build them from your checkout instead (no `./configure` needed; the C++
code is compiled straight into the Python module):
```bash
pip install .
```

To link them against the libdateblock library built above instead (as the
distribution packages do), run `make` first and then:
```bash
DATEBLOCK_SHARED=1 pip install .
```
If you installed libdateblock to `/usr/local` (the default), you may need to run `sudo ldconfig` so the library can be
found when Python loads the module.

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
