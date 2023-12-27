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
	    snap/gui/io.github.guillaumechereau.Goxel.appdata.xml \
	    $$(printf '%s%s' $(DESTDIR)$(PREFIX)/share/metainfo/ \
	        io.github.guillaumechereau.Goxel.appdata.xml)

.PHONY: uninstall
uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/goxel
	for size in 16 24 32 48 64 128 256; do \
	    rm -f $$(printf '%s%s' $(DESTDIR)$(PREFIX)/share/icons/hicolor/ \
	        $${size}x$${size}/apps/goxel.png)
	done
	rm -f $(DESTDIR)$(PREFIX)/share/applications/goxel.desktop
	rm -f $$(printf '%s%s' $(DESTDIR)$(PREFIX)/share/metainfo/ \
	         io.github.guillaumechereau.Goxel.appdata.xml)
