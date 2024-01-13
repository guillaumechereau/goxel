SHELL = bash
.ONESHELL:

all:
	scons -j 8

release:
	scons mode=release

profile:
	scons mode=profile

run:
	./goxel

clean:
	scons -c

analyze:
	scan-build scons mode=analyze

# Generate an AppImage.  Used by github CI.
appimage:
	scons mode=release
	mkdir AppDir
	DESTDIR=AppDir PREFIX=/usr make install
	curl https://github.com/linuxdeploy/linuxdeploy/releases/download/1-alpha-20231206-1/linuxdeploy-x86_64.AppImage \
		--output linuxdeploy.AppImage -L -f
	curl https://raw.githubusercontent.com/linuxdeploy/linuxdeploy-plugin-gtk/master/linuxdeploy-plugin-gtk.sh \
		--output linuxdeploy-plugin-gtk.sh
	chmod +x linuxdeploy.AppImage linuxdeploy-plugin-gtk.sh
	./linuxdeploy.AppImage --output=appimage --appdir=AppDir --plugin=gtk

# Targets to install/uninstall goxel and its data files on unix system.
PREFIX ?= /usr/local

.PHONY: install
install:
	install -Dm755 goxel $(DESTDIR)$(PREFIX)/bin/goxel
	for size in 16 24 32 48 64 128 256; do
	    install -Dm644 data/icons/icon$${size}.png \
	        $$(printf '%s%s' $(DESTDIR)$(PREFIX)/share/icons/hicolor/ \
	            $${size}x$${size}/apps/goxel.png)
	done
	install -Dm644 snap/gui/goxel.desktop \
	    $(DESTDIR)$(PREFIX)/share/applications/goxel.desktop
	install -Dm644 \
	    snap/gui/io.github.guillaumechereau.Goxel.metainfo.xml \
	    $$(printf '%s%s' $(DESTDIR)$(PREFIX)/share/metainfo/ \
	        io.github.guillaumechereau.Goxel.metainfo.xml)

.PHONY: uninstall
uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/goxel
	for size in 16 24 32 48 64 128 256; do \
	    rm -f $$(printf '%s%s' $(DESTDIR)$(PREFIX)/share/icons/hicolor/ \
	        $${size}x$${size}/apps/goxel.png)
	done
	rm -f $(DESTDIR)$(PREFIX)/share/applications/goxel.desktop
	rm -f $$(printf '%s%s' $(DESTDIR)$(PREFIX)/share/metainfo/ \
	         io.github.guillaumechereau.Goxel.metainfo.xml)
