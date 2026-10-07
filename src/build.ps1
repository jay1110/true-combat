param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Release',
    [switch]$SkipTests
)
$ErrorActionPreference = 'Stop'
$buildDirectory = Join-Path (Split-Path $PSScriptRoot -Parent) 'build/sdk-win32'
$localTestInputs = (Test-Path -LiteralPath (Join-Path $PSScriptRoot 'tests/tce_shared_test.c')) -and
    (Test-Path -LiteralPath (Join-Path (Split-Path $PSScriptRoot -Parent) 'tcetest/cgame_mp_x86.dll')) -and
    (Test-Path -LiteralPath (Join-Path (Split-Path $PSScriptRoot -Parent) 'tcetest/qagame_mp_x86.dll')) -and
    (Test-Path -LiteralPath (Join-Path (Split-Path $PSScriptRoot -Parent) 'tcetest/ui_mp_x86.dll'))
if (-not $localTestInputs) {
    $SkipTests = $true
    Write-Host 'Local comparison tests/reference DLLs absent: building production modules only.'
}
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
$clientArchiveName = 'zz_tce2_vm.pk3'
foreach ($module in $modules) {
    if (-not (Test-Path -LiteralPath (Join-Path "$buildDirectory/bin/$Configuration" $module))) {
        throw "Build output missing: $module"
    }
}
New-Item -ItemType Directory -Force -Path $testDirectory | Out-Null
# Detect a running game/server before replacing the first module. Holding all
# handles until the check finishes also catches files locked later in the list.
$destinationHandles = @()
try {
    foreach ($module in ($modules + $clientArchiveName)) {
        $destinationFile = Join-Path $testDirectory $module
        if (Test-Path -LiteralPath $destinationFile) {
            $destinationHandles += [IO.File]::Open($destinationFile, [IO.FileMode]::Open,
                [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
        }
    }
} catch {
    throw "Build/tests passed, but deployment is blocked by a locked DLL. Close the tce2 game/server and rerun src/build.ps1. $($_.Exception.Message)"
} finally {
    foreach ($destinationHandle in $destinationHandles) { $destinationHandle.Dispose() }
}
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
# ET pure clients extract cgame/ui from a referenced PK3. Loose DLLs alone
# allow etmain/mp_bin.pk3 to overwrite the reconstructed client modules.
Add-Type -AssemblyName System.IO.Compression.FileSystem
Add-Type -AssemblyName System.IO.Compression
$archiveStaging = Join-Path $buildDirectory $clientArchiveName
if (Test-Path -LiteralPath $archiveStaging) { Remove-Item -LiteralPath $archiveStaging }
$archive = [IO.Compression.ZipFile]::Open($archiveStaging, [IO.Compression.ZipArchiveMode]::Create)
try {
    foreach ($module in @('cgame_mp_x86.dll', 'ui_mp_x86.dll')) {
        [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive,
            (Join-Path "$buildDirectory/bin/$Configuration" $module), $module,
            [IO.Compression.CompressionLevel]::Optimal) | Out-Null
    }
} finally { $archive.Dispose() }
$archiveDestination = Join-Path $testDirectory $clientArchiveName
Copy-Item -LiteralPath $archiveStaging -Destination $archiveDestination -Force
$archiveHash = (Get-FileHash -LiteralPath $archiveStaging -Algorithm SHA256).Hash
if ($archiveHash -ne (Get-FileHash -LiteralPath $archiveDestination -Algorithm SHA256).Hash) {
    throw 'Client PK3 deployment verification failed.'
}
[ordered]@{
    builtAtUtc = [DateTime]::UtcNow.ToString('o')
    configuration = $Configuration
    reconstructionComplete = $false
    testsRun = -not $SkipTests
    modules = $deployment
    clientArchive = [ordered]@{ file = $clientArchiveName; sha256 = $archiveHash }
} | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $testDirectory 'build-info.json') -Encoding utf8
Write-Host "All three DLLs copied and SHA-256 verified in: $testDirectory"
Write-Host "Pure-client module package: $archiveDestination"
Write-Host 'TC:E reconstruction is incomplete; see src/reconstruction/STATUS.md.'
