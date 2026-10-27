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
    'mapping source'
)) {
    if ($ui -notmatch [regex]::Escape($token) -and $version -notmatch [regex]::Escape($token)) {
        throw "Source mapping contract token is missing: $token"
    }
}

Write-Output 'source_mapping_contract: PASS'
