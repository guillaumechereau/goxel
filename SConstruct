import glob
import os
import sys

# CCFLAGS   : C and C++
# CFLAGS    : only C
# CXXFLAGS  : only C++

target_os = str(Platform())

debug = int(ARGUMENTS.get('debug', 1))
profile = int(ARGUMENTS.get('profile', 0))
glut = int(ARGUMENTS.get('glut', 0))
emscripten = ARGUMENTS.get('emscripten', 0)
werror = int(ARGUMENTS.get("werror", 1))
clang = int(ARGUMENTS.get("clang", 0))
argp_standalone = int(ARGUMENTS.get("argp_standalone", 0))
sound = False

if profile: debug = 0
if emscripten: target_os = 'js'

env = Environment(ENV = os.environ)
conf = env.Configure()

if clang:
    env.Replace(CC='clang', CXX='clang++')

# Asan & Ubsan (need to come first).
if debug and target_os == 'posix':
    env.Append(CCFLAGS=['-fsanitize=address', '-fsanitize=undefined'],
               LIBS=['asan', 'ubsan'])

env.Append(CFLAGS= '-Wall -std=gnu99 -Wno-unknown-pragmas',
           CXXFLAGS='-std=gnu++11 -Wall -Wno-narrowing '
                    '-Wno-unknown-pragmas -Wno-unused-function'
        )

if werror:
    env.Append(CCFLAGS='-Werror')

if debug:
    if env['CC'] == 'gcc':
        env.Append(CFLAGS='-Og')
else:
    env.Append(CCFLAGS='-Ofast -DNDEBUG')

if profile or debug:
    env.Append(CCFLAGS='-g')

env.Append(CPPPATH=['src'])

sources = glob.glob('src/*.c') + glob.glob('src/*.cpp') + \
          glob.glob('src/formats/*.c') + \
          glob.glob('src/tools/*.c')

if target_os == 'posix':
    env.Append(LIBS=['GL', 'm', 'z'])
    # Note: add '--static' to link with all the libs needed by glfw3.
    env.ParseConfig('pkg-config --libs glfw3')

if glut:
    env.Append(CCFLAGS='-DUSE_GLUT=1', LIBS='glut')

if argp_standalone:
    env.Append(LIBS='argp')

if target_os == 'msys':
    env.Append(CCFLAGS='-DNO_ARGP')
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

if conf.CheckLib('libpng'):
    env.Append(CCFLAGS='-DHAVE_LIBPNG=1')

sources += glob.glob('ext_src/imgui/*.cpp')
env.Append(CPPPATH=['ext_src/imgui'])
env.Append(CXXFLAGS='-DIMGUI_INCLUDE_IMGUI_USER_INL')

sources += glob.glob('ext_src/inih/*.c')
env.Append(CPPPATH=['ext_src/inih'])
env.Append(CFLAGS='-DINI_HANDLER_LINENO=1')

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
