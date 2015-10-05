
all:
	scons -j 8

release:
	scons debug=0

profile:
	scons profile=1

gprof:
	scons gprof=1

run:
	./goxel

clean:
	scons -c
