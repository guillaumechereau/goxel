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
