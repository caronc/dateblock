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
#   make release                  build everything into dist/release/
#
# DISTRO picks the container (f44 by default) for docker-build,
# docker-rpm and docker-shell; one of: f44 el9 el10 debian13 arch.
# Results are written to dist/<distro>/.  Set CONTAINER=podman to use
# podman instead of docker.  docker-pypi (and so release) also needs uv
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
DOCKER_FILE = packaging/docker/Dockerfile.$(DISTRO)
DOCKER_IMAGE = dateblock-builder:$(DISTRO)
DOCKER_OUT = $(CURDIR)/dist/$(DISTRO)

CONTAINER ?= $(shell command -v docker 2>/dev/null || command -v podman 2>/dev/null)

# Files created in dist/ should belong to you, not root
DOCKER_USER = $(shell $(CONTAINER) --version 2>/dev/null | grep -qi podman \
                && echo --userns=keep-id || echo --user $$(id -u):$$(id -g))

DOCKER_RUN = $(CONTAINER) run --rm $(DOCKER_USER) -e HOME=/tmp \
             -v $(CURDIR):/src:ro,z -v $(DOCKER_OUT):/out:z

docker-image:
	@test -n "$(CONTAINER)" || \
	   { echo "Neither docker nor podman was found."; exit 1; }
	@test -f "$(DOCKER_FILE)" || \
	   { echo "Unknown DISTRO '$(DISTRO)'; use one of: $(DOCKER_DISTROS)"; exit 1; }
	$(CONTAINER) build -t $(DOCKER_IMAGE) -f $(DOCKER_FILE) .

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
# this machine's.  Use PYPI_ARCHS="x86_64 aarch64" to add 64-bit ARM (ie:
# Raspberry Pi) wheels; that runs under emulation (needs qemu-user-static)
# and is slow.  macOS and Windows wheels can only be built on those systems
# (see .github/workflows/wheels.yml).
CIBUILDWHEEL_VERSION = 4.3.4
PYPI_ARCHS ?= native
PYPI_OUT = dist/pypi
PYRUN = $(shell command -v uvx >/dev/null 2>&1 && echo uvx || \
          { command -v pipx >/dev/null 2>&1 && echo pipx run; })

docker-pypi:
	@test -n "$(PYRUN)" || \
	   { echo "docker-pypi needs uv (uvx) or pipx installed."; exit 1; }
	rm -rf $(PYPI_OUT)
	$(PYRUN) --from build pyproject-build --sdist --outdir $(PYPI_OUT) .
	CIBW_CONTAINER_ENGINE=$(notdir $(CONTAINER)) \
	   $(PYRUN) cibuildwheel==$(CIBUILDWHEEL_VERSION) --platform linux \
	   --archs "$(PYPI_ARCHS)" --output-dir $(PYPI_OUT) .
	$(PYRUN) twine check --strict $(PYPI_OUT)/*
	@echo "PyPI files written to $(PYPI_OUT)/"

# Everything for a release, gathered into dist/release/ with checksums:
#   dateblock-<version>.tar.gz  the source tarball
#   pypi/                       upload with: twine upload dist/release/pypi/*
#   <distro>/                   the RPM, Debian and Arch packages
release: docker-build docker-all docker-pypi
	rm -rf dist/release
	mkdir -p dist/release
	cp dist/f44/dateblock-*.tar.gz dist/release/
	cp -r $(PYPI_OUT) dist/release/pypi
	for d in $(DOCKER_DISTROS); do \
	   mkdir -p dist/release/$$d; \
	   find dist/$$d -maxdepth 1 -type f ! -name 'dateblock-*.tar.gz' \
	      -exec cp {} dist/release/$$d/ \; ; \
	done
	cd dist/release && find . -type f ! -name SHA256SUMS | sort | \
	   xargs sha256sum > SHA256SUMS
	@echo "Release files written to dist/release/"

.PHONY: docker-image docker-build docker-rpm docker-rpm-all docker-deb \
        docker-arch docker-all docker-package docker-man docker-shell \
        docker-pypi release
