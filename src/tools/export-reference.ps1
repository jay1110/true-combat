param([string]$GhidraHome)
$ErrorActionPreference = 'Stop'
$repository = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
if (-not $GhidraHome) {
    $lastRun = Join-Path $env:APPDATA 'ghidra/lastrun'
    if (Test-Path -LiteralPath $lastRun) { $GhidraHome = (Get-Content -LiteralPath $lastRun -Raw).Trim() }
}
if (-not $GhidraHome) { throw 'Pass -GhidraHome with your Ghidra installation directory.' }
$headless = Join-Path $GhidraHome 'support/analyzeHeadless.bat'
if (-not (Test-Path -LiteralPath $headless)) { throw "Ghidra headless launcher missing: $headless" }
$project = Join-Path $repository 'build/ghidra-reference'
$output = Join-Path $repository 'build/reference'
New-Item -ItemType Directory -Force -Path $project | Out-Null
$arguments = @($project, 'tce-linux', '-overwrite')
foreach ($module in @('ui', 'cgame', 'qagame')) {
    $arguments += @('-import', (Join-Path $repository "tcetest/$module.mp.i386.so"))
}
$arguments += @('-scriptPath', (Join-Path $PSScriptRoot 'ghidra'),
    '-postScript', 'ExportTceReference.java', $output, '-max-cpu', '4')
& $headless @arguments
if ($LASTEXITCODE -ne 0) { throw 'Ghidra reference export failed.' }
Write-Host "Decompiler references (not buildable C): $output"
