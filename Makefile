SHELL = bash
.ONESHELL:

all:
	scons -j 4

release:
	scons mode=release

profile:
	scons mode=profile

run:
	./goxel2

clean:
	scons -c
	rm -rf lib/lua-5.4.4/build

lua:
	cd lib/lua-5.4.4/
	make
