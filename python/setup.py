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
# Python bindings for libdateblock.
#
# - By default this builds against the dateblock source tree it lives in
#   (run ./configure && make at the top first).
# - Set DATEBLOCK_SRCDIR to point at a different dateblock source tree, or
#   use CPPFLAGS/LDFLAGS to build against an installed libdateblock
#   (ie: CPPFLAGS="$(pkg-config --cflags dateblock)").
import os
from setuptools import setup, Extension

HERE = os.path.dirname(os.path.abspath(__file__))
SRCDIR = os.environ.get(
    'DATEBLOCK_SRCDIR', os.path.normpath(os.path.join(HERE, '..')))

setup(
    name='dateblock',
    version='1.1.0',
    url='https://github.com/caronc/dateblock/',
    description='Cron-like blocking (sleep until a moment in time)',
    author='Chris Caron',
    author_email='lead2gold@gmail.com',
    license='GPL-2.0-or-later',
    python_requires='>=3.9',
    classifiers=[
        'Development Status :: 5 - Production/Stable',
        'Intended Audience :: Developers',
        'Operating System :: POSIX :: Linux',
        'Natural Language :: English',
        'Programming Language :: C++',
        'Programming Language :: Python :: 3',
    ],
    ext_modules=[
        Extension(
            'dateblock',
            sources=['dateblockmodule.cpp'],
            include_dirs=[os.path.join(SRCDIR, 'src')],
            library_dirs=[os.path.join(SRCDIR, 'src', '.libs')],
            libraries=['dateblock'],
        ),
    ],
    keywords='date sleep block time cron',
)
