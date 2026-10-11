#!/usr/bin/env python3
"""Build, test and package the plugin. No files are installed into the game."""
import hashlib
import os
import re
import subprocess
import zipfile
from pathlib import Path

root = Path(__file__).resolve().parents[1]
version = re.search(r'project\(EDF6AutoLoot VERSION (\d+\.\d+\.\d+)', (root / 'CMakeLists.txt').read_text())[1]
subprocess.run(['powershell', '-NoProfile', '-File', str(root / 'build.ps1'), '-Test'] if os.name == 'nt'
               else ['bash', str(root / 'build.sh'), '-t'], cwd=root, check=True)
release = root / 'release'
release.mkdir(exist_ok=True)
archive = release / f'EDF6AutoLoot-{version}.zip'
files = {
    'EDF6AutoLoot.dll': root / 'out/EDF6AutoLoot.dll',
    'README.md': root / 'README.md',
    'RELEASE_NOTES.md': root / f'release-notes/{version}.md',
    'EDFModLoader_LICENSE.txt': root / 'third_party/EDFModLoader/LICENSE.txt',
    'EDF6Coop_LICENSE.txt': root / 'third_party/EDF6Coop/LICENSE.txt',
}
for name in ['INSTALL.bat', 'UNINSTALL.bat', 'install.ps1', 'install.sh']:
    files[name] = root / 'packaging' / name
for path in files.values():
    if not path.is_file():
        raise FileNotFoundError(path)
with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as output:
    for name, path in files.items():
        output.write(path, name)
digest = hashlib.sha256(archive.read_bytes()).hexdigest()
archive.with_suffix('.zip.sha256').write_text(f'{digest}  {archive.name}\n')
print(archive)
print(digest)
