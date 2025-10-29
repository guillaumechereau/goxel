# Goxel 3D voxels editor
#
# copyright (c) 2018 Guillaume Chereau <guillaume@noctua-software.com>
#
# Goxel is free software: you can redistribute it and/or modify it under the
# terms of the GNU General Public License as published by the Free Software
# Foundation, either version 3 of the License, or (at your option) any later
# version.
#
# Goxel is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
# details.
#
# You should have received a copy of the GNU General Public License along with
# goxel.  If not, see <http://www.gnu.org/licenses/>.

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

if target_os == 'posix':
    vars.AddVariables(
        EnumVariable('nfd_backend', 'Native file dialog backend', default='gtk',
                     allowed_values=('gtk', 'portal')),
    )


env = Environment(variables=vars, ENV=os.environ)
conf = env.Configure()

if env['mode'] == 'analyze':
    # Make sure clang static analyzer has a chance to override de compiler
    # and set CCC settings
    env["CC"] = os.getenv("CC") or env["CC"]
    env["CXX"] = os.getenv("CXX") or env["CXX"]
    env["ENV"].update(x for x in os.environ.items() if x[0].startswith("CCC_"))


if os.environ.get('CC') == 'clang':
    env.Replace(CC='clang', CXX='clang++')

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
    env.Append(CPPDEFINES='NDEBUG', CCFLAGS='-O3')

if env['mode'] == 'debug':
    env.Append(CCFLAGS=['-O0'])

if env['mode'] in ('profile', 'debug'):
    env.Append(CCFLAGS='-g')

env.Append(CPPPATH=['src', '.'])
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
    env.Append(LIBS=['GL', 'm', 'dl', 'pthread'])
    # Note: add '--static' to link with all the libs needed by glfw3.
    env.ParseConfig('pkg-config --libs glfw3')

    # Pick the proper native file dialog backend.
    if env['nfd_backend'] == 'portal':
        env.ParseConfig('pkg-config --cflags --libs dbus-1')
        sources.append('ext_src/nfd/nfd_portal.cpp')

    if env['nfd_backend'] == 'gtk':
        env.ParseConfig('pkg-config --cflags --libs gtk+-3.0')
        sources.append('ext_src/nfd/nfd_gtk.cpp')


# Windows compilation support.
if target_os == 'msys':
    env.Append(CXXFLAGS=['-Wno-attributes', '-Wno-unused-variable',
                         '-Wno-unused-function'])
    env.Append(CCFLAGS=['-Wno-error=address']) # To remove if possible.
    env.Append(LIBS=['glfw3', 'opengl32', 'z', 'tre', 'gdi32', 'Comdlg32',
                     'ole32', 'uuid', 'shell32'],
               LINKFLAGS='--static')
    sources += glob.glob('ext_src/glew/glew.c')
    sources.append('ext_src/nfd/nfd_win.cpp')
    env.Append(CPPPATH=['ext_src/glew'])
    env.Append(CPPDEFINES=['GLEW_STATIC', 'FREE_WINDOWS'])

# OSX Compilation support.
if target_os == 'darwin':
    sources += glob.glob('src/*.m')
    sources.append('ext_src/nfd/nfd_cocoa.m')
    env.Append(FRAMEWORKS=[
        'OpenGL', 'Cocoa', 'AppKit', 'UniformTypeIdentifiers'])
    env.Append(LIBS=['m', 'objc', 'z'])
    # Fix warning in noc_file_dialog (the code should be fixed instead).
    env.Append(CCFLAGS=['-Wno-deprecated-declarations'])
    env.ParseConfig('pkg-config --cflags --libs glfw3')
    env['sound'] = False


# Add external libs.
env.Append(CPPPATH=['ext_src'])
env.Append(CPPPATH=['ext_src/uthash'])
env.Append(CPPPATH=['ext_src/stb'])
env.Append(CPPPATH=['ext_src/nfd'])
env.Append(CPPPATH=['ext_src/noc'])
env.Append(CPPPATH=['ext_src/xxhash'])
env.Append(CPPPATH=['ext_src/meshoptimizer'])

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

# Build compile_commands.json.
try:
    env.Tool('compilation_db')
    env.CompilationDatabase()
except:
    pass

env.Program(target='goxel', source=sorted(sources))
