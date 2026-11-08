param(
    [string]$Instance = '',
    [ValidateSet('ET', 'ETL')][string]$Engine = 'ET',
    [Alias('Map')][string]$MapName = '',
    [int]$GameType = 0,
    [int]$Bots = -1,
    [switch]$NoDialog
)

# One visible listen client in the existing installation. No editor/input lock.
$ErrorActionPreference = 'Stop'
$launchMutex = $null
$mutexHeld = $false
function Show-LaunchMessage([string]$Text) {
    if ($NoDialog) { Write-Output $Text; return }
    Add-Type -AssemblyName System.Windows.Forms
    [void][System.Windows.Forms.MessageBox]::Show($Text, 'TCE2 Testspiel')
}
function Send-TestCommand([string]$Text) {
    Start-Sleep -Milliseconds 700
    $socket = New-Object Net.Sockets.UdpClient
    try {
        $socket.Client.ReceiveTimeout = 250
        $socket.Connect('127.0.0.1', 27986)
        $prefix = [byte[]](255, 255, 255, 255)
        $body = [Text.Encoding]::ASCII.GetBytes("rcon navgen_local $Text`n")
        [void]$socket.Send(($prefix + $body), ($prefix.Length + $body.Length))
        $endpoint = New-Object Net.IPEndPoint([Net.IPAddress]::Any, 0)
        $answer = ''
        while ($true) {
            try {
                $packet = [Text.Encoding]::GetEncoding(28591).GetString($socket.Receive([ref]$endpoint))
                $answer += $packet -replace '^\xFF{4}print\r?\n', ''
            }
            catch [Net.Sockets.SocketException] { break }
        }
        return $answer
    } finally { $socket.Dispose() }
}
function Save-LaunchStatus([string]$Phase, [string]$Detail = '') {
    [ordered]@{
        phase = $Phase; engine = $Engine; map = $MapName; gametype = $GameType
        bots = $Bots; pid = $gameProcess.Id; detail = $Detail
        updated_utc = [DateTime]::UtcNow.ToString('o')
    } | ConvertTo-Json | Set-Content -LiteralPath $statusPath -Encoding UTF8
}
try {
    if (!$Instance) {
        if (Test-Path -LiteralPath (Join-Path $PSScriptRoot 'ET.exe')) { $Instance = $PSScriptRoot }
        else { $Instance = Join-Path $PSScriptRoot '../../build/navgen-overkill' }
    }
    $instancePath = (Resolve-Path -LiteralPath $Instance).Path.TrimEnd('\')
    foreach ($required in @('ET.exe', 'etmain', 'tce2', 'etl-home')) {
        if (!(Test-Path -LiteralPath (Join-Path $instancePath $required))) {
            throw "Die vorhandene Testinstanz ist unvollstaendig: $required fehlt."
        }
    }
    $settingsPath = Join-Path $instancePath 'Testspiel-Einstellungen.json'
    $settings = if (Test-Path -LiteralPath $settingsPath) {
        Get-Content -LiteralPath $settingsPath -Raw | ConvertFrom-Json
    } else { [pscustomobject]@{ MapName = 'obj_snow'; GameType = 2; Bots = 6; ETLegacyPath = '' } }
    if (!$MapName) { $MapName = [string]$settings.MapName }
    if (!$GameType) { $GameType = [int]$settings.GameType }
    if ($Bots -lt 0) { $Bots = [int]$settings.Bots }
    if ($MapName -notmatch '^[A-Za-z0-9_-]{1,64}$' -or $GameType -notin @(2,5,7) -or $Bots -lt 0 -or $Bots -gt 24) {
        throw 'Ungueltige Testeinstellungen: Mapname, GameType (2/5/7) oder Bots (0..24) pruefen.'
    }
    $hasher = [Security.Cryptography.SHA256]::Create()
    try { $key = [BitConverter]::ToString($hasher.ComputeHash([Text.Encoding]::UTF8.GetBytes($instancePath.ToLowerInvariant()))).Replace('-', '') }
    finally { $hasher.Dispose() }
    $launchMutex = [Threading.Mutex]::new($false, ('Local\TCE2_TestGame_' + $key))
    try { $mutexHeld = $launchMutex.WaitOne(0) }
    catch [Threading.AbandonedMutexException] { $mutexHeld = $true }
    if (!$mutexHeld) { throw 'Das Testspiel wird bereits gestartet. Bitte kurz warten.' }
    $existing = @(Get-CimInstance Win32_Process -Filter "Name='ET.exe' OR Name='etl.exe' OR Name='etl64.exe'" |
        Where-Object { $_.ExecutablePath -eq (Join-Path $instancePath 'ET.exe') -or
            ($_.CommandLine -and $_.CommandLine.IndexOf($instancePath, [StringComparison]::OrdinalIgnoreCase) -ge 0) })
    if ($existing.Count) {
        Show-LaunchMessage 'Das Testspiel laeuft bereits. Zum Wechsel zwischen ET und ETLegacy zuerst das laufende Spiel ueber das Spielmenue beenden.'
        exit 0
    }
    $gameExe = Join-Path $instancePath 'ET.exe'
    if ($Engine -eq 'ETL') {
        $candidates = @([string]$settings.ETLegacyPath,
            (Join-Path ${env:ProgramFiles(x86)} 'ETLegacy/etl64.exe'),
            (Join-Path $env:ProgramFiles 'ETLegacy/etl64.exe'),
            (Join-Path ${env:ProgramFiles(x86)} 'ETLegacy/etl.exe'))
        $gameExe = $candidates | Where-Object { $_ -and (Test-Path -LiteralPath $_ -PathType Leaf) } | Select-Object -First 1
        if (!$gameExe) { throw 'ETLegacy fehlt. Den Pfad in Testspiel-Einstellungen.json unter ETLegacyPath eintragen.' }
    }
    $testHome = Join-Path $instancePath 'etl-home/tce2'
    $profileName = if ($Engine -eq 'ET') { 'TCE2TestPlay' } else { 'TCE2TestPlayETL' }
    $profileDirectory = Join-Path $testHome ('profiles/' + $profileName)
    if (!(Test-Path -LiteralPath (Join-Path $profileDirectory 'etconfig.cfg'))) {
        New-Item -ItemType Directory -Path $profileDirectory -Force | Out-Null
        $seed = Join-Path $testHome 'profiles/TCE2TestPlay/etconfig.cfg'
        if (!(Test-Path -LiteralPath $seed)) { $seed = Join-Path $testHome 'profiles/NavStudioTest/etconfig.cfg' }
        if (Test-Path -LiteralPath $seed) {
            $text = Get-Content -LiteralPath $seed -Raw -Encoding Default
            if ($Engine -eq 'ET') { $text = $text.Replace('LEFTALT','ALT').Replace('LEFTCTRL','CTRL').Replace('LEFTSHIFT','SHIFT') }
            [IO.File]::WriteAllText((Join-Path $profileDirectory 'etconfig.cfg'), $text, [Text.Encoding]::GetEncoding(28591))
        }
        [IO.File]::WriteAllText((Join-Path $profileDirectory 'profile.dat'), ('"' + $profileName + '"'), [Text.Encoding]::ASCII)
    }
    $launcherDirectory = Join-Path $testHome 'launcher'
    New-Item -ItemType Directory -Path $launcherDirectory -Force | Out-Null
    $statusPath = Join-Path $launcherDirectory ($Engine + '-status.json')
    $configName = $Engine.ToLowerInvariant() + '-test.cfg'
    $configuration = @(
        'set sv_official 0', ('set g_gametype ' + $GameType), 'set sv_pure 0',
        'set g_doWarmup 0', 'set g_warmup 0', 'set g_useralliedrespawntime 1',
        'set g_useraxisrespawntime 1', 'set timelimit 999', 'set g_redlimbotime 1000',
        'set g_bluelimbotime 1000', 'set rconpassword navgen_local',
        'set r_maxpolyverts 16384', 'set r_maxpolys 4096',
        'bind F10 toggleconsole', 'bind n noclip', 'bind k kill', 'exec omnibot-release.cfg'
    )
    [IO.File]::WriteAllText((Join-Path $launcherDirectory $configName), (($configuration -join "`n") + "`n"), [Text.Encoding]::ASCII)
    $arguments = @('+set','com_ignorecrash','1','+set','com_recommendedSet','1',
        '+set','fs_basepath',('"'+$instancePath+'"'),'+set','fs_homepath',('"'+(Join-Path $instancePath 'etl-home')+'"'),
        '+set','fs_game','tce2','+set','cl_profile',$profileName,
        '+set','g_gametype',"$GameType",'+set','sv_official','0','+set','dedicated','0',
        '+set','net_ip','127.0.0.1','+set','net_port','27986',
        '+set','com_hunkMegs','512','+set','com_soundMegs','256',
        '+set','in_mouse','1','+set','s_initsound','1','+set','r_fullscreen','0',
        '+exec',('launcher/'+$configName),'+devmap',$MapName)
    $gameProcess = Start-Process -FilePath $gameExe -WorkingDirectory $instancePath -ArgumentList $arguments -WindowStyle Normal -PassThru
    $gameProcess.Id | Set-Content -LiteralPath (Join-Path $instancePath 'client.pid') -Encoding ASCII
    Save-LaunchStatus 'starting'
    $started = [DateTime]::UtcNow
    $ready = $false
    while (([DateTime]::UtcNow-$started).TotalSeconds -lt 75) {
        if ($gameProcess.HasExited) { throw 'Das Spiel wurde waehrend des Starts beendet. Siehe etl-home/tce2/etconsole.log.' }
        $status = Send-TestCommand 'status'
        if (([DateTime]::UtcNow-$started).TotalSeconds -ge 12 -and $status -match ('map\s*:\s*'+[regex]::Escape($MapName)+'\b')) {
            $ready = $true; break
        }
    }
    if (!$ready) { throw 'Das sichtbare Spiel antwortet noch nicht auf Port27986. Spielkonsole und etconsole.log pruefen.' }
    $actualType = Send-TestCommand 'g_gametype'
    if ($actualType -notmatch ('is:\s*"'+$GameType+'(?:\^|"|\s)')) {
        throw 'Der gestartete Gametype stimmt nicht mit der Auswahl ueberein. Die Mapunterstuetzung pruefen.'
    }
    for ($index = 0; $index -lt $Bots; ++$index) {
        $teamNumber = 1+($index % 2)
        $classNumber = 1+([int][math]::Floor($index/2) % 3)
        $reply = Send-TestCommand ("bot addbot $teamNumber $classNumber TestBot"+($index+1))
        if ($reply -match 'No navigation file loaded|Omni-bot not loaded|Command unavailable') {
            throw 'Omni-bot oder die Navigation konnte nicht geladen werden. Siehe Spielkonsole.'
        }
    }
    Start-Sleep -Seconds 2
    # Map/round initialization can restore the map's own limits.
    foreach ($command in @('set timelimit 999', 'set g_useralliedrespawntime 1',
        'set g_useraxisrespawntime 1', 'set g_redlimbotime 1000', 'set g_bluelimbotime 1000')) {
        [void](Send-TestCommand $command)
    }
    Save-LaunchStatus 'ready' (Send-TestCommand 'status')
} catch {
    if ($statusPath -and $gameProcess) { Save-LaunchStatus 'error' $_.Exception.Message }
    Show-LaunchMessage $_.Exception.Message
    exit 1
} finally {
    if ($mutexHeld) { $launchMutex.ReleaseMutex() }
    if ($launchMutex) { $launchMutex.Dispose() }
}
