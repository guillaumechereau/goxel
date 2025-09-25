SHELL = bash
ifeq ($(OS),Linux)
	JOBS := "-j $(shell nproc)"
else
	JOBS := "-j $(shell getconf _NPROCESSORS_ONLN)"
endif

.ONESHELL:

all: .FORCE
	scons $(JOBS)

release:
	scons $(JOBS) mode=release

profile:
	scons $(JOBS) mode=profile

run:
	./Goxel++

clean: .FORCE
	scons -c

analyze:
	scan-build scons mode=analyze

# For the moment only apply the format to uncommited changes.
format: .FORCE
	git clang-format -f

# Generate an AppImage.  Used by github CI.
appimage: .FORCE
	scons mode=release nfd_backend=portal
	rm -rf AppDir
	mkdir AppDir
	DESTDIR=AppDir PREFIX=/usr make install
	curl https://github.com/linuxdeploy/linuxdeploy/releases/download/1-alpha-20231206-1/linuxdeploy-x86_64.AppImage \
		--output linuxdeploy.AppImage -L -f
	chmod +x linuxdeploy.AppImage
	./linuxdeploy.AppImage --output=appimage --appdir=AppDir

# Targets to install/uninstall goxel and its data files on unix system.
PREFIX ?= /usr/local

.PHONY: install
install:
	install -Dm755 Goxel++ $(DESTDIR)$(PREFIX)/bin/Goxel++
	for size in 16 24 32 48 64 128 256; do
	    install -Dm644 data/icons/icon$${size}.png \
	        $$(printf '%s%s' $(DESTDIR)$(PREFIX)/share/icons/hicolor/ \
	            $${size}x$${size}/apps/Goxel++.png)
	done
	install -Dm644 snap/gui/Goxel++.desktop \
	    $(DESTDIR)$(PREFIX)/share/applications/Goxel++.desktop
	install -Dm644 \
	    snap/gui/co.magicengineering.GoxelPlusPlus.metainfo.xml \
	    $$(printf '%s%s' $(DESTDIR)$(PREFIX)/share/metainfo/ \
	        co.magicengineering.GoxelPlusPlus.metainfo.xml)

.PHONY: uninstall
uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/Goxel++
	for size in 16 24 32 48 64 128 256; do \
	    rm -f $$(printf '%s%s' $(DESTDIR)$(PREFIX)/share/icons/hicolor/ \
	        $${size}x$${size}/apps/goxel.png)
	done
	rm -f $(DESTDIR)$(PREFIX)/share/applications/Goxel++.desktop
	rm -f $$(printf '%s%s' $(DESTDIR)$(PREFIX)/share/metainfo/ \
	         co.magicengineering.GoxelPlusPlus.metainfo.xml)

.PHONY: all

.FORCE:
