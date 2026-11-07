#!/usr/bin/env python3
"""Builds every program in programs/ with the Barnix BDK and writes the
repository that brm reads: packages/NAME.elf, the system packages of Barnix
(packages/system/, made by ~/Barnix/scripts/system_packages.py: every
command, Happy Gouse, GCC) and two indexes.

programs/NAME/src/main.c       the source (BARNIX_APP_NAME "NAME.elf")
programs/NAME/package.info     version=, description=, description.ru=, category=

index  (brm 1): NAME|VERSION|packages/NAME.elf|SIZE|SHA256|DESCRIPTION|DESCRIPTION_RU
index2 (brm 2): the same, then CATEGORY|FLAGS|INSTALLED_KIB|FILES; system packages too

GCC comes from ~/Barnix/live-ext2-gcc.img (BARNIX_GCC_IMAGE=... for another);
without it the GCC package published last time stays.
"""
import hashlib
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
SDK = Path(os.environ.get('BDK', Path.home() / 'Barnix/BDK/dist/sdk'))
BARNIX = Path(os.environ.get('BARNIX', Path.home() / 'Barnix'))
GCC_IMAGE = Path(os.environ.get('BARNIX_GCC_IMAGE', BARNIX / 'live-ext2-gcc.img'))
LIMIT = 640 * 1024


def info(path):
    fields = {}
    for line in path.read_text(encoding='utf-8').splitlines():
        if '=' in line and not line.lstrip().startswith('#'):
            key, value = line.split('=', 1)
            fields[key.strip()] = value.strip()
    for key in ('version', 'description', 'description.ru'):
        if not fields.get(key) or '|' in fields[key]:
            raise SystemExit(f'{path}: {key}= is missing or contains |')
    return fields


def main():
    if not (SDK / 'tools/build.py').exists():
        raise SystemExit(f'Barnix BDK not found in {SDK}; run "make bdk" in Barnix or set BDK=/path/to/sdk')
    only = set(sys.argv[1:])
    packages = ROOT / 'packages'
    packages.mkdir(exist_ok=True)
    lines = ['# Barnix repository v1: name|version|file|size|sha256|description|description.ru']
    lines2 = ['# Barnix repository v2: name|version|file|size|sha256|description|description.ru|category|flags|installed KiB|files']
    for project in sorted(p for p in (ROOT / 'programs').iterdir() if (p / 'package.info').exists()):
        name = project.name
        if not re.fullmatch(r'[a-z0-9][a-z0-9_-]{0,17}', name):
            raise SystemExit(f'{name}: package names are a-z, 0-9, _ and -, at most 18 characters')
        fields = info(project / 'package.info')
        target = packages / f'{name}.elf'
        if not only or name in only:
            subprocess.run([sys.executable, str(SDK / 'tools/build.py'), str(project)], check=True)
            shutil.copyfile(project / 'elf' / f'{name}.elf', target)
        data = target.read_bytes()
        if len(data) > LIMIT:
            raise SystemExit(f'{name}: {len(data)} bytes, over the 640 KiB Barnix file limit')
        digest = hashlib.sha256(data).hexdigest()
        line = '|'.join([name, fields['version'], f'packages/{name}.elf', str(len(data)), digest,
                         fields['description'], fields['description.ru']])
        lines.append(line)
        lines2.append('|'.join([line, fields.get('category', 'apps'), '', str((len(data) + 1023) // 1024), '1']))
        print(f'{name:12} {fields["version"]:6} {len(data):7} bytes')
    (ROOT / 'index').write_text('\n'.join(lines) + '\n', encoding='utf-8')
    print(f'index: {len(lines) - 1} packages')
    # Barnix itself: every system program, Happy Gouse and GCC
    if (BARNIX / 'scripts/system_packages.py').exists() and (BARNIX / 'apps/bin/brm').exists():
        sys.path.insert(0, str(BARNIX / 'scripts'))
        from system_packages import build_repository
        print('System packages from', BARNIX)
        lines2 += build_repository(ROOT, GCC_IMAGE if GCC_IMAGE.exists() else None)
    else:
        print(f'No built Barnix in {BARNIX}: system packages are not updated (set BARNIX=...)')
        old = ROOT / 'index2'
        if old.exists():
            lines2 += [l for l in old.read_text(encoding='utf-8').splitlines() if '|packages/system/' in l]
    (ROOT / 'index2').write_text('\n'.join(lines2) + '\n', encoding='utf-8')
    print(f'index2: {len(lines2) - 1} packages')


if __name__ == '__main__':
    main()
