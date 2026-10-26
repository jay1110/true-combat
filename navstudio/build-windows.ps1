param(
    [string]$Python = 'python',
    [string]$DesktopDependencies = 'build/navstudio-desktop-deps',
    [string]$RecastBuild = 'build/navgen-overkill/navmesh-build',
    [string]$RecastExecutable = '',
    [string]$RecastLicense = '',
    [ValidatePattern('^[A-Za-z0-9_-]+$')][string]$PackageName = 'TCE2-NavStudio',
    [string]$Destination = 'build/navgen-overkill/navstudio-package-ready'
)
$ErrorActionPreference = 'Stop'
$navWorkspace = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$navPreviousPythonPath = $env:PYTHONPATH
Push-Location $navWorkspace
try {
    $navDependencies = [IO.Path]::GetFullPath($DesktopDependencies)
    if (Test-Path -LiteralPath $navDependencies -PathType Container) {
        $env:PYTHONPATH = $navDependencies + $(if ($navPreviousPythonPath) { ';' + $navPreviousPythonPath } else { '' })
    }
    & $Python (Join-Path $PSScriptRoot 'build_guide.py')
    if ($LASTEXITCODE -ne 0) { throw 'NavStudio guide generation failed.' }
    & $Python (Join-Path $PSScriptRoot 'build_botmenu.py')
    if ($LASTEXITCODE -ne 0) { throw 'Local Omni-bot voice menu generation failed.' }
    $navDestinationRoot = [IO.Path]::GetFullPath($Destination)
    $navDestination = Join-Path $navDestinationRoot $PackageName
    $navPackageExe = Join-Path $navDestination ($PackageName + '.exe')
    if (@(Get-Process -Name $PackageName -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $navPackageExe }).Count -gt 0) {
        throw 'The destination package is running. Choose a separate -Destination; no active package will be replaced.'
    }
    $navRecast = if ($RecastExecutable) { [IO.Path]::GetFullPath($RecastExecutable) } else { [IO.Path]::GetFullPath((Join-Path $RecastBuild 'Release/tce_navmesh_preview.exe')) }
    $navLicense = if ($RecastLicense) { [IO.Path]::GetFullPath($RecastLicense) } else { [IO.Path]::GetFullPath((Join-Path $RecastBuild '_deps/recast-src/License.txt')) }
    if (!(Test-Path -LiteralPath $navRecast) -or !(Test-Path -LiteralPath $navLicense)) {
        throw 'Build navstudio/navmesh with CMake first; Recast executable and License.txt are required.'
    }
    $navResources = Join-Path $navDestinationRoot ('resources-' + [Guid]::NewGuid().ToString('N'))
    & $Python (Join-Path $PSScriptRoot 'package_preflight.py') stage --workspace $navWorkspace --package $navResources --name $PackageName
    if ($LASTEXITCODE -ne 0) { throw 'NavStudio source staging failed.' }
    $navArguments = @('-m', 'PyInstaller', '--noconfirm', '--clean', '--onedir',
        '--console', '--hide-console', 'hide-early', '--name', $PackageName,
        '--distpath', $navDestinationRoot, '--workpath', (Join-Path $navDestinationRoot '_build'),
        '--specpath', (Join-Path $navDestinationRoot '_spec'),
        '--paths', 'navstudio/workers', '--paths', 'navstudio/navmesh', '--paths', 'navstudio',
        '--hidden-import', 'layout', '--hidden-import', 'completion', '--hidden-import', 'desktop', '--collect-all', 'webview', '--collect-all', 'pythonnet', '--collect-all', 'clr_loader',
        '--hidden-import', 'server', '--hidden-import', 'edit', '--hidden-import', 'watch', '--hidden-import', 'settings', '--hidden-import', 'tce_bsp_navmesh',
        '--hidden-import', 'projects', '--hidden-import', 'spawns', '--hidden-import', 'goalgen', '--hidden-import', 'mapscript', '--hidden-import', 'navbundle', '--hidden-import', 'botmenu',
        '--hidden-import', 'tce_bsp_patch', '--hidden-import', 'tce_navgen',
        '--hidden-import', 'waygraph', '--hidden-import', 'report', '--hidden-import', 'repair', '--hidden-import', 'doorrepair', '--hidden-import', 'pathrepair', '--hidden-import', 'objectiveanchors', '--hidden-import', 'quiet', '--hidden-import', 'etsnapshot',
        '--hidden-import', 'objective_contact', '--hidden-import', 'navmesh.objective_contact',
        '--add-data', ((Join-Path $navResources 'navstudio') + ':navstudio'),
        '--add-data', ((Join-Path $navResources 'src/game/surfaceflags.h') + ':src/game'),
        '--add-data', ($navLicense + ':licenses/recast'),
        '--add-binary', ($navRecast + ':bin'),
        'navstudio/entry.py')
    & $Python @navArguments
    if ($LASTEXITCODE -ne 0) { throw 'NavStudio package build failed.' }
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'README.md') -Destination $navDestination -Force
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'verify-package.ps1') -Destination $navDestination -Force
    $navLauncher = (Get-Content -LiteralPath (Join-Path $PSScriptRoot 'Package-Start.cmd') -Raw).Replace('@PACKAGE_NAME@', $PackageName)
    [IO.File]::WriteAllText((Join-Path $navDestination 'Start.cmd'), $navLauncher, [Text.Encoding]::ASCII)
    & $Python (Join-Path $PSScriptRoot 'package_preflight.py') create --package $navDestination --workspace $navWorkspace --recast $navRecast --name $PackageName
    if ($LASTEXITCODE -ne 0) { throw 'NavStudio desktop package preflight failed. Do not install this package.' }
    # Separater kleiner Weiterleiter fÃƒÂ¼r den bisherigen dist-Pfad. Kein
    # aktives Paket und keine Testinstallation werden beim Bauen verÃƒÂ¤ndert.
    $navRedirect = Join-Path $navDestinationRoot 'legacy-redirect'
    New-Item -ItemType Directory -Force -Path $navRedirect | Out-Null
    $navCompiler = Join-Path $env:WINDIR 'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
    if (!(Test-Path -LiteralPath $navCompiler)) { $navCompiler = Join-Path $env:WINDIR 'Microsoft.NET/Framework/v4.0.30319/csc.exe' }
    if (!(Test-Path -LiteralPath $navCompiler)) { throw 'Windows C# compiler for the small legacy launcher is missing.' }
    & $navCompiler /nologo /target:winexe /optimize+ /reference:System.Web.Extensions.dll (('/out:' + (Join-Path $navRedirect 'TCE2-NavStudio.exe'))) (Join-Path $PSScriptRoot 'legacy-redirect.cs')
    if ($LASTEXITCODE -ne 0) { throw 'Legacy redirect launcher compilation failed.' }
    Write-Host "Ready: $navDestination/$PackageName.exe"
    Write-Host 'Install only after stopping Studio: copy the package CONTENTS beside ET.exe/etl.exe in the existing test installation.'
    Write-Host "Legacy redirect only: $navRedirect/TCE2-NavStudio.exe (for dist/TCE2-NavStudio)."
} finally { $env:PYTHONPATH = $navPreviousPythonPath; Pop-Location }
