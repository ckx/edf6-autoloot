# Steam discovery adapted from EDF6Coop; see EDF6Coop_LICENSE.txt.
param([switch]$Uninstall, [string]$GameDir = '')
$ErrorActionPreference = 'Stop'
function Find-Game {
    foreach ($d in @($PSScriptRoot, (Split-Path -Parent $PSScriptRoot))) {
        if (Test-Path -LiteralPath (Join-Path $d 'EDF6.exe')) { return $d }
    }
    foreach ($key in @('HKCU:\Software\Valve\Steam', 'HKLM:\SOFTWARE\WOW6432Node\Valve\Steam', 'HKLM:\SOFTWARE\Valve\Steam')) {
        $entry = Get-ItemProperty -LiteralPath $key -ErrorAction SilentlyContinue
        foreach ($root in @($entry.SteamPath, $entry.InstallPath)) {
            if (-not $root) { continue }
            $libs = @($root)
            $vdf = Join-Path $root 'steamapps/libraryfolders.vdf'
            if (Test-Path -LiteralPath $vdf) {
                foreach ($m in [regex]::Matches((Get-Content -LiteralPath $vdf -Raw -Encoding UTF8), '"path"\s+"([^"]+)"')) {
                    $libs += $m.Groups[1].Value.Replace('\\', '\')
                }
            }
            foreach ($lib in $libs) {
                $d = Join-Path $lib 'steamapps/common/EARTH DEFENSE FORCE 6'
                if (Test-Path -LiteralPath (Join-Path $d 'EDF6.exe')) { return $d }
            }
        }
    }
}
try {
    if (-not $GameDir) { $GameDir = Find-Game }
    if (-not $GameDir) { $GameDir = (Read-Host 'Paste the EDF6 folder from Steam > Manage > Browse local files').Trim('"', ' ') }
    if (-not $GameDir -or -not (Test-Path -LiteralPath (Join-Path $GameDir 'EDF6.exe'))) { throw 'No EDF6.exe in that folder.' }
    if (Get-Process -Name EDF6, EDF6.exe -ErrorAction SilentlyContinue) { throw 'Quit EDF6 first.' }
    $target = Join-Path $GameDir 'Mods/Plugins/EDF6AutoLoot.dll'
    function Backup-Existing {
        $backup = Join-Path $GameDir ('AutoLoot-backup-' + [guid]::NewGuid().ToString('N'))
        [IO.Directory]::CreateDirectory($backup) | Out-Null
        [IO.File]::Copy($target, (Join-Path $backup 'EDF6AutoLoot.dll'), $false)
        Write-Host "Saved previous DLL: $backup/EDF6AutoLoot.dll"
    }
    if ($Uninstall) {
        if (Test-Path -LiteralPath $target) { Backup-Existing; [IO.File]::Delete($target) }
        Write-Host 'AutoLoot removed. Backups, logs, loader and other mods remain.'
        exit 0
    }
    if (-not (Test-Path -LiteralPath (Join-Path $GameDir 'winmm.dll'))) { throw 'Install EDFModLoader first. AutoLoot does not include a loader.' }
    if (Test-Path -LiteralPath (Join-Path $GameDir 'Lootmaster Readme.txt')) { throw 'Remove the old Lootmaster installation using its saved manifest before installing AutoLoot.' }
    $source = Join-Path $PSScriptRoot 'EDF6AutoLoot.dll'
    if (-not (Test-Path -LiteralPath $source)) { throw 'Extract the whole release ZIP first.' }
    [IO.Directory]::CreateDirectory((Split-Path -Parent $target)) | Out-Null
    if (Test-Path -LiteralPath $target) {
        if ((Get-FileHash -LiteralPath $source).Hash -eq (Get-FileHash -LiteralPath $target).Hash) {
            Write-Host 'This AutoLoot DLL is already installed.'
            exit 0
        }
        Backup-Existing
    }
    $stage = Join-Path (Split-Path -Parent $target) ('.autoloot-' + [guid]::NewGuid().ToString('N'))
    try {
        [IO.File]::Copy($source, $stage, $false)
        if (Test-Path -LiteralPath $target) { [IO.File]::Replace($stage, $target, [NullString]::Value) }
        else { [IO.File]::Move($stage, $target) }
    } finally { if (Test-Path -LiteralPath $stage) { [IO.File]::Delete($stage) } }
    Write-Host 'Installed AutoLoot (offline prototype). Back up saves before testing.'
} catch {
    Write-Error $_ -ErrorAction Continue
    exit 1
}
