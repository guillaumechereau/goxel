import glob
import os
import sys

# CCFLAGS   : C and C++
# CFLAGS    : only C
# CXXFLAGS  : only C++

target_os = str(Platform())

debug = int(ARGUMENTS.get('debug', 1))
profile = int(ARGUMENTS.get('profile', 0))
emscripten = ARGUMENTS.get('emscripten', 0)
werror = int(ARGUMENTS.get("werror", 1))
clang = int(ARGUMENTS.get("clang", 0))
argp_standalone = int(ARGUMENTS.get("argp_standalone", 0))
cycles = int(ARGUMENTS.get('cycles', 1))
sound = False

if os.environ.get('CC') == 'clang': clang = 1
if profile: debug = 0
if emscripten: target_os = 'js'

env = Environment(ENV = os.environ)
conf = env.Configure()

if clang:
    env.Replace(CC='clang', CXX='clang++')

# Asan & Ubsan (need to come first).
# Cycles doesn't like libasan with clang, so we only use it on
# C code with clang.
if debug and target_os == 'posix':
    if not clang:
        env.Append(CCFLAGS=['-fsanitize=address', '-fsanitize=undefined'],
                   LINKFLAGS=['-fsanitize=address', '-fsanitize=undefined'],
                   LIBS=['asan', 'ubsan'])
    else:
        env.Append(CFLAGS=['-fsanitize=address', '-fsanitize=undefined'],
                   LINKFLAGS=['-fsanitize=address', '-fsanitize=undefined'])


env.Append(CFLAGS= '-Wall -std=gnu99 -Wno-unknown-pragmas '
                   '-Wno-unknown-warning-option',
           CXXFLAGS='-std=gnu++11 -Wall -Wno-narrowing '
                    '-Wno-unknown-pragmas -Wno-unused-function'
        )

if werror:
    env.Append(CCFLAGS='-Werror')

if debug:
    env.Append(CCFLAGS=['-O1'])
    if env['CC'] == 'gcc': env.Append(CCFLAGS='-Og')
else:
    env.Append(CCFLAGS=['-O3', '-DNDEBUG'])
    if env['CC'] == 'gcc': env.Append(CCFLAGS='-Ofast')

if profile or debug:
    env.Append(CCFLAGS='-g')

env.Append(CPPPATH=['src'])

sources = glob.glob('src/*.c') + glob.glob('src/*.cpp') + \
          glob.glob('src/formats/*.c') + \
          glob.glob('src/tools/*.c')

if target_os == 'posix':
    env.Append(LIBS=['GL', 'm', 'z'])
    if not conf.CheckDeclaration('__GLIBC__', includes='#include <features.h>'):
        env.Append(LIBS=['argp'])
    # Note: add '--static' to link with all the libs needed by glfw3.
    env.ParseConfig('pkg-config --libs glfw3')

if argp_standalone:
    env.Append(LIBS='argp')

if target_os == 'msys':
    env.Append(CCFLAGS='-DNO_ARGP')
    env.Append(CXXFLAGS=['-Wno-attributes', '-Wno-unused-variable',
                         '-DFREE_WINDOWS'])
    env.Append(LIBS=['glfw3', 'opengl32', 'Imm32', 'gdi32', 'Comdlg32',
                     'z', 'tre', 'intl', 'iconv'],
               LINKFLAGS='--static')

if target_os == 'darwin':
    sources += glob.glob('src/*.m')
    env.Append(FRAMEWORKS=['OpenGL', 'Cocoa'])
    env.Append(LIBS=['m', 'z', 'argp', 'glfw3', 'objc'])

env.Append(CPPPATH=['ext_src/uthash'])
env.Append(CPPPATH=['ext_src/stb'])
env.Append(CPPPATH=['ext_src/noc'])

if conf.CheckLibWithHeader('libpng', 'png.h', 'c'):
    env.Append(CCFLAGS='-DHAVE_LIBPNG=1')

sources += glob.glob('ext_src/inih/*.c')
env.Append(CPPPATH=['ext_src/inih'])
env.Append(CFLAGS='-DINI_HANDLER_LINENO=1')

# Cycles rendering support.
if cycles:
    sources += glob.glob('ext_src/cycles/src/util/*.cpp')
    sources = [x for x in sources if not x.endswith('util_view.cpp')]
    sources += glob.glob('ext_src/cycles/src/bvh/*.cpp')
    sources += glob.glob('ext_src/cycles/src/render/*.cpp')
    sources += glob.glob('ext_src/cycles/src/graph/*.cpp')
    sources = [x for x in sources if not x.endswith('node_xml.cpp')]

    sources += glob.glob('ext_src/cycles/src/device/device.cpp')
    sources += glob.glob('ext_src/cycles/src/device/device_cpu.cpp')
    sources += glob.glob('ext_src/cycles/src/device/device_memory.cpp')
    sources += glob.glob('ext_src/cycles/src/device/device_denoising.cpp')
    sources += glob.glob('ext_src/cycles/src/device/device_split_kernel.cpp')
    sources += glob.glob('ext_src/cycles/src/device/device_task.cpp')

    sources += glob.glob('ext_src/cycles/src/kernel/kernels/cpu/*.cpp')
    sources += glob.glob('ext_src/cycles/src/subd/*.cpp')

    env.Append(CPPPATH=['ext_src/cycles/src'])
    env.Append(CPPPATH=['ext_src/cycles/third_party/atomic'])
    env.Append(CPPFLAGS=[
        '-DCYCLES_STD_UNORDERED_MAP',
        '-DCCL_NAMESPACE_BEGIN=namespace ccl {',
        '-DCCL_NAMESPACE_END=}',
        '-DWITH_CUDA_DYNLOAD',
        '-DWITHOUT_OPENIMAGEIO',
        '-DWITH_GLEW_MX',
        '-DWITH_CYCLES',
    ])
    # Try to improve compilation speed on linux.
    if not clang: env.Append(CPPFLAGS='-fno-var-tracking-assignments')
    # Seems to fix a crash on windows and i386 targets!
    env.Append(CXXFLAGS='-msse2 -fno-tree-slp-vectorize')
    if target_os == 'msys': env.Append(CXXFLAGS='-O3')

env.Append(CPPFLAGS=['-Wno-sign-compare', '-Wno-strict-aliasing',
                     '-Wno-uninitialized'])
if clang:
    env.Append(CPPFLAGS=['-Wno-overloaded-virtual'])


if target_os == 'posix':
    env.ParseConfig('pkg-config --cflags --libs gtk+-3.0')

if target_os == 'msys':
    sources += glob.glob('ext_src/glew/glew.c')
    env.Append(CPPPATH=['ext_src/glew'])
    env.Append(CCFLAGS='-DGLEW_STATIC')

if target_os == 'js':
    assert(os.environ['EMSCRIPTEN_TOOL_PATH'])
    env.Tool('emscripten', toolpath=[os.environ['EMSCRIPTEN_TOOL_PATH']])

    funcs = ['_main']
    funcs = ','.join("'{}'".format(x) for x in funcs)
    flags = [
             '-s', 'ALLOW_MEMORY_GROWTH=1',
             '-s', 'USE_GLFW=3',
             '-Os',
             '-s', 'NO_EXIT_RUNTIME=1',
             '-s', '"EXPORTED_FUNCTIONS=[{}]"'.format(funcs),
            ]

    env.Append(CCFLAGS=['-DGLES2 1', '-DNO_ZLIB', '-DNO_ARGP'] + flags)
    env.Append(LINKFLAGS=flags)
    env.Append(LIBS=['GL'])

if sound:
    env.Append(LIBS='openal')
    env.Append(CCFLAGS='-DSOUND=OPENAL')

# Append external environment flags
env.Append(
    CFLAGS=os.environ.get("CFLAGS", "").split(),
    CXXFLAGS=os.environ.get("CXXFLAGS", "").split(),
    LINKFLAGS=os.environ.get("LDFLAGS", "").split()
)

env.Program(target='goxel', source=sources)
