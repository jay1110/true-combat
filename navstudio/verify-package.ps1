param([string]$Package = $PSScriptRoot, [string]$Workspace = '')
$ErrorActionPreference = 'Stop'
function Get-NavPackageHash([string]$Path) {
    $navDigest = [Security.Cryptography.SHA256]::Create()
    $navStream = [IO.File]::OpenRead($Path)
    try { return [BitConverter]::ToString($navDigest.ComputeHash($navStream)).Replace('-', '').ToLowerInvariant() }
    finally { $navStream.Dispose(); $navDigest.Dispose() }
}
try {
    $navPackageRoot = [IO.Path]::GetFullPath($Package)
    $navManifest = Get-Content -LiteralPath (Join-Path $navPackageRoot 'navstudio-package.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($navManifest.schema -ne 1 -or $navManifest.entry_kind -ne 'desktop-projects' -or !$navManifest.package_files) {
        throw 'Dieses NavStudio-Paket ist veraltet. Bitte das aktuelle Desktop-Paket bauen/installieren.'
    }
    $navGroups = @(@{ Root = $navPackageRoot; Files = $navManifest.package_files })
    if ($Workspace) { $navGroups += @{ Root = [IO.Path]::GetFullPath($Workspace); Files = $navManifest.source_files } }
    foreach ($navGroup in $navGroups) {
        $navPrefix = $navGroup.Root.TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
        foreach ($navRecord in $navGroup.Files) {
            if (!$navRecord.path -or [IO.Path]::IsPathRooted($navRecord.path) -or $navRecord.path -match '(^|/)\.\.(/|$)|:|\\') {
                throw 'Ungueltiger Paketpfad im Manifest.'
            }
            $navFile = [IO.Path]::GetFullPath((Join-Path $navGroup.Root $navRecord.path))
            if (!$navFile.StartsWith($navPrefix, [StringComparison]::OrdinalIgnoreCase) -or !(Test-Path -LiteralPath $navFile -PathType Leaf)) {
                throw ('Paketdatei fehlt: ' + $navRecord.path)
            }
            if ($navRecord.sha256 -notmatch '^[0-9a-f]{64}$' -or (Get-NavPackageHash $navFile) -ne $navRecord.sha256) {
                throw ('Paket/Quellen sind nicht aktuell: ' + $navRecord.path + '. Bitte neu bauen/installieren.')
            }
        }
    }
    Write-Output 'Aktuelles Desktop-Paket geprueft.'
    exit 0
} catch { Write-Error $_; exit 1 }
