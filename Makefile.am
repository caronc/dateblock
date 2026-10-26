ACLOCAL_AMFLAGS = -I m4

# configure takes the version from here, so rebuild it when this changes.
# autoconf has to be forced to, or it reuses what it cached before.
CONFIGURE_DEPENDENCIES = $(top_srcdir)/python/dateblock/__init__.py
AUTOCONF = $(SHELL) $(top_srcdir)/missing autoconf --force
AUTOMAKE_OPTIONS = foreign
SUBDIRS = src tests

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = dateblock.pc

dist_man_MANS = packaging/man/dateblock.1

EXTRA_DIST = \
	autogen.sh \
	GNUmakefile \
	.dockerignore \
	README.md \
	INSTALL.md \
	COPYING \
	LICENSE \
	ChangeLog \
	AUTHORS \
	packaging/README.md \
	packaging/man/dateblock.md \
	packaging/man/dateblock.1.html \
	packaging/redhat/dateblock.spec \
	packaging/redhat/dateblock.rpmlintrc \
	packaging/redhat/dateblock.rpmlintrc.el9 \
	packaging/debian/changelog \
	packaging/debian/control \
	packaging/debian/copyright \
	packaging/debian/rules \
	packaging/debian/source/format \
	packaging/debian/dateblock.install \
	packaging/debian/libdateblock1.install \
	packaging/debian/libdateblock-dev.install \
	packaging/debian/dateblock.lintian-overrides \
	packaging/debian/libdateblock1.lintian-overrides \
	packaging/debian/libdateblock-dev.lintian-overrides \
	packaging/debian/python3-dateblock.lintian-overrides \
	packaging/arch/PKGBUILD \
	packaging/docker/build.sh \
	packaging/docker/recipes.sh \
	packaging/homebrew/dateblock.rb \
	packaging/docker/Dockerfile.f44 \
	packaging/docker/Dockerfile.el9 \
	packaging/docker/Dockerfile.el10 \
	packaging/docker/Dockerfile.debian13 \
	packaging/docker/Dockerfile.arch \
	.github/workflows/wheels.yml \
	.github/workflows/pypi-upload.sh \
	pyproject.toml \
	setup.py \
	MANIFEST.in \
	python/_dateblock.cpp \
	python/dateblock/__init__.py \
	python/tests/test_dateblock.py
