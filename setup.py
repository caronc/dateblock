# -*- coding: utf-8 -*-
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
#
# Python packaging for dateblock (pip install dateblock).
#
# - By default the C++ code in src/ is compiled straight into the Python
#   module, so the wheel needs nothing else installed.  This is what PyPI
#   gets.
# - Set DATEBLOCK_SHARED=1 to link against libdateblock instead (as the
#   RPM, Debian and Arch packages do).  Run ./configure && make first; or
#   point CPPFLAGS/LDFLAGS at an installed copy, ie:
#     CPPFLAGS="$(pkg-config --cflags dateblock)"
#   The 'dateblock' command is left out in this mode because the C++ tool
#   already provides it.
# - The version comes from configure.ac (AC_INIT).
import os
import re
import sys

from setuptools import Extension, setup

HERE = os.path.dirname(os.path.abspath(__file__))
SHARED = os.environ.get("DATEBLOCK_SHARED", "") not in ("", "0")


def read(name):
    with open(os.path.join(HERE, name), encoding="utf-8") as f:
        return f.read()


VERSION = re.search(
    r"AC_INIT\(\[dateblock\],\s*\[([^]]+)\]", read("configure.ac")).group(1)

if SHARED:
    extension = Extension(
        "dateblock._dateblock",
        sources=["python/_dateblock.cpp"],
        include_dirs=["src"],
        library_dirs=[os.path.join("src", ".libs")],
        libraries=["dateblock"],
    )
    entry_points = {}
else:
    extension = Extension(
        "dateblock._dateblock",
        sources=["python/_dateblock.cpp", "src/Date.cpp"],
        include_dirs=["src"],
        # MSVC needs to be told to use standard C++ exception handling
        extra_compile_args=["/EHsc"] if sys.platform == "win32" else [],
    )
    entry_points = {"console_scripts": ["dateblock = dateblock.cli:main"]}

setup(
    name="dateblock",
    version=VERSION,
    description="Block (sleep) until a cron-style moment in time",
    long_description=read("README.md"),
    long_description_content_type="text/markdown",
    url="https://github.com/caronc/dateblock",
    project_urls={
        "Source": "https://github.com/caronc/dateblock",
        "Issues": "https://github.com/caronc/dateblock/issues",
        "Changelog":
            "https://github.com/caronc/dateblock/blob/master/ChangeLog",
    },
    author="Chris Caron",
    author_email="lead2gold@gmail.com",
    license="GPL-2.0-or-later",
    python_requires=">=3.9",
    package_dir={"": "python"},
    packages=["dateblock"],
    ext_modules=[extension],
    entry_points=entry_points,
    classifiers=[
        "Development Status :: 5 - Production/Stable",
        "Environment :: Console",
        "Intended Audience :: Developers",
        "Intended Audience :: System Administrators",
        "Natural Language :: English",
        "Operating System :: MacOS",
        "Operating System :: Microsoft :: Windows",
        "Operating System :: POSIX :: Linux",
        "Programming Language :: C++",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3 :: Only",
        "Programming Language :: Python :: 3.9",
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: 3.11",
        "Programming Language :: Python :: 3.12",
        "Programming Language :: Python :: 3.13",
        "Programming Language :: Python :: 3.14",
        "Topic :: System :: Systems Administration",
        "Topic :: Utilities",
    ],
    keywords="cron crontab sleep block schedule date time",
)
