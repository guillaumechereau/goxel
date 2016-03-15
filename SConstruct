import glob
import os
import sys

# CCFLAGS   : C and C++
# CFLAGS    : only C
# CXXFLAGS  : only C++

target_os = str(Platform())

debug = int(ARGUMENTS.get('debug', 1))
gprof = int(ARGUMENTS.get('gprof', 0))
profile = int(ARGUMENTS.get('profile', 0))
glut = int(ARGUMENTS.get('glut', 0))
emscripten = ARGUMENTS.get('emscripten', 0)

if gprof or profile: debug = 0
if emscripten: target_os = 'js'

env = Environment(ENV = os.environ)

# Asan & Ubsan (need to come first).
if debug and target_os == 'posix':
    env.Append(CCFLAGS=['-fsanitize=address', '-fsanitize=undefined'],
               LIBS=['asan', 'ubsan'])

env.Append(CFLAGS= '-Wall -Werror -std=gnu99 -Wno-unknown-pragmas',
           CXXFLAGS='-std=gnu++11 -Wall -Werror -Wno-narrowing '
                    '-Wno-unknown-pragmas'
        )

if debug:
    env.Append(CCFLAGS='-g')
else:
    env.Append(CCFLAGS='-O3 -DNDEBUG -D_FORTIFY_SOURCE=2')

if gprof:
    env.Append(CCFLAGS='-pg', LINKFLAGS='-pg')
if profile:
    env.Append(CCFLAGS='-DPROFILER=1')

env.Append(CPPPATH=['src'])

sources = glob.glob('src/*.c') + glob.glob('src/*.cpp')

if target_os == 'posix':
    env.Append(LIBS=['GL', 'm', 'z'])
    # Note: add '--static' to link with all the libs needed by glfw3.
    env.ParseConfig('pkg-config --libs glfw3')

if glut:
    env.Append(CCFLAGS='-DUSE_GLUT=1', LIBS='glut')

if target_os == 'msys':
    env.Append(CCFLAGS='-DNO_ARPG')
    env.Append(LIBS=['glfw3', 'opengl32', 'Imm32', 'gdi32', 'Comdlg32', 'z'],
               LINKFLAGS='--static')

if target_os == 'darwin':
    sources += glob.glob('src/*.m')
    env.Append(FRAMEWORKS=['OpenGL', 'Cocoa'])
    env.Append(LIBS=['m', 'z', 'argp', 'glfw3', 'objc'])

env.Append(CPPPATH=['ext_src/uthash'])
env.Append(CPPPATH=['ext_src/stb'])

sources += glob.glob('ext_src/imgui/*.cpp')
env.Append(CPPPATH=['ext_src/imgui'])
env.Append(CXXFLAGS='-DIMGUI_INCLUDE_IMGUI_USER_INL')

if target_os == 'posix':
    env.ParseConfig('pkg-config --cflags --libs gtk+-3.0')

if target_os == 'msys':
    sources += glob.glob('ext_src/glew/glew.c')
    env.Append(CPPPATH=['ext_src/glew'])
    env.Append(CCFLAGS='-DGLEW_STATIC')

if target_os == 'js':
    assert(os.environ['EMSCRIPTEN_TOOL_PATH'])
    env.Tool('emscripten', toolpath=[os.environ['EMSCRIPTEN_TOOL_PATH']])
    env.Append(CCFLAGS=['-s', 'USE_GLFW=3',
                        '-DGLES2 1', '-DNO_ZLIB', '-DNO_ARGP',
                        '-s', 'ALLOW_MEMORY_GROWTH=1',
                        '-s', '"EXPORTED_FUNCTIONS=[\'_main\']"'])
    env.Append(LINKFLAGS=['--preload-file', 'data',
                          '-s', 'USE_GLFW=3',
                          '-s', 'ALLOW_MEMORY_GROWTH=1',
                          '-s', '"EXPORTED_FUNCTIONS=[\'_main\']"'])
    env.Append(LIBS=['GL'])

env.Program(target='goxel', source=sources)
