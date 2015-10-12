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
if gprof or profile: debug = 0

env = Environment(ENV = os.environ)

# Asan & Ubsan (need to come first).
if debug and target_os == 'posix':
    env.Append(CCFLAGS=['-fsanitize=address', '-fsanitize=undefined'],
               LIBS=['asan', 'ubsan'])

env.Append(CFLAGS= '-Wall -Werror -std=gnu99 -Wno-unknown-pragmas',
           CXXFLAGS='-Wall -Werror -Wno-narrowing -Wno-unknown-pragmas'
        )

if debug:
    env.Append(CCFLAGS='-g')
else:
    env.Append(CCFLAGS='-O3 -DNDEBUG')

if gprof:
    env.Append(CCFLAGS='-pg', LINKFLAGS='-pg')
if profile:
    env.Append(CCFLAGS='-DPROFILER=1')

env.Append(CPPPATH=['src'])

sources = glob.glob('src/*.c') + glob.glob('src/*.cpp')

if target_os == 'posix':
    env.Append(LIBS=['GL', 'glfw', 'm'])

if target_os == 'msys':
    env.Append(LIBS=['glfw3', 'opengl32', 'Imm32', 'gdi32', 'Comdlg32'],
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

env.Program(target='goxel', source=sources)
