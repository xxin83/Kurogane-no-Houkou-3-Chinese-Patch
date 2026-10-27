$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$ui = Get-Content -Raw (Join-Path $root 'src\UiDiag.cpp')
$version = Get-Content -Raw (Join-Path $root 'src\version.cpp')

foreach ($token in @(
    'TrackSourceFileMapping',
    'TrackSourceMappedView',
    'TrackSourceUnmappedView',
    'CreateFileMappingA',
    'MapViewOfFile',
    'UnmapViewOfFile',
    'mapping source',
    'SourceTrackingStats',
    'snapshots=',
    'mappings=',
    'views='
)) {
    if ($ui -notmatch [regex]::Escape($token) -and $version -notmatch [regex]::Escape($token)) {
        throw "Source mapping contract token is missing: $token"
    }
}

Write-Output 'source_mapping_contract: PASS'

$mergeStart = $ui.IndexOf('void MergeSourceSnapshotLocked')
$mergeEnd = $ui.IndexOf('std::vector<SourceCandidate>', $mergeStart)
if ($mergeStart -lt 0 -or $mergeEnd -lt 0) {
    throw 'Unable to locate source snapshot merge implementation.'
}
$mergeBody = $ui.Substring($mergeStart, $mergeEnd - $mergeStart)
if ($mergeBody -match 'g_sourceCacheBytes\s*-=' -and
    $mergeBody -match 'RemoveSourceSnapshotLocked\(i\)') {
    throw 'Merged source snapshots must not subtract cache bytes twice.'
}
