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
# this program; if not, see <https://www.gnu.org/licenses/>.
#
# GNU make reads this file before 'Makefile'.  It adds the docker-* targets
# below (which work from a fresh checkout, without running ./configure) and
# hands every other target to the Makefile that ./configure generates.
#
#   make docker-build             compile and run all of the tests
#   make docker-rpm               build the RPMs (DISTRO=f44, el9 or el10)
#   make docker-rpm-all           build the RPMs for f44, el9 and el10
#   make docker-deb               build the Debian 13 packages
#   make docker-arch              build the Arch Linux packages
#   make docker-all               build every package listed above
#   make docker-man               regenerate packaging/man/dateblock.1
#   make docker-shell             open a shell in the build container
#   make docker-pypi              build the PyPI source package and wheels
#   make release-assets VERSION=x.y.z
#                                 build every file for a release (see below)
#   make release-recipes VERSION=x.y.z
#                                 the AUR and Homebrew files for a release
#   make release-clean            delete what the builds left behind
#   make clean                    also deletes dist/, __pycache__/ and the
#                                 Python build files (see clean-tree)
#
# DISTRO picks the container (f44 by default) for docker-build,
# docker-rpm and docker-shell; one of: f44 el9 el10 debian13 arch.
# ARCH=aarch64 builds the packages for 64-bit ARM instead of this machine's
# CPU (under emulation: needs qemu-user-static, and is slow; Arch Linux has
# no ARM image).  Results are written to dist/<distro>/ (or
# dist/<distro>-<arch>/).  Set CONTAINER=podman to use podman instead of
# docker.  docker-pypi (and so release-assets) also needs uv
# (https://docs.astral.sh/uv/) or pipx to run cibuildwheel and twine.

ifneq ($(wildcard Makefile),)
include Makefile
else
.DEFAULT_GOAL := no-makefile
no-makefile:
	@echo "No Makefile yet; either run './autogen.sh && ./configure' first,"
	@echo "or build inside a container with 'make docker-build'."
.PHONY: no-makefile
endif

DISTRO ?= f44
DOCKER_DISTROS = f44 el9 el10 debian13 arch
DOCKER_RPM_DISTROS = f44 el9 el10
DOCKER_ARM_DISTROS = f44 el9 el10 debian13
DOCKER_FILE = packaging/docker/Dockerfile.$(DISTRO)

# ARCH=native (the default) builds for this machine's CPU
ARCH ?= native
ifeq ($(ARCH),native)
ARCH_SUFFIX =
DOCKER_PLATFORM =
else
ARCH_SUFFIX = -$(ARCH)
DOCKER_PLATFORM = --platform linux/$(subst aarch64,arm64,$(subst x86_64,amd64,$(ARCH)))
endif
DOCKER_IMAGE = dateblock-builder:$(DISTRO)$(ARCH_SUFFIX)
DOCKER_OUT = $(CURDIR)/dist/$(DISTRO)$(ARCH_SUFFIX)

# Every image these builds make carries this label, so release-clean only
# ever removes those
DOCKER_LABEL = io.github.caronc.dateblock-builder=1

CONTAINER ?= $(shell command -v docker 2>/dev/null || command -v podman 2>/dev/null)

# Files created in dist/ should belong to you, not root
DOCKER_USER = $(shell $(CONTAINER) --version 2>/dev/null | grep -qi podman \
                && echo --userns=keep-id || echo --user $$(id -u):$$(id -g))

DOCKER_RUN = $(CONTAINER) run --rm $(DOCKER_PLATFORM) $(DOCKER_USER) -e HOME=/tmp \
             -v $(CURDIR):/src:ro,z -v $(DOCKER_OUT):/out:z

docker-image:
	@test -n "$(CONTAINER)" || \
	   { echo "Neither docker nor podman was found."; exit 1; }
	@test -f "$(DOCKER_FILE)" || \
	   { echo "Unknown DISTRO '$(DISTRO)'; use one of: $(DOCKER_DISTROS)"; exit 1; }
	$(CONTAINER) build $(DOCKER_PLATFORM) --label $(DOCKER_LABEL) \
	   -t $(DOCKER_IMAGE) -f $(DOCKER_FILE) .

docker-build: docker-image
	mkdir -p $(DOCKER_OUT)
	$(DOCKER_RUN) $(DOCKER_IMAGE) /src/packaging/docker/build.sh build
	@echo "Source tarball written to dist/$(DISTRO)/"

docker-rpm:
	@case " $(DOCKER_RPM_DISTROS) " in *" $(DISTRO) "*) ;; \
	   *) echo "docker-rpm needs DISTRO set to one of: $(DOCKER_RPM_DISTROS)"; \
	      exit 1;; esac
	$(MAKE) --no-print-directory docker-package DOCKER_MODE=rpm

docker-rpm-all:
	@for d in $(DOCKER_RPM_DISTROS); do \
	   $(MAKE) --no-print-directory docker-rpm DISTRO=$$d || exit 1; \
	done

docker-deb:
	$(MAKE) --no-print-directory docker-package DISTRO=debian13 DOCKER_MODE=deb

docker-arch:
	$(MAKE) --no-print-directory docker-package DISTRO=arch DOCKER_MODE=arch

docker-all: docker-rpm-all docker-deb docker-arch

docker-package: docker-image
	mkdir -p $(DOCKER_OUT)
	$(DOCKER_RUN) $(DOCKER_IMAGE) /src/packaging/docker/build.sh $(DOCKER_MODE)
	@echo "Results written to $(DOCKER_OUT:$(CURDIR)/%=%)/"

# The man page is written in packaging/man/dateblock.md (ronn format)
docker-man:
	$(MAKE) --no-print-directory docker-package DISTRO=f44 DOCKER_MODE=man \
	   DOCKER_OUT=$(CURDIR)/dist/man
	cp dist/man/dateblock.1 dist/man/dateblock.1.html packaging/man/
	@echo "Updated packaging/man/dateblock.1 and dateblock.1.html"

docker-shell: docker-image
	mkdir -p $(DOCKER_OUT)
	$(DOCKER_RUN) -it -w /src $(DOCKER_IMAGE) /bin/bash

# PyPI: the source package plus Linux wheels (manylinux and musllinux) for
# every supported Python.  PYPI_ARCHS picks the CPU types; the default is
# this machine's.  The tests run in one wheel (the newest Python) for each
# Linux type and CPU; every wheel holds the same C++ code, so testing them
# all again only takes longer (set PYPI_TEST_ALL=1 to do it anyway).  Use PYPI_ARCHS="x86_64 aarch64" to add 64-bit ARM (ie:
# Raspberry Pi) wheels; that runs under emulation (needs qemu-user-static)
# and is slow.  macOS and Windows wheels can only be built on those systems
# (see .github/workflows/wheels.yml).
CIBUILDWHEEL_VERSION = 4.3.4
PYPI_ARCHS ?= native
PYPI_OUT = dist/pypi
PYPI_TEST_SKIP = $(if $(PYPI_TEST_ALL),,cp39-* cp310-* cp311-* cp312-* cp313-*)
PYRUN = $(shell command -v uvx >/dev/null 2>&1 && echo uvx || \
          { command -v pipx >/dev/null 2>&1 && echo pipx run; })

docker-pypi:
	@test -n "$(PYRUN)" || \
	   { echo "docker-pypi needs uv (uvx) or pipx installed."; exit 1; }
	rm -rf $(PYPI_OUT)
	$(PYRUN) --from build pyproject-build --sdist --outdir $(PYPI_OUT) .
	CIBW_CONTAINER_ENGINE=$(notdir $(CONTAINER)) \
	   CIBW_TEST_SKIP="$(PYPI_TEST_SKIP)" \
	   $(PYRUN) cibuildwheel==$(CIBUILDWHEEL_VERSION) --platform linux \
	   --archs "$(PYPI_ARCHS)" --output-dir $(PYPI_OUT) .
	$(PYRUN) twine check --strict $(PYPI_OUT)/*
	@echo "PyPI files written to $(PYPI_OUT)/"

# The files for a GitHub release, in dist/release-<version>/, plus the
# release notes (that version's ChangeLog section) beside it:
#   - the source tarball (made by 'make dist', so it includes ./configure)
#   - SHA256SUMS, its checksum (sha256sum -c SHA256SUMS)
# Before gathering them, every RPM, Debian and Arch Linux package is built
# and tested, as proof that the release builds everywhere; the packages
# themselves are published through their own channels (EPEL, the AUR,
# Homebrew), not attached.  The wheels are left to the GitHub workflow,
# which builds and tests them on every platform and publishes them to PyPI.
# RELEASE_ARCHS="native aarch64" also builds the packages for 64-bit ARM
# (under emulation, so it takes much longer).
# VERSION must match __version__ (python/dateblock/__init__.py), and the
# checkout must be that version's
# tag with no local changes (UNTAGGED=1 skips that check, for a test run).
# Each build image is removed as soon as its packages are done, to save
# disk space.  Run 'make release-clean' once you have the files.
VERSION_IN_CODE = $(shell sed -n 's/^__version__ = "\(.*\)"$$/\1/p' python/dateblock/__init__.py)
RELEASE_DIR = dist/release-$(VERSION)
RELEASE_NOTES = dist/release-$(VERSION)-notes.md
RELEASE_ARCHS ?= native

release-check:
	@test -n "$(VERSION)" || \
	   { echo "Usage: make release-assets VERSION=x.y.z"; exit 1; }
	@test "$(VERSION)" = "$(VERSION_IN_CODE)" || \
	   { echo "VERSION=$(VERSION) does not match __version__ ($(VERSION_IN_CODE))"; exit 1; }
	@test "$(UNTAGGED)" = 1 || \
	   { test -z "$$(git status --porcelain)" && \
	     test "$$(git describe --exact-match --tags HEAD 2>/dev/null)" = "v$(VERSION)"; } || \
	   { echo "Check out the v$(VERSION) tag (with no local changes) first;"; \
	     echo "or set UNTAGGED=1 for a test run."; exit 1; }

release-assets: release-check
	@# Start from empty build folders, so only this run's files are gathered
	rm -rf $(RELEASE_DIR) $(RELEASE_NOTES) \
	   $(foreach d,$(DOCKER_DISTROS),dist/$(d) dist/$(d)-*)
	@for arch in $(RELEASE_ARCHS); do \
	   distros="$(DOCKER_DISTROS)"; \
	   [ $$arch = native ] || distros="$(DOCKER_ARM_DISTROS)"; \
	   for d in $$distros; do \
	      case $$d in debian13) mode=deb;; arch) mode=arch;; *) mode=rpm;; esac; \
	      $(MAKE) --no-print-directory docker-package DISTRO=$$d \
	         DOCKER_MODE=$$mode ARCH=$$arch || exit 1; \
	      $(MAKE) --no-print-directory docker-forget DISTRO=$$d ARCH=$$arch; \
	   done; \
	done
	mkdir -p $(RELEASE_DIR)
	cp dist/f44/dateblock-$(VERSION).tar.gz $(RELEASE_DIR)/
	cd $(RELEASE_DIR) && sha256sum -- * > SHA256SUMS
	awk -v v="$(VERSION)" '/^## \[/ { show = index($$0, "[" v "]") > 0; next } \
	   show' ChangeLog > $(RELEASE_NOTES)
	@echo "The source tarball's checksum is in SHA256SUMS (sha256sum -c SHA256SUMS)." \
	   >> $(RELEASE_NOTES)
	@echo
	@echo "$$(ls $(RELEASE_DIR) | wc -l) files written to $(RELEASE_DIR)/"
	@echo "Release notes written to $(RELEASE_NOTES)"

# The files to publish for a release, in dist/recipes/: the Arch User
# Repository's PKGBUILD and .SRCINFO, and the Homebrew formula.  Both use
# GitHub's tarball of the vx.y.z tag, so push the tag first; the same
# VERSION and tag checks as release-assets apply.
release-recipes: DISTRO = arch
release-recipes: ARCH = native
release-recipes: DOCKER_OUT = $(CURDIR)/dist/recipes
release-recipes: release-check docker-image
	rm -rf $(DOCKER_OUT)
	mkdir -p $(DOCKER_OUT)
	$(DOCKER_RUN) $(DOCKER_IMAGE) /src/packaging/docker/recipes.sh $(VERSION)
	@echo "Release recipes written to dist/recipes/"

# 'make clean' also removes what the docker-*, release-* and Python builds
# leave in the source tree, whether or not ./configure has been run (the
# build images are removed by release-clean)
clean: clean-tree

clean-tree:
	rm -rf dist build wheelhouse *.egg-info python/*.egg-info \
	   dateblock-*.tar.gz
	find . -name __pycache__ -type d -prune -exec rm -rf {} +

# Removes one build image (and the build cache) to free disk space
docker-forget:
	-@$(CONTAINER) rmi $(DOCKER_IMAGE) >/dev/null 2>&1
	-@$(CONTAINER) image prune -f --filter label=$(DOCKER_LABEL) >/dev/null 2>&1
	-@$(CONTAINER) builder prune -f >/dev/null 2>&1

# Deletes everything the builds left behind: dist/ and the build images
# (only ever the ones labelled as dateblock's), cibuildwheel's images and
# the build cache.  Volumes are never touched.
release-clean:
	rm -rf dist
	-@images=$$($(CONTAINER) images -a -q --filter label=$(DOCKER_LABEL) | sort -u); \
	   [ -z "$$images" ] || $(CONTAINER) rmi -f $$images >/dev/null
	@# cibuildwheel's images carry no tag, so they are removed by ID
	-@images=$$($(CONTAINER) images -a --format '{{.Repository}} {{.ID}}' | \
	   awk '$$1 ~ /^quay.io\/pypa\// { print $$2 }' | sort -u); \
	   [ -z "$$images" ] || $(CONTAINER) rmi $$images >/dev/null
	-@$(CONTAINER) builder prune -f >/dev/null 2>&1
	@echo "Removed dist/ and the build images"

.PHONY: docker-image docker-build docker-rpm docker-rpm-all docker-deb \
        docker-arch docker-all docker-package docker-man docker-shell \
        docker-pypi docker-forget release-check release-assets \
        release-recipes release-clean clean clean-tree
