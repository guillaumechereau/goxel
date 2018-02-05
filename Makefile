
all:
	scons -j 8

release:
	scons debug=0

profile:
	scons profile=1

run:
	./goxel

.PHONY: js
js:
	$(EMSCRIPTEN)/emscons scons debug=0 emscripten=1

clean:
	scons -c

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
