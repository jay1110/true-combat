param(
    [string]$Instance = $PSScriptRoot,
    [Parameter(Mandatory=$true)][ValidatePattern('^[A-Za-z0-9_-]{1,64}$')][string]$MapName,
    [string]$GameExecutable = '',
    [ValidateSet('tce2','omnibot')][string]$GameMod = 'tce2',
    [ValidateSet(2,5,7)][int]$GameType = 2
)
# TCE_NAVSTUDIO_VISIBLE_V1: single existing installation, visible listen client.
$ErrorActionPreference = 'Stop'
$navHome = (Resolve-Path -LiteralPath $Instance).Path
if ($GameExecutable) {
    if (![IO.Path]::IsPathRooted($GameExecutable)) { throw 'GameExecutable must be an absolute path.' }
    $navExe = (Resolve-Path -LiteralPath $GameExecutable).Path
    if ((Split-Path -Parent $navExe) -ne $navHome -or (Split-Path -Leaf $navExe) -notin @('ET.exe', 'etl.exe')) {
        throw 'GameExecutable must be ET.exe or etl.exe directly inside the configured test installation.'
    }
} else { $navExe = Join-Path $navHome 'ET.exe' }
$navBaseConfig = if ($GameMod -eq 'omnibot') { 'navstudio-base.cfg' } else { 'navstress.cfg' }
if (!(Test-Path -LiteralPath $navExe) -or !(Test-Path -LiteralPath (Join-Path $navHome ($GameMod + '/' + $navBaseConfig)))) {
    throw 'Existing test installation/config missing. No game copy was created.'
}
$navHash = [Security.Cryptography.SHA256]::Create()
$navKey = [BitConverter]::ToString($navHash.ComputeHash([Text.Encoding]::UTF8.GetBytes($navHome.ToLowerInvariant()))).Replace('-', '')
$navHash.Dispose()
$navMutex = [Threading.Mutex]::new($false, ('Local\TCE2_NavStudio_' + $navKey))
if (!$navMutex.WaitOne(0)) { throw 'Test launch already in progress.' }
function Get-NavConfigHash([string]$Path) {
    # Funktioniert auch im eingebetteten Starter ohne geladenes Utility-Modul.
    $navHasher = [Security.Cryptography.SHA256]::Create()
    $navStream = [IO.File]::OpenRead($Path)
    try { [BitConverter]::ToString($navHasher.ComputeHash($navStream)).Replace('-', '') }
    finally { $navStream.Dispose(); $navHasher.Dispose() }
}
try {
    $navExisting = @(Get-Process -Name ET, etl -ErrorAction SilentlyContinue | Where-Object {
        $_.Path -and (Split-Path -Parent $_.Path) -eq $navHome
    })
    if ($navExisting.Count -gt 0) { throw 'Existing test process still running. NavStudio must reuse or stop this exact instance first.' }
    # Vor dem ersten Engine-Schreibzugriff vorhandene Eingabekonfigurationen
    # bytegenau sichern. Keine Benutzerbelegungen oder Profile überschreiben.
    $navConfigFiles = @()
    foreach ($navGame in @('etmain', $GameMod)) {
        $navConfigRoot = Join-Path $navHome $navGame
        foreach ($navConfigName in @('etconfig.cfg', 'autoexec.cfg', 'default.cfg')) {
            $navConfigPath = Join-Path $navConfigRoot $navConfigName
            if (Test-Path -LiteralPath $navConfigPath -PathType Leaf) { $navConfigFiles += Get-Item -LiteralPath $navConfigPath }
        }
        $navProfiles = Join-Path $navConfigRoot 'profiles'
        if (Test-Path -LiteralPath $navProfiles -PathType Container) {
            $navConfigFiles += Get-ChildItem -LiteralPath $navProfiles -Filter '*.cfg' -File -Recurse
        }
    }
    $navBackupRoot = Join-Path $navHome ($GameMod + '/navstudio-input-backups/launch-' + [Guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $navBackupRoot | Out-Null
    $navSavedConfigs = @()
    foreach ($navConfigFile in $navConfigFiles) {
        $navRelative = $navConfigFile.FullName.Substring($navHome.Length).TrimStart('\', '/')
        $navDestination = Join-Path $navBackupRoot $navRelative
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $navDestination) | Out-Null
        Copy-Item -LiteralPath $navConfigFile.FullName -Destination $navDestination
        $navOriginalHash = Get-NavConfigHash $navConfigFile.FullName
        if ((Get-NavConfigHash $navDestination) -ne $navOriginalHash) { throw 'Configuration backup verification failed.' }
        $navSavedConfigs += @{path=$navRelative; sha256=$navOriginalHash; bytes=$navConfigFile.Length}
    }
    @{schema=1; instance=$navHome; executable=$navExe; files=$navSavedConfigs} | ConvertTo-Json -Depth 5 |
        Set-Content -LiteralPath (Join-Path $navBackupRoot 'manifest.json') -Encoding UTF8
    # Keep the existing test settings, but select the map only once below.
    $navConfig = @(Get-Content -LiteralPath (Join-Path $navHome ($GameMod + '/' + $navBaseConfig)) |
        Where-Object { $_ -notmatch '^\s*(map|devmap|map_restart|unbindall)\b' })
    # Separate test-only Lua module: preserve existing telemetry/admin modules.
    $navLuaModules = @()
    foreach ($navLine in $navConfig) {
        if ($navLine -match '^\s*seta?\s+lua_modules\s+(.+)$') {
            $navLuaValue = $Matches[1].Trim()
            if ($navLuaValue -match '^"([^"]*)"') { $navLuaValue = $Matches[1] }
            else { $navLuaValue = $navLuaValue -replace '\s+//.*$', '' }
            $navLuaModules = @($navLuaValue -split '[;\s]+' | Where-Object { $_ -and $_ -ne 'navstudio-display.lua' })
        }
    }
    if ($GameMod -eq 'tce2') {
        $navLuaModules += 'navstudio-display.lua'
        Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'navstudio-display.lua') -Destination (Join-Path $navHome ($GameMod + '/navstudio-display.lua')) -Force
        $navConfig += @(('set lua_modules "' + ($navLuaModules -join ' ') + '"'), 'set navstudio_waypoint_view 1', 'set navstudio_editor 1')
    }
    # Keep the requested renderer setting and a short debug view after older cfgs.
    $navConfig += @(('set g_gametype ' + $GameType), 'set sv_official 0')
    $navConfig += @('set r_maxpolyverts 16384', 'set r_maxpolys 4096', 'set cg_omnibot_render_distance 800')
    $navConfig += @('bind n noclip', 'bind k kill', 'bind ESCAPE togglemenu', 'bind ` toggleconsole', 'set in_mouse 1', 'set g_useralliedrespawntime 1', 'set g_useraxisrespawntime 1', 'set timelimit 999', 'set g_timelimit 999', 'set g_redlimbotime 1000', 'set g_bluelimbotime 1000')
    # Authoring addon is installed only by this explicit local test launcher.
    # It is never part of the public mod/client PK3.
    if ($GameMod -eq 'tce2') {
        $navEditor = Join-Path $PSScriptRoot 'editor-ui'
        if (!(Test-Path -LiteralPath (Join-Path $navEditor 'tce_omnibot.menu'))) { throw 'Local editor assets missing.' }
        # Copying a desktop upgrade can leave removed pages beside the new
        # ones. Only this generated manifest belongs in the editor PK3.
        $navEditorManifest = Join-Path $navEditor 'editor-files.txt'
        if (!(Test-Path -LiteralPath $navEditorManifest)) { throw 'Local editor asset manifest missing; rebuild NavStudio.' }
        $navEditorNames = @(Get-Content -LiteralPath $navEditorManifest | Where-Object { $_ })
        if (!$navEditorNames.Count -or @($navEditorNames | Sort-Object -Unique).Count -ne $navEditorNames.Count) { throw 'Invalid local editor asset manifest.' }
        foreach ($navEditorName in $navEditorNames) {
            if ($navEditorName -notmatch '^(?:tce_[A-Za-z0-9_]+\.menu|wm_(?:ft)?quickmessage(?:Alt)?\.menu|menus(?:_gt[257])?\.txt)$' -or
                !(Test-Path -LiteralPath (Join-Path $navEditor $navEditorName) -PathType Leaf)) { throw ('Invalid or missing local editor asset: ' + $navEditorName) }
        }
        Add-Type -AssemblyName System.IO.Compression
        $navAddon = Join-Path $navHome 'tce2/zz_tce2_navstudio_editor.pk3'
        $navStream = [IO.File]::Open($navAddon, [IO.FileMode]::Create)
        $navZip = [IO.Compression.ZipArchive]::new($navStream, [IO.Compression.ZipArchiveMode]::Create)
        try {
            foreach ($navEditorName in $navEditorNames) {
                $navEntry = $navZip.CreateEntry('ui/' + $navEditorName)
                $navOutput = $navEntry.Open()
                $navInput = [IO.File]::OpenRead((Join-Path $navEditor $navEditorName))
                try { $navInput.CopyTo($navOutput) } finally { $navInput.Dispose(); $navOutput.Dispose() }
            }
        } finally { $navZip.Dispose(); $navStream.Dispose() }
    }
    $navConfig | Set-Content -LiteralPath (Join-Path $navHome ($GameMod + '/navstudio-test.cfg')) -Encoding ASCII
    $navClient = Start-Process -FilePath $navExe -WorkingDirectory $navHome -ArgumentList @(
        '+set', 'com_hunkMegs', '512', '+set', 'com_zoneMegs', '64', '+set', 'com_soundMegs', '64',
        # Skip the engine's first-run recommendation/preset + vid_restart.
        '+set', 'com_recommendedSet', '1', '+set', 'com_introPlayed', '1',
        '+set', 'r_maxpolyverts', '16384', '+set', 'r_maxpolys', '4096',
        '+set', 'cg_omnibot_render_distance', '800',
        '+set', 'fs_basepath', ('"' + $navHome + '"'), '+set', 'fs_homepath', ('"' + $navHome + '"'),
        '+set', 'fs_game', $GameMod, '+set', 'dedicated', '0', '+set', 'net_ip', '127.0.0.1',
        '+set', 'net_port', '27986', '+set', 's_initsound', '0', '+set', 'r_fullscreen', '0',
        '+set', 'r_mode', '-1', '+set', 'r_customwidth', '1280', '+set', 'r_customheight', '720',
        '+set', 'name', 'NavStudioObserver',
        # The visible game always keeps normal mouse and keyboard control.
        '+exec', 'navstudio-test.cfg', '+set', 'g_gametype', $GameType, '+set', 'sv_official', '0', '+devmap', $MapName
    ) -WindowStyle Normal -PassThru
    $navClient.Id | Set-Content -LiteralPath (Join-Path $navHome 'client.pid')
    [pscustomobject]@{pid=$navClient.Id; mode='listen'; map=$MapName; visible=$true; executable=$navExe; input_backup=$navBackupRoot} | ConvertTo-Json -Compress
} finally {
    $navMutex.ReleaseMutex()
    $navMutex.Dispose()
}
