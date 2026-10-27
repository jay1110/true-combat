param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Release',
    [switch]$SkipTests,
    [switch]$WithOmnibot,
    [switch]$ReferenceTests,
    [string]$GeoIPDatabase
)
$ErrorActionPreference = 'Stop'
if ($GeoIPDatabase -and (-not (Test-Path -LiteralPath $GeoIPDatabase -PathType Leaf) -or
    (Get-Item -LiteralPath $GeoIPDatabase).Length -lt 6 -or (Get-Item -LiteralPath $GeoIPDatabase).Length -gt 16777216)) {
    throw 'GeoIPDatabase must be an existing legacy IPv4 GeoIP.dat between 6 bytes and 16 MiB.'
}
$buildDirectory = Join-Path (Split-Path $PSScriptRoot -Parent) 'build/sdk-win32'
$localTestInputs = (Test-Path -LiteralPath (Join-Path $PSScriptRoot 'tests/tce_shared_test.c')) -and
    (Test-Path -LiteralPath (Join-Path (Split-Path $PSScriptRoot -Parent) 'tcetest/cgame_mp_x86.dll')) -and
    (Test-Path -LiteralPath (Join-Path (Split-Path $PSScriptRoot -Parent) 'tcetest/qagame_mp_x86.dll')) -and
    (Test-Path -LiteralPath (Join-Path (Split-Path $PSScriptRoot -Parent) 'tcetest/ui_mp_x86.dll'))
if ($ReferenceTests -and ($SkipTests -or -not $localTestInputs)) {
    throw 'ReferenceTests requires tests enabled and the local original-DLL comparison inputs.'
}
$testOption = if ($SkipTests) { '-DBUILD_TESTING=OFF' } else { '-DBUILD_TESTING=ON' }
$referenceOption = if ($ReferenceTests) { '-DTCE_BUILD_REFERENCE_TESTS=ON' } else { '-DTCE_BUILD_REFERENCE_TESTS=OFF' }
$botOption = if ($WithOmnibot) { '-DFEATURE_OMNIBOT=ON' } else { '-DFEATURE_OMNIBOT=OFF' }
& cmake -S $PSScriptRoot -B $buildDirectory -A Win32 $testOption $referenceOption -DFEATURE_NAVSTUDIO=OFF $botOption
if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
& cmake --build $buildDirectory --config $Configuration --parallel
if ($LASTEXITCODE -ne 0) { throw 'Compilation failed.' }
if (-not $SkipTests) {
    & ctest --test-dir $buildDirectory -C $Configuration --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'Mod validation failed; see the CTest output.' }
}
Write-Host "SDK baseline with recovered components: $buildDirectory/bin/$Configuration"
$testDirectory = Join-Path (Split-Path $PSScriptRoot -Parent) 'tce2'
$modules = @('cgame_mp_x86.dll', 'qagame_mp_x86.dll', 'ui_mp_x86.dll')
$versionHeader = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'game/tce_version.h') -Raw
if ($versionHeader -notmatch '(?m)^#define TCE_VERSION "([0-9]+\.[0-9]+\.[0-9]+)"') {
    throw 'Invalid TCE_VERSION'
}
$modVersion = $Matches[1]
$clientArchiveName = "tce2-$modVersion.pk3"
foreach ($module in $modules) {
    if (-not (Test-Path -LiteralPath (Join-Path "$buildDirectory/bin/$Configuration" $module))) {
        throw "Build output missing: $module"
    }
}
# Check the complete mod search path before replacing even the first loose
# DLL. Another client-DLL PK3 can override the canonical versioned package.
# Asset/map packages are never silently moved or rewritten by this helper.
Add-Type -AssemblyName System.IO.Compression.FileSystem
if (Test-Path -LiteralPath (Join-Path $testDirectory 'zz_tce2_navstudio_editor.pk3')) {
    throw 'The editor UI addon is present in the public mod directory. Move it outside the mod search path before deployment; no DLL has been replaced.'
}
foreach ($candidate in @(Get-ChildItem -LiteralPath $testDirectory -Filter '*.pk3' -File -ErrorAction SilentlyContinue)) {
    if ($candidate.Name -eq $clientArchiveName) { continue }
    $candidateArchive = [IO.Compression.ZipFile]::OpenRead($candidate.FullName)
    try {
        $clientModules = @($candidateArchive.Entries | Where-Object {
            $_.FullName -match '(^|/)(cgame|ui)(_mp_(x86|x64)\.dll|\.mp\.[^/]+\.so)$'
        })
        if ($clientModules.Count -gt 0) {
            throw ("Conflicting client-DLL PK3: " + $candidate.FullName +
                ". Move this obsolete module override outside the mod search path before deployment; no DLL has been replaced.")
        }
    } finally { $candidateArchive.Dispose() }
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
    $versionEntry = $archive.CreateEntry("tce2/$modVersion/client.manifest")
    $versionStream = [IO.StreamWriter]::new($versionEntry.Open(), [Text.UTF8Encoding]::new($false))
    try { $versionStream.Write($modVersion) } finally { $versionStream.Dispose() }
    foreach ($asset in @('ui/options_customise_hud.menu', 'ui/options_controls.menu', 'ui/ingame_main.menu', 'ui/menus.txt', 'ui/menus_gt2.txt',
                         'ui/menus_gt5.txt', 'ui/menus_gt7.txt',
                         'gfx/misc/radar_arrow.tga', 'gfx/misc/radar_dot.tga',
                         'gfx/flags/world_flags.tga', 'gfx/flags/LICENSE.xmod.txt', 'gfx/flags/SOURCE.json',
                         'levelshots/obj_delta_cc.tga', 'levelshots/obj_delta_cc_blueprint.tga',
                         'levelshots/SOURCE.json')) {
        [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive,
            (Join-Path $PSScriptRoot ('assets/' + $asset)), $asset,
            [IO.Compression.CompressionLevel]::Optimal) | Out-Null
    }
} finally { $archive.Dispose() }
$archiveDestination = Join-Path $testDirectory $clientArchiveName
Copy-Item -LiteralPath $archiveStaging -Destination $archiveDestination -Force
if ($GeoIPDatabase) {
    Copy-Item -LiteralPath $GeoIPDatabase -Destination (Join-Path $testDirectory 'GeoIP.dat') -Force
}
$archiveHash = (Get-FileHash -LiteralPath $archiveStaging -Algorithm SHA256).Hash
if ($archiveHash -ne (Get-FileHash -LiteralPath $archiveDestination -Algorithm SHA256).Hash) {
    throw 'Client PK3 deployment verification failed.'
}
[ordered]@{
    builtAtUtc = [DateTime]::UtcNow.ToString('o')
    configuration = $Configuration
    reconstructionComplete = $false
    testsRun = -not $SkipTests
    referenceTestsRun = [bool]$ReferenceTests
    modules = $deployment
    clientArchive = [ordered]@{ file = $clientArchiveName; sha256 = $archiveHash }
} | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $testDirectory 'build-info.json') -Encoding utf8
Write-Host "All three DLLs copied and SHA-256 verified in: $testDirectory"
Write-Host "Pure-client module package: $archiveDestination"
Write-Host 'TC:E reconstruction is incomplete; see src/reconstruction/STATUS.md.'
