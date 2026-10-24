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
# this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
Name:           dateblock
Version:        1.1.0
Release:        1%{?dist}.nuxref
Summary:        Block (sleep) until a cron-style moment in time
License:        GPL-2.0-or-later
URL:            https://github.com/caronc/dateblock
Source0:        %{url}/archive/v%{version}/%{name}-%{version}.tar.gz

BuildRequires:  gcc-c++
BuildRequires:  make
BuildRequires:  autoconf
BuildRequires:  automake
BuildRequires:  libtool
BuildRequires:  boost-devel
BuildRequires:  python3-devel
BuildRequires:  pyproject-rpm-macros
BuildRequires:  python3-setuptools
BuildRequires:  python3-pip
%if 0%{?rhel}
# Older setuptools releases need this to build wheels
BuildRequires:  python3-wheel
%endif

Requires:       %{name}-libs%{?_isa} = %{version}-%{release}

# This project used to be called datetools; that package only pulled in
# dateblock (and the now retired datemath)
Obsoletes:      datetools < 1.1.0

%description
Dateblock works like sleep, except that instead of waiting for a number of
seconds it waits until a crontab-style schedule is matched.  It also
supports matching on seconds and adding an extra drift time.

%package libs
Summary:        Shared library for cron-style date calculations

%description libs
The libdateblock shared library; it works out the next moment in time
matching a cron-style schedule (with optional seconds and drift).  It is
used by the dateblock tool and its Python bindings.

%package devel
Summary:        Development files for the dateblock library
Requires:       %{name}-libs%{?_isa} = %{version}-%{release}

%description devel
Header files, pkg-config file and the linkable library needed to build
programs that use libdateblock.

%package -n python3-dateblock
Summary:        Python bindings for dateblock (cron-style blocking)
Requires:       %{name}-libs%{?_isa} = %{version}-%{release}
# The old Python 2 bindings
Obsoletes:      python-dateblock < 1.1.0

%description -n python3-dateblock
Python bindings for dateblock; block until (or just work out) the next
moment in time matching a cron-style schedule.

%prep
%autosetup -p1

%build
autoreconf --force --install
%configure --disable-static
%make_build

# The Python bindings link against the libdateblock that was just built
pushd python
%pyproject_wheel
popd

%install
%make_install
find %{buildroot} -name '*.la' -delete
%pyproject_install

%check
export TZ=UTC
make check
export LD_LIBRARY_PATH=%{buildroot}%{_libdir}
export PYTHONPATH=%{buildroot}%{python3_sitearch}
%{python3} -m unittest discover -v -s python/tests

%files
%license COPYING
%doc README.md ChangeLog AUTHORS
%{_bindir}/dateblock
%{_mandir}/man1/dateblock.1*

%files libs
%license COPYING
%{_libdir}/libdateblock.so.1{,.*}

%files devel
%{_includedir}/dateblock/
%{_libdir}/libdateblock.so
%{_libdir}/pkgconfig/dateblock.pc

%files -n python3-dateblock
%{python3_sitearch}/dateblock*.so
%{python3_sitearch}/dateblock-%{version}.dist-info/

%changelog
* Sat Oct 24 2026 Chris Caron <lead2gold@gmail.com> - 1.1.0-1
- Updated to version 1.1.0
- Project renamed from datetools to dateblock; datemath has been retired
- libdateblock is now a shared library (dateblock-libs and dateblock-devel)
- Python 3 bindings (python3-dateblock)
- Builds on EL9, EL10 and current Fedora

* Tue May  1 2018 Chris Caron <lead2gold@gmail.com> 1.0.3-1
- Updated to version 1.0.3

* Fri Apr 21 2017 Chris Caron <lead2gold@gmail.com> 1.0.0-1
- Updated to version 1.0.0

* Fri Jul 17 2015 Chris Caron <lead2gold@gmail.com> 0.8.1-1
- Updated to version 0.8.1

* Fri Oct 18 2013 Chris Caron <lead2gold@gmail.com> 0.8.0-1
- Initial RPM release
