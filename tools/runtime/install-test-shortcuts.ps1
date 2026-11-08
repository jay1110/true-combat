param([string]$Instance = (Join-Path $PSScriptRoot '../../build/navgen-overkill'))

# Install launchers into the sole existing test installation; no game copy.
$ErrorActionPreference = 'Stop'
$instancePath = (Resolve-Path -LiteralPath $Instance).Path.TrimEnd('\')
if (!(Test-Path -LiteralPath (Join-Path $instancePath 'ET.exe'))) {
    throw 'Existing ET test installation missing.'
}
$launcherSource = Join-Path $PSScriptRoot 'start-test-game.ps1'
$launcherTarget = Join-Path $instancePath 'start-test.ps1'
$archiveDirectory = Join-Path $PSScriptRoot '../../src/reconstruction/modernization/cleanup-20261108/launchers'
$archiveDirectory = [IO.Path]::GetFullPath($archiveDirectory)
New-Item -ItemType Directory -Path $archiveDirectory -Force | Out-Null
if ((Test-Path -LiteralPath $launcherTarget) -and
    (Get-FileHash -LiteralPath $launcherTarget).Hash -ne (Get-FileHash -LiteralPath $launcherSource).Hash) {
    $oldHash = (Get-FileHash -LiteralPath $launcherTarget).Hash.ToLowerInvariant()
    Copy-Item -LiteralPath $launcherTarget -Destination (Join-Path $archiveDirectory ($oldHash + '-start-test.ps1'))
}
Copy-Item -LiteralPath $launcherSource -Destination $launcherTarget -Force
$settingsPath = Join-Path $instancePath 'Testspiel-Einstellungen.json'
if (!(Test-Path -LiteralPath $settingsPath)) {
    [ordered]@{ MapName='obj_snow'; GameType=2; Bots=6; ETLegacyPath='' } |
        ConvertTo-Json | Set-Content -LiteralPath $settingsPath -Encoding UTF8
}
$desktopDirectory = [Environment]::GetFolderPath('Desktop')
$shell = New-Object -ComObject WScript.Shell
$powershellExe = Join-Path $env:SystemRoot 'System32/WindowsPowerShell/v1.0/powershell.exe'
$created = @()
foreach ($engineName in @('ET','ETL')) {
    $wrapper = @('@echo off','setlocal',
        ('powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0start-test.ps1" -Engine ' + $engineName),
        'if errorlevel 1 (','  echo Testspiel konnte nicht gestartet werden.','  pause','  exit /b 1',')') -join "`r`n"
    [IO.File]::WriteAllText((Join-Path $instancePath ('Start-' + $engineName + '.cmd')), $wrapper + "`r`n", [Text.Encoding]::ASCII)
    $label = if ($engineName -eq 'ET') { 'TCE2 Testspiel - ET 2.60b' } else { 'TCE2 Testspiel - ETLegacy' }
    $icon = Join-Path $instancePath 'ET.exe'
    if ($engineName -eq 'ETL') {
        $legacyIcon = Join-Path ${env:ProgramFiles(x86)} 'ETLegacy/etl64.exe'
        if (Test-Path -LiteralPath $legacyIcon) { $icon = $legacyIcon }
    }
    foreach ($directory in @($desktopDirectory, $instancePath)) {
        $linkPath = Join-Path $directory ($label + '.lnk')
        if (Test-Path -LiteralPath $linkPath) {
            $oldHash = (Get-FileHash -LiteralPath $linkPath).Hash.ToLowerInvariant()
            Copy-Item -LiteralPath $linkPath -Destination (Join-Path $archiveDirectory ($oldHash + '-' + $label + '.lnk')) -Force
        }
        $link = $shell.CreateShortcut($linkPath)
        $link.TargetPath = $powershellExe
        $link.Arguments = '-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File "' + $launcherTarget + '" -Engine ' + $engineName
        $link.WorkingDirectory = $instancePath
        $link.IconLocation = $icon + ',0'
        $link.Description = 'TCE2 Testspiel: Map, Gametype und Botzahl in Testspiel-Einstellungen.json. Volle Maus- und Tastatursteuerung.'
        $link.WindowStyle = 7
        $link.Save()
        $created += $linkPath
    }
}
$readme = @'
TCE2 TESTSPIEL

Starten: Auf dem Desktop oder hier im Ordner die Verknuepfung
  TCE2 Testspiel - ET 2.60b
  TCE2 Testspiel - ETLegacy
doppelt anklicken. Alternativ Start-ET.cmd / Start-ETL.cmd.

Beide verwenden diese eine vorhandene Spielinstallation. Vor einem Wechsel
zwischen ET und ETLegacy das laufende Spiel ueber Exit beenden.
Das Spiel bleibt mit Maus und Tastatur voll bedienbar.

Standard: obj_snow, Gametype 2, sechs Bots. In Testspiel-Einstellungen.json
koennen MapName, GameType (2, 5 oder 7), Bots (0 bis 24) und bei Bedarf der
ETLegacyPath geaendert werden. ETLegacy wird sonst automatisch gesucht.
Map und Gametype werden direkt beim Start per devmap gesetzt.

Tasten: L Deployment, Escape Menue, F10 Konsole, N Noclip, K Kill.
Die sonstigen persoenlichen Belegungen bleiben erhalten. ET und ETLegacy
verwenden getrennte Profile, damit sich die Tastenbelegungen nicht stoeren.
Der Starter beendet sich nach dem Laden und Hinzufuegen der Bots.
Es wird kein separater Dedicated Server gestartet.

Start.cmd gehoert zum alten NavStudio-Paket, nicht zum Spielstart.
Startprobleme: etl-home/tce2/launcher/ET-status.json bzw. ETL-status.json.
'@
[IO.File]::WriteAllText((Join-Path $instancePath 'README-Testspiel.txt'), $readme.Replace("`n","`r`n") + "`r`n", [Text.Encoding]::UTF8)
$created | ForEach-Object { Write-Output $_ }
