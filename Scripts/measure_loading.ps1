param(
    [string]$LogPath = (Join-Path $PSScriptRoot '../Saved/Logs/Third_Person_RPG.log')
)

$ErrorActionPreference = 'Stop'
$logFile = Get-Item -LiteralPath $LogPath
$lines = Get-Content -LiteralPath $logFile.FullName
$preloadEvents = @()
$mapLoads = @()
$activeLoad = $null
foreach ($line in $lines) {
    if ($line -match '\[Preload\] (.+)$') {
        $preloadEvents += $line
    }
    if ($line -match 'LogLoad: LoadMap: (\S+)') {
        $activeLoad = [ordered]@{
            Map = $Matches[1]
            Seconds = $null
            TextureBuilds = 0
            StaticMeshBuilds = 0
            SkeletalMeshBuilds = 0
            AnimationBuilds = 0
            AudioBuilds = 0
        }
    }
    if ($null -ne $activeLoad) {
        if ($line -match 'LogTexture:.*Building textures:') { $activeLoad.TextureBuilds++ }
        if ($line -match 'LogStaticMesh:.*Building static mesh ') { $activeLoad.StaticMeshBuilds++ }
        if ($line -match 'LogSkeletalMesh: Building Skeletal Mesh ') { $activeLoad.SkeletalMeshBuilds++ }
        if ($line -match 'Building compressed animation data') { $activeLoad.AnimationBuilds++ }
        if ($line -match 'Building compressed audio format') { $activeLoad.AudioBuilds++ }
        if ($line -match 'LogLoad: Took ([0-9.]+) seconds to LoadMap\(([^)]+)\)') {
            $activeLoad.Seconds = [double]::Parse($Matches[1], [Globalization.CultureInfo]::InvariantCulture)
            $activeLoad.Map = $Matches[2]
            $mapLoads += [pscustomobject]$activeLoad
            $activeLoad = $null
        }
    }
}
[pscustomobject]@{
    LogPath = $logFile.FullName
    LogLastWriteTime = $logFile.LastWriteTime.ToString('o')
    HasNewPreloaderEvidence = $preloadEvents.Count -gt 0
    PreloadEvents = $preloadEvents
    MapLoads = $mapLoads
    Note = 'LoadMap duration excludes background work after map entry. No preload events means this log does not verify the new preloader.'
} | ConvertTo-Json -Depth 5
