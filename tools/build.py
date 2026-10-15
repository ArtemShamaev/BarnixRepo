#!/usr/bin/env python3
"""Builds every program in programs/ with the Barnix BDK and writes the
repository that brm reads: packages/NAME.elf and the index.

programs/NAME/src/main.c       the source (BARNIX_APP_NAME "NAME.elf")
programs/NAME/package.info     version=, description=, description.ru=

index lines: NAME|VERSION|packages/NAME.elf|SIZE|SHA256|DESCRIPTION|DESCRIPTION_RU
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
        lines.append('|'.join([name, fields['version'], f'packages/{name}.elf', str(len(data)), digest,
                               fields['description'], fields['description.ru']]))
        print(f'{name:12} {fields["version"]:6} {len(data):7} bytes')
    (ROOT / 'index').write_text('\n'.join(lines) + '\n', encoding='utf-8')
    print(f'index: {len(lines) - 1} packages')


if __name__ == '__main__':
    main()
