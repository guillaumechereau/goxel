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

# Make the doc using natualdocs.  On debian, we only have an old version
# of naturaldocs available, where it is not possible to exclude files by
# pattern.  I don't want to parse the C files (only the headers), so for
# the moment I use a tmp file to copy the sources and remove the C files.
# It's a bit ugly.
.PHONY: doc
doc:
	rm -rf /tmp/goxel_src
	cp -rf src /tmp/goxel_src
	find /tmp/goxel_src -name '*.c' | xargs rm
	mkdir -p doc ndconfig
	naturaldocs -i /tmp/goxel_src -o html doc -p ndconfig

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
