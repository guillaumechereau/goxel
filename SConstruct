import glob
import os
import sys

vars = Variables('settings.py')
vars.AddVariables(
    EnumVariable('mode', 'Build mode', 'debug',
        allowed_values=('debug', 'release', 'profile', 'analyze')),
    BoolVariable('werror', 'Warnings as error', True),
    BoolVariable('sound', 'Enable sound', False),
    BoolVariable('yocto', 'Enable yocto renderer', True),
    PathVariable('config_file', 'Config file to use', 'src/config.h'),
)

target_os = str(Platform())

env = Environment(variables = vars, ENV = os.environ)
conf = env.Configure()

if env['mode'] == 'analyze':
    # Make sure clang static analyzer has a chance to override de compiler
    # and set CCC settings
    env["CC"] = os.getenv("CC") or env["CC"]
    env["CXX"] = os.getenv("CXX") or env["CXX"]
    env["ENV"].update(x for x in os.environ.items() if x[0].startswith("CCC_"))


if os.environ.get('CC') == 'clang':
    env.Replace(CC='clang', CXX='clang++')
else:
    env.Replace(CC='gcc', CXX='g++')

# Hack for gcc <= 5, since pragma diagnostic push doesn't seem to work.
if env['CCVERSION'] and int(env['CCVERSION'].split('.')[0]) <= 5:
    env.Append(CCFLAGS=['-Wno-unused-function'])

# Asan & Ubsan (need to come first).
if env['mode'] == 'debug' and target_os == 'posix':
    env.Append(CCFLAGS=['-fsanitize=address', '-fsanitize=undefined'],
               LINKFLAGS=['-fsanitize=address', '-fsanitize=undefined'],
               LIBS=['asan', 'ubsan'])


# Global compilation flags.
# CCFLAGS   : C and C++
# CFLAGS    : only C
# CXXFLAGS  : only C++
env.Append(
    CFLAGS=['-std=gnu99', '-Wall',
            '-Wno-unknow-pragma', '-Wno-unknown-warning-option'],
    CXXFLAGS=['-std=gnu++17', '-Wall', '-Wno-narrowing']
)

if env['werror']:
    env.Append(CCFLAGS='-Werror')

if env['mode'] not in ['debug', 'analyze']:
    env.Append(CPPDEFINES='NDEBUG', CCFLAGS='-Ofast')

if env['mode'] == 'debug':
    env.Append(CCFLAGS=['-O0'])

if env['mode'] in ('profile', 'debug'):
    env.Append(CCFLAGS='-g')

env.Append(CPPPATH=['src'])
env.Append(CCFLAGS=['-include', '$config_file'])

# Get all the c and c++ files in src, recursively.
sources = []
for root, dirnames, filenames in os.walk('src'):
    for filename in filenames:
        if filename.endswith('.c') or filename.endswith('.cpp'):
            sources.append(os.path.join(root, filename))

# Check for libpng.
if conf.CheckLibWithHeader('libpng', 'png.h', 'c'):
    env.Append(CPPDEFINES='HAVE_LIBPNG=1')

# Linux compilation support.
if target_os == 'posix':
    env.Append(LIBS=['GL', 'm'])
    # Note: add '--static' to link with all the libs needed by glfw3.
    env.ParseConfig('pkg-config --libs glfw3')
    env.ParseConfig('pkg-config --cflags --libs gtk+-3.0')

# Windows compilation support.
if target_os == 'msys':
    env.Append(CXXFLAGS=['-Wno-attributes', '-Wno-unused-variable',
                         '-Wno-unused-function'])
    env.Append(LIBS=['glfw3', 'opengl32', 'Imm32', 'gdi32', 'Comdlg32',
                     'z', 'tre', 'intl', 'iconv'],
               LINKFLAGS='--static')
    sources += glob.glob('ext_src/glew/glew.c')
    env.Append(CPPPATH=['ext_src/glew'])
    env.Append(CPPDEFINES=['GLEW_STATIC', 'FREE_WINDOWS'])

# OSX Compilation support.
if target_os == 'darwin':
    sources += glob.glob('src/*.m')
    env.Append(FRAMEWORKS=['OpenGL', 'Cocoa'])
    env.Append(LIBS=['m', 'glfw', 'objc'])

# Add external libs.
env.Append(CPPPATH=['ext_src/uthash'])
env.Append(CPPPATH=['ext_src/stb'])
env.Append(CPPPATH=['ext_src/noc'])
env.Append(CPPPATH=['ext_src/xxhash'])

if env['sound']:
    env.Append(LIBS='openal')
    env.Append(CPPDEFINES='SOUND=1')

if not env['yocto']:
    env.Append(CPPDEFINES='YOCTO=0')

# Append external environment flags
env.Append(
    CFLAGS=os.environ.get("CFLAGS", "").split(),
    CXXFLAGS=os.environ.get("CXXFLAGS", "").split(),
    LINKFLAGS=os.environ.get("LDFLAGS", "").split()
)

env.Program(target='goxel2', source=sorted(sources))
