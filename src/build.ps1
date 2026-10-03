param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Release',
    [switch]$SkipTests
)
$ErrorActionPreference = 'Stop'
$buildDirectory = Join-Path (Split-Path $PSScriptRoot -Parent) 'build/sdk-win32'
$testOption = if ($SkipTests) { '-DBUILD_TESTING=OFF' } else { '-DBUILD_TESTING=ON' }
& cmake -S $PSScriptRoot -B $buildDirectory -A Win32 $testOption
if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
& cmake --build $buildDirectory --config $Configuration --parallel
if ($LASTEXITCODE -ne 0) { throw 'Compilation failed.' }
if (-not $SkipTests) {
    & ctest --test-dir $buildDirectory -C $Configuration --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'Original-DLL comparison failed.' }
}
Write-Host "SDK baseline with recovered components: $buildDirectory/bin/$Configuration"
$testDirectory = Join-Path (Split-Path $PSScriptRoot -Parent) 'tce2'
$modules = @('cgame_mp_x86.dll', 'qagame_mp_x86.dll', 'ui_mp_x86.dll')
foreach ($module in $modules) {
    if (-not (Test-Path -LiteralPath (Join-Path "$buildDirectory/bin/$Configuration" $module))) {
        throw "Build output missing: $module"
    }
}
New-Item -ItemType Directory -Force -Path $testDirectory | Out-Null
$deployment = @()
foreach ($module in $modules) {
    $sourceFile = Join-Path "$buildDirectory/bin/$Configuration" $module
    $destinationFile = Join-Path $testDirectory $module
    Copy-Item -LiteralPath $sourceFile -Destination $destinationFile -Force
    $sourceHash = (Get-FileHash -LiteralPath $sourceFile -Algorithm SHA256).Hash
    $destinationHash = (Get-FileHash -LiteralPath $destinationFile -Algorithm SHA256).Hash
    if ($sourceHash -ne $destinationHash) { throw "Deployment verification failed: $module" }
    $deployment += [ordered]@{ file = $module; sha256 = $sourceHash }
}
[ordered]@{
    builtAtUtc = [DateTime]::UtcNow.ToString('o')
    configuration = $Configuration
    reconstructionComplete = $false
    testsRun = -not $SkipTests
    modules = $deployment
} | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $testDirectory 'build-info.json') -Encoding utf8
Write-Host "All three DLLs copied and SHA-256 verified in: $testDirectory"
Write-Host 'TC:E reconstruction is incomplete; see src/reconstruction/STATUS.md.'
