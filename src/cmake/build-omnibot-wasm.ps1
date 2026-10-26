param(
    [string]$Emsdk = 'C:/emsdk',
    [string]$BoostIncludeDir = 'C:/boost',
    [string]$BuildDir = '',
    [int]$Jobs = 6
)
$ErrorActionPreference = 'Stop'
$env:EM_CONFIG = Join-Path $Emsdk '.emscripten'
$sourceRoot = Split-Path $PSScriptRoot -Parent
$repoRoot = Split-Path $sourceRoot -Parent
if (-not $BuildDir) { $BuildDir = Join-Path $repoRoot 'build/omnibot-wasm32' }
$toolchain = Join-Path $Emsdk 'upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake'
if (-not (Test-Path -LiteralPath $toolchain)) { throw "Missing Emscripten toolchain: $toolchain" }
if (-not (Test-Path -LiteralPath (Join-Path $BoostIncludeDir 'boost/version.hpp'))) {
    throw 'BoostIncludeDir must contain boost/version.hpp (headers only are required).'
}
& cmake -S (Join-Path $repoRoot 'vendor/omni-bot/source/Omnibot') -B $BuildDir -G Ninja `
    "-DCMAKE_TOOLCHAIN_FILE=$toolchain" -DCMAKE_BUILD_TYPE=Release `
    -DOMNIBOT_ET=ON -DOMNIBOT_RTCW=OFF -DOMNIBOT_STD_FILESYSTEM=ON -DFEATURE_NAVSTUDIO=OFF `
    "-DOMNIBOT_BOOST_INCLUDEDIR=$BoostIncludeDir"
if ($LASTEXITCODE -ne 0) { throw "Omni-bot WASM configure failed: $LASTEXITCODE" }
& cmake --build $BuildDir --parallel $Jobs
if ($LASTEXITCODE -ne 0) { throw "Omni-bot WASM build failed: $LASTEXITCODE" }
$module = Join-Path $BuildDir 'ET/omnibot_et.wasm32.so'
$bytes = [IO.File]::ReadAllBytes($module)
if ($bytes.Length -lt 8 -or [BitConverter]::ToString($bytes[0..7]) -ne '00-61-73-6D-01-00-00-00') {
    throw "Not a WebAssembly module: $module"
}
Write-Output "Built WebAssembly side module: $module"
# ETLegacy's browser loader must preload this side module before qagame dlopen.
# Its global_scripts/ and et/ data directories must reside beside the module.
# This helper intentionally does not deploy or change any reference mod.
