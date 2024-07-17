#!/usr/bin/env python3

# Stellarium Web Engine - Copyright (c) 2024-present - Stellarium Labs SRL
# All rights reserved.

# Generate the mo files from the source po files.

import json
import contextlib
import glob
import subprocess
import os

def ensure_dir(file_path):
    '''Create a directory for a path if it doesn't exists yet'''
    directory = os.path.dirname(file_path)
    if not os.path.exists(directory):
        os.makedirs(directory)

def write_pot(values, path):
    '''Write a pot file from an array of strings'''
    file = open(path, 'w')
    file.write('msgid ""\n')
    file.write('msgstr ""\n')
    file.write('"Content-Type: text/plain; charset=UTF-8\\n"\n')
    file.write('"Content-Transfer-Encoding: 8bit\\n"\n')
    file.write('"Project-Id-Version: \\n"\n\n')
    for value in values:
        value = value.replace('"', '\\"')
        file.write(f'msgid "{value}"\n')
        file.write(f'msgstr ""\n\n')


def update_po_files():
    sources = glob.glob('src/**/*.c', recursive=True);
    subprocess.check_call([
        'xgettext', '-k_', '-kN_', '-c', '--from-code=utf-8',
        '-o', '/tmp/goxel.pot', *sources])
    for po_file in glob.glob('po/*.po'):
        subprocess.check_call([
            'msgmerge', '--update', po_file, '/tmp/goxel.pot'])


def generate_mo_files():
    mo_dir = 'data/locale'
    for po_file in glob.glob('po/*.po'):
        filename = os.path.basename(po_file).rsplit('.', 1)[0]
        mo_file = os.path.join(mo_dir, f'{filename}.mo')
        ensure_dir(mo_file)
        subprocess.check_call(['msgfmt', po_file, '-o', mo_file, '-c'])


def run():
    update_po_files()
    generate_mo_files()

if __name__ == '__main__':
    with contextlib.chdir(os.path.join(os.path.dirname(__file__), '../')):
        run()
