"""Run real installers in disposable game folders; never touch an installed game."""
import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class InstallerTests(unittest.TestCase):
    def exercise(self, shell):
        with tempfile.TemporaryDirectory(prefix='autoloot [test] ') as temp:
            root = Path(temp)
            package = root / 'package'
            shutil.copytree(ROOT / 'packaging', package)
            (package / 'EDF6AutoLoot.dll').write_bytes(b'new plugin')
            game = root / 'game [1]'
            game.mkdir()
            (game / 'EDF6.exe').touch()
            plugins = game / 'Mods/Plugins'
            plugins.mkdir(parents=True)
            target = plugins / 'EDF6AutoLoot.dll'
            other = plugins / 'OtherMod.dll'
            other.write_bytes(b'other mod')
            def run(remove=False, success=True):
                if shell == 'bash':
                    cmd = ['bash', str(package / 'install.sh')]
                    if remove:
                        cmd += ['--uninstall']
                    cmd += [str(game)]
                else:
                    cmd = [shell, '-NoProfile', '-File', str(package / 'install.ps1'), '-GameDir', str(game)]
                    if remove:
                        cmd += ['-Uninstall']
                result = subprocess.run(cmd, capture_output=True, text=True)
                self.assertEqual(result.returncode == 0, success, result.stdout + result.stderr)
                self.assertEqual(other.read_bytes(), b'other mod')
            run(success=False)
            self.assertFalse(target.exists())
            (game / 'winmm.dll').write_bytes(b'existing loader')
            (game / 'ModLoader.ini').write_bytes(b'existing settings')
            lootmaster = game / 'Lootmaster Readme.txt'
            lootmaster.touch()
            run(success=False)
            self.assertFalse(target.exists())
            lootmaster.unlink()
            run()
            self.assertEqual(target.read_bytes(), b'new plugin')
            run()
            self.assertEqual(list(game.glob('AutoLoot-backup-*')), [])
            target.write_bytes(b'previous plugin')
            run()
            self.assertEqual(target.read_bytes(), b'new plugin')
            self.assertEqual([p.read_bytes() for p in game.glob('AutoLoot-backup-*/*.dll')], [b'previous plugin'])
            run(remove=True)
            self.assertFalse(target.exists())
            self.assertEqual(sorted(p.read_bytes() for p in game.glob('AutoLoot-backup-*/*.dll')), [b'new plugin', b'previous plugin'])
            run(remove=True)
            self.assertEqual((game / 'winmm.dll').read_bytes(), b'existing loader')
            self.assertEqual((game / 'ModLoader.ini').read_bytes(), b'existing settings')

    @unittest.skipUnless(os.name != 'nt' and shutil.which('bash'), 'Linux bash test')
    def test_bash(self):
        self.exercise('bash')

    @unittest.skipUnless(shutil.which('pwsh'), 'PowerShell unavailable')
    def test_powershell(self):
        self.exercise('pwsh')

    @unittest.skipUnless(os.name == 'nt', 'Windows PowerShell test')
    def test_windows_powershell(self):
        self.exercise('powershell')


if __name__ == '__main__':
    unittest.main()
