param([switch]$Test, [switch]$RequireGame)
$ErrorActionPreference = 'Stop'
$build = Join-Path $PSScriptRoot 'build-msvc'
$require = if ($RequireGame) { 'ON' } else { 'OFF' }
cmake -S $PSScriptRoot -B $build -G 'Visual Studio 17 2022' -A x64 "-DEDF6_REQUIRE_GAME=$require" "-DEDF6_GAME_DIR=$env:EDF6_GAME_DIR"
if ($LASTEXITCODE) { throw 'CMake configure failed' }
cmake --build $build --config Release
if ($LASTEXITCODE) { throw 'Build failed' }
if ($Test) {
    ctest --test-dir $build -C Release --output-on-failure --no-tests=error --timeout 300
    if ($LASTEXITCODE) { throw 'Tests failed' }
}
