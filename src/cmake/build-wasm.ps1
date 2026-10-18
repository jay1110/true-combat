param(
    [string]$Emsdk = 'C:/emsdk',
    [string]$BuildDir = '',
    [int]$Jobs = 8
)
$ErrorActionPreference = 'Stop'
$sourceRoot = Split-Path $PSScriptRoot -Parent
if (-not $BuildDir) { $BuildDir = Join-Path (Split-Path $sourceRoot -Parent) 'build/wasm32' }
$env:EM_CONFIG = Join-Path $Emsdk '.emscripten'
$toolchain = Join-Path $Emsdk 'upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake'
& cmake -S $sourceRoot -B $BuildDir -G Ninja "-DCMAKE_TOOLCHAIN_FILE=$toolchain" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
if ($LASTEXITCODE -ne 0) { throw 'WASM configure failed' }
& cmake --build $BuildDir --target cgame ui qagame --parallel $Jobs
if ($LASTEXITCODE -ne 0) { throw 'WASM build failed' }
# ETLegacy ABI1 side modules; package only cgame/ui, keep qagame loose.
Get-ChildItem -LiteralPath (Join-Path $BuildDir 'bin') -Filter '*.wasm32.so'
