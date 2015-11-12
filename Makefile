
all:
	scons -j 8

release:
	scons debug=0

profile:
	scons profile=1

gprof:
	scons gprof=1

static_glfw:
	scons static_glfw=1

run:
	./goxel

.PHONY: js
js:
	$(EMSCRIPTEN)/emscons scons debug=0 emscripten=1

clean:
	scons -c
